#include <atomic>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <netinet/in.h>

#include <rte_config.h>
#ifdef RTE_ENABLE_STDATOMIC
#undef RTE_ENABLE_STDATOMIC
#endif
extern "C" {
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_tcp.h>
#include <rte_eal.h>
}

#include "fix_parser.hpp"

namespace {

std::atomic<bool> running{true};

extern "C" void handle_signal(int) {
    running.store(false, std::memory_order_relaxed);
}

void print_fix_message(const fix::MessageView& message) {
    std::cout << "FIX " << message.msg_type
              << " SenderCompID(49)=" << message.sender
              << " TargetCompID(56)=" << message.target
              << " MsgSeqNum(34)=" << message.sequence
              << " ClOrdID(11)=" << message.client_order_id << '\n';
}

void parse_tcp_payload(const char* payload, std::size_t length,
                       std::uint64_t& valid_messages,
                       std::uint64_t& invalid_messages,
                       std::uint64_t& incomplete_messages) {
    std::size_t offset = 0;
    while (offset < length) {
        fix::MessageView message;
        std::size_t consumed = 0;
        const auto status =
            fix::parse_message(payload + offset, length - offset, consumed,
                               message);
        if (status == fix::ParseStatus::ok) {
            print_fix_message(message);
            ++valid_messages;
            offset += consumed;
        } else if (status == fix::ParseStatus::incomplete) {
            ++incomplete_messages;
            return;
        } else {
            ++invalid_messages;
            std::cerr << "Rejected malformed FIX message in TCP payload\n";
            return;
        }
    }
}

void parse_packet(rte_mbuf* packet, std::uint64_t& valid_messages,
                  std::uint64_t& invalid_messages,
                  std::uint64_t& incomplete_messages,
                  std::uint64_t& ignored_packets) {
    if (packet->nb_segs != 1 || packet->data_len != packet->pkt_len ||
        packet->pkt_len < sizeof(rte_ether_hdr)) {
        ++ignored_packets;
        return;
    }

    const auto* bytes = rte_pktmbuf_mtod(packet, const std::uint8_t*);
    std::size_t l2_length = sizeof(rte_ether_hdr);
    const auto* ethernet =
        reinterpret_cast<const rte_ether_hdr*>(bytes);
    std::uint16_t ether_type = rte_be_to_cpu_16(ethernet->ether_type);

    while (ether_type == RTE_ETHER_TYPE_VLAN ||
           ether_type == RTE_ETHER_TYPE_QINQ) {
        if (packet->pkt_len < l2_length + sizeof(rte_vlan_hdr)) {
            ++ignored_packets;
            return;
        }
        const auto* vlan = reinterpret_cast<const rte_vlan_hdr*>(bytes + l2_length);
        ether_type = rte_be_to_cpu_16(vlan->eth_proto);
        l2_length += sizeof(rte_vlan_hdr);
    }
    if (ether_type != RTE_ETHER_TYPE_IPV4 ||
        packet->pkt_len < l2_length + sizeof(rte_ipv4_hdr)) {
        ++ignored_packets;
        return;
    }

    const auto* ip =
        reinterpret_cast<const rte_ipv4_hdr*>(bytes + l2_length);
    const std::size_t ip_header_length = (ip->version_ihl & 0x0fU) * 4U;
    const std::size_t ip_total_length = rte_be_to_cpu_16(ip->total_length);
    if ((ip->version_ihl >> 4U) != 4 ||
        ip_header_length < sizeof(rte_ipv4_hdr) ||
        ip_total_length < ip_header_length ||
        ip_total_length > packet->pkt_len - l2_length ||
        ip->next_proto_id != IPPROTO_TCP ||
        (rte_be_to_cpu_16(ip->fragment_offset) & 0x3fffU) != 0) {
        ++ignored_packets;
        return;
    }

    const std::size_t tcp_offset = l2_length + ip_header_length;
    const std::size_t ip_payload_length = ip_total_length - ip_header_length;
    if (ip_payload_length < sizeof(rte_tcp_hdr)) {
        ++ignored_packets;
        return;
    }
    const auto* tcp = reinterpret_cast<const rte_tcp_hdr*>(bytes + tcp_offset);
    const std::size_t tcp_header_length = (tcp->data_off >> 4U) * 4U;
    if (tcp_header_length < sizeof(rte_tcp_hdr) ||
        tcp_header_length > ip_payload_length) {
        ++ignored_packets;
        return;
    }

    const std::size_t payload_length = ip_payload_length - tcp_header_length;
    if (payload_length == 0) {
        ++ignored_packets;
        return;
    }
    parse_tcp_payload(reinterpret_cast<const char*>(bytes + tcp_offset +
                                                    tcp_header_length),
                      payload_length, valid_messages, invalid_messages,
                      incomplete_messages);
}

}  // namespace

int main(int argc, char** argv) {
    const int eal_arguments = rte_eal_init(argc, argv);
    if (eal_arguments < 0) {
        rte_exit(EXIT_FAILURE, "DPDK EAL initialization failed\n");
    }
    if (rte_eth_dev_count_avail() == 0) {
        rte_exit(EXIT_FAILURE,
                 "No DPDK port found. Pass a net_pcap or net_tap vdev.\n");
    }

    constexpr std::uint16_t port_id = 0;
    constexpr std::uint16_t rx_descriptors = 1024;
    constexpr unsigned int mbuf_count = 8191;
    rte_eth_conf port_configuration{};
    int result = rte_eth_dev_configure(port_id, 1, 0, &port_configuration);
    if (result < 0) {
        rte_exit(EXIT_FAILURE, "Port configuration failed: %s\n",
                 rte_strerror(-result));
    }

    rte_mempool* mbuf_pool = rte_pktmbuf_pool_create(
        "FIX_RX_POOL", mbuf_count, 256, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_eth_dev_socket_id(port_id));
    if (mbuf_pool == nullptr) {
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool: %s\n",
                 rte_strerror(rte_errno));
    }
    result = rte_eth_rx_queue_setup(port_id, 0, rx_descriptors,
                                    rte_eth_dev_socket_id(port_id), nullptr,
                                    mbuf_pool);
    if (result < 0) {
        rte_exit(EXIT_FAILURE, "RX queue setup failed: %s\n",
                 rte_strerror(-result));
    }
    result = rte_eth_dev_start(port_id);
    if (result < 0) {
        rte_exit(EXIT_FAILURE, "Port start failed: %s\n",
                 rte_strerror(-result));
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
    std::cout << "FIX-over-TCP DPDK parser started; press Ctrl-C to stop.\n";

    std::uint64_t valid_messages = 0;
    std::uint64_t invalid_messages = 0;
    std::uint64_t incomplete_messages = 0;
    std::uint64_t ignored_packets = 0;
    rte_mbuf* packets[32];
    while (running.load(std::memory_order_relaxed)) {
        const std::uint16_t received =
            rte_eth_rx_burst(port_id, 0, packets, 32);
        for (std::uint16_t index = 0; index < received; ++index) {
            parse_packet(packets[index], valid_messages, invalid_messages,
                         incomplete_messages, ignored_packets);
            rte_pktmbuf_free(packets[index]);
        }
    }

    rte_eth_dev_stop(port_id);
    rte_eth_dev_close(port_id);
    rte_eal_cleanup();
    std::cout << "Stopped. FIX valid=" << valid_messages
              << ", invalid=" << invalid_messages
              << ", incomplete=" << incomplete_messages
              << ", ignored packets=" << ignored_packets << '\n';
    return 0;
}
