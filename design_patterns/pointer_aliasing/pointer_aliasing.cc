#include <benchmark/benchmark.h>
#include <cstring>
#include <vector>
#include <random>

void example2a(float *a, float *b, float *c) {
    for (int i = 0; i < 1024; i++) {
        a[i] = 0.0f;
        b[i] = 0.0f;
        for (int j = 0; j < 1024; j++) {
            a[i] = a[i] + c[i*1024 + j];
            b[i] = b[i] + c[i*1024 + j] * c[i*1024 + j];
        }
    }
}

void example2b(float * __restrict__ a, float * __restrict__ b, float * __restrict__ c) {
    for (int i = 0; i < 1024; i++) {
        a[i] = 0.0f;
        b[i] = 0.0f;
        for (int j = 0; j < 1024; j++) {
            a[i] = a[i] + c[i*1024 + j];
            b[i] = b[i] + c[i*1024 + j] * c[i*1024 + j];
        }
    }
}

// Size configuration: 1024 * 1024 matrix
constexpr size_t N = 1024;
constexpr size_t MATRIX_SIZE = N * N;

// --- Benchmark Registrations ---
class MemoryAliasFixture : public benchmark::Fixture {
public:
    std::vector<float> a;
    std::vector<float> b;
    std::vector<float> c;

    void SetUp(const ::benchmark::State&) override {
        a.resize(N, 0.0f);
        b.resize(N, 0.0f);
        c.resize(MATRIX_SIZE);

        // Initialize with random values to prevent compiler optimizing away the math
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        for (size_t i = 0; i < MATRIX_SIZE; ++i) {
            c[i] = dis(gen);
        }
    }

    void TearDown(const ::benchmark::State&) override {
        a.clear();
        b.clear();
        c.clear();
    }
};

BENCHMARK_F(MemoryAliasFixture, BM_Example2a_NoRestrict)(benchmark::State& state) {
    for (auto _ : state) {
        example2a(a.data(), b.data(), c.data());
        // Prevent compiler from optimizing out the whole function call
        benchmark::DoNotOptimize(a.data());
        benchmark::DoNotOptimize(b.data());
        benchmark::ClobberMemory();
    }
}

BENCHMARK_F(MemoryAliasFixture, BM_Example2b_WithRestrict)(benchmark::State& state) {
    for (auto _ : state) {
        example2b(a.data(), b.data(), c.data());
        benchmark::DoNotOptimize(a.data());
        benchmark::DoNotOptimize(b.data());
        benchmark::ClobberMemory();
    }
}

BENCHMARK_MAIN();
