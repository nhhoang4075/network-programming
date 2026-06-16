#!/usr/bin/env python3
# Mô phỏng chat_server_test (lebavui) để kiểm thử nhanh trên macOS.
# Tái hiện đúng 45 bước, so khớp EXACT kèm '\n' (giống expect_exact),
# và expect_empty (không nhận được gì trong 1 timeout).
import socket, subprocess, sys, time

HOST, PORT = "127.0.0.1", 9001
IDLE = 0.4  # giây: coi như hết dữ liệu (mô phỏng SO_RCVTIMEO)

total = 0
failed = 0

def recv_available(s):
    s.settimeout(IDLE)
    buf = b""
    while True:
        try:
            d = s.recv(4096)
            if not d:
                break
            buf += d
        except socket.timeout:
            break
    return buf.decode(errors="replace")

def expect_exact(label, s, expected):
    global total, failed
    total += 1
    got = recv_available(s)
    if got == expected:
        print(f'[PASS] {label}: nhan dung {expected!r}')
    else:
        failed += 1
        print(f'[FAIL] {label}:\n       Expected: {expected!r}\n       Actual  : {got!r}')

def expect_empty(label, s):
    global total, failed
    total += 1
    got = recv_available(s)
    if got == "":
        print(f'[PASS] {label}: khong nhan gi (dung)')
    else:
        failed += 1
        print(f'[FAIL] {label}: dang le khong nhan gi nhung nhan {got!r}')

def send(s, text):
    s.sendall(text.encode())
    time.sleep(0.05)

def conn():
    last = None
    for _ in range(40):                 # retry tối đa ~4s cho lần kết nối đầu
        try:
            return socket.create_connection((HOST, PORT), timeout=2)
        except OSError as e:
            last = e; time.sleep(0.1)
    raise last

srv = subprocess.Popen(["./chat_server", str(PORT)],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(0.6)
try:
    alice = conn(); bob = conn(); carol = conn(); dup = conn()

    # 1-2 JOIN alice
    send(alice, "JOIN alice\n");  expect_exact("JOIN alice", alice, "100 OK\n")
    # 3-5 JOIN bob
    send(bob, "JOIN bob\n");      expect_exact("JOIN bob response", bob, "100 OK\n")
    expect_exact("JOIN bob broadcast", alice, "JOIN bob\n")
    # 6-7 duplicate
    send(dup, "JOIN bob\n");      expect_exact("JOIN duplicate nickname", dup, "200 NICKNAME IN USE\n")
    # 8-9 invalid
    send(dup, "JOIN Bob\n");      expect_exact("JOIN invalid nickname", dup, "201 INVALID NICK NAME\n")
    # 10-13 JOIN carol
    send(carol, "JOIN carol\n");  expect_exact("JOIN carol response", carol, "100 OK\n")
    expect_exact("JOIN carol broadcast to alice", alice, "JOIN carol\n")
    expect_exact("JOIN carol broadcast to bob", bob, "JOIN carol\n")
    # 14-17 MSG (bob)
    send(bob, "MSG hello room\n"); expect_exact("MSG response", bob, "100 OK\n")
    expect_exact("MSG broadcast to alice", alice, "MSG bob hello room\n")
    expect_exact("MSG broadcast to carol", carol, "MSG bob hello room\n")
    # 18-21 PMSG (alice -> bob)
    send(alice, "PMSG bob secret message\n"); expect_exact("PMSG response", alice, "100 OK\n")
    expect_exact("PMSG to bob", bob, "PMSG alice secret message\n")
    expect_empty("PMSG not broadcast to carol", carol)
    # 22-23 PMSG unknown
    send(alice, "PMSG nobody hello\n"); expect_exact("PMSG unknown nickname", alice, "202 UNKNOWN NICKNAME\n")
    # 24-25 TOPIC denied (carol is not owner)
    send(carol, "TOPIC denied topic\n"); expect_exact("TOPIC denied for non-owner", carol, "203 DENIED\n")
    # 26-29 OP bob (alice owner -> bob)
    send(alice, "OP bob\n"); expect_exact("OP response", alice, "100 OK\n")
    expect_exact("OP broadcast to bob", bob, "OP bob\n")
    expect_exact("OP broadcast to carol", carol, "OP bob\n")
    # 30-33 TOPIC by bob (now owner)
    send(bob, "TOPIC new room topic\n"); expect_exact("TOPIC response", bob, "100 OK\n")
    expect_exact("TOPIC broadcast to alice", alice, "TOPIC bob new room topic\n")
    expect_exact("TOPIC broadcast to carol", carol, "TOPIC bob new room topic\n")
    # 34-35 KICK denied for old owner (alice)
    send(alice, "KICK carol\n"); expect_exact("KICK denied for old owner", alice, "203 DENIED\n")
    # 36-39 KICK carol by bob
    send(bob, "KICK carol\n"); expect_exact("KICK response", bob, "100 OK\n")
    expect_exact("KICK broadcast to alice", alice, "KICK carol bob\n")
    expect_exact("KICK message to kicked client", carol, "KICK carol bob\n")
    # 40-41 KICK nobody
    send(bob, "KICK nobody\n"); expect_exact("KICK unknown nickname", bob, "202 UNKNOWN NICKNAME\n")
    # 42-44 QUIT (alice)
    send(alice, "QUIT\n"); expect_exact("QUIT response", alice, "100 OK\n")
    expect_exact("QUIT broadcast", bob, "QUIT alice\n")
finally:
    srv.terminate()
    try: srv.wait(timeout=2)
    except Exception: srv.kill()

print(f"\nTong so test: {total}, that bai: {failed}")
sys.exit(1 if failed else 0)
