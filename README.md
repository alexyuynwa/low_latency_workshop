# Low Latency Workshop

This repository is a C++ experimental workspace focused on low-latency, high-throughput, and system performance optimization. It contains two main areas:

- `design_patterns/`: experiments and implementations related to CPU cache behavior, branch prediction, SIMD, lock-free programming, memory layout, and micro-optimizations.
- `dpdk/`: data-plane development and performance testing using DPDK.

The goal of this project is to help understand how to design faster software, benchmark hotspots, and evaluate networking-related low-latency implementations.

## Repository Structure

```text
.
├── design_patterns/       # Low-latency design patterns and optimization experiments
│   ├── abseil-cpp/        # Abseil source tree
│   ├── branch_prediction/
│   ├── branch_reduction/
│   ├── cache_line/
│   ├── cache_warming/
│   ├── crtp/
│   ├── dictionary/
│   ├── google_benchmark/  # Google Benchmark
│   ├── lto_wpo/
│   ├── lock_free/
│   ├── loop_unrolling/
│   ├── pointer_aliasing/
│   ├── prefetching/
│   ├── short_circuiting/
│   ├── simd/
│   ├── sso/
│   ├── test/
│   ├── union_vs_variant/
│   ├── CMakeLists.txt
│   └── README.md
│
├── dpdk/                  # DPDK-related experiments and demos
│   ├── ITCH/
│   ├── FIX/
│   ├── simple_no_lock/
│   ├── simple_tap_test/
│   ├── test/
│   └── ...
│
├── .gitignore
├── .vscode/
├── README.md             # Repository overview
└── ...
```

## 1. design_patterns

The `design_patterns` directory focuses on practical techniques commonly used in performance engineering, including:

- Branch prediction
- Branch reduction
- Cache-friendly design
- SIMD and vectorization
- Loop unrolling
- Lock-free programming
- Pointer aliasing analysis
- Link-time optimization (LTO) and whole-program optimization (WPO)
- `std::variant` vs `union`
- Small string optimization (SSO)
- Dictionary and text-processing experiments

Many of these examples are benchmarked using Google Benchmark and built with CMake.

### Dependencies

Before building `design_patterns`, make sure the following are available:

- CMake
- A C++20-capable compiler (GCC or Clang)
- Google Benchmark
- Google Test

### Build Steps

```bash
cd /path/to/low_latency_workshop/design_patterns

git clone https://github.com/google/benchmark google_benchmark
cd google_benchmark
git clone https://github.com/google/googletest
cd ..

mkdir -p build
cd build
cmake ../
make -j$(nproc)
```

To force Clang as the compiler:

```bash
cmake -DCMAKE_CXX_COMPILER=clang++ ../
```

## 2. dpdk

The `dpdk/` directory contains DPDK-based experiments intended for packet processing, data-plane development, low-latency handling, and basic performance studies.

Current examples include:

- `simple_no_lock/`: experiments related to lock-free or low-contention processing
- `simple_tap_test/`: basic tap testing examples
- `ITCH/`: handling of ITCH data feeds
- `FIX/`: FIX 4.4 message parsing from DPDK Ethernet/IPv4/TCP packets
- `test/`: general DPDK testing examples

### Dependencies

These experiments depend on DPDK and typically require:

- DPDK library
- `pkg-config`
- CMake
- C++17 compiler support

### Example Build

```bash
cd /path/to/low_latency_workshop/dpdk/test
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

To confirm DPDK is installed on the system:

```bash
pkg-config --modversion libdpdk
```

## 3. Use Cases

This repository is suitable for:

- System performance tuning and low-latency design
- C++ micro-benchmarking and bottleneck analysis
- Networking and data-plane experiments with DPDK
- Understanding CPU architecture effects such as cache behavior, branch prediction, SIMD, and out-of-order execution
- Concurrency and lock-free structure validation

## 4. Practical Guidance

- Start with `design_patterns` to understand the fundamentals of performance engineering.
- Use benchmark tools to measure hotspots instead of relying on assumptions.
- In DPDK scenarios, pay attention to NUMA, core pinning, cache affinity, batching, and interrupt handling.
- For low-latency systems, keep a close eye on cache miss rate, branch prediction efficiency, instruction count, and memory access patterns.

## 5. Notes

This repository is primarily experimental and educational. It is not necessarily intended to be a production-ready template. Its value lies in helping you understand:

- Why some code paths are fast
- Why some code paths are slow
- How software structure and CPU/compiler mechanisms can be used to improve performance

## 6. Possible Extensions

Potential future improvements include:

- More complete benchmark results and charts
- Performance comparison reports for each module
- DPDK deployment and setup scripts
- A more unified CMake build system
- CI validation and automated testing
