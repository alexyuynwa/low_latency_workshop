#pragma once

#include "math_operations.h"
#include <iostream>

class DataProcessor {
public:
    DataProcessor(int data_size) : size_(data_size), result_(0.0) {
        buffer_ = new double[data_size];
    }

    ~DataProcessor()
    {
            delete[] buffer_;

    }
    
    // Performance-critical function
    void process_data(const double* input)
    {
        // Copy data to buffer
        for (int i = 0; i < size_; ++i) {
            buffer_[i] = input[i];
        }
        
        // Apply transformation
        apply_transform(buffer_, size_);
        
        // Calculate magnitude - cross-module call!
        result_ = calculate_vector_magnitude(buffer_, size_);
    }

    double get_result() const 
    { 
        return result_; 
    }
    
private:
    double* buffer_;
    int size_;
    double result_;
    
    // Private helper function - use static for easier optimization
    static void apply_transform(double* data, int size)
    {
        // Static function - compiler can aggressively optimize
        for (int i = 0; i < size; ++i) 
        {
            data[i] = std::sin(data[i]) + std::cos(data[i]);  // math computations
        }
    }
};
