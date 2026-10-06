#include <benchmark/benchmark.h>
#include <cmath>

// A typical error checking setup
int errorCounterA = 0;

// __attribute__((noinline))
bool checkForErrorA() 
{
    volatile double sum = 0;
    for (int i = 0; i < 1000; ++i) 
    {
        sum += std::sqrt(i * 1.01);
    }
    benchmark::DoNotOptimize(sum);
    
    // Produce an error once every 10 calls
    errorCounterA++;

    return (errorCounterA % 10) == 0;
}

// __attribute__((noinline))
bool checkForErrorB() {
    // Simulate some error check
    volatile double sum = 0;
    for (int i = 0; i < 1000; ++i) 
    {
        sum += std::sqrt(i * 1.01);
    }
    benchmark::DoNotOptimize(sum);
    
    return false;
}

// __attribute__((noinline))
bool checkForErrorC() {
    // Simulate some error check
    volatile double sum = 0;
    for (int i = 0; i < 1000; ++i) 
    {
        sum += std::sqrt(i * 1.01);
    }
    benchmark::DoNotOptimize(sum);
    return false;
}

__attribute__((noinline))
void handleErrorA() 
{
    // Simulate some error handling
    volatile double sum = 0;
    for (int i = 0; i < 10000; ++i) 
    {
        sum += std::sqrt(i * 1.01);
    }
    benchmark::DoNotOptimize(sum);
}

__attribute__((noinline))
void handleErrorB() 
{
    // Simulate some error handling
    volatile double sum = 0;
    for (int i = 0; i < 10000; ++i) 
    {
        sum += std::sqrt(i * 1.01);
    }
    benchmark::DoNotOptimize(sum);
}

__attribute__((noinline))
void handleErrorC() {
    // Simulate some error handling
    volatile double sum = 0;
    for (int i = 0; i < 10000; ++i) 
    {
        sum += std::sqrt(i * 1.01);
    }
    benchmark::DoNotOptimize(sum);
}

__attribute__((noinline))
void executeHotpath() {
  // Simulate some hot path execution
}

static void Branching(benchmark::State& state) {
    errorCounterA = 0;  // reset the counter before benchmark run
    for (auto _ : state) {
        if (checkForErrorA())
            handleErrorA();
        else if (checkForErrorB())
            handleErrorB();
        else if (checkForErrorC())
            handleErrorC();
        else
            executeHotpath(); // 90% goes here
        
        benchmark::DoNotOptimize(errorCounterA);
        benchmark::ClobberMemory();
    }
}

// A new setup using flags
enum ErrorFlags {
  ErrorA = 1 << 0,
  ErrorB = 1 << 1,
  ErrorC = 1 << 2,
  NoError = 0
};

int errorCounterFlags = 0;

// __attribute__((noinline))
ErrorFlags checkErrors() {
    volatile double sum = 0;
    for (int i = 0; i < 1000; ++i) 
    {
        sum += std::sqrt(i * 1.01);
    }
    benchmark::DoNotOptimize(sum);

    // Produce ErrorA once every 10 calls
    errorCounterFlags++;
    return (errorCounterFlags % 10) == 0 ? ErrorA : NoError;
}

void HandleError(ErrorFlags errorFlags) {
    // Simulate some error handling based on flags
    if (errorFlags & ErrorA) {
        handleErrorA();
    }
    // handle other errors similarly...
}

void hotpath() {
  // Simulate some hot path execution
}

static void ReducedBranching(benchmark::State& state) 
{
    errorCounterFlags = 0;  // reset the counter before benchmark run
    for (auto _ : state) 
    {
        ErrorFlags errorFlags = checkErrors();
        if (!errorFlags)
            hotpath(); // 90% goes here
        else
            HandleError(errorFlags);
        benchmark::DoNotOptimize(errorCounterFlags);
        benchmark::ClobberMemory();
    }
}

// Register the functions as a benchmark
BENCHMARK(Branching);
BENCHMARK(ReducedBranching);

BENCHMARK_MAIN();
