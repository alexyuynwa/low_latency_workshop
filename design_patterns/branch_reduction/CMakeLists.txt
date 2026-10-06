add_executable(branch_reduction
    branch_reduction.cc
)

target_link_libraries(branch_reduction
    PRIVATE
    benchmark::benchmark
    pthread
)
