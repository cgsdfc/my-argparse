set(ARGPARSE_VERSION 0.0.0)

if (CMAKE_VERSION VERSION_LESS 3.0)
  project(argparse CXX C)
  set(PROJECT_VERSION ${ARGPARSE_VERSION})
else()
  cmake_policy(SET CMP0048 NEW)
  project(argparse VERSION ${ARGPARSE_VERSION} LANGUAGES CXX C)
endif()

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 11)

include(CheckIncludeFile)
include(CTest)
include(DownloadProject.cmake)

option(ARGPARSE_USE_GFLAGS "Whether to use gflags as an backend" ON)
option(ARGPARSE_USE_ARGP "Whether to use argp as an backend" ON)

download_project(PROJ                googletest
                 GIT_REPOSITORY      https://github.com/google/googletest.git
                 GIT_TAG             master
                 UPDATE_DISCONNECTED 1
                 QUITE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})

download_project(PROJ               abseil
                GIT_REPOSITORY      https://github.com/abseil/abseil-cpp.git 
                GIT_TAG             master
                UPDATE_DISCONNECTED 1)
add_subdirectory(${abseil_SOURCE_DIR} ${abseil_BINARY_DIR})

if (ARGPARSE_USE_GFLAGS)
    download_project(PROJ                gflags
                    GIT_REPOSITORY      https://github.com/gflags/gflags.git 
                    GIT_TAG             master
                    UPDATE_DISCONNECTED 1)
    add_subdirectory(${gflags_SOURCE_DIR} ${gflags_BINARY_DIR})
    add_definitions(-DARGPARSE_USE_GFLAGS=1)
elseif (ARGPARSE_USE_ARGP)
    check_include_file(argp.h HAVE_ARGP_H)
    check_include_file(cxxabi.h HAVE_CXXABI_H)
    if (NOT HAVE_ARGP_H)
        message(FATAL "argparse needs argp.h (GNU argument parser library) to work.")
    endif()
    add_definitions(-DARGPARSE_USE_ARGP=1)
else()
    message(FATAL "A parser must be selected")
endif()


# When using CMake 2.8.11 or later, header path dependencies
# are automatically added to the gtest and gmock targets.
# For earlier CMake versions, we have to explicitly add the
# required directories to the header search path ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
    include_directories("${gtest_SOURCE_DIR}/include"
                        "${gmock_SOURCE_DIR}/include")
endif()

include_directories(${CMAKE_SOURCE_DIR})

set(ARGPARSE_SOURCE 
   ${CMAKE_SOURCE_DIR}/argparse/argparse-open-mode.cc
   ${CMAKE_SOURCE_DIR}/argparse/argparse-traits.cc
   ${CMAKE_SOURCE_DIR}/argparse/internal/argparse-logging.cc
   ${CMAKE_SOURCE_DIR}/argparse/internal/argparse-info.cc
   ${CMAKE_SOURCE_DIR}/argparse/internal/argparse-argument-builder.cc
   ${CMAKE_SOURCE_DIR}/argparse/internal/argparse-argument-holder.cc
   ${CMAKE_SOURCE_DIR}/argparse/internal/argparse-subcommand.cc
   ${CMAKE_SOURCE_DIR}/argparse/internal/argparse-port.cc
   ${CMAKE_SOURCE_DIR}/argparse/internal/argparse-argument-container.cc
   ${CMAKE_SOURCE_DIR}/argparse/internal/argparse-argument-controller.cc
   ${CMAKE_SOURCE_DIR}/argparse/internal/argparse-argument.cc
   ${CMAKE_SOURCE_DIR}/argparse/internal/argparse-argument-parser.cc
)

if (ARGPARSE_USE_GFLAGS)
    list(APPEND ARGPARSE_SOURCE ${CMAKE_SOURCE_DIR}/argparse/internal/argparse-gflags-parser.cc)
elseif (ARGPARSE_USE_ARGP)
    list(APPEND ARGPARSE_SOURCE ${CMAKE_SOURCE_DIR}/argparse/internal/argparse-argp-parser.cc)
endif()

add_library(argparse ${ARGPARSE_SOURCE})

target_link_libraries(argparse
    absl::flat_hash_set
    absl::inlined_vector 
    absl::strings 
    absl::str_format
    absl::base 
    absl::meta 
    absl::memory)

if (ARGPARSE_USE_GFLAGS)
    target_link_libraries(argparse gflags::gflags)
endif()

add_executable(test_main test_main.cc)
target_link_libraries(test_main argparse)

add_executable(argparse_test
    argparse/internal/argparse-any_test.cc
    argparse/internal/argparse-opaque-ptr_test.cc
    argparse/argparse-builder_test.cc
    argparse/argparse-conversion-result_test.cc
)

target_link_libraries(argparse_test argparse gtest gtest_main)

add_test(NAME argparse_test COMMAND argparse_test)

add_executable(example-gflags-parser example/example-gflags-parser.cc)
target_link_libraries(example-gflags-parser argparse)
