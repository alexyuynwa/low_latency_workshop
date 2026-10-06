add_executable(pointer_aliasing
    pointer_aliasing.cc
)

target_link_libraries(pointer_aliasing
    PRIVATE
    benchmark::benchmark
    pthread
)
