#pragma once

#include <cstdio>
#include <fstream>

#include "argparse/base/common.h"
#include "argparse/base/result.h"

namespace argparse {

constexpr const char kDefaultOpenFailureMsg[] = "Failed to open file";

OpenMode CharsToMode(const char* str);
std::string ModeToChars(OpenMode mode);

OpenMode StreamModeToMode(std::ios_base::openmode stream_mode);
std::ios_base::openmode ModeToStreamMode(OpenMode m);

template <typename T>
struct OpenTraits {
  static constexpr void* Run = nullptr;
};

struct CFileOpenTraits {
  static void Run(const std::string& in, OpenMode mode, Result<FILE*>* out);
};

template <typename T>
struct StreamOpenTraits {
  static void Run(const std::string& in, OpenMode mode, Result<T>* out) {
    auto ios_mode = ModeToStreamMode(mode);
    T stream(in, ios_mode);
    if (stream.is_open())
      return out->set_value(std::move(stream));
    out->set_error(kDefaultOpenFailureMsg);
  }
};

template <>
struct OpenTraits<FILE*> : CFileOpenTraits {};
template <>
struct OpenTraits<std::fstream> : StreamOpenTraits<std::fstream> {};
template <>
struct OpenTraits<std::ifstream> : StreamOpenTraits<std::ifstream> {};
template <>
struct OpenTraits<std::ofstream> : StreamOpenTraits<std::ofstream> {};

}  // namespace argparse
