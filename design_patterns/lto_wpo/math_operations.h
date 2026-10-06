#pragma once

#include <cmath>

// Without static - may not be inlined during cross-module calls
double calculate_vector_magnitude(const double* data, int size) {
    double sum = 0.0;
    for (int i = 0; i < size; ++i) {
        sum += data[i] * data[i];  // Hot loop
    }
    return std::sqrt(sum);
}

// Use static - restrict to current module, easier to optimize
static double square(double x) {
    return x * x;  // Simple function, significant benefits from inlining
}

double dot_product(const double* a, const double* b, int size) {
    double result = 0.0;
    for (int i = 0; i < size; ++i) {
        result += a[i] * b[i];  // Hot loop
    }
    return result;
}

void normalize_vector(double* data, int size) {
    double mag = calculate_vector_magnitude(data, size);
    if (mag != 0.0) {
        for (int i = 0; i < size; ++i) {
            data[i] /= mag;
        }
    }
}

double calculate_distance(const double* a, const double* b, int size) {
    double sum = 0.0;
    for (int i = 0; i < size; ++i) {
        double diff = a[i] - b[i];
        sum += square(diff);  // Calling static functions, can be inlined
    }
    return std::sqrt(sum);
}
