#include <benchmark/benchmark.h>
#include <string>
#include <vector>
#include <array>
#include <iostream>
#include <unordered_map>

// Basic SSO Benchmark
// Benchmark for string creation and destruction
static void BM_StringCreation_Short(benchmark::State& state) {
    for (auto _ : state) {
        std::string s = "short";
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_StringCreation_Short);

static void BM_StringCreation_Long(benchmark::State& state) {
    for (auto _ : state) {
        std::string s = "this is a very long string that definitely exceeds SSO buffer size";
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_StringCreation_Long);

// Benchmark for string copying
static void BM_StringCopy_Short(benchmark::State& state) {
    std::string short_str = "short";
    for (auto _ : state) {
        std::string copy = short_str;
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK(BM_StringCopy_Short);

static void BM_StringCopy_Long(benchmark::State& state) {
    std::string long_str = "this is a very long string that definitely exceeds SSO buffer size";
    for (auto _ : state) {
        std::string copy = long_str;
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK(BM_StringCopy_Long);

// Test different string lengths to find the SSO threshold
static void BM_StringCreation_VariousLengths(benchmark::State& state) {
    int length = state.range(0);
    std::string pattern(length, 'x'); // Create a string of 'x' with given length

    for (auto _ : state) {
        std::string s = pattern;
        benchmark::DoNotOptimize(s);
    }
    state.SetLabel("length=" + std::to_string(length));
}

// Advanced SSO Benchmark with Different String Sizes

// Test various lenths around typical SSO boundaries
BENCHMARK(BM_StringCreation_VariousLengths)->Arg(4)->Arg(8)->Arg(15)->Arg(16)->Arg(22)->Arg(23)->Arg(32);

static void BM_StringAssignment_VariousLengths(benchmark::State& state) {
    int length = state.range(0);
    std::string source(length, 'x');
    std::string target;
    
    for (auto _ : state) {
        target = source;
        benchmark::DoNotOptimize(target);
    }
    state.SetLabel("length=" + std::to_string(length));
}
BENCHMARK(BM_StringAssignment_VariousLengths)->Arg(4)->Arg(15)->Arg(16)->Arg(22)->Arg(23)->Arg(50);

// Vector Operations with SSO vs Non-SSO Strings

// Benchmark vector operations with small strings (SSO)
static void BM_VectorPushBack_ShortStrings(benchmark::State& state) {
    const int num_strings = state.range(0);
    std::vector<std::string> vec;
    vec.reserve(num_strings);

    for (auto _ : state) {
        state.PauseTiming();
        vec.clear();
        state.ResumeTiming();

        for (int i = 0; i < num_strings; ++i) {
            vec.push_back("short"); // SSO string
        }

        benchmark::DoNotOptimize(vec);
    }
}
BENCHMARK(BM_VectorPushBack_ShortStrings)->Arg(100)->Arg(1000)->Arg(10000);

// Benchmark vector operations with long strings (heap allocated)
static void BM_VectorPushBack_LongStrings(benchmark::State& state) {
    const int num_strings = state.range(0);
    std::vector<std::string> vec;
    vec.reserve(num_strings);
    std::string long_str = "this is a very long string that definitely exceeds SSO buffer size";

    for (auto _ : state) {
        state.PauseTiming();
        vec.clear();
        state.ResumeTiming();

        for (int i = 0; i < num_strings; ++i) {
            vec.push_back(long_str); // Heap-allocated string
        }
        benchmark::DoNotOptimize(vec);
    }
}
BENCHMARK(BM_VectorPushBack_LongStrings)->Arg(100)->Arg(1000)->Arg(10000);

// String Concatenation Benchmark
// Benchmark string concatenation with SSO
static void BM_StringConcatenation_Short(benchmark::State& state) {
    for (auto _ : state) {
        std::string result;
        for (int i = 0; i < state.range(0); ++i) {
            result += "abc"; // Short string, likely SSO
        }
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_StringConcatenation_Short)->Arg(10)->Arg(100);

// Benchmark string concatenation with long strings
static void BM_StringConcatenation_Long(benchmark::State& state) {
    std::string long_str = "this is a relatively long string fragment";
    for (auto _ : state) {
        std::string result;
        for (int i = 0; i < state.range(0); ++i) {
            result += long_str; // Long string, heap allocated
        }
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_StringConcatenation_Long)->Arg(10)->Arg(100);

// Memory Allocation Pattern Detection
bool likely_uses_sso(const std::string& str) {
    // This is a heuristic - the actual implementation is library specific
    return str.capacity() <= 15; // Common SSO threshold
}

// Benchmark to detect SSO threshold
static void BM_SSOThresholdDetection(benchmark::State& state) {
    int length = state.range(0);
    
    for (auto _ : state) {
        std::string s(length, 'x');
        bool uses_sso = likely_uses_sso(s);
        benchmark::DoNotOptimize(uses_sso);
        benchmark::DoNotOptimize(s);
        
        // Store the result in the counter
        state.counters["UsesSSO"] = uses_sso ? 1.0 : 0.0;
        state.counters["Capacity"] = s.capacity();
    }
    state.SetLabel("len=" + std::to_string(length));
}

// Test a range of string lengths to find the SSO boundary
BENCHMARK(BM_SSOThresholdDetection)->DenseRange(1, 30, 1);

// Benchmark map with short string keys (SSO)
static void BM_MapShortStringKeys(benchmark::State& state) {
    std::map<std::string, int> m;
    const int num_elements = state.range(0);
    
    // Insert elements
    for (int i = 0; i < num_elements; ++i) {
        m["key" + std::to_string(i)] = i;
    }
    
    for (auto _ : state) {
        // Lookup random elements
        for (int i = 0; i < 1000; ++i) {
            auto it = m.find("key" + std::to_string(i % num_elements));
            benchmark::DoNotOptimize(it);
        }
    }
}
BENCHMARK(BM_MapShortStringKeys)->Arg(100)->Arg(1000);

// Benchmark map with long string keys (heap)
static void BM_MapLongStringKeys(benchmark::State& state) {
    std::map<std::string, int> m;
    const int num_elements = state.range(0);
    
    // Insert elements with long keys
    for (int i = 0; i < num_elements; ++i) {
        m["this_is_a_very_long_string_key_" + std::to_string(i)] = i;
    }
    
    for (auto _ : state) {
        // Lookup random elements
        for (int i = 0; i < 1000; ++i) {
            auto it = m.find("this_is_a_very_long_string_key_" + std::to_string(i % num_elements));
            benchmark::DoNotOptimize(it);
        }
    }
}
BENCHMARK(BM_MapLongStringKeys)->Arg(100)->Arg(1000);

// Simple SSO detection (heuristic)
void demonstrate_sso_threshold() {
    std::cout << "SSO Threshold Detection:\n";
    for (int len : {10, 15, 16, 22, 23, 30}) {
        std::string s(len, 'x');
        std::cout << "Length " << len << ": capacity=" << s.capacity() 
                  << ", likely SSO=" << (s.capacity() <= 23) << "\n";
    }
    std::cout << "\n";
}

// Main macro for the benchmark
// BENCHMARK_MAIN();

// Optional: demonstrate SSO in action
int main(int argc, char** argv) {
    demonstrate_sso_threshold();
    
    // Run benchmarks
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
}
