cmake_minimum_required(VERSION 3.13)
project(Neon)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(EXECUTABLE_OUTPUT_PATH Bin)
set(LIBRARY_OUTPUT_PATH lib)

set(SRC_LIST)
set(INCLUDE_LIST ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE tmp ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cc)
list(APPEND SRC_LIST ${tmp})

include_directories(${INCLUDE_LIST})
# src list need compile to library

add_executable(TestGEMM Test/gemm.cc ${SRC_LIST})
add_executable(TestBlur Test/blur.cc ${SRC_LIST})