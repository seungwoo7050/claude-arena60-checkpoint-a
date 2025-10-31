cmake_minimum_required(VERSION 3.20)

if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "" FORCE)
endif()

project(arena60 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

option(ENABLE_COVERAGE "Enable coverage" OFF)
if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

# Find packages (CONFIG mode for vcpkg)
find_package(Boost REQUIRED COMPONENTS system)
find_package(libpq CONFIG)
if(NOT libpq_FOUND)
    find_package(PostgreSQL REQUIRED)
    add_library(libpq::pq ALIAS PostgreSQL::PostgreSQL)
endif()
find_package(Protobuf REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Subdirectories
enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
