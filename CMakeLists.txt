cmake_minimum_required(VERSION 3.20)
project(testr)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_subdirectory(lextest)
include(lextest/cmake/lextest.cmake)


add_library(mempx mempx.hpp mempx.cpp)

lx_add_test(test1 test.cpp)
target_link_libraries(test1 PUBLIC mempx)
