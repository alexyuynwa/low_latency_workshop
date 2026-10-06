add_executable(simd
    simd.cc
)

target_link_libraries(simd
    PRIVATE
    benchmark::benchmark
    pthread
)
