#include <benchmark/benchmark.h>
#include <array>
#include <atomic>
#include <vector>
#include <thread>
#include <latch>
#include <iostream>
#include <numeric>
#include <random>

// ==================== Benchmark for Direct Sharing ====================
// WRONG: Atomic Contention
/*
    BM_DirectSharing (Atomic Contention): All threads concurrently execute 
    fetch_add on the exact same std::atomic<int>. At the CPU assembly level 
    (e.g., x86_64), this emits a LOCK XADD instruction. 
    
    This does not just invalidate cache lines; it triggers a cache lock or bus 
    lock at the hardware level. The 4 threads are forced to strictly serialize 
    (queue up) to modify that single piece of memory.
*/ 
void BM_DirectSharing(benchmark::State& state)
{
    // Number of total iterations to run
    const int num_iterations = 1 << 20;

    // Number of threads to spawn
    const int num_threads = 4;

    // Number of elements to process per thread
    const int elements_per_thread = num_iterations / num_threads;

    // Atomic integer to increment
    std::atomic<int> counter = 0;

    std::latch start_latch(num_threads);

    // Lambda for our work
    auto work = [&]()
    {
        // start_latch.arrive_and_wait(); // Synchronize thread start(macOS doesn't support!)
        for (int i = 0; i < elements_per_thread; i++) 
        {
            benchmark::DoNotOptimize(counter.fetch_add(1, std::memory_order_relaxed));
            benchmark::ClobberMemory();
        }
    };

    for(auto _ : state)
    {
        counter.store(0, std::memory_order_relaxed);
        // Spawn threads
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back(work);
        }

        for (auto& thread : threads)
        {
            thread.join();
        }

        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_DirectSharing);

struct Int {
    int value{0};
};

// WRONG: This causes false sharing
// The alignas(std::hardware_destructive_interference_size) applies to the 
// std::array itself (its starting address). The elements 
// inside std::array<Int, N> are packed tightly together.
template <size_t N>
struct BadCounters {
    alignas(std::hardware_destructive_interference_size) std::array<Int, N> counters;
};

// Why false sharing is bad?
/*
    Your threads execute a regular, non-atomic += 1. While the 4 variables 
    reside on the same cache line—causing the cache line to bounce violently 
    back and forth between CPU cores (Ping-Pong Effect)—it avoids the 
    massive overhead of hardware-level atomic synchronization and strict 
    queueing.

    (Ping-Pong effect on the MESI status: Modified -> Invalid -> (RFO) -> Modified...)
    
*/
void BM_FalseSharing(benchmark::State& state)
{
    // Number of total iterations to run
    const int num_iterations = 1 << 20;

    // Number of threads to spawn
    const int num_threads = 4;

    // Number of elements to process per thread
    const int elements_per_thread = num_iterations / num_threads;

    // atomic integers to increment
    BadCounters<num_threads> counters;
    for (int i = 0; i < num_threads; i++)
    {
        counters.counters[i].value = 0;
    }
    
    // Atomic integer to increment
    std::atomic<int> final_sum = 0;

    std::latch start_latch(num_threads);

    // Lambda for our work
    auto work = [&](int thread_id)
    {
        // start_latch.arrive_and_wait(); // Synchronize thread start(macOS doesn't support!)
        for (int i = 0; i < elements_per_thread; i++)
        {
            benchmark::DoNotOptimize(counters.counters[thread_id].value += 1);
        }
        
        benchmark::DoNotOptimize(final_sum.fetch_add(counters.counters[thread_id].value, std::memory_order_relaxed));
        benchmark::DoNotOptimize(counters.counters[thread_id].value = 0);

        benchmark::ClobberMemory();
    };

    for(auto _ : state)
    {
        final_sum.store(0, std::memory_order_relaxed);
        // Spawn threads
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back(work, i);
        }

        for (auto& thread : threads)
        {
            thread.join();
        }

        benchmark::ClobberMemory();
    }

}
BENCHMARK(BM_FalseSharing);

struct alignas(std::hardware_destructive_interference_size) PaddedInt {
    int value{0};
};

template <size_t N>
struct GoodCounters
{
    std::array<PaddedInt, N> counters;
};

void BM_NoSharing(benchmark::State& state)
{
    // Number of total iterations to run
    const int num_iterations = 1 << 20;

    // Number of threads to spawn
    const int num_threads = 4;

    // Number of elements to process per thread
    const int elements_per_thread = num_iterations / num_threads;

    // atomic integers to increment
    GoodCounters<num_threads> counters;
    for (int i = 0; i < num_threads; i++)
    {
        counters.counters[i].value = 0;
    }

    // Atomic integer to increment
    std::atomic<int> final_sum = 0;

    std::latch start_latch(num_threads);

    // Lambda for our work
    auto work = [&](int thread_id)
    {
        // start_latch.arrive_and_wait(); // Synchronize thread start (macOS doesn't support!)
        for (int i = 0; i < elements_per_thread; i++)
        {
            benchmark::DoNotOptimize(counters.counters[thread_id].value += 1);
        }
        
        benchmark::DoNotOptimize(final_sum.fetch_add(counters.counters[thread_id].value, std::memory_order_relaxed));
        benchmark::DoNotOptimize(counters.counters[thread_id].value = 0);

        benchmark::ClobberMemory();
    };

    for(auto _ : state)
    {
        final_sum.store(0, std::memory_order_relaxed);
        // Spawn threads
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back(work, i);
        }

        for (auto& thread : threads)
        {
            thread.join();
        }

        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_NoSharing);

struct alignas(64) CacheLine {
    uint8_t data[64];
};

static std::vector<CacheLine>& GetMemoryPool() {
    static  std::vector<CacheLine> pool(2);
    return pool;
}

// No Cache Split Penalty
static void BM_CacheLine_Aligned(benchmark::State& state) {
    auto& pool = GetMemoryPool();
    uint8_t* raw_ptr = reinterpret_cast<uint8_t*>(pool.data());
    
    volatile uint64_t* aligned_ptr = reinterpret_cast<volatile uint64_t*>(raw_ptr);

    for (auto _ : state) {
        for (size_t i = 0; i < 1000; ++i) {
            *aligned_ptr = i;
            benchmark::DoNotOptimize(*aligned_ptr);
        }
    }
}
BENCHMARK(BM_CacheLine_Aligned);

// Has Cache Split Penalty
static void BM_CacheLine_Split(benchmark::State& state) {
    auto& pool = GetMemoryPool();
    uint8_t* raw_ptr = reinterpret_cast<uint8_t*>(pool.data());
    
    volatile uint64_t* split_ptr = reinterpret_cast<volatile uint64_t*>(raw_ptr + 60);

    for (auto _ : state) {
        for (size_t i = 0; i < 1000; ++i) {
            *split_ptr = i;
            benchmark::DoNotOptimize(*split_ptr);
        }
    }
}
BENCHMARK(BM_CacheLine_Split);


// Benchmark the performance impackted by L2 Adjacent Prefetcher
struct Aligned64Container {
    alignas(64) std::atomic<uint64_t> line_a{0};
    alignas(64) std::atomic<uint64_t> line_b{0};
};

static Aligned64Container g_data64;

static void BM_Adjacent_Conflict_64(benchmark::State& state) {
    for (auto _ : state) {
        if (state.thread_index() == 0) {
            // Thread 0 only writes to Line A
            for (int i = 0; i < state.range(0); ++i) {
                g_data64.line_a.fetch_add(1, std::memory_order_relaxed);
            }
        } else {
            // Thread 1 only writes to Line B
            for (int i = 0; i < state.range(0); ++i) {
                g_data64.line_b.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
}
BENCHMARK(BM_Adjacent_Conflict_64)->Arg(10000000)->Threads(2);

struct Aligned128Container {
    alignas(128) std::atomic<uint64_t> line_a{0};
    alignas(128) std::atomic<uint64_t> line_b{0};
};

static Aligned128Container g_data128;

static void BM_Adjacent_Conflict_128(benchmark::State& state) {
    for (auto _ : state) {
        if (state.thread_index() == 0) {
            // Thread 0 only writes to Line A
            for (int i = 0; i < state.range(0); ++i) {
                g_data128.line_a.fetch_add(1, std::memory_order_relaxed);
            }
        } else {
            // Thread 1 only writes to Line B
            for (int i = 0; i < state.range(0); ++i) {
                g_data128.line_b.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
}
BENCHMARK(BM_Adjacent_Conflict_128)->Arg(10000000)->Threads(2);

// Main macro for the benchmark
BENCHMARK_MAIN();
