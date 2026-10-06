#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include "data_processing.h"

std::vector<double> generate_test_data(int size) {
    std::vector<double> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    
    for (int i = 0; i < size; ++i) {
        data[i] = dist(gen);
    }
    return data;
}

// Unified performance test function
void run_benchmark() {
    const int data_size = 100000;
    const int iterations = 1000;
    
    auto test_data = generate_test_data(data_size);
    DataProcessor processor(data_size);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        processor.process_data(test_data.data());
        // Prevent compiler over-optimization
        asm volatile("" : : "r,m"(processor.get_result()) : "memory");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Execution time: " << duration.count() << " ms" << std::endl;
}

int main() {
    std::cout << "Running benchmark..." << std::endl;
    run_benchmark();
    return 0;
}
