#include <vector>
#include <iostream>
#include <emmintrin.h> // SSE
#include <immintrin.h> // AVX
#include <benchmark/benchmark.h>

void GenerateTestData(float* a, float* b, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        a[i] = 1.0;
        b[i] = 2.0;
    }
}

//__attribute__((optimize("no-tree-vectorize")))
void AddArrays(float* a, float* b, float* c, size_t size)
{
	for (size_t i = 0; i < size; ++i) 
    {
    	c[i] = a[i] + b[i];
        benchmark::ClobberMemory();
	}
}

static void BM_ArrayAddition(benchmark::State& state)
{
    const size_t size = 10000; // Test array size
    
    float* a = new float[size];
	float* b = new float[size];
	float* c = new float[size];

    GenerateTestData(a, b, size);

	for (auto _ : state) 
    {
        AddArrays(a, b, c, size);
	}

    state.SetItemsProcessed(state.iterations() * size);
    //benchmark::DoNotOptimize(c);

    delete[] a;
    delete[] b;
    delete[] c;
}
BENCHMARK(BM_ArrayAddition);

// SSE
void AddArraysSSE(float* a, float* b, float* c, size_t size)
{
    __m128 a_chunk, b_chunk, c_chunk; // Declare three 128-bit vector registers

    for (size_t i = 0; i < size; i += 4) // Process 4 floats per loop iteration
    {
        a_chunk = _mm_loadu_ps(&a[i]); // Unaligned load of 4 floats from memory into a_chunk
        b_chunk = _mm_loadu_ps(&b[i]); // Unaligned load of 4 floats from memory into b_chunk
        c_chunk = _mm_add_ps(a_chunk, b_chunk);  // SIMD addition: 4 floats added simultaneously
        _mm_storeu_ps(&c[i], c_chunk);   // Unaligned store of results back to memory
    }
    benchmark::ClobberMemory();
}

static void BM_AddArraysSSE(benchmark::State& state)
{
    const size_t size = 10000; // Test array size
    
    float* a = new float[size];
	float* b = new float[size];
	float* c = new float[size];

    GenerateTestData(a, b, size);

	for (auto _ : state) 
    {
        AddArraysSSE(a, b, c, size);
	}

    state.SetItemsProcessed(state.iterations() * size);
    //benchmark::DoNotOptimize(c);

    delete[] a;
    delete[] b;
    delete[] c;
}
BENCHMARK(BM_AddArraysSSE);

// avx2 (unaligned version)
void AddArraysAVX2(float* a, float* b, float* c, size_t size)
{
    __m256 a_chunk, b_chunk, c_chunk; // Declare three 256-bit vector registers

    for (size_t i = 0; i < size; i += 8) // Process 8 floats per loop iteration
    {
        a_chunk = _mm256_loadu_ps(a + i); // Unaligned load of 8 floats from memory into a_chunk
        b_chunk = _mm256_loadu_ps(b + i); // Unaligned load of 8 floats from memory into b_chunk
        c_chunk = _mm256_add_ps(a_chunk, b_chunk);  // SIMD addition: 8 floats added simultaneously
        _mm256_storeu_ps(c + i, c_chunk);   // Unaligned store of results back to memory
    }
    benchmark::ClobberMemory(); // asm volatile ("" : : : "memory");
}

static void BM_AddArraysAVX2(benchmark::State& state)
{
    const size_t size = 10000; // Test array size
    
    float* a = new float[size];
	float* b = new float[size];
	float* c = new float[size];

    GenerateTestData(a, b, size);

	for (auto _ : state) 
    {
        AddArraysAVX2(a, b, c, size);
	}

    state.SetItemsProcessed(state.iterations() * size);
    //benchmark::DoNotOptimize(c);

    delete[] a;
    delete[] b;
    delete[] c;
}
BENCHMARK(BM_AddArraysAVX2);

// avx2 (aligned version)
void AddArraysAVX2Aligned(float* a, float* b, float* c, size_t size)
{
    __m256 a_chunk, b_chunk, c_chunk; // Declare three 256-bit vector registers

    for (size_t i = 0; i < size; i += 8) // Process 8 floats per loop iteration
    {
        a_chunk = _mm256_load_ps(a + i); // Unaligned load of 8 floats from memory into a_chunk
        b_chunk = _mm256_load_ps(b + i); // Unaligned load of 8 floats from memory into b_chunk
        c_chunk = _mm256_add_ps(a_chunk, b_chunk);  // SIMD addition: 8 floats added simultaneously
        _mm256_store_ps(c + i, c_chunk);   // Unaligned store of results back to memory

        benchmark::ClobberMemory(); // asm volatile ("" : : : "memory");
    }
}

static void BM_AddArraysAVX2Aligned (benchmark::State& state)
{
    const size_t size = 10000; // Test array size
    
    float* a = static_cast<float*>(_mm_malloc(size * sizeof(float), 32));
    float* b = static_cast<float*>(_mm_malloc(size * sizeof(float), 32));
    float* c = static_cast<float*>(_mm_malloc(size * sizeof(float), 32));
    
    if (!a || !b || !c) {
        state.SkipWithError("Memory allocation failed");
        return;
    }

    GenerateTestData(a, b, size);

	for (auto _ : state) 
    {
        AddArraysAVX2Aligned(a, b, c, size);
	}

    state.SetItemsProcessed(state.iterations() * size);
    //benchmark::DoNotOptimize(c);

    // Use aligned free
    _mm_free(a);
    _mm_free(b);
    _mm_free(c);
}
BENCHMARK(BM_AddArraysAVX2Aligned);

// Requires compilation with -mavx512 flag, ensure CPU supports AVX512
// void AddArraysAVX512(float* a, float* b, float* c, size_t size)
// {
//     __m512 a_chunk, b_chunk, c_chunk; // Declare three 512-bit vector registers

//     for (size_t i = 0; i < size; i += 16) // Process 16 floats per loop iteration
//     {
//         a_chunk = _mm512_loadu_ps(a + i); // Unaligned load of 16 floats from memory into a_chunk
//         b_chunk = _mm512_loadu_ps(b + i); // Unaligned load of 16 floats from memory into b_chunk
//         c_chunk = _mm512_add_ps(a_chunk, b_chunk);  // SIMD addition: 16 floats added simultaneously
//         _mm512_storeu_ps(c + i, c_chunk);   // Unaligned store of results back to memory

//         benchmark::ClobberMemory(); // asm volatile ("" : : : "memory");
//     }
// }

// static void BM_AddArraysAVX512(benchmark::State& state)
// {
//     const size_t size = 10000; // Test array size
//     const size_t alignment = 16; // How many bytes alignment
    
//     // float* a = new float[size];
//     // float* b = new float[size];
// 	// float* c = new float[size];
//     // std::cout << "max_align_t: " << alignof(std::max_align_t) << " bytes\n";
//     float* a = static_cast<float*>(aligned_alloc(alignment, size * sizeof(float)));

//     if (a == nullptr) {
//         throw std::bad_alloc();
//     }
//     float* b = static_cast<float*>(aligned_alloc(alignment, size * sizeof(float)));
//     float* c = static_cast<float*>(aligned_alloc(alignment, size * sizeof(float)));


//     GenerateTestData(a, b, size);

// 	for (auto _ : state) 
//     {
//         AddArraysAVX512(a, b, c, size);
// 	}

//     benchmark::DoNotOptimize(c);

//     delete[] a;
//     delete[] b;
//     delete[] c;
// }
// BENCHMARK(BM_AddArraysAVX512);

//#pragma GCC optimize("no-tree-vectorize") // stop vectorization
static void __attribute__((optimize("no-tree-vectorize"))) 
BM_Convert_Scalar(benchmark::State& state) {
    const size_t n = state.range(0);
    std::vector<uint8_t> src(n);
    std::vector<uint32_t> dst(n);
    
    for (auto _ : state) {
        for (size_t i = 0; i < n; ++i) {
            dst[i] = src[i];
        }
        benchmark::DoNotOptimize(dst.data());  // Prevent the compiler from optimizing away the whole loop
    }
    state.SetBytesProcessed(state.iterations() * n * sizeof(uint8_t));
}
//BENCHMARK(BM_Convert_Scalar)->Arg(1024)->Arg(10240)->Arg(102400);
//#pragma GCC optimize("tree-vectorize")  // recover vectorization

// 2. AVX2 version: this is the core path you are testing
static void BM_Convert_AVX2(benchmark::State& state) {
    const size_t n = state.range(0);
    std::vector<uint8_t> src(n);
    std::vector<uint32_t> dst(n);
    
    for (auto _ : state) {
        size_t i = 0;
        for (; i + 8 <= n; i += 8) {
            // Process 8 elements per iteration
            __m128i xmm = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src.data() + i));
            __m256i ymm = _mm256_cvtepu8_epi32(xmm);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst.data() + i), ymm);
        }
        // Process the remaining elements when there are fewer than 8 left
        for (; i < n; ++i) {
            dst[i] = src[i];
        }
        benchmark::DoNotOptimize(dst.data());
    }
    state.SetBytesProcessed(state.iterations() * n * sizeof(uint8_t));
}
//BENCHMARK(BM_Convert_AVX2)->Arg(1024)->Arg(10240)->Arg(102400);

BENCHMARK_MAIN();
