add_executable(crtp
    crtp.cc
)

target_link_libraries(crtp
    PRIVATE
    benchmark::benchmark
    pthread
)
