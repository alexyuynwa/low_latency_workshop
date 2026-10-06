#include <benchmark/benchmark.h>
#include <iostream>
#include <array>
#include <memory>

// Note
/*
  It looks both BM_VirtualFunction and BM_CRTPFunction have the same performance. 
  How come? 
*/

// Hypothesis - what makes virtual calls slower
/*
    -   Extra indirection (pointer dereference) for each call to a virtual 
        method.

    -   Virtual methods usually can’t be inlined, which may be a significant 
        cost hit for some small methods.

    -   Additional pointer per object. On 64-bit systems which are prevalent 
        these days, this is 8 bytes per object. For small objects that carry 
        little data this may be a serious overhead.
*/

// Config file
struct Config 
{
    bool use_a = true;
    bool UseOrderSenderA() const { return use_a; }
};

constexpr long long N = 200'000'000;  // Number of calls


//
// ==================== Virtual Function Interface ====================
class VirtualBase {
public:
    virtual void execute() = 0;    
    virtual ~VirtualBase() = default; // Good practice: virtual destructor
};

class VirtualDerivedA : public VirtualBase {
public:
void execute() override {
        // Some non-trivial work that's easy to optimize away
    }
};

class VirtualDerivedB : public VirtualBase {
public:
void execute() override {
        // Some non-trivial work that's easy to optimize away
    }
};

struct ExecuteManagerVirtual
{
    ExecuteManagerVirtual(std::unique_ptr<VirtualBase> e) : executor(std::move(e)) {}

    void MainLoop()
    {        
        volatile long long volatile_N = N; // Prevent loop iterations from being optimized away
        int dummy_result = 0;
        
        for (long long i = 0; i < volatile_N; ++i) {
            executor->execute(); // This is a dynamic dispatch!
            dummy_result += i; // accumulated result
        }
        // The core loop: call the CRTP interface function
        benchmark::DoNotOptimize(dummy_result);
        benchmark::ClobberMemory();
    }

    std::unique_ptr<VirtualBase> executor;
};

std::unique_ptr<ExecuteManagerVirtual> MakeExecuteManagerVirtual(const Config& c)
{
    if (c.UseOrderSenderA())
        return std::make_unique<ExecuteManagerVirtual>(std::make_unique<VirtualDerivedA>());
    else
        return std::make_unique<ExecuteManagerVirtual>(std::make_unique<VirtualDerivedB>());
}

// ==================== Benchmark for Virtual Calls ====================
void BM_VirtualFunction(benchmark::State& state)
{
    Config cfg;
    cfg.use_a = true;
    auto manager_v_ptr = MakeExecuteManagerVirtual(cfg);

    for (auto _ : state) 
    {
        manager_v_ptr->MainLoop();
    }
}
BENCHMARK(BM_VirtualFunction);

// ==================== CRTP Interface ====================
template <typename Derived>
class CrtpBase {
public:
    void execute() {
        // Static polymorphism: delegate to the derived class
        static_cast<Derived*>(this)->execute_impl();
    }

    // Non-virtual destructor is fine for CRTP
};

class CrtpDerivedA : public CrtpBase<CrtpDerivedA> {
public:
    void execute_impl() 
    {
        // Perform the *exact same* operation as VirtualDerived
    }
};

class CrtpDerivedB : public CrtpBase<CrtpDerivedB> {
public:
    void execute_impl() 
    {
        // Perform the *exact same* operation as VirtualDerived
    }
};

class IExecuteManagerCrtp
{
public:
    virtual ~IExecuteManagerCrtp() = default;
    virtual void MainLoop() = 0;
};

template <typename T>
class ExecuteManagerCrtp : public IExecuteManagerCrtp
{
public:
    void MainLoop()
    {        
        volatile long long volatile_N = N; // Prevent loop iterations from being optimized away
        int dummy_result = 0;
        
        for (long long i = 0; i < volatile_N; ++i) {
            executor.execute(); // This is a dynamic dispatch!
            dummy_result += i; // accumulated result
        }
        // The core loop: call the CRTP interface function
        benchmark::DoNotOptimize(dummy_result);
        benchmark::ClobberMemory();
    }
    
    T executor;
};

std::unique_ptr<IExecuteManagerCrtp> MakeExecuteManagerCrtp(const Config& c)
{
    if (c.UseOrderSenderA())
        return std::make_unique<ExecuteManagerCrtp<CrtpDerivedA>>();
    else
        return std::make_unique<ExecuteManagerCrtp<CrtpDerivedB>>();
}

// ==================== Benchmark for CRTP Calls ====================
static void BM_CRTPFunction(benchmark::State& state) 
{
    // auto manager_crtp_ptr = std::make_unique<ExecuteManagerCrtp>(std::make_unique<CrtpDerivedA>());
    Config cfg;    
    cfg.use_a = true;
    auto manager_template_ptr = MakeExecuteManagerCrtp(cfg);

    for (auto _ : state)
    {
        // manager_crtp_ptr->MainLoop();        
        manager_template_ptr->MainLoop();
    }
}
BENCHMARK(BM_CRTPFunction);

// Main macro for the benchmark
BENCHMARK_MAIN();
