// Use DPDK's GCC built-in atomics instead of its C11 stdatomic path in C++.
#include <rte_config.h>
#undef RTE_ENABLE_STDATOMIC

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_lcore.h>
#include <rte_byteorder.h>
#include <rte_ethdev.h>
#include <rte_ether.h>

#include <iostream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <cstring>
#include <cstdlib>

constexpr size_t CACHE_LINE_SIZE = 64;

// 1. Simulate a standard mbuf structure (256 bytes, exactly 4x 64 bytes)
struct alignas(CACHE_LINE_SIZE) MockMbuf {
    uint32_t pkt_len;
    uint32_t data_off;
    uint64_t next_pointer;
    uint8_t  padding_meta[48]; // Metadata padding to reach 64 bytes
    uint8_t  data_buffer[192]; // Data area occupies 3 cache lines (192 bytes)
};

// 2. Simulate the physical channel mapping of the CPU memory controller
// With 4 channels, each 256-byte address increment (4 cache lines) causes the same modulo result when taken sequentially
size_t predict_memory_channel(uintptr_t address, size_t channels) {
    return (address / CACHE_LINE_SIZE) % channels;
}

void test1() {
    // Force 4 channels (the real hardware configuration most likely to trigger frequent collisions)
    constexpr size_t MEMORY_CHANNELS = 4;
    constexpr size_t OBJ_COUNT = 5;
    
    size_t raw_obj_size = sizeof(MockMbuf); // Strictly equals 256 bytes
    uintptr_t base_address = 0x10000;       // Simulated base huge-page physical address aligned to 64 bytes

    std::cout << "====================================================\n";
    std::cout << "--- Scheme A: compact layout (without DPDK padding optimization) ---\n";
    std::cout << "Original size of each mbuf: " << raw_obj_size << " bytes\n\n";

    for (size_t i = 0; i < OBJ_COUNT; ++i) {
        // In scheme A, objects are tightly packed in memory; the stride is 256
        uintptr_t obj_addr = base_address + (i * raw_obj_size);
        size_t ch = predict_memory_channel(obj_addr, MEMORY_CHANNELS);
        
        std::cout << "Object [" << i << "] memory address: 0x" 
                  << std::hex << std::uppercase << obj_addr 
                  << " -> 🎯 mapped to [memory channel " << std::dec << ch << "]\n";
    }
    std::cout << "\n❌ Disaster! All packets are crowded into [channel 0], while the other 3 channels remain completely idle!\n";
    std::cout << "====================================================\n\n";


    std::cout << "====================================================\n";
    std::cout << "--- Scheme B: DPDK dynamic cross-padding scatter (Padding) ---\n";
    
    // [DPDK core algorithm]
    size_t padding_size = 0;
    size_t stride_size = raw_obj_size;

    // Determine how much padding to insert
    /*
        1. Break down the variable meanings

            -   CACHE_LINE_SIZE (64 bytes): the basic memory cell (one carriage block).
            -   raw_obj_size (256 bytes): the actual size of your structure. stride_size 
            -   (stride): the total width consumed in memory after adding padding to one object.
            -   slots_per_obj: how many 64-byte cells this object occupies in total.

        2. Simulate the execution of the while loop

            -   🛑 First loop iteration (just entered):
            
                -   At this point stride_size is the original size, 256 bytes.

                -   Compute how many cells it occupies: slots_per_obj = 256 / 64 = 4 cells.

                -   Key check: if (4 % 4 != 0) -> 0 != 0 is false.

                -   Reasoning: “This is bad! 4 cells are evenly divisible by 4 channels! If I keep this
                    width, the first object will be in channel 0, and the second object will jump 4 cells and land back in channel 0 exactly.
                    This is a complete collision. We cannot exit the loop; we need to add padding to break this
                    pattern!”
                    
                -   Action:
                
                    -   Add 64 to padding_size, making it 64.
                    -   Add 64 to stride_size, making it 320 bytes.

            -   🔄 Second loop iteration:
            
                -   Now stride_size becomes 320 bytes.
                
                -   Compute its cell count again: slots_per_obj = 320 / 64 = 5 cells.
                
                -   Key check: if (5 % 4 != 0) -> 5 % 4 has a remainder of 1, and 1 != 0 is true!
                
                -   Reasoning: “Excellent! 5 cells are not divisible by 4. This means each object consumes a width of 5 cells.
                    The first object is in channel 0, and the second object shifts 5 cells and lands in channel 1 (0+5=5, 5%4=1).
                    This perfectly separates them!”
                    
                -   Action: execute break;. The loop exits successfully. The final padding is fixed at 64 bytes.
    */

    while (true) {
        size_t slots_per_obj = stride_size / CACHE_LINE_SIZE;
        // Core logic: if the number of cache-line blocks occupied by one object is divisible by the number of channels, collisions are inevitable
        // We must add padding so the block count is out of sync with the channel count (coprime)
        if (slots_per_obj % MEMORY_CHANNELS != 0) {
            break;
        }
        padding_size += CACHE_LINE_SIZE; // Force an extra cache line (64 bytes) each time
        stride_size += CACHE_LINE_SIZE;
    }

    std::cout << "DPDK detected a conflict and forcibly appended padding: " << padding_size << " bytes\n";
    std::cout << "Final actual optimized stride: " << stride_size << " bytes (0x" << std::hex << stride_size << ")\n\n";

    for (size_t i = 0; i < OBJ_COUNT; ++i) {
        // In scheme B, objects are separated by gaps, and the stride becomes 320 bytes (0x140)
        uintptr_t obj_addr = base_address + (i * stride_size);
        size_t ch = predict_memory_channel(obj_addr, MEMORY_CHANNELS);
        
        std::cout << "Object [" << i << "] memory address: 0x" 
                  << std::hex << std::uppercase << obj_addr 
                  << " -> ➔ mapped to [memory channel " << std::dec << ch << "]\n";
    }
    std::cout << "\n🎉 Perfect! Channels are evenly distributed as 0 -> 1 -> 2 -> 3 -> 0, and memory bandwidth is quadrupled!\n";
    std::cout << "====================================================\n";
}

static unsigned get_gcd(unsigned a, unsigned b) {
    unsigned c;

    if (0 == a)
        return b;
    if (0 == b)
        return a;

    // make sure b < a
    if (b < a) {
        c = a;
        a = b;
        b = c;
    }

    // gcd(a, b) = gcd(b, r) = gcd(b, a % b)
    /*
        This process is like a relay race in which each loop drops the old value and replaces it with a smaller pair (b, a % b) 
        and continues until b (the remainder) becomes 0.
    */
    while (b != 0) {
        c = a % b;
        a = b;
        b = c;
    }

    return a;
}

static unsigned arch_mem_object_align(unsigned obj_size) {
    unsigned nrank, nchan;
	unsigned new_obj_size;

    /* get number of channels */
	nchan = rte_memory_get_nchannel();
	if (nchan == 0)
		nchan = 4;

    /* get number of ranks */
	nrank = rte_memory_get_nrank();
	if (nrank == 0)
		nrank = 1;
}

void test2() {
    auto nchan = rte_memory_get_nchannel();
    if (nchan == 0)
        nchan = 2;

    auto nrank = rte_memory_get_nrank();
    if (nrank == 0)
        nrank = 1;

    std::cout << "nchan = " << nchan << std::endl;
    std::cout << "nrank = " << nrank << std::endl;

    std::cout << "RTE_MEMPOOL_ALIGN_MASK = " << RTE_MEMPOOL_ALIGN_MASK << std::endl;
    std::cout << "RTE_MEMPOOL_ALIGN = " << RTE_MEMPOOL_ALIGN << std::endl;

    // Integer ceiling technique

}

static rte_mbuf* build_packet(rte_mempool* pool, uint16_t port_id) {
    rte_mbuf* mbuf = rte_pktmbuf_alloc(pool);
    if (mbuf == nullptr) {
        return nullptr;
    }

    // Write the original payload first
    constexpr char payload[] = "hello dpdk";
    auto* payload_addr = static_cast<char*>(rte_pktmbuf_append(mbuf, sizeof(payload) - 1));

    if (payload_addr == nullptr) {
        rte_pktmbuf_free(mbuf);
        return nullptr;
    }

    std::memcpy(payload_addr, payload, sizeof(payload) - 1);

    // Use headroom to prepend an Ethernet header before the payload
    auto* eth = reinterpret_cast<rte_ether_hdr*>
                (rte_pktmbuf_prepend(mbuf, sizeof(rte_ether_hdr)));

    if (eth == nullptr) {
        rte_pktmbuf_free(mbuf);
        return nullptr;
    }

    rte_ether_addr src{};
    rte_eth_macaddr_get(port_id, &src);

    rte_ether_addr dst = {
        .addr_bytes = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
    };

    rte_ether_addr_copy(&src, &eth->src_addr);
    rte_ether_addr_copy(&dst, &eth->dst_addr);

    eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    return mbuf;
}

int send_one_packet(uint16_t port_id, uint16_t tx_queue,
                    rte_mempool* pool) {

    rte_mbuf* mbuf = build_packet(pool, port_id);
    if (mbuf == nullptr) {
        return -1;
    }

    rte_mbuf* packets[] = {mbuf};

    uint16_t sent = rte_eth_tx_burst(
        port_id,
        tx_queue,
        packets,
        1);

    if (sent != 1) {
        // The NIC did not accept the mbuf; the application must still free it
        rte_pktmbuf_free(mbuf);
        return -1;
    }

    // After a successful send, ownership of the mbuf is transferred to the NIC driver
    return 0;
}

// Need to bind the NIC to DPDK and 2GB huge pages
// Run command: ./test -l 0-1 -n 4
int test3(int argc, char** argv) {
    // Initialize the environment abstraction layer (EAL).
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    }

    uint16_t port_id;
    if (rte_eth_dev_count_avail() == 0) {
        rte_exit(EXIT_FAILURE, "No available Ethernet port\n");
    }
    port_id = 0;

    rte_eth_dev_info dev_info{};
    ret = rte_eth_dev_info_get(port_id, &dev_info);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE, "Cannot get port information: %s\n",
                 rte_strerror(-ret));
    }

    constexpr uint16_t tx_queue_id = 0;
    constexpr uint16_t rx_queue_id = 0;
    constexpr uint16_t rx_ring_size = 1024;
    constexpr uint16_t tx_ring_size = 1024;
    constexpr unsigned mbuf_count = 8192;
    constexpr unsigned mbuf_cache_size = 256;

    int socket_id = rte_eth_dev_socket_id(port_id);
    if (socket_id < 0) {
        socket_id = rte_socket_id();
    }

    rte_mempool* pool = rte_pktmbuf_pool_create(
        "TX_MBUF_POOL",
        mbuf_count,
        mbuf_cache_size,
        0,
        RTE_MBUF_DEFAULT_BUF_SIZE,
        socket_id);
    if (pool == nullptr) {
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool: %s\n",
                 rte_strerror(rte_errno));
    }

    rte_eth_conf port_conf{};
    ret = rte_eth_dev_configure(port_id, 1, 1, &port_conf);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Cannot configure port %u: %s\n",
                 port_id, rte_strerror(-ret));
    }

    rte_eth_rxconf rx_conf = dev_info.default_rxconf;
    rx_conf.offloads = port_conf.rxmode.offloads;
    ret = rte_eth_rx_queue_setup(port_id, rx_queue_id, rx_ring_size,
                                 socket_id, &rx_conf, pool);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Cannot setup RX queue: %s\n",
                 rte_strerror(-ret));
    }

    rte_eth_txconf tx_conf = dev_info.default_txconf;
    tx_conf.offloads = port_conf.txmode.offloads;
    ret = rte_eth_tx_queue_setup(port_id, tx_queue_id, tx_ring_size,
                                 socket_id, &tx_conf);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Cannot setup TX queue: %s\n",
                 rte_strerror(-ret));
    }

    ret = rte_eth_dev_start(port_id);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Cannot start port %u: %s\n",
                 port_id, rte_strerror(-ret));
    }

    std::cout << "Sending one packet through port " << port_id << '\n';
    ret = send_one_packet(port_id, tx_queue_id, pool);
    if (ret != 0) {
        std::cerr << "rte_eth_tx_burst() did not accept the packet\n";
    } else {
        // This only means the NIC driver accepted the mbuf; it does not mean the peer has actually received it on the physical link
        std::cout << "Packet accepted by the TX burst\n";
    }

    rte_eth_dev_stop(port_id);
    rte_eth_dev_close(port_id);
    rte_eal_cleanup();
    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(int argc, char** argv) {
    test3(argc, argv);

    return 0;
}
