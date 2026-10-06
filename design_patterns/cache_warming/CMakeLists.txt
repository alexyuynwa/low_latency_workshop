add_executable(cache_warming
    cache_warming.cc
)

target_link_libraries(cache_warming
    PRIVATE
    benchmark::benchmark
    pthread
)
