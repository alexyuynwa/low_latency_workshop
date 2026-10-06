add_executable(short_circuiting
    short_circuiting.cc
)

target_link_libraries(short_circuiting
    PRIVATE
    benchmark::benchmark
    pthread
)
