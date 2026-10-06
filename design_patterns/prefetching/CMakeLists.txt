add_executable(prefetching
    prefetching.cc
)

target_link_libraries(prefetching
    PRIVATE
    benchmark::benchmark
    pthread
)
