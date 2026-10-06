
cmake_minimum_required(VERSION 3.10)
project(test C CXX)

# Require the C++17 standard (to better support std::atomic)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable common compiler warnings
add_compile_options(-Wall -Wextra)

# 1. Find the pkg-config tool
find_package(PkgConfig REQUIRED)

# 2. Automatically locate the DPDK library (libdpdk) via pkg-config
pkg_check_modules(DPDK REQUIRED libdpdk)

# 3. Define the executable
add_executable(test main.cpp)

# DPDK's x86 inline memcpy implementation uses SSSE3 intrinsics.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i.86")
	target_compile_options(test PRIVATE -mssse3)
endif()

# 4. Include the DPDK header paths
target_include_directories(test PRIVATE ${DPDK_INCLUDE_DIRS})

# 5. Link the DPDK and its dependent system libraries
target_link_directories(test PRIVATE ${DPDK_LIBRARY_DIRS})
target_link_libraries(test PRIVATE ${DPDK_LIBRARIES})

# 6. DPDK must use specific linker flags to load static/dynamic drivers correctly
target_link_options(test PRIVATE ${DPDK_LDFLAGS_OTHER})
