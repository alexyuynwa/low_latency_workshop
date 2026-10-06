#include <variant>
#include <benchmark/benchmark.h>

union TradingUnion {
    int int_val;
    double double_val;
    int64_t long_val;
};

using TradingVariant = std::variant<int, double, int64_t>;

// Union benchmark
static void BM_UnionAccess(benchmark::State& state) {
    TradingUnion data;
    for (auto _ : state) {
        data.double_val = 150.25;
        benchmark::DoNotOptimize(data.double_val);
        data.int_val = 1000;
        benchmark::DoNotOptimize(data.int_val);
    }
}
BENCHMARK(BM_UnionAccess);

// Variant benchmark 
static void BM_VariantAccess(benchmark::State& state) {
    TradingVariant var;
    for (auto _ : state) {
        var = 150.25;
        benchmark::DoNotOptimize(std::get<double>(var));
        var = 1000;
        benchmark::DoNotOptimize(std::get<int>(var));
    }
}
BENCHMARK(BM_VariantAccess);

BENCHMARK_MAIN();
