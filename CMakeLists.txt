cmake_minimum_required(VERSION 3.24.2)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For clang-tidy

set(CMAKE_CXX_STANDARD 20) # Compile as C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(B-TCP)

if(${PROJECT_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "Minnow must be built outside its source directory, e.g. `cmake -B build`.")
endif()

# #####################################################################################################################
# COMPILER SETUP
# #####################################################################################################################


# Includes.
set(B-TCP_SRC_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/src/include)
set(B-TCP_ETC_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/etc)

include_directories(${B-TCP_SRC_INCLUDE_DIR} ${B-TCP_ETC_INCLUDE_DIR})
# 可能用到的
# include_directories(BEFORE src) # This is needed for gtest.

# function(disable_target_warnings NAME)
#         target_compile_options(${NAME} PRIVATE "-w")
# endfunction()


# #####################################################################################################################
# Other CMake modules
# MUST BE ADDED AFTER CONFIGURING COMPILER PARAMETERS
# #####################################################################################################################

add_subdirectory("${PROJECT_SOURCE_DIR}/src")
add_subdirectory("${PROJECT_SOURCE_DIR}/tools")