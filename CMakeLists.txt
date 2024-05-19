cmake_minimum_required(VERSION 3.24.2)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For clang-tidy

set(CMAKE_CXX_STANDARD 20) # Compile as C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(B-TCP)

# Includes.
set(B-TCP_SRC_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/src/include)
include(etc/tests.cmake)

include_directories(${B-TCP_SRC_INCLUDE_DIR})


add_subdirectory("${PROJECT_SOURCE_DIR}/src")
add_subdirectory("${PROJECT_SOURCE_DIR}/tools")
add_subdirectory("${PROJECT_SOURCE_DIR}/test")