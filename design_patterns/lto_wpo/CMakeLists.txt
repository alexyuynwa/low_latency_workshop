add_executable(lto_wpo
    lto_wpo.cc
    data_processing.h
    math_operations.h
)

target_link_libraries(lto_wpo
    PRIVATE
    benchmark::benchmark
    pthread
)