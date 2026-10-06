cmake_minimum_required(VERSION 3.10)
project(dpdk_cpp_demo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Automatically locate the DPDK library in the system (usually via pkg-config)
find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED libdpdk)

# Add the executable
add_executable(dpdk_cpp_demo main.cpp)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i.86")
	target_compile_options(dpdk_cpp_demo PRIVATE -mssse3)
endif()

# Link the DPDK headers and libraries
target_compile_options(dpdk_cpp_demo PRIVATE ${DPDK_CFLAGS})
target_link_libraries(dpdk_cpp_demo PRIVATE ${DPDK_LIBRARIES})

target_link_options(dpdk_cpp_demo PRIVATE ${DPDK_LDFLAGS})