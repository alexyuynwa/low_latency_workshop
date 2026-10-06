add_executable(induction_variables
    induction_variables.cc
)

target_link_libraries(induction_variables
    PRIVATE
    benchmark::benchmark
    pthread
)
