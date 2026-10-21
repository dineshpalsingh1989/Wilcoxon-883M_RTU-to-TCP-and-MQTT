"""Check an MQTT broker and watch a running 883M gateway's MQTT traffic. Standard library only.

Connects like the gateway does (plain TCP or TLS), subscribes to the gateway's topics,
optionally sends a command, and prints what arrives. Run it on a computer that can reach
the broker while the gateway is running.

Examples:
  python scripts/mqtt_check.py --host broker.example.com --port 8883 --tls --insecure
  python scripts/mqtt_check.py --host 192.168.1.10 --port 1883 --command get_metrics --seconds 60

Credentials: --user, and the password from the MQTT_PASSWORD environment variable so it is not
saved in shell history. Never commit credentials.
"""
import argparse
import json
import os
import random
import socket
import ssl
import struct
import sys
import time
from collections import Counter


def encode_length(n):
    out = bytearray()
    while True:
        byte, n = n % 128, n // 128
        out.append(byte | (0x80 if n else 0))
        if not n:
            return bytes(out)


def utf8(text):
    data = text.encode()
    return struct.pack("!H", len(data)) + data


def packet(kind, body):
    return bytes([kind]) + encode_length(len(body)) + body


def read_exact(sock, n):
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("broker closed the connection")
        data += chunk
    return data


def read_packet(sock):
    first = read_exact(sock, 1)[0]
    length, shift = 0, 0
    while True:
        byte = read_exact(sock, 1)[0]
        length |= (byte & 0x7F) << shift
        shift += 7
        if not byte & 0x80:
            break
    return first, read_exact(sock, length)


CONNACK_CODES = {0: "accepted", 1: "bad protocol version", 2: "client ID rejected",
                 3: "server unavailable", 4: "bad user name or password", 5: "not authorized"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--host", required=True)
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--tls", action="store_true", help="use TLS (usually port 8883)")
    ap.add_argument("--insecure", action="store_true", help="skip certificate validation, like the gateway's 'Skip Certificate Validation'")
    ap.add_argument("--allow-tls13", action="store_true",
                    help="also allow TLS 1.3; by default TLS 1.2 is used, the newest version the gateways support")
    ap.add_argument("--user", default="")
    ap.add_argument("--topic", default="883M", help="gateway base topic (default 883M)")
    ap.add_argument("--seconds", type=int, default=30, help="how long to listen")
    ap.add_argument("--command", choices=["get_metrics", "trigger_all"], help="send this command to the gateway")
    ap.add_argument("--serial", help="sensor serial for --command; learned from gateway messages if omitted")
    args = ap.parse_args()
    password = os.environ.get("MQTT_PASSWORD", "")
    ok = True

    # 1. TCP and TLS
    started = time.monotonic()
    try:
        raw = socket.create_connection((args.host, args.port), timeout=10)
    except OSError as error:
        print(f"FAIL TCP connect to {args.host}:{args.port}: {error}")
        return 1
    print(f"OK   TCP connect to {args.host}:{args.port} ({(time.monotonic() - started) * 1000:.0f} ms)")
    sock = raw
    if args.tls:
        context = ssl.create_default_context()
        if not args.allow_tls13:
            context.maximum_version = ssl.TLSVersion.TLSv1_2  # gateways (mbedTLS / BearSSL) speak TLS 1.2
        if args.insecure:
            context.check_hostname = False
            context.verify_mode = ssl.CERT_NONE
        started = time.monotonic()
        try:
            sock = context.wrap_socket(raw, server_hostname=args.host)
        except (ssl.SSLError, OSError) as error:
            print(f"FAIL TLS handshake: {error}")
            print("     Port not using TLS, certificate not trusted (compare with --insecure), or the broker")
            print("     requires TLS 1.3 (compare with --allow-tls13), which the gateways cannot use.")
            return 1
        print(f"OK   TLS {sock.version()} {sock.cipher()[0]} ({(time.monotonic() - started) * 1000:.0f} ms)"
              + ("" if args.insecure else ", certificate valid for this host name"))
        der = sock.getpeercert(binary_form=True)
        if der:
            print(f"     Server certificate: {len(der)} bytes")

    # 2. MQTT CONNECT (MQTT 3.1.1, clean session, like PubSubClient)
    client_id = f"ila-check-{random.randint(0, 0xFFFFFF):06x}"
    flags = 0x02 | (0x80 if args.user else 0) | (0x40 if args.user and password else 0)
    body = utf8("MQTT") + bytes([4, flags]) + struct.pack("!H", 60) + utf8(client_id)
    if args.user:
        body += utf8(args.user)
        if password:
            body += utf8(password)
    sock.settimeout(10)
    sock.sendall(packet(0x10, body))
    kind, data = read_packet(sock)
    if kind >> 4 != 2 or data[1] != 0:
        code = data[1] if len(data) > 1 else -1
        print(f"FAIL MQTT CONNECT refused: {CONNACK_CODES.get(code, code)}")
        return 1
    print(f"OK   MQTT connected as {client_id}")

    # 3. Subscribe to the gateway's data, status and command topics
    # "base/#" also matches "base" itself; status and command always live under 883M/.
    topics = sorted({f"{args.topic}/#", "883M/#"})
    sub = struct.pack("!H", 1) + b"".join(utf8(t) + b"\x00" for t in topics)
    sock.sendall(packet(0x82, sub))
    print(f"     Listening {args.seconds} s on: {', '.join(topics)}")

    counts, serial, last_health, command_sent = Counter(), args.serial, None, False
    deadline = time.monotonic() + args.seconds
    last_ping = time.monotonic()
    sock.settimeout(1)
    while time.monotonic() < deadline:
        if args.command and not command_sent and serial:
            command = json.dumps({"serial_number": serial, "command": args.command})
            sock.sendall(packet(0x30, utf8("883M/command") + command.encode()))
            print(f"SENT {args.command} for serial {serial}")
            command_sent = True
        if time.monotonic() - last_ping > 30:
            sock.sendall(b"\xc0\x00")
            last_ping = time.monotonic()
        try:
            kind, data = read_packet(sock)
        except (socket.timeout, TimeoutError):
            continue
        except (ConnectionError, OSError) as error:
            print(f"FAIL connection lost: {error}")
            ok = False
            break
        if kind >> 4 != 3:
            continue
        topic_len = struct.unpack("!H", data[:2])[0]
        topic = data[2:2 + topic_len].decode(errors="replace")
        offset = 2 + topic_len + (2 if (kind >> 1) & 0x03 else 0)
        payload = data[offset:]
        try:
            message = json.loads(payload)
        except ValueError:
            counts[f"{topic} (not JSON)"] += 1
            continue
        kind_name = message.get("type") or message.get("command") or "other"
        counts[f"{topic} {kind_name}"] += 1
        serial = serial or (message.get("serial_number") if message.get("serial_number") not in (None, "Unknown") else None)
        if kind_name == "health_status":
            last_health = message
        if counts[f"{topic} {kind_name}"] == 1:
            print(f"RECV {topic} {kind_name} ({len(payload)} bytes)")

    sock.close()
    print("\nSummary")
    if not counts:
        print("  No gateway messages. Check that the gateway shows MQTT 'Connected', uses this broker and topic,")
        print("  and that metrics publishing is active. Health status is sent every 5 minutes.")
        ok = False
    for name, count in sorted(counts.items()):
        print(f"  {count:5d}  {name}")
    if last_health:
        keys = ("serial_number", "uptime", "free_heap_memory", "reset_reason", "wifi_signal_rssi")
        print("  Last health: " + ", ".join(f"{k}={last_health.get(k)}" for k in keys))
    if args.command and not command_sent:
        print("  Command not sent: no sensor serial seen. Pass --serial.")
        ok = False
    return 0 if ok else 2


if __name__ == "__main__":
    sys.exit(main())
