# FIX over TCP with DPDK

A small FIX 4.4 message parser driven by DPDK's `net_pcap` virtual Ethernet
device. It extracts TCP payloads from Ethernet/IPv4 packets, checks FIX
`BodyLength(9)` and `CheckSum(10)`, and prints common order/session fields.

This is an educational packet parser, not a FIX engine: it does not implement
TCP connection management, stream reassembly, retransmission handling, FIX
session state, or order routing. Each complete FIX message must fit in one TCP
segment. TCP segments with split messages need reassembly before passing bytes
to the FIX parser.

## Build and test

Requirements: DPDK with `libdpdk` pkg-config metadata, CMake, and a C++17
compiler.

```bash
cd dpdk/FIX
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Replay a generated sample

Generate a one-packet PCAP with two FIX NewOrderSingle messages, then replay it
through DPDK's PCAP PMD:

```bash
python3 make_sample_pcap.py
sudo ./build/fix_dpdk \
  -l 0 --no-huge --no-pci \
  --vdev="net_pcap0,rx_pcap=sample_fix.pcap"
```

Stop with Ctrl-C. The output should contain `FIX D` twice with sequence numbers
1 and 2, followed by a summary showing two valid messages.

`net_pcap` support must be enabled in the installed DPDK build. For live
traffic, replace the PCAP vdev with a suitable DPDK port or TAP vdev and send
Ethernet/IPv4/TCP packets containing complete FIX messages to it.

The parser supports untagged and VLAN/QinQ Ethernet frames, IPv4 options, and
TCP options. It rejects fragmented IPv4 packets and ignores non-IPv4/TCP
traffic. TCP/IP checksums and FIX session sequencing are not validated.
