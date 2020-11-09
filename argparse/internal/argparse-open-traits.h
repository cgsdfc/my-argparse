// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <cstdio>
#include <fstream>
#include <memory>

#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"
#include "argparse/internal/argparse-logging.h"

namespace argparse {

// File open mode. This is not enum class since we do & | on it.
enum OpenMode {
  kModeNoMode = 0x0,
  kModeRead = 1,
  kModeWrite = 2,
  kModeAppend = 4,
  kModeTruncate = 8,
  kModeBinary = 16,
};

OpenMode CharsToMode(absl::string_view str);
std::string ModeToChars(OpenMode mode);

OpenMode StreamModeToMode(std::ios_base::openmode stream_mode);
std::ios_base::openmode ModeToStreamMode(OpenMode m);

struct CloseFile {
  void operator()(FILE* file) const noexcept {
    ARGPARSE_INTERNAL_DCHECK(file, "");
    fclose(file);
  }
};

using ScopedFile = std::unique_ptr<FILE, CloseFile>;

namespace internal {
namespace open_traits_internal {

inline bool ArgparseOpen(absl::string_view name, OpenMode mode,
                         ScopedFile* file) {
  auto mode_chars = ModeToChars(mode);
  auto* f = fopen(name.data(), mode_chars.data());
  if (f == nullptr) return false;
  file->reset(f);
  return true;
}

inline bool ArgparseOpen(absl::string_view name, OpenMode mode, FILE** file) {
  ScopedFile scoped_file;
  if (!ArgparseOpen(name, mode, &scoped_file)) return false;
  ARGPARSE_INTERNAL_DCHECK(scoped_file, "");
  *file = scoped_file.release();
  return true;
}

template <typename T>
struct IsStdStream : absl::disjunction<std::is_same<T, std::fstream>,
                                       std::is_same<T, std::ifstream>,
                                       std::is_same<T, std::ofstream>> {};

template <typename T>
absl::enable_if_t<IsStdStream<T>::value, bool> ArgparseOpen(
    absl::string_view name, OpenMode mode, T* stream) {
  stream->open(name.data(), ModeToStreamMode(mode));
  return stream->is_open();
}

}  // namespace open_traits_internal

template <typename T>
struct IsOpenDefined {
 private:
  template <typename U,
            typename = absl::enable_if_t<std::is_same<
                bool, decltype(ArgparseOpen(std::declval<absl::string_view>(),
                                            std::declval<OpenMode>(),
                                            std::declval<U*>()))>::value>>
  static std::true_type Test(int);
  static std::false_type Test(char);

 public:
  static constexpr bool value = decltype(Test<T>())::value;
};

template <typename T>
absl::enable_if_t<IsOpenDefined<T>::value, bool> Open(absl::string_view name,
                                                      OpenMode mode, T* file) {
  return ArgparseOpen(name, mode, file);
}

}  // namespace internal
}  // namespace argparse
