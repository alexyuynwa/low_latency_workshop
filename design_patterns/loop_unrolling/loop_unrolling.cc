#include <benchmark/benchmark.h>
#include <string>
#include <vector>
#include <cmath>
#include <random>

using namespace std;

// Note
/*
    -   Clang optimizer advantage

        Clang has more a aggressive optimizer which will translate the 
        following for loop:

        for (int i = 0; i < n; ++i) {
            result += i;
        }

        into a single statement:

        result = n * (n-1) / 2;

        Therefore, using benchmark::DoNotOptimize is necessary 
        to avoid the optimization. 

        Therefore, you will see big differences between 
        gcc and clang in terms of benchmark performance.
*/

// Benchmark function without loop unrolling
static void BM_LoopWithoutUnrolling(benchmark::State& state) 
{
    for (auto _ : state) 
    {
        int result = 0;
        for (int i = 0; i < state.range(0); ++i) 
        {
            benchmark::DoNotOptimize(result += i);
        }
    }
}
//BENCHMARK(BM_LoopWithoutUnrolling)->Arg(1000);

// Benchmark function with loop unrolling
static void BM_LoopWithUnrolling(benchmark::State& state) 
{
    for (auto _ : state) 
    {
        int result = 0;
        /*
        for (int i = 0; i < state.range(0); i += 4) {
            benchmark::DoNotOptimize(result += i + (i + 1) + (i + 2) + (i + 3));
        }
        */

        /*
        for (int i = 0; i < state.range(0); i += 5) {
            benchmark::DoNotOptimize(result += i + (i + 1) + (i + 2) + (i + 3) + (i + 4));
        }
        */

        for (int i = 0; i < state.range(0); i += 8) 
        {
            benchmark::DoNotOptimize(result += i + (i + 1) + (i + 2) + (i + 3) \ 
                                            + (i + 4) + (i + 5) + (i + 6) + (i + 7));
        }

        // benchmark::DoNotOptimize(result = state.range(0) * (state.range(0) - 1) / 2);

    }
}
//BENCHMARK(BM_LoopWithUnrolling)->Arg(1000);


class LoopFissionFixture : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State& state) override {
        n = state.range(0);
        
        // Initialize the test data
        a.resize(n);
        b.resize(n);
        c.resize(n);
        d.resize(n);
        result1.resize(n);
        result2.resize(n);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dist(1.0, 100.0);
        
        for (int i = 0; i < n; i++) {
            a[i] = dist(gen);
            b[i] = dist(gen);
            c[i] = dist(gen);
            d[i] = dist(gen);
        }
    }
    
    void TearDown(const benchmark::State&) override {
        // do some clean-ups
        a.clear();
        b.clear();
        c.clear();
        d.clear();
        result1.clear();
        result2.clear();
    }
    
protected:
    int n;
    std::vector<double> a, b, c, d;
    std::vector<double> result1, result2;
};

// Example 1: Math computation intensive
BENCHMARK_DEFINE_F(LoopFissionFixture, MathOperations_Fused)(benchmark::State& state) {
    for (auto _ : state) {
        // Fused loops - mixing different types of mathematical operations
        for (int i = 0; i < n; i++) {
            result1[i] = std::sin(a[i]) + std::cos(b[i]); // Trigonometric functions
            result2[i] = std::log(c[i] + 1.0) * std::sqrt(d[i]); // Logarithms and square roots
        }
        benchmark::DoNotOptimize(result1);
        benchmark::DoNotOptimize(result2);
        benchmark::ClobberMemory();
    }
}

BENCHMARK_DEFINE_F(LoopFissionFixture, MathOperations_Fission)(benchmark::State& state) {
    for (auto _ : state) {
        // Split loops - separate different types of mathematical operations
        for (int i = 0; i < n; i++) {
            result1[i] = std::sin(a[i]) + std::cos(b[i]); // Only perform trigonometric functions
        }
        for (int i = 0; i < n; i++) {
            result2[i] = std::log(c[i] + 1.0) * std::sqrt(d[i]); // Only perform logarithms and square roots
        }
        benchmark::DoNotOptimize(result1);
        benchmark::DoNotOptimize(result2);
        benchmark::ClobberMemory();
    }
}

// example 2: Memory access pattern optimization
BENCHMARK_DEFINE_F(LoopFissionFixture, MemoryAccess_Fused)(benchmark:: State& state) {
    for (auto _ : state) {
        // Fused loops - alternating access to different arrays
        for (int i = 0; i < n; i++) {
            a[i] = b[i] * 2.0 + c[i];  // Access arrays b and c
            d[i] = a[i] + b[i] * 0.5;  // Access b again, mixed access pattern
        }
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(d);
        benchmark::ClobberMemory();
    }
}

BENCHMARK_DEFINE_F(LoopFissionFixture, MemoryAccess_Fission)(benchmark::State& state) {
    for (auto _ : state) {
        // Split loops - each loop focuses on accessing relevant data
        for (int i = 0; i < n; i++) {
            a[i] = b[i] * 2.0 + c[i];  // Mainly access b and c
        }
        for (int i = 0; i < n; i++) {
            d[i] = a[i] + b[i] * 0.5;  // access a and b
        }
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(d);
        benchmark::ClobberMemory();
    }
}

// example 3: Mix of simple and complex operations
BENCHMARK_DEFINE_F(LoopFissionFixture, MixedComplexity_Fused)(benchmark::State& state) {
    for (auto _ : state) {
        // Fused loops - mixing simple and complex operations
        for (int i = 0; i < n; i++) {
            result1[i] = a[i] + b[i];                    // Simple addition
            result2[i] = std::exp(std::sin(c[i]));       // complex math calculation
        }
        benchmark::DoNotOptimize(result1);
        benchmark::DoNotOptimize(result2);
        benchmark::ClobberMemory();
    }
}

BENCHMARK_DEFINE_F(LoopFissionFixture, MixedComplexity_Fission)(benchmark::State& state) {
    for (auto _ : state) {
        // Split loops - separate simple and complex operations
        for (int i = 0; i < n; i++) {
            result1[i] = a[i] + b[i];  // Simple addition only
        }
        for (int i = 0; i < n; i++) {
            result2[i] = std::exp(std::sin(c[i]));  // Complex calculation only
        }
        benchmark::DoNotOptimize(result1);
        benchmark::DoNotOptimize(result2);
        benchmark::ClobberMemory();
    }
}

// Example 4: Conditional check separation
BENCHMARK_DEFINE_F(LoopFissionFixture, ConditionCheck_Fused)(benchmark::State& state) {
    for (auto _ : state) {
        // Fused loops - mixing conditional checks and computations
        for (int i = 0; i < n; i++) {
            if (a[i] > 50.0) {  // conditional checks
                result1[i] = std::sqrt(a[i]) * std::log(b[i]);  // complex calculation
            } else {
                result1[i] = 0.0;
            }
        }
        benchmark::DoNotOptimize(result1);
        benchmark::ClobberMemory();
    }
}

BENCHMARK_DEFINE_F(LoopFissionFixture, ConditionCheck_Fission)(benchmark::State& state) {
    for (auto _ : state) {
        // Split loops - perform conditional checks first, then do computations
        std::vector<bool> should_compute(n);
        for (int i = 0; i < n; i++) {
            should_compute[i] = (a[i] > 50.0);  // conditional checks only
        }
        for (int i = 0; i < n; i++) {
            if (should_compute[i]) {
                result1[i] = std::sqrt(a[i]) * std::log(b[i]);  // complex calculation only
            } else {
                result1[i] = 0.0;
            }
        }
        benchmark::DoNotOptimize(result1);
        benchmark::ClobberMemory();
    }
}

// Register benchmark
BENCHMARK_REGISTER_F(LoopFissionFixture, MathOperations_Fused)->Range(1024, 256*1024);
BENCHMARK_REGISTER_F(LoopFissionFixture, MathOperations_Fission)->Range(1024, 256*1024);
BENCHMARK_REGISTER_F(LoopFissionFixture, MemoryAccess_Fused)->Range(1024, 256*1024);
BENCHMARK_REGISTER_F(LoopFissionFixture, MemoryAccess_Fission)->Range(1024, 256*1024);
BENCHMARK_REGISTER_F(LoopFissionFixture, MixedComplexity_Fused)->Range(1024, 256*1024);
BENCHMARK_REGISTER_F(LoopFissionFixture, MixedComplexity_Fission)->Range(1024, 256*1024);
BENCHMARK_REGISTER_F(LoopFissionFixture, ConditionCheck_Fused)->Range(1024, 256*1024);
BENCHMARK_REGISTER_F(LoopFissionFixture, ConditionCheck_Fission)->Range(1024, 256*1024);

// Simple function version (without using Fixture)
static void SimpleLoopFission_Benchmark(benchmark::State& state) {
    int n = state.range(0);
    std::vector<double> input(n), output1(n), output2(n);
    
    // Initialization
    for (int i = 0; i < n; i++) {
        input[i] = i * 0.1;
    }
    
    for (auto _ : state) {
        // Test different loop strategies
        if (state.range(1) == 0) {
            // Fused loops
            for (int i = 0; i < n; i++) {
                output1[i] = std::sin(input[i]);
                output2[i] = std::cos(input[i]);
            }
        } else {
            // Split loops
            for (int i = 0; i < n; i++) {
                output1[i] = std::sin(input[i]);
            }
            for (int i = 0; i < n; i++) {
                output2[i] = std::cos(input[i]);
            }
        }
        benchmark::DoNotOptimize(output1);
        benchmark::DoNotOptimize(output2);
    }
}

BENCHMARK(SimpleLoopFission_Benchmark)
    ->Args({1024, 0})    // Fused, 1024 elements
    ->Args({1024, 1})    // Split, 1024 elements
    ->Args({8192, 0})    // Fused, 8192 elements
    ->Args({8192, 1})    // Split, 8192 elements
    ->Args({65536, 0})   // Fused, 65536 elements
    ->Args({65536, 1});  // Split, 65536 elements

BENCHMARK_MAIN();
