#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <iostream>
#include <thread>

// Note
/*
    Performance comparison:

    LockFree > SpinLock > Mutex > Shared_Mutex

    Please keep in mind that in our benchmark, the critical section has only 
    one operation but in real scenarios it has more.
*/

template <typename LockType>
class LockBenchmark : public benchmark::Fixture
{
public:
    void SetUp(const benchmark::State&) override
    {
        counter = 0;
    }

    void TearDown(const benchmark::State& state) override
    {
        // std::thread::id this_id = std::this_thread::get_id();
        // int thread_index = state.thread_index();

        // std::cout << "Thread " << thread_index << " (ID: " << this_id 
        //         << ") ran " << state.iterations() << " iterations" << std::endl;
        
        // std::cout << "=== Benchmark Completion Report ===" << std::endl;
        // std::cout << "Requested max iterations: " << state.max_iterations << std::endl;
        // std::cout << "Actual iterations run: " << state.iterations() << std::endl;
        // std::cout << "Final counter: " << counter << std::endl;

    }

protected:
    // std::atomic<int> counter;
    int counter;
    LockType lock; // Templated lock object
};

class LockFreeBenchmark : public benchmark::Fixture
{
public:
    void SetUp(const benchmark::State&) override
    {
        counter = 0;
    }

    void TearDown(const benchmark::State& state) override
    {
        // std::thread::id this_id = std::this_thread::get_id();
        // int thread_index = state.thread_index();

        // std::cout << "Thread " << thread_index << " (ID: " << this_id 
        //         << ") ran " << state.iterations() << " iterations" << std::endl;
        
        // std::cout << "=== Benchmark Completion Report ===" << std::endl;
        // std::cout << "Requested max iterations: " << state.max_iterations << std::endl;
        // std::cout << "Actual iterations run: " << state.iterations() << std::endl;
        // std::cout << "Final counter: " << counter << std::endl;

    }
protected:
    std::atomic<int> counter;
};

BENCHMARK_DEFINE_F(LockFreeBenchmark, LockFreeTest)(benchmark::State& state) {
    for (auto _ : state) {
        counter.fetch_add(1, std::memory_order_relaxed);
    }
    state.SetItemsProcessed(state.iterations());
}

// Analysis
/*
    -   Why this SpinLock is so efficient under high contention:

        1. Reduces expensive exchange operations

            lock_.exchange() is a Read-Modify-Write (RMW) operation that requires
            cache coherence synchronization (MESI protocol) between CPUs, which 
            is very expensive.
            
            The exchange operation is only performed when the lock is likely to 
            be released (after internal load sees false), significantly reducing 
            RMW operations.

        2. Better cache friendliness

            -   load() is a pure read operation, much cheaper than exchange
            -   Multiple threads can simultaneously read the lock state without 
                generating cache coherence traffic
            -   When the lock is released, all waiting threads can observe the 
                change almost simultaneously

        3. Reduces bus contention

            -   Frequent exchange operations can saturate the memory bus and 
                cache coherence protocol
            -   Implementation two allows waiting threads to spin "quietly", 
                only participating in intense lock competition when necessary

    -   Correctness of memory ordering

        The memory ordering usage in both implementations is correct:

        -   exchange(..., std::memory_order_acquire): Establishes acquire semantics when acquiring the lock,
            ensuring critical section operations are not reordered before it

        -   load(std::memory_order_relaxed): During spin waiting, only atomicity is needed, not synchronization semantics,
            so relaxed is sufficient and lightest weight
    
*/
struct SpinLock {
    std::atomic<bool> lock_ = {false};

    void lock() 
    {
        // The first implementation
        /*
        for (;;) 
        {
            if (!lock_.exchange(true, std::memory_order_acquire)) 
            {
                break; // Successfully acquired the lock, breaking out
            }
            while (lock_.load(std::memory_order_relaxed));
            // {
            //     // Optional: Reduce CPU usage
            //     #ifdef __x86_64__
            //     __builtin_ia32_pause();
            //     #elif defined(__aarch64__)
            //     asm volatile("yield" ::: "memory");
            //     #else
            //     std::this_thread::yield();
            //     #endif
            // }
        }
        */

        // The second implementation performs almost equivalently to the first one.
        /*
        while(lock_.exchange(true, std::memory_order_acquire))
        {
            while(lock_.load(std::memory_order_relaxed)); //spin
        }
        */

        // The third implementation
        // Step 1: Fast path, assuming the lock is free(false)
        // and if exchange the lock to from false to true successfully, return 
        // immediately. This operation is the same as try_lock()
        if (!lock_.load(std::memory_order_relaxed) && 
            !lock_.exchange(true, std::memory_order_acquire)) {
            return; // Quick acquisition successful
        }

        while (true)
        {
            // First, wait for the lock to potentially release
            while (lock_.load(std::memory_order_relaxed)) {
                // Optionally add CPU pause instruction or thread yield
                // __builtin_ia32_pause(); // x86 pause instruction
                // std::this_thread::yield(); // Yield timeslice
            }

            // Try to acquire again
            // Why using exchange() is ok here?
            // -    exchange() is an atomic operation which means no one could 
            //      interrupt in between. 
            // -    If successfully change lock_'s value from false to true, 
            //      !lock.exchange() returns the lock_'s previous value, which 
            //      is false thus breaking out of the loop.
            //  -   ABA problem could happen here. As long as lock_exchange() returns 
            //      false, everything is fine.
            if (!lock_.exchange(true, std::memory_order_acquire)) {
                break;
            }
        }
        
        // The fourth implementation (from M$ chatbot)
        /*
        while (flag.test_and_set(std::memory_order_acquire)) {
            std::atomic_signal_fence(std::memory_order_acquire);   
        }*/
    }

    // the fourth implementation
    //alignas(64) std::atomic_flag flag = ATOMIC_FLAG_INIT;


    void unlock() 
    { 
        // !! guarantee the critical section codes are not reordered after it
        lock_.store(false, std::memory_order_release);

        // the fouth implementation
        //flag.clear(std::memory_order_release);
    }

    /*
        The try_lock() should first check if the lock is free before 
        attempting to acquire it. This would prevent excessive coherency 
        traffic in case someone loops over try_lock().
    */
    bool try_lock() noexcept
    {
        // First do a relaxed load to check if lock is free in order 
        // to prevent unnecessary cache misses if someone does while (!try_lock())
        return !lock_.load(std::memory_order_relaxed) && 
               !lock_.exchange(true, std::memory_order_acquire);
    }
};

// Define templated benchmarks
BENCHMARK_TEMPLATE_DEFINE_F(LockBenchmark, MutexTest, std::mutex)(benchmark::State& state) {
    for (auto _ : state) {
        lock.lock();
        // counter.fetch_add(1, std::memory_order_relaxed);
        counter++;
        lock.unlock();
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_TEMPLATE_DEFINE_F(LockBenchmark, SharedMutexTest, std::shared_mutex)(benchmark::State& state) {
    for (auto _ : state) {
        lock.lock();
        // counter.fetch_add(1, std::memory_order_relaxed);
        counter++;
        lock.unlock();
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_TEMPLATE_DEFINE_F(LockBenchmark, SpinLockTest, SpinLock)(benchmark::State& state) {
    for (auto _ : state) {
        lock.lock();
        // counter.fetch_add(1, std::memory_order_relaxed);
        counter++;
        lock.unlock();
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_REGISTER_F(LockBenchmark, MutexTest)->Name("Mutex")
    ->Threads(1)  // Test with 1 thread
    ->Threads(2)  // Test with 2 threads
    ->Threads(4); // Test with 4 threads

BENCHMARK_REGISTER_F(LockBenchmark, SharedMutexTest)->Name("SharedMutex")
    ->Threads(1)  // Test with 1 thread
    ->Threads(2)  // Test with 2 threads
    ->Threads(4);  // Test with 4 threads

BENCHMARK_REGISTER_F(LockBenchmark, SpinLockTest)->Name("SpinLock")
    ->Threads(1)  // Test with 1 thread
    ->Threads(2)  // Test with 2 threads
    ->Threads(4); // Test with 4 threads

BENCHMARK_REGISTER_F(LockFreeBenchmark, LockFreeTest)->Name("LockFreeTest")
    ->Threads(1)  // Test with 1 thread
    ->Threads(2)  // Test with 2 threads
    ->Threads(4);  // Test with 4 threads

BENCHMARK_MAIN();
