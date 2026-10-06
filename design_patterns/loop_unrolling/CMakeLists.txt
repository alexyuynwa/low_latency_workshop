add_executable(loop_unrolling
    loop_unrolling.cc
)

target_link_libraries(loop_unrolling
    PRIVATE
    benchmark::benchmark
    pthread
)
