add_executable(branch_prediction
    branch_prediction.cc
)

target_link_libraries(branch_prediction
    PRIVATE
    benchmark::benchmark
    pthread
)
