#include <iostream>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <thread>

// DPDK's C11 stdatomic path is not compatible with GCC's C++ stdatomic header.
#include <rte_config.h>
#undef RTE_ENABLE_STDATOMIC

// include DPDK's C headers
extern "C" {
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_cycles.h>
}

static volatile bool keep_running = true;

extern "C" void stop_test(int) {
    keep_running = false;
}

static void print_packet(const rte_mbuf *packet, uint16_t length) {
    const auto *data = rte_pktmbuf_mtod(packet, const uint8_t *);
    std::cout << "RX " << length << " bytes: ";
    const uint16_t bytes_to_print = length < 32 ? length : 32;
    for (uint16_t index = 0; index < bytes_to_print; ++index) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<unsigned>(data[index]) << ' ';
    }
    std::cout << std::dec << std::setfill(' ') << '\n';
}

static int send_test_packet(uint16_t port_id, rte_mempool *pool,
                            uint64_t sequence) {
    rte_mbuf *packet = rte_pktmbuf_alloc(pool);
    if (packet == nullptr) {
        return -1;
    }

    constexpr uint16_t packet_length = 60;
    auto *data = rte_pktmbuf_mtod(packet, uint8_t *);
    std::memset(data, 0, packet_length);
    std::memset(data, 0xff, 6); // Ethernet broadcast destination.
    const uint8_t source_mac[] = {0x02, 0x44, 0x50, 0x44, 0x4b, 0x01};
    std::memcpy(data + 6, source_mac, sizeof(source_mac));
    data[12] = 0x88;
    data[13] = 0xb5; // Experimental EtherType.
    const char message[] = "DPDK TAP test packet";
    std::memcpy(data + 14, message, sizeof(message) - 1);
    std::memcpy(data + 40, &sequence, sizeof(sequence));
    packet->pkt_len = packet_length;
    packet->data_len = packet_length;

    if (rte_eth_tx_burst(port_id, 0, &packet, 1) != 1) {
        rte_pktmbuf_free(packet);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    const int eal_args = rte_eal_init(argc, argv);
    if (eal_args < 0) {
        rte_exit(EXIT_FAILURE, "rte_eal_init failed\n");
    }

    if (rte_eth_dev_count_avail() == 0) {
        rte_exit(EXIT_FAILURE,
                 "No DPDK port found. Start with --vdev=net_tap0,iface=dpdk_tap0\n");
    }

    const uint16_t port_id = 0;
    rte_mempool *pool = rte_pktmbuf_pool_create(
        "TAP_MBUF_POOL", 4096, 256, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id());
    if (pool == nullptr) {
        rte_exit(EXIT_FAILURE, "rte_pktmbuf_pool_create failed\n");
    }

    rte_eth_conf port_conf{};
    int ret = rte_eth_dev_configure(port_id, 1, 1, &port_conf);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "rte_eth_dev_configure failed: %s\n",
                 rte_strerror(-ret));
    }

    ret = rte_eth_rx_queue_setup(port_id, 0, 128, rte_eth_dev_socket_id(port_id),
                                 nullptr, pool);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup failed: %s\n",
                 rte_strerror(-ret));
    }
    ret = rte_eth_tx_queue_setup(port_id, 0, 128, rte_eth_dev_socket_id(port_id),
                                 nullptr);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup failed: %s\n",
                 rte_strerror(-ret));
    }
    ret = rte_eth_dev_start(port_id);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "rte_eth_dev_start failed: %s\n",
                 rte_strerror(-ret));
    }

    std::signal(SIGINT, stop_test);
    std::signal(SIGTERM, stop_test);
    std::cout << "TAP port started. Press Ctrl-C to stop.\n"
              << "Send packets from the host through the TAP interface.\n";

    rte_mbuf *received[32];
    uint64_t last_tx_tsc = 0;
    uint64_t sequence = 0;
    uint64_t rx_packets = 0;
    uint64_t tx_packets = 0;
    const uint64_t tx_interval = rte_get_tsc_hz();
    while (keep_running) {
        // rte_eth_rx_burst is a non-blocking function that retrieves packets in bursts (maximum 32).
        const uint16_t received_count = rte_eth_rx_burst(port_id, 0, received, 32);
        for (uint16_t index = 0; index < received_count; ++index) {
            print_packet(received[index], received[index]->pkt_len);
            ++rx_packets;
            rte_pktmbuf_free(received[index]);
        }

        const uint64_t now = rte_get_tsc_cycles();
        if (now - last_tx_tsc >= tx_interval) {
            if (send_test_packet(port_id, pool, sequence++) == 0) {
                ++tx_packets;
            }
            last_tx_tsc = now;
            std::cout << "TX packets: " << tx_packets
                      << ", RX packets: " << rx_packets << '\n';
        }
        if (received_count == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    rte_eth_dev_stop(port_id);
    rte_eth_dev_close(port_id);
    std::cout << "Stopped. TX packets: " << tx_packets
              << ", RX packets: " << rx_packets << '\n';
    rte_eal_cleanup();
    return 0;
}
