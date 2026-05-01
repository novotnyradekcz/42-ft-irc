#!/usr/bin/env python3
# This ft_irc robustness suite focuses on the subject's reliability checks:
# no crashes, no memory leaks, and correct TCP stream aggregation. It includes
# the exact Ctrl-D style split-command scenario described in the subject
# ("com", then "man", then "d\n"), plus fragmented registration, multiple
# commands in one packet, malformed input, disconnects during partial commands,
# and a platform-selected leak check: macOS leaks or Linux valgrind.
# Usage examples:
#   python3 tests/robustness_suite.py --platform macos
#   python3 tests/robustness_suite.py --platform linux memory_leaks

import os
import shutil
import socket
import subprocess
import sys
import time


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SERVER = os.path.join(ROOT, "ircserv")
PASSWORD = "pw"
MEMORY_PLATFORM = "auto"


class TestFailure(Exception):
    pass


def assert_true(condition, message):
    if not condition:
        raise TestFailure(message)


def assert_contains(text, needle, message):
    if needle not in text:
        raise TestFailure("%s\nExpected: %r\nActual: %r" % (message, needle, text))


def assert_not_contains(text, needle, message):
    if needle in text:
        raise TestFailure("%s\nDid not expect: %r\nActual: %r" % (message, needle, text))


def free_port():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(("0.0.0.0", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


def connect_retry(port):
    deadline = time.time() + 3
    last_error = None
    while time.time() < deadline:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(1)
        try:
            sock.connect(("127.0.0.1", port))
            return sock
        except OSError as exc:
            last_error = exc
            sock.close()
            time.sleep(0.05)
    raise last_error


def read_available(sock, timeout=0.2):
    sock.settimeout(timeout)
    chunks = []
    while True:
        try:
            data = sock.recv(65536)
        except socket.timeout:
            break
        if not data:
            break
        chunks.append(data)
    return b"".join(chunks).decode("utf-8", "replace")


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode("ascii"))


def register_client(port, nick):
    sock = connect_retry(port)
    send_line(sock, "PASS %s" % PASSWORD)
    send_line(sock, "NICK %s" % nick)
    send_line(sock, "USER %s 0 * :%s" % (nick, nick))
    data = read_available(sock, 0.3)
    assert_contains(data, " 001 %s " % nick, "Client should register successfully")
    return sock


class ServerProcess:
    def __init__(self, command=None):
        self.port = free_port()
        if command is None:
            self.command = [SERVER, str(self.port), PASSWORD]
        else:
            self.command = command(self.port)
        self.proc = None

    def __enter__(self):
        self.proc = subprocess.Popen(
            self.command,
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        time.sleep(0.15)
        self.assert_alive("ircserv exited during startup")
        return self

    def __exit__(self, exc_type, exc, tb):
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(timeout=3)

    def assert_alive(self, message="ircserv crashed"):
        if self.proc.poll() is not None:
            out, err = self.proc.communicate(timeout=1)
            raise TestFailure("%s\nexit=%s\nstdout=%s\nstderr=%s" % (message, self.proc.returncode, out, err))


def run_make():
    result = subprocess.call(["make"], cwd=ROOT)
    assert_true(result == 0, "make failed")


def test_subject_ctrl_d_split_unknown_command():
    with ServerProcess() as server:
        client = register_client(server.port, "splitcmd")
        client.sendall(b"com")
        time.sleep(0.1)
        assert_true(read_available(client, 0.1) == "", "Incomplete command must not be processed after first fragment")
        client.sendall(b"man")
        time.sleep(0.1)
        assert_true(read_available(client, 0.1) == "", "Incomplete command must not be processed after second fragment")
        client.sendall(b"d\n")
        data = read_available(client, 0.4)
        client.close()
        server.assert_alive()
        assert_contains(data, " 421 splitcmd COMMAND ", "Server should rebuild 'com' + 'man' + 'd\\n' as COMMAND")


def test_fragmented_registration():
    with ServerProcess() as server:
        client = connect_retry(server.port)
        for part in [b"PA", b"SS pw\r\nNI", b"CK frag\r\nUSER fr", b"ag 0 * :Frag User\r\n"]:
            client.sendall(part)
            time.sleep(0.05)
        data = read_available(client, 0.5)
        client.close()
        server.assert_alive()
        assert_contains(data, " 001 frag ", "Server should register a client from fragmented TCP packets")


def test_many_commands_in_one_packet():
    with ServerProcess() as server:
        client = connect_retry(server.port)
        client.sendall(
            b"PASS pw\r\nNICK packed\r\nUSER packed 0 * :Packed\r\nJOIN #packed\r\nTOPIC #packed\r\n"
        )
        data = read_available(client, 0.6)
        client.close()
        server.assert_alive()
        assert_contains(data, " 001 packed ", "Server should process registration from one TCP packet")
        assert_contains(data, " JOIN #packed", "Server should process JOIN from the same TCP packet")
        assert_contains(data, " 331 packed #packed ", "Server should process following TOPIC query from the same packet")


def test_malformed_input_does_not_crash():
    with ServerProcess() as server:
        client = register_client(server.port, "rough")
        commands = [
            "JOIN",
            "JOIN room_without_hash",
            "PRIVMSG",
            "PRIVMSG #missing :hello",
            "KICK",
            "MODE",
            "MODE rough +i",
            "TOPIC",
            "INVITE",
            "UNKNOWNCOMMAND arg1 arg2",
        ]
        for command in commands:
            send_line(client, command)
        data = read_available(client, 0.6)
        client.close()
        server.assert_alive("Server crashed while handling malformed IRC commands")
        assert_contains(data, " 421 rough UNKNOWNCOMMAND ", "Unknown commands should produce ERR_UNKNOWNCOMMAND")


def test_disconnect_during_partial_command_does_not_crash():
    with ServerProcess() as server:
        client = connect_retry(server.port)
        client.sendall(b"PASS pw\r\nNICK half")
        client.close()
        time.sleep(0.2)
        server.assert_alive("Server crashed after client disconnected mid-command")
        survivor = register_client(server.port, "survivor")
        survivor.close()
        server.assert_alive("Server did not accept a new client after partial disconnect")


def test_many_connect_disconnect_cycles_do_not_crash():
    with ServerProcess() as server:
        for i in range(30):
            client = register_client(server.port, "u%d" % i)
            send_line(client, "JOIN #cycle")
            send_line(client, "PRIVMSG #cycle :hello")
            client.close()
        time.sleep(0.3)
        server.assert_alive("Server crashed during repeated connect/disconnect cycles")


def memory_platform():
    if MEMORY_PLATFORM != "auto":
        return MEMORY_PLATFORM
    if sys.platform == "darwin":
        return "macos"
    if sys.platform.startswith("linux"):
        return "linux"
    return "unsupported"


def run_memory_scenario(port):
    alice = register_client(port, "leakalice")
    bob = register_client(port, "leakbob")
    send_line(alice, "JOIN #leaks")
    send_line(bob, "JOIN #leaks")
    send_line(alice, "PRIVMSG #leaks :hello")
    send_line(alice, "MODE #leaks +i")
    send_line(alice, "INVITE bob #leaks")
    time.sleep(0.3)
    read_available(alice)
    read_available(bob)
    alice.close()
    bob.close()


def test_memory_leaks():
    platform = memory_platform()
    if platform == "macos":
        test_memory_leaks_with_macos_leaks()
    elif platform == "linux":
        test_memory_leaks_with_valgrind()
    else:
        print("SKIP memory_leaks: unsupported platform %s" % sys.platform)


def test_memory_leaks_with_macos_leaks():
    leaks = shutil.which("leaks")
    if not leaks:
        print("SKIP memory_leaks_with_macos_leaks: leaks command not found")
        return
    with ServerProcess() as server:
        run_memory_scenario(server.port)
        server.assert_alive("Server crashed before leaks inspection")

        result = subprocess.run(
            [leaks, str(server.proc.pid)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=20,
        )
        combined = result.stdout + result.stderr
        assert_contains(combined, "0 leaks for 0 total leaked bytes", "macOS leaks should report no leaked bytes")


def test_memory_leaks_with_valgrind():
    valgrind = shutil.which("valgrind")
    if not valgrind:
        print("SKIP memory_leaks_with_valgrind: valgrind command not found")
        return

    def command(port):
        return [
            valgrind,
            "--leak-check=full",
            "--show-leak-kinds=definite",
            "--errors-for-leak-kinds=definite",
            "--error-exitcode=42",
            "--",
            SERVER,
            str(port),
            PASSWORD,
        ]

    server = ServerProcess(command=command)
    server.__enter__()
    try:
        run_memory_scenario(server.port)
        server.assert_alive("Server crashed before valgrind shutdown")
        server.proc.terminate()
        out, err = server.proc.communicate(timeout=20)
        combined = out + err
        assert_true(server.proc.returncode == 0, "valgrind-wrapped server exited with code %s\n%s" % (server.proc.returncode, combined))
        assert_contains(combined, "definitely lost: 0 bytes", "Valgrind should report no definitely lost bytes")
        assert_contains(combined, "ERROR SUMMARY: 0 errors", "Valgrind should report zero errors")
    finally:
        if server.proc.poll() is None:
            server.__exit__(None, None, None)


TESTS = [
    ("make", run_make),
    ("subject_ctrl_d_split_unknown_command", test_subject_ctrl_d_split_unknown_command),
    ("fragmented_registration", test_fragmented_registration),
    ("many_commands_in_one_packet", test_many_commands_in_one_packet),
    ("malformed_input_does_not_crash", test_malformed_input_does_not_crash),
    ("disconnect_during_partial_command_does_not_crash", test_disconnect_during_partial_command_does_not_crash),
    ("many_connect_disconnect_cycles_do_not_crash", test_many_connect_disconnect_cycles_do_not_crash),
    ("memory_leaks", test_memory_leaks),
    ("memory_leaks_with_macos_leaks", test_memory_leaks_with_macos_leaks),
    ("memory_leaks_with_valgrind", test_memory_leaks_with_valgrind),
]


def parse_args(argv):
    global MEMORY_PLATFORM
    selected = []
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == "--platform":
            if i + 1 >= len(argv):
                raise TestFailure("--platform requires auto, macos, or linux")
            MEMORY_PLATFORM = argv[i + 1]
            if MEMORY_PLATFORM not in ("auto", "macos", "linux"):
                raise TestFailure("--platform requires auto, macos, or linux")
            i += 2
        elif arg.startswith("--platform="):
            MEMORY_PLATFORM = arg.split("=", 1)[1]
            if MEMORY_PLATFORM not in ("auto", "macos", "linux"):
                raise TestFailure("--platform requires auto, macos, or linux")
            i += 1
        else:
            selected.append(arg)
            i += 1
    return selected


def main():
    try:
        selected = parse_args(sys.argv[1:])
    except TestFailure as exc:
        print(exc)
        return 2
    tests = [(name, fn) for name, fn in TESTS if not selected or name in selected]
    if selected and len(tests) != len(selected):
        known = set(name for name, _ in TESTS)
        unknown = [name for name in selected if name not in known]
        print("Unknown test(s): %s" % ", ".join(unknown))
        return 2

    failures = 0
    for name, fn in tests:
        start = time.time()
        try:
            fn()
            print("PASS %-52s %.2fs" % (name, time.time() - start))
        except Exception as exc:
            failures += 1
            print("FAIL %-52s %s" % (name, exc))
    print("%d/%d tests passed" % (len(tests) - failures, len(tests)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
