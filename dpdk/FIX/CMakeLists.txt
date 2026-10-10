cmake_minimum_required(VERSION 3.16)

project(fix_dpdk LANGUAGES CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED IMPORTED_TARGET libdpdk)

add_executable(fix_dpdk main.cpp)
target_compile_features(fix_dpdk PRIVATE cxx_std_17)
target_compile_options(fix_dpdk PRIVATE -Wall -Wextra)
target_link_libraries(fix_dpdk PRIVATE PkgConfig::DPDK)

include(CTest)
if(BUILD_TESTING)
    add_executable(fix_parser_test fix_parser_test.cpp)
    target_compile_features(fix_parser_test PRIVATE cxx_std_17)
    target_compile_options(fix_parser_test PRIVATE -Wall -Wextra)
    add_test(NAME fix_parser_test COMMAND fix_parser_test)
endif()
