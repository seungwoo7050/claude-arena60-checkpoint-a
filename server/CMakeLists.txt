cmake_minimum_required(VERSION 3.20)

if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "" FORCE)
endif()

project(arena60 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Find packages (CONFIG mode for vcpkg)
find_package(Boost REQUIRED COMPONENTS system CONFIG)
find_package(libpq CONFIG)
if(NOT libpq_FOUND)
    find_package(PostgreSQL REQUIRED)
    add_library(libpq::pq ALIAS PostgreSQL::PostgreSQL)
endif()
find_package(Protobuf CONFIG REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Subdirectories
add_subdirectory(src)
add_subdirectory(tests)

# Enable testing
enable_testing()
