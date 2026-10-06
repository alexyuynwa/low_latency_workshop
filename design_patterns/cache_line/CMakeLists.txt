add_executable(cache_line
    cache_line.cc
)

target_link_libraries(cache_line
    PRIVATE
    benchmark::benchmark
    pthread
)
