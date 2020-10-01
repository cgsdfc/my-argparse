project(argparse CXX C)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

include(CheckIncludeFile)
include(CTest)
include(DownloadProject.cmake)

download_project(PROJ                googletest
                 GIT_REPOSITORY      https://github.com/google/googletest.git
                 GIT_TAG             master
                 ${UPDATE_DISCONNECTED_IF_AVAILABLE}
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})

# When using CMake 2.8.11 or later, header path dependencies
# are automatically added to the gtest and gmock targets.
# For earlier CMake versions, we have to explicitly add the
# required directories to the header search path ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
    include_directories("${gtest_SOURCE_DIR}/include"
                        "${gmock_SOURCE_DIR}/include")
endif()

check_include_file(argp.h HAVE_ARGP_H)
check_include_file(cxxabi.h HAVE_CXXABI_H)
if (NOT HAVE_ARGP_H)
    message(FATAL "argparse needs argp.h (GNU argument parser library) to work.")
endif()

include_directories(include)

add_library(argparse 
   src/argparse.cc 
   src/argparse-internal.cc
)

add_executable(test_main test_main.cc)
target_link_libraries(test_main argparse)

add_executable(argparse_test
    test/argparse-any_test.cc
    test/argparse-result_test.cc
)

target_link_libraries(argparse_test argparse gtest gtest_main)

add_test(NAME argparse_test COMMAND argparse_test)
