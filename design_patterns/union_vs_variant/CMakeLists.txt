add_executable(union_vs_variant
    union_vs_variant.cc
)

target_link_libraries(union_vs_variant
    PRIVATE
    benchmark::benchmark
    pthread
)
