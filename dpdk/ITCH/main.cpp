// to run:
// stdbuf -oL ./build/itch_parser -l 0 --no-huge --no-pci --vdev="net_pcap0,rx_pcap=./ny4-xnas-tvitch-a-20230822T133000.pcap" > log.txt

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <atomic>
#include <arpa/inet.h>
#include <rte_config.h>
#ifdef RTE_ENABLE_STDATOMIC
#undef RTE_ENABLE_STDATOMIC
#endif
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

// 1. Define the structures strictly according to the Nasdaq ITCH 5.0 specification (1-byte alignment)
#pragma pack(push, 1)

// MoldUDP64 session-layer header (20 bytes total)
struct MoldUDP64Header {
    uint8_t  session[10];    // Session identifier
    uint64_t sequence_num;   // Global sequence number of the first message in this batch (big-endian)
    uint16_t message_count;  // Number of ITCH messages aggregated in the current UDP packet (big-endian)
};

// Common variable-length wrapper header for ITCH messages
struct ItchMsgHeader {
    uint16_t msg_len;        // Length of one ITCH message (including msg_type) (big-endian)
    char     msg_type;       // Message type, e.g. 'A'=Add Order, 'E'=Order Executed
};

// Classic interview topic: add-order message (Add Order Message - Type 'A')
struct ItchAddOrderMsg {
    uint16_t stock_locate;   // Stock location code
    uint16_t tracking_num;   // Tracking number
    uint8_t  timestamp[6];   // Nanosecond timestamp (48-bit big-endian integer since midnight)
    uint64_t order_ref_num;  // Unique order reference number (8 bytes, big-endian)
    char     buy_sell_indicator; // 'B'=Buy, 'S'=Sell
    uint32_t shares;         // Number of shares (4 bytes, big-endian)
    char     stock[8];       // Ticker symbol (8-byte ASCII, right-padded with spaces)
    uint32_t price;          // Price (4-byte fixed-point value, big-endian, divide by 10000)
};

#pragma pack(pop)

std::atomic<bool> g_running{true};

void signal_handler(int signum) {
    if (signum == SIGINT) {
        g_running.store(false, std::memory_order_release);
    }
}

// Follow these principles
/*
    1.  Locate the header pointer (zero-cost copy)
    
        rte_pktmbuf_mtod + rte_ether_hdr*
        or for other packets, use
        rte_pktmbuf_mtod_offset + rte_pkttype_hdr* + offset

    2. Defensive programming:
    
        2.1 Check for undersized packets, for example:
    
            m->pkt_len < sizeof(struct rte_pkttype_hdr), then return immediately

        2.2 Check for oversized packets, for example:

            ip_total_len > m->pkt_len - l2_len

    3. Once the MoldUDP64 header is reached:

        const uint8_t* itch_data_ptr = reinterpret_cast<const uint8_t*>(mold_hdr + 1);
        size_t remaining_bytes = payload_len - sizeof(struct MoldUDP64Header);

        mold_hdr + 1 points to the first ITCH message after the MoldUDP64 header. remaining_bytes 
        records how many bytes remain parsable.
*/

uint64_t get_time_stamp(const uint8_t bytes[], size_t length) {
    uint64_t timestamp = 0;
    
    for (size_t i = 0; i < length; ++i) {
        timestamp = (timestamp << 8) | bytes[i];
    }

    return timestamp;
}

// 2. Main streaming business parsing function
static void parse_itch_packet(struct rte_mbuf* m) {
    // First, ensure the total packet length (m->pkt_len) can hold at least a basic Ethernet header
    if (m->pkt_len < sizeof(struct rte_ether_hdr)) return;

    // m->nb_segs != 1: the mbuf chain is not exactly one segment; if multiple segments exist, the code
    // cannot access the whole packet using only the first segment pointer.

    // m->data_len != m->pkt_len: the first-segment data length differs from the total packet length. For a single-segment packet,
    // these should normally match, so this also checks that the length metadata is consistent.
    if (m->nb_segs != 1 || m->data_len != m->pkt_len) {
        return; // or log and discard
    }

    // Extract the Ethernet type: use rte_pktmbuf_mtod to get the Ethernet header pointer and read 
    // ether_type and convert the 16-bit integer from network byte order to host byte order
    auto* eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr*);
    uint16_t ether_type = ntohs(eth_hdr->ether_type);
    size_t l2_len = sizeof(*eth_hdr);

    // Use a while loop to handle possible VLAN nesting (QinQ). As long as ether_type matches 
    // VLAN or various QinQ types, continue moving forward by l2_len and read the inner rte_vlan_hdr,
    // until the real network-layer protocol (such as IPv4) is exposed.
    while (ether_type == RTE_ETHER_TYPE_VLAN ||
           ether_type == RTE_ETHER_TYPE_QINQ ||
           ether_type == RTE_ETHER_TYPE_QINQ1 ||
           ether_type == RTE_ETHER_TYPE_QINQ2 ||
           ether_type == RTE_ETHER_TYPE_QINQ3) {

        // Before reading memory, ensure the actual packet length (m->pkt_len) is at least the current parsed
        // L2 length (l2_len) + the length of this VLAN header.
        if (m->pkt_len < l2_len + sizeof(struct rte_vlan_hdr)) return;
        auto* vlan_hdr = rte_pktmbuf_mtod_offset(m, struct rte_vlan_hdr*, l2_len);
        ether_type = ntohs(vlan_hdr->eth_proto);
        l2_len += sizeof(*vlan_hdr);
    }

    if (ether_type != RTE_ETHER_TYPE_IPV4 ||
        m->pkt_len < l2_len + sizeof(struct rte_ipv4_hdr)) return;

    // Use the l2_len value obtained after stripping VLANs (18 bytes) to precisely locate and decode IPv4
    auto* ip_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr*, l2_len);
    // Extract the dynamic IP header length: the low 4 bits of version_ihl are the IHL (Internet Header Length,
    // IP header length). Because it represents a count of 32-bit words (4 bytes), it must be multiplied by 4
    size_t ip_header_len = (ip_hdr->version_ihl & 0x0f) * 4;

    // Safety filter
    // 1. ip_header_len < 20: if a malformed packet declares an IP header shorter than the base structure, discard it immediately.
    // 2. ip_total_len < ip_header_len + UDP header: the IP total length must be at least large enough for its own
    //    header plus the following UDP header
    // 3. ip_total_len > m->pkt_len - l2_len (key guard): prevent maliciously forged long packets.
    //    If the packet claims to contain 1000 bytes internally but the NIC actually received only 500 bytes (m->pkt_len),
    //    without this check, subsequent pointer operations can cause memory out-of-bounds (segment fault).
    // 4. next_proto_id != IPPROTO_UDP: veto. If it is not UDP (for example TCP
    //    or ICMP), exit immediately.
    uint16_t ip_total_len = ntohs(ip_hdr->total_length);
    if (ip_header_len < sizeof(struct rte_ipv4_hdr) ||
        ip_total_len < ip_header_len + sizeof(struct rte_udp_hdr) ||
        ip_total_len > m->pkt_len - l2_len ||
        ip_hdr->next_proto_id != IPPROTO_UDP) return;

    size_t udp_offset = l2_len + ip_header_len;
    auto* udp_hdr = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr*, udp_offset);
    uint16_t udp_len = ntohs(udp_hdr->dgram_len);
    if (udp_len < sizeof(struct rte_udp_hdr) + sizeof(struct MoldUDP64Header) ||
        udp_len > ip_total_len - ip_header_len) return;

    size_t payload_offset = udp_offset + sizeof(struct rte_udp_hdr);
    size_t payload_len = udp_len - sizeof(struct rte_udp_hdr);
    if (payload_len < sizeof(struct MoldUDP64Header)) return;

    auto* mold_hdr = rte_pktmbuf_mtod_offset(m, struct MoldUDP64Header*, payload_offset);
    uint16_t msg_count = ntohs(mold_hdr->message_count);
    uint64_t base_seq  = __builtin_bswap64(mold_hdr->sequence_num);
    const uint8_t* itch_data_ptr = reinterpret_cast<const uint8_t*>(mold_hdr + 1);
    size_t remaining_bytes = payload_len - sizeof(struct MoldUDP64Header);

    for (uint16_t i = 0; i < msg_count; ++i) {
        // Each message begins with a 2-byte length field; if the remaining data is not even large enough for the length field, stop parsing.
        if (remaining_bytes < sizeof(ItchMsgHeader)) break;

        // Read the message length and convert its byte order. current_msg_len is the length of the ITCH message itself, including the
        // message type byte but not the preceding 2-byte length field. Therefore a whole message occupies:
        auto* itch_hdr = reinterpret_cast<const struct ItchMsgHeader*>(itch_data_ptr);
        uint16_t current_msg_len = ntohs(itch_hdr->msg_len);
        size_t framed_msg_len = sizeof(itch_hdr->msg_len) + current_msg_len;
        if (current_msg_len < sizeof(itch_hdr->msg_type) ||
            remaining_bytes < framed_msg_len) break;
        
        if (itch_hdr->msg_type == 'A' &&
            current_msg_len >= sizeof(itch_hdr->msg_type) + sizeof(struct ItchAddOrderMsg)) {

            auto* order = reinterpret_cast<const struct ItchAddOrderMsg*>(
                itch_data_ptr + sizeof(struct ItchMsgHeader));

            uint64_t ref_num = __builtin_bswap64(order->order_ref_num);
            uint32_t shares  = ntohl(order->shares);
            uint32_t raw_px  = ntohl(order->price);
            double price     = raw_px / 10000.0;

            char sym[9] = {0};
            std::memcpy(sym, order->stock, 8);

            // In the ITCH 5.0 protocol standard, the Timestamp field
            // is defined as the number of nanoseconds since midnight (Nanoseconds 
            // since midnight). For example: 34199999894278 corresponds to:
            // 09:29:59.999989427 in the morning. This is the typical U.S. stock market opening time (09:30:00)
            // for the previous market snapshot.
            auto timestamp = get_time_stamp(order->timestamp, 6);

            // std::cout << "[ITCH AddOrder] Seq: " << (base_seq + i)
            //           << " | Stock: " << sym
            //           << " | Side: " << order->buy_sell_indicator
            //           << " | Qty: " << shares
            //           << " | Price: " << price
            //           << " | OrderRef: " << ref_num 
            //           << " | Timestamp: " << timestamp
            //           << "\n";
        }

        itch_data_ptr += framed_msg_len;
        remaining_bytes -= framed_msg_len;
    }
}

int main(int argc, char** argv) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // 1. Initialize the environment abstraction layer (EAL)
    // Be sure to pass: --vdev="net_pcap0,rx_pcap=nasdaq_sample.pcap" when starting
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Error: DPDK EAL Initialization Failed.\n");
    }
    argc -= ret;
    argv += ret;

    // Check the binding state: the virtual PCAP driver is registered as port 0 by default
    uint16_t port_id = 0;
    if (!rte_eth_dev_is_valid_port(port_id)) {
        rte_exit(EXIT_FAILURE, "Error: No valid port found. Did you pass --vdev=net_pcap0?\n");
    }

    constexpr uint16_t rx_queue_count = 1;
    constexpr uint16_t rx_desc_count = 1024;
    constexpr unsigned int mbuf_count = 8191;
    constexpr unsigned int mbuf_cache_size = 256;

    struct rte_eth_conf port_conf{};
    ret = rte_eth_dev_configure(port_id, rx_queue_count, 0, &port_conf);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Error: Port configuration failed: %s\n", rte_strerror(-ret));
    }

    struct rte_mempool* mbuf_pool = rte_pktmbuf_pool_create(
        "ITCH_RX_POOL", mbuf_count, mbuf_cache_size, 0,
        RTE_MBUF_DEFAULT_BUF_SIZE, rte_eth_dev_socket_id(port_id));
    if (mbuf_pool == nullptr) {
        rte_exit(EXIT_FAILURE, "Error: Cannot create RX mbuf pool: %s\n",
                 rte_strerror(rte_errno));
    }

    ret = rte_eth_rx_queue_setup(port_id, 0, rx_desc_count,
                                 rte_eth_dev_socket_id(port_id), nullptr, mbuf_pool);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Error: RX queue setup failed: %s\n", rte_strerror(-ret));
    }

    ret = rte_eth_dev_start(port_id);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Error: Port start failed: %s\n", rte_strerror(-ret));
    }

    std::cout << "========================================================\n";
    std::cout << "🎉 DPDK PCAP HFT market-stream processor is ready; starting poll loop...\n";
    std::cout << "========================================================\n";

    // 2. Main packet-receive loop (quant team PMD polling core)
    constexpr uint16_t BURST_SIZE = 32;
    struct rte_mbuf* bufs[BURST_SIZE];

    while (g_running.load(std::memory_order_acquire)) {
        // Fast lock-free polling: drain up to 32 packets at a time from the virtual NIC
        // rte_eth_rx_burst is a non-blocking function that retrieves packets in bursts (maximum BURST_SIZE)
        uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);

        if (nb_rx > 0) {
            for (uint16_t i = 0; i < nb_rx; ++i) {
                // Serial pipeline: in-place zero-copy unpacking
                parse_itch_packet(bufs[i]);
                
                // Memory reclamation: return the buffer immediately to the local per-core cache after use
                rte_pktmbuf_free(bufs[i]);
            }
        }
    }

    return 0;
}
