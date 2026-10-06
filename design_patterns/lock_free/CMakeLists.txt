add_executable(lock_free
    lock_free.cc
)

target_link_libraries(lock_free
    PRIVATE
    benchmark::benchmark
    pthread
)
