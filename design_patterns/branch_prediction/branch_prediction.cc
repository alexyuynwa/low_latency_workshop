#include <benchmark/benchmark.h>
#include <vector>
#include <random>

// gcc optimization: 
/*
    -   Remove branching 
*/

const size_t ARRAY_SIZE = 100'000;

std::vector<int> generate_random_data() {
    std::vector<int> data(ARRAY_SIZE);
    std::mt19937 prng(42);
    std::uniform_int_distribution<int> dist(0, 255);
    for (auto& val : data) val = dist(prng);
    return data;
}

const auto raw_data = generate_random_data();

static void BM_UnsortedArraySum(benchmark::State& state) {
    auto data = raw_data;

    for (auto _ : state) {
        long long sum = 0;
        for (size_t i = 0; i < ARRAY_SIZE; ++i) {
            if (data[i] >= 128) {
                sum += data[i];
            }
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_UnsortedArraySum);

static void BM_SortedArraySum(benchmark::State& state) {
    auto data = raw_data;
    std::sort(data.begin(), data.end());

    for (auto _ : state) {
        long long sum = 0;
        for (size_t i = 0; i < ARRAY_SIZE; ++i) {
            if (data[i] >= 128) {
                sum += data[i];
            }
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_SortedArraySum);

BENCHMARK_MAIN();