#include <benchmark/benchmark.h>
#include <iostream>
#include <array>
#include <memory>

// induction variables
/*
    variables that change by fixed amounts on each loop iteration.

    It looks the optimizer will use induction variables optimization 
*/

// "noinline" force not to do induction variable optimization
__attribute__((noinline))
int calculate(int i)
{
    return i * 10 + 12;
}

// ==================== Benchmark for No Induction Variables Calls ====================
static void BM_NoInductionVariablesFunction(benchmark::State& state) {
    for (auto _ : state) {
        std::array<int, 100> a;
        for(auto i = 0; i < 100; ++i) {
            // a[i] = i * 10 + 12; // still do induction variable optimization
            a[i] = calculate(i);
        }
        benchmark::DoNotOptimize(a);
    }
}
BENCHMARK(BM_NoInductionVariablesFunction);

// ==================== Benchmark for Induction Variables Calls ====================
static void BM_WithInductionVariablesFunction(benchmark::State& state) {
    for (auto _ : state) {
        std::array<int, 100> a;

        int temp = 12;
        for(auto i = 0; i < 100; ++i) {
            a[i] = temp;
            temp += 10;
        }
        benchmark::DoNotOptimize(a);
    }
}
BENCHMARK(BM_WithInductionVariablesFunction);

// Main macro for the benchmark
BENCHMARK_MAIN();
