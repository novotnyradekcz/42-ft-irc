#!/usr/bin/env python3
# This ft_irc evaluation helper runs subject-oriented integration tests against
# ./ircserv. It covers registration, partial TCP command assembly, JOIN,
# PRIVMSG, channel operator commands, required channel modes, and slow-client
# backpressure. The suite is meant to provide quick confidence before and
# during peer evaluation; it is not a replacement for testing with the chosen
# reference IRC client.

import os
import socket
import subprocess
import sys
import threading
import time


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SERVER = os.path.join(ROOT, "ircserv")
PASSWORD = "pw"


class TestFailure(Exception):
    pass


class IrcClient:
    def __init__(self, port, nick=None):
        self.sock = self._connect(port)
        self.nick = nick
        if nick:
            self.send("PASS %s" % PASSWORD)
            self.send("NICK %s" % nick)
            self.send("USER %s 0 * :%s" % (nick, nick))
            self.read(0.2)

    def _connect(self, port):
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

    def send(self, line):
        self.sock.sendall((line + "\r\n").encode("ascii"))

    def send_raw(self, data):
        self.sock.sendall(data)

    def read(self, timeout=0.25):
        self.sock.settimeout(timeout)
        chunks = []
        while True:
            try:
                data = self.sock.recv(65536)
            except socket.timeout:
                break
            if not data:
                break
            chunks.append(data)
        return b"".join(chunks).decode("utf-8", "replace")

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def assert_true(condition, message):
    if not condition:
        raise TestFailure(message)


def assert_contains(text, needle, message):
    if needle not in text:
        raise TestFailure("%s\nExpected to find: %r\nActual: %r" % (message, needle, text))


def assert_not_contains(text, needle, message):
    if needle in text:
        raise TestFailure("%s\nDid not expect: %r\nActual: %r" % (message, needle, text))


def free_port():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(("0.0.0.0", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


class ServerProcess:
    def __enter__(self):
        self.port = free_port()
        self.proc = subprocess.Popen(
            [SERVER, str(self.port), PASSWORD],
            cwd=ROOT,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        time.sleep(0.15)
        if self.proc.poll() is not None:
            raise TestFailure("ircserv exited during startup")
        return self

    def __exit__(self, exc_type, exc, tb):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait(timeout=2)


def run_make():
    result = subprocess.call(["make"], cwd=ROOT)
    assert_true(result == 0, "make failed")


def test_registration_and_welcome():
    with ServerProcess() as server:
        client = IrcClient(server.port)
        client.send("PASS %s" % PASSWORD)
        client.send("NICK alice")
        client.send("USER alice 0 * :Alice")
        data = client.read(0.4)
        client.close()
        assert_contains(data, " 001 alice ", "Registered client should receive welcome numeric 001")


def test_wrong_password_blocks_registration():
    with ServerProcess() as server:
        client = IrcClient(server.port)
        client.send("PASS wrong")
        client.send("NICK bad")
        client.send("USER bad 0 * :Bad")
        client.send("JOIN #room")
        data = client.read(0.4)
        client.close()
        assert_contains(data, " 464 ", "Wrong PASS should produce ERR_PASSWDMISMATCH")
        assert_contains(data, " 451 ", "Client with wrong PASS should not be allowed to JOIN")


def test_partial_command_input():
    with ServerProcess() as server:
        client = IrcClient(server.port)
        client.send("PASS %s" % PASSWORD)
        client.send_raw(b"NI")
        time.sleep(0.05)
        client.send_raw(b"CK split\r\n")
        client.send_raw(b"USER split 0 * :Split User\r\n")
        data = client.read(0.4)
        client.close()
        assert_contains(data, " 001 split ", "Server should aggregate partial TCP data into IRC commands")


def test_channel_privmsg_goes_to_other_clients_only():
    with ServerProcess() as server:
        alice = IrcClient(server.port, "alice")
        bob = IrcClient(server.port, "bob")
        alice.send("JOIN #room")
        bob.send("JOIN #room")
        time.sleep(0.2)
        alice.read()
        bob.read()
        alice.send("PRIVMSG #room :hello")
        time.sleep(0.2)
        alice_data = alice.read()
        bob_data = bob.read()
        alice.close()
        bob.close()
        assert_contains(bob_data, "PRIVMSG #room :hello", "Channel message should reach other members")
        assert_not_contains(alice_data, "PRIVMSG #room :hello", "Subject says channel messages are forwarded to every other client")


def test_private_message():
    with ServerProcess() as server:
        alice = IrcClient(server.port, "alice")
        bob = IrcClient(server.port, "bob")
        alice.send("PRIVMSG bob :secret")
        time.sleep(0.2)
        bob_data = bob.read()
        alice.close()
        bob.close()
        assert_contains(bob_data, "PRIVMSG bob :secret", "Private message should reach target nickname")


def test_non_operator_invite_is_rejected():
    with ServerProcess() as server:
        op = IrcClient(server.port, "op")
        bob = IrcClient(server.port, "bob")
        charlie = IrcClient(server.port, "charlie")
        op.send("JOIN #room")
        bob.send("JOIN #room")
        time.sleep(0.2)
        op.read()
        bob.read()
        charlie.read()
        bob.send("INVITE charlie #room")
        time.sleep(0.2)
        bob_data = bob.read()
        charlie_data = charlie.read()
        op.close()
        bob.close()
        charlie.close()
        assert_contains(bob_data, " 482 bob #room ", "INVITE is a channel operator command")
        assert_not_contains(charlie_data, " INVITE charlie ", "Rejected INVITE must not be delivered")


def test_invite_only_channel_allows_invited_user():
    with ServerProcess() as server:
        op = IrcClient(server.port, "op")
        bob = IrcClient(server.port, "bob")
        op.send("JOIN #room")
        op.send("MODE #room +i")
        time.sleep(0.2)
        op.read()
        bob.read()
        bob.send("JOIN #room")
        denied = bob.read(0.3)
        op.send("INVITE bob #room")
        time.sleep(0.2)
        bob.read()
        bob.send("JOIN #room")
        joined = bob.read(0.4)
        op.close()
        bob.close()
        assert_contains(denied, " 473 bob #room ", "Invite-only channel should reject non-invited users")
        assert_contains(joined, " JOIN #room", "Invited user should be able to JOIN invite-only channel")


def test_topic_restriction():
    with ServerProcess() as server:
        op = IrcClient(server.port, "op")
        bob = IrcClient(server.port, "bob")
        op.send("JOIN #room")
        bob.send("JOIN #room")
        op.send("MODE #room +t")
        time.sleep(0.2)
        op.read()
        bob.read()
        bob.send("TOPIC #room :not allowed")
        denied = bob.read(0.3)
        op.send("TOPIC #room :allowed")
        time.sleep(0.2)
        bob_data = bob.read(0.3)
        op.close()
        bob.close()
        assert_contains(denied, " 482 bob #room ", "Non-operator should not change +t topic")
        assert_contains(bob_data, "TOPIC #room :allowed", "Operator should be able to change topic")


def test_channel_key_and_limit_modes():
    with ServerProcess() as server:
        op = IrcClient(server.port, "op")
        bob = IrcClient(server.port, "bob")
        carol = IrcClient(server.port, "carol")
        op.send("JOIN #locked")
        op.send("MODE #locked +k key")
        time.sleep(0.2)
        op.read()
        bob.send("JOIN #locked wrong")
        wrong_key = bob.read(0.3)
        bob.send("JOIN #locked key")
        joined = bob.read(0.4)
        op.send("MODE #locked +l 2")
        time.sleep(0.2)
        op.read()
        bob.read()
        carol.send("JOIN #locked key")
        limited = carol.read(0.3)
        op.close()
        bob.close()
        carol.close()
        assert_contains(wrong_key, " 475 bob #locked ", "Wrong channel key should be rejected")
        assert_contains(joined, " JOIN #locked", "Correct channel key should allow JOIN")
        assert_contains(limited, " 471 carol #locked ", "User limit should reject excess clients")


def test_operator_mode_and_kick():
    with ServerProcess() as server:
        op = IrcClient(server.port, "op")
        bob = IrcClient(server.port, "bob")
        carol = IrcClient(server.port, "carol")
        op.send("JOIN #room")
        bob.send("JOIN #room")
        carol.send("JOIN #room")
        time.sleep(0.3)
        op.read()
        bob.read()
        carol.read()
        bob.send("KICK #room carol :nope")
        denied = bob.read(0.3)
        op.send("MODE #room +o bob")
        time.sleep(0.2)
        bob.read()
        bob.send("KICK #room carol :bye")
        time.sleep(0.2)
        carol_data = carol.read(0.3)
        op.close()
        bob.close()
        carol.close()
        assert_contains(denied, " 482 bob #room ", "Regular user should not KICK")
        assert_contains(carol_data, "KICK #room carol :bye", "Operator granted with +o should be able to KICK")


def test_backpressure_no_message_loss():
    count = 3000
    channel = "#flood"
    with ServerProcess() as server:
        slow = IrcClient(server.port, "slow")
        sender = IrcClient(server.port, "sender")
        slow.send("JOIN %s" % channel)
        sender.send("JOIN %s" % channel)
        time.sleep(0.2)
        slow.read()
        sender.read()

        stop = threading.Event()
        sender_echoes = [0]

        def drain_sender():
            while not stop.is_set():
                data = sender.read(0.05)
                sender_echoes[0] += data.count(" PRIVMSG %s :" % channel)

        thread = threading.Thread(target=drain_sender)
        thread.daemon = True
        thread.start()

        payload = "x" * 350
        for i in range(count):
            sender.send("PRIVMSG %s :%05d-%s" % (channel, i, payload))
            if i % 500 == 0:
                time.sleep(0.01)

        deadline = time.time() + 6
        while time.time() < deadline and sender_echoes[0] < count:
            time.sleep(0.05)

        stop.set()
        thread.join(1)
        slow_data = slow.read(1.0)
        slow_count = slow_data.count(" PRIVMSG %s :" % channel)
        slow.close()
        sender.close()
        assert_true(slow_count == count, "Slow client should receive all queued channel messages: got %d/%d" % (slow_count, count))


TESTS = [
    ("make", run_make),
    ("registration_and_welcome", test_registration_and_welcome),
    ("wrong_password_blocks_registration", test_wrong_password_blocks_registration),
    ("partial_command_input", test_partial_command_input),
    ("channel_privmsg_goes_to_other_clients_only", test_channel_privmsg_goes_to_other_clients_only),
    ("private_message", test_private_message),
    ("non_operator_invite_is_rejected", test_non_operator_invite_is_rejected),
    ("invite_only_channel_allows_invited_user", test_invite_only_channel_allows_invited_user),
    ("topic_restriction", test_topic_restriction),
    ("channel_key_and_limit_modes", test_channel_key_and_limit_modes),
    ("operator_mode_and_kick", test_operator_mode_and_kick),
    ("backpressure_no_message_loss", test_backpressure_no_message_loss),
]


def main():
    selected = sys.argv[1:]
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
            print("PASS %-45s %.2fs" % (name, time.time() - start))
        except Exception as exc:
            failures += 1
            print("FAIL %-45s %s" % (name, exc))
    print("%d/%d tests passed" % (len(tests) - failures, len(tests)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
