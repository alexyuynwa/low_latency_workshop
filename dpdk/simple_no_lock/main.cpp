#include <iostream>
#include <atomic>
#include <csignal>

// Use DPDK's GCC built-in atomics instead of its C11 stdatomic path in C++.
#include <rte_config.h>
#undef RTE_ENABLE_STDATOMIC

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_lcore.h>
#include <rte_byteorder.h>

#include <rte_ethdev.h>

// 1. Global control and configuration
std::atomic<bool> g_running{true};
constexpr unsigned int RING_SIZE = 65536; // Ring size; must be a power of two to optimize low-level bit operations
constexpr uint16_t BURST_SIZE = 32; // Batch size for packet receive and enqueue

// Simulated high-frequency market data structure
struct alignas(64) MarketDataPayload {         // 64-byte alignment to avoid crossing cache lines
    uint32_t security_id;
    uint32_t price;
    uint32_t volume;
    uint64_t timestamp_ns;
};

static const char* packet_type_name(const rte_mbuf* mbuf) {
    // 1. Basic validation: discard packets that do not even contain a full Ethernet header
    if (mbuf->pkt_len < sizeof(struct rte_ether_hdr)) {
        return "short Ethernet frame";
    }

    // Get the Ethernet header pointer
    const auto* eth_hdr = rte_pktmbuf_mtod(mbuf, const struct rte_ether_hdr*);
    uint16_t ether_type = rte_be_to_cpu_16(eth_hdr->ether_type);

    switch (ether_type) {
    case RTE_ETHER_TYPE_ARP:
        return "ARP";
    case RTE_ETHER_TYPE_IPV4:
        if (mbuf->pkt_len < sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr)) {
            return "IPv4 (short frame)";
        }
        {
            // Map directly to DPDK's official structures
            const auto* ip_hdr = reinterpret_cast<const struct rte_ipv4_hdr*>(eth_hdr + 1);
            switch (ip_hdr->next_proto_id) {
                case IPPROTO_ICMP: return "IPv4/ICMP";
                case IPPROTO_TCP:  return "IPv4/TCP";
                case IPPROTO_UDP:  return "IPv4/UDP";
                default:           return "IPv4/other protocol";
            }
        }
    case RTE_ETHER_TYPE_IPV6:
        // Safety check: Ethernet (14) + standard IPv6 header (40) = 54 bytes
        if (mbuf->pkt_len < sizeof(rte_ether_hdr) + sizeof(rte_ipv6_hdr)) {
            return "IPv6 (short frame)";
        }
        {
            // Map directly to DPDK's official IPv6 structures
            const auto* ip6_hdr = reinterpret_cast<const struct rte_ipv6_hdr*>(eth_hdr + 1);
            switch (ip6_hdr->proto) {
            case IPPROTO_ICMPV6:
                return "IPv6/ICMPv6";
            case IPPROTO_TCP:
                return "IPv6/TCP";
            case IPPROTO_UDP:
                return "IPv6/UDP";
            default:
                return "IPv6/other protocol";
            }
        }
    default:
        return "other EtherType";
    }
}

// Simplified NIC/port initialization function
static inline int init_port(uint16_t port_id, struct rte_mempool* mbuf_pool) {
    // 1. Define NIC configuration parameters
    struct rte_eth_conf port_conf;
    memset(&port_conf, 0, sizeof(struct rte_eth_conf));

    const uint16_t rx_rings = 1; // 1 receive queue
    const uint16_t tx_rings = 1; // The TAP PMD requires an equal number of RX/TX queues
    uint16_t nb_rxd = 1024;      // Number of receive queue descriptors (buffer size)
    uint16_t nb_txd = 1024;      // Number of transmit queue descriptors

    // 2. Configure the port (one RX queue and one TX queue)
    int retval = rte_eth_dev_configure(port_id, rx_rings, tx_rings, &port_conf);
    if (retval != 0) return retval;

    // 3. Adjust the descriptor counts to match driver requirements
    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rxd, &nb_txd);
    if (retval != 0) return retval;

    // 4. Allocate the specific RX queue 0 for the port and bind the previously created mbuf_pool memory pool to it
    retval = rte_eth_rx_queue_setup(port_id, 0, nb_rxd, rte_eth_dev_socket_id(port_id), nullptr, mbuf_pool);
    if (retval < 0) return retval;

    retval = rte_eth_tx_queue_setup(port_id, 0, nb_txd,
                                    rte_eth_dev_socket_id(port_id), nullptr);
    if (retval < 0) return retval;

    // 5. Start the network port
    retval = rte_eth_dev_start(port_id);
    if (retval < 0) return retval;

    // 6. Enable promiscuous mode (essential in HFT systems: accept all traffic regardless of destination MAC)
    retval = rte_eth_promiscuous_enable(port_id);
    if (retval != 0) return retval;

    std::cout << "Port " << port_id << " successfully initialized and started." << std::endl;
    return 0;
}


// 2. Strategy calculation thread (core 2): handles business logic
int strategy_core_loop(void* arg) {
    struct rte_ring* rx_ring = static_cast<struct rte_ring*>(arg);
    unsigned int lcore_id = rte_lcore_id(); // NUMA core id

    std::cout << "[Strategy Core] Started on lcore " << lcore_id << std::endl;

    void* mbufs[BURST_SIZE];

    while (g_running.load(std::memory_order_relaxed)) {
        // Dequeue mbuf pointers in batches from the lock-free ring
        // RING_F_SC_DEQ guarantees nanosecond-scale performance for single-consumer reads
        unsigned int dequeued = rte_ring_sc_dequeue_burst(rx_ring, mbufs, BURST_SIZE, nullptr);

        if (likely(dequeued == 0)) {
            // HFT core loops in a busy poll; do not sleep to minimize latency
        }

        // Core business processing section
        for (unsigned int i = 0; i < dequeued; ++i) {
            struct rte_mbuf* mbuf = static_cast<struct rte_mbuf*>(mbufs[i]);

            // [Zero-copy parsing] directly access the NIC data buffer pointer and cast it to the market-data structure
            // Assume an offset of 42 bytes (14-byte Ethernet header + 20-byte IP header + 8-byte UDP header)
            if (mbuf->pkt_len >= 42) {
                char* packet_data = rte_pktmbuf_mtod(mbuf, char*);
                MarketDataPayload* md = reinterpret_cast<MarketDataPayload*>(packet_data + 42);

                // Execute ultra-fast business strategy logic
                if (unlikely(md->price > 5000)) {
                    // Trigger risk-control or order-placement logic...
                }
            }

            // [Critical performance point] Since the strategy thread is the end of the data flow, it is responsible for returning mbufs to the mempool.
            // At this time, the core returns the mbuf to its own local cache (the mempool private cache) to improve cache reuse!
            rte_pktmbuf_free(mbuf);
        }
    }

    return 0;
}

// 3. Receive polling thread (core 1): this function runs on the main thread (EAL master/main core)
void receive_core_loop(uint16_t port_id, struct rte_ring* rx_ring) {
    unsigned int lcore_id = rte_lcore_id();
    std::cout << "[Receive Core] Polling port " << port_id << " on lcore " << lcore_id << std::endl;

    struct rte_mbuf* mbufs[BURST_SIZE];

    while (g_running.load(std::memory_order_relaxed)) {
        // Poll the physical NIC or virtual NIC directly for packet receive (zero-copy, bypassing the kernel)
        // rte_eth_rx_burst is a non-blocking function that retrieves packets in bursts (up to BURST_SIZE)
        uint16_t rx_count = rte_eth_rx_burst(port_id, 0, mbufs, BURST_SIZE);
        
        if (likely(rx_count == 0)) {
            continue; // Busy-loop polling
        }

        std::cout << "[Receive Core] Received " << rx_count
              << " packet(s), first type=" << packet_type_name(mbufs[0])
              << ", length=" << mbufs[0]->pkt_len 
              << ", addr = " << mbufs[0]
              << ", buf_addr = " << mbufs[0]->buf_addr
              << std::endl;

        // Enqueue the received mbuf pointers in batches into the lock-free ring queue
        // RING_F_SP_ENQ guarantees maximum performance for single-producer writes
        unsigned int enqueued = rte_ring_sp_enqueue_burst(rx_ring, reinterpret_cast<void**>(mbufs), rx_count, nullptr);

        // Error handling: if the ring is full (the strategy thread is too slow), the packets that failed to enqueue must be freed to avoid memory leaks (pool exhaustion)
        if (unlikely(enqueued < rx_count)) {
            for (unsigned int i = enqueued; i < rx_count; ++i) {
                rte_pktmbuf_free(mbufs[i]);
            }
            // In an HFT system, this should trigger a serious alarm (indicating packet loss or performance jitter)
        }
    }
}

void signal_handler(int signum) {
    if (signum == SIGINT) {
        g_running.store(false);
    }
}

int main(int argc, char** argv) {
    // Register the signal to exit gracefully
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Initialize the environment abstraction layer (EAL)
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

    uint16_t port_id = 0; // Assume the bound NIC port ID is 0

    // Create a lock-free ring dedicated to the hot path with single producer/single consumer semantics
    // Key parameters: RING_F_SP_ENQ (single producer) and RING_F_SC_DEQ (single consumer)
    struct rte_ring* rx_ring = rte_ring_create(
        "HFT_Rx_To_Strategy_Ring", 
        RING_SIZE, 
        rte_socket_id(), 
        RING_F_SP_ENQ | RING_F_SC_DEQ
    );

    if (rx_ring == nullptr) rte_exit(EXIT_FAILURE, "Cannot create rx ring\n");

    // Get and check available CPU cores
    unsigned int master_core = rte_lcore_id();
    std::cout << "master_core = " << master_core << std::endl;
    unsigned int strategy_core = rte_get_next_lcore(master_core, 1, 0);
    if (strategy_core == RTE_MAX_LCORE) {
        rte_exit(EXIT_FAILURE, "Error: Please provide at least 2 cores (e.g., -l 0-1)\n");
    }

    // [New step A] In a high-frequency trading system or demo, create a huge-page memory pool first to provide packet buffers for the NIC receive path
    // Our init_port depends on this pool
    struct rte_mempool* mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL", 
        8191,              // Total number of slots in the pool
        256,               // Per-core local cache size for each CPU core
        0, 
        RTE_MBUF_DEFAULT_BUF_SIZE, 
        rte_socket_id()
    );
    if (mbuf_pool == nullptr) rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    // [New step B] Before starting threads, initialize and start NIC port 0
    if (init_port(port_id, mbuf_pool) < 0) {
        rte_exit(EXIT_FAILURE, "Cannot initialize network port %u\n", port_id);
    }

    // 4. Thread binding and startup
    // Let core 2 (strategy_core) run the strategy loop in the background forever
    rte_eal_remote_launch(strategy_core_loop, rx_ring, strategy_core);

    // The main thread (core 1) takes over the receive loop and immediately begins polling
    receive_core_loop(port_id, rx_ring);

    // Wait for the background strategy core to exit cleanly
    rte_eal_mp_wait_lcore();

    // Clean up resources
    rte_ring_free(rx_ring);
    rte_eal_cleanup();
    std::cout << "HFT system cleaned up safely." << std::endl;
    return 0;
}
