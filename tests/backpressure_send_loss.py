#!/usr/bin/env python3
# This focused ft_irc stress test checks outgoing-message backpressure.
# It pauses one client by not reading from its socket, floods a channel from
# another client, then verifies that the slow client still receives every
# queued PRIVMSG. It is useful for catching direct send() calls without a
# per-client output buffer and POLLOUT-driven flushing.

import os
import socket
import subprocess
import sys
import threading
import time


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SERVER = os.path.join(ROOT, "ircserv")
PASSWORD = "pw"
CHANNEL = "#flood"
COUNT = int(os.environ.get("COUNT", "12000"))


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("0.0.0.0", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def recv_available(sock, timeout=0.2):
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
    return b"".join(chunks)


def connect_client(port, nick, read_initial=True):
    deadline = time.time() + 3
    while True:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
        s.settimeout(3)
        try:
            s.connect(("127.0.0.1", port))
            break
        except ConnectionRefusedError:
            s.close()
            if time.time() >= deadline:
                raise
            time.sleep(0.05)
    s.sendall(
        ("PASS %s\r\nNICK %s\r\nUSER %s 0 * :%s\r\nJOIN %s\r\n"
         % (PASSWORD, nick, nick, nick, CHANNEL)).encode("ascii")
    )
    if read_initial:
        time.sleep(0.2)
        recv_available(s)
    return s


def main():
    port = free_port()
    server = subprocess.Popen(
        [SERVER, str(port), PASSWORD],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        time.sleep(0.3)
        if server.poll() is not None:
            print("server exited during startup")
            return 2

        slow = connect_client(port, "slow", read_initial=True)
        sender = connect_client(port, "sender", read_initial=True)

        stop_drain = threading.Event()
        sender_seen = [0]

        def drain_sender():
            while not stop_drain.is_set():
                data = recv_available(sender, timeout=0.05)
                if data:
                    sender_seen[0] += data.count(b" PRIVMSG " + CHANNEL.encode("ascii") + b" :")

        drain_thread = threading.Thread(target=drain_sender)
        drain_thread.daemon = True
        drain_thread.start()

        payload = "x" * 350
        for i in range(COUNT):
            sender.sendall(("PRIVMSG %s :%05d-%s\r\n" % (CHANNEL, i, payload)).encode("ascii"))
            if i % 1000 == 0:
                time.sleep(0.01)

        deadline = time.time() + 8
        while time.time() < deadline and sender_seen[0] < COUNT:
            time.sleep(0.05)

        stop_drain.set()
        drain_thread.join(1)

        received = recv_available(slow, timeout=1.0)
        slow_count = received.count(b" PRIVMSG " + CHANNEL.encode("ascii") + b" :")

        print("sent=%d sender_echoes=%d slow_received=%d" % (COUNT, sender_seen[0], slow_count))
        if slow_count < COUNT:
            print("FAIL: slow client missed %d messages" % (COUNT - slow_count))
            return 1
        print("PASS: slow client received all messages")
        return 0
    finally:
        try:
            server.terminate()
            server.wait(timeout=2)
        except Exception:
            server.kill()
        try:
            slow.close()
        except Exception:
            pass
        try:
            sender.close()
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main())
