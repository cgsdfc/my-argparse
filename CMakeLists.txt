project(argparse CXX C)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

include(CheckIncludeFile)

check_include_file(argp.h HAVE_ARGP_H)
if (NOT HAVE_ARGP_H)
    message(FATAL "argparse needs argp.h (GNU argument parser library) to work.")
endif()

include_directories(src)

add_library(argparse 
    src/argparse/argparse.h 
    src/argparse/argparse.cc
)

add_executable(test_main test_main.cc)
target_link_libraries(test_main argparse)

find_package(GTest)
add_executable(argparse_unittests src/argparse/argparse_unittests.cc)
target_link_libraries(argparse_unittests gtest_main)
