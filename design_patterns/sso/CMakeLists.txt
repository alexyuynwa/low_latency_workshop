add_executable(sso
    sso.cc
)

target_link_libraries(sso
    PRIVATE
    benchmark::benchmark
    pthread
)
