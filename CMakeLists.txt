project(argparse CXX C)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

include(CheckIncludeFile)

check_include_file(argp.h HAVE_ARGP_H)
if (NOT HAVE_ARGP_H)
    message(FATAL "argparse needs argp.h (GNU argument parser library) to work.")
endif()

include_directories(src)

add_executable(test_main test_main.cc)
