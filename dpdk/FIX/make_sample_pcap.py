#!/usr/bin/env python3
"""Write a small Ethernet/IPv4/TCP PCAP containing two FIX 4.4 messages."""

import socket
import struct


SOH = b"\x01"


def checksum(data):
    if len(data) & 1:
        data += b"\x00"
    words = struct.unpack("!%dH" % (len(data) // 2), data)
    total = sum(words)
    total = (total & 0xFFFF) + (total >> 16)
    total = (total & 0xFFFF) + (total >> 16)
    return (~total) & 0xFFFF


def fix_message(sequence, order_id, side, quantity, symbol, price):
    body = (
        f"35=D\x01"
        f"49=CLIENT\x01"
        f"56=EXCHANGE\x01"
        f"34={sequence}\x01"
        f"11={order_id}\x01"
        f"21=1\x01"
        f"55={symbol}\x01"
        f"54={side}\x01"
        f"38={quantity}\x01"
        f"40=2\x01"
        f"44={price}\x01"
        f"59=0\x01"
    ).encode("ascii")
    prefix = b"8=FIX.4.4\x01" + f"9={len(body)}\x01".encode("ascii") + body
    check = sum(prefix) & 0xFF
    return prefix + f"10={check:03d}\x01".encode("ascii")


def make_packet(payload):
    source_ip = socket.inet_aton("192.0.2.10")
    target_ip = socket.inet_aton("192.0.2.20")
    source_mac = bytes.fromhex("020000000010")
    target_mac = bytes.fromhex("020000000020")

    tcp_without_checksum = struct.pack(
        "!HHIIHHHH", 50000, 5001, 1, 1, (5 << 12) | 0x18, 65535, 0, 0
    )
    pseudo_header = (
        source_ip + target_ip + struct.pack("!BBH", 0, socket.IPPROTO_TCP,
                                             len(tcp_without_checksum) + len(payload))
    )
    tcp_checksum = checksum(pseudo_header + tcp_without_checksum + payload)
    tcp_header = struct.pack(
        "!HHIIHHHH",
        50000,
        5001,
        1,
        1,
        (5 << 12) | 0x18,
        65535,
        tcp_checksum,
        0,
    )

    ip_total_length = 20 + len(tcp_header) + len(payload)
    ip_without_checksum = struct.pack(
        "!BBHHHBBH4s4s",
        0x45,
        0,
        ip_total_length,
        1,
        0,
        64,
        socket.IPPROTO_TCP,
        0,
        source_ip,
        target_ip,
    )
    ip_checksum = checksum(ip_without_checksum)
    ip_header = ip_without_checksum[:10] + struct.pack("!H", ip_checksum) + ip_without_checksum[12:]
    ethernet = target_mac + source_mac + struct.pack("!H", 0x0800)
    return ethernet + ip_header + tcp_header + payload


fix_payload = fix_message(1, "order-1001", "1", 100, "AAPL", "189.50")
fix_payload += fix_message(2, "order-1002", "2", 50, "MSFT", "421.25")
packet = make_packet(fix_payload)

with open("sample_fix.pcap", "wb") as capture:
    capture.write(struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1))
    capture.write(struct.pack("<IIII", 0, 0, len(packet), len(packet)))
    capture.write(packet)
print("Wrote sample_fix.pcap")
