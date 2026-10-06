cmake_minimum_required(VERSION 3.16)

project(itch_parser LANGUAGES CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED IMPORTED_TARGET libdpdk)

add_executable(itch_parser main.cpp)
target_compile_features(itch_parser PRIVATE cxx_std_17)
target_link_libraries(itch_parser PRIVATE PkgConfig::DPDK)