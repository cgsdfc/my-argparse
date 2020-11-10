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

struct CloseFile {
  void operator()(FILE* file) const noexcept {
    ARGPARSE_INTERNAL_DCHECK(file, "");
    fclose(file);
  }
};

using ScopedFile = std::unique_ptr<FILE, CloseFile>;

namespace internal {
namespace open_traits_internal {

using StdIosBaseOpenMode = std::ios_base::openmode;

StdIosBaseOpenMode CharsToStreamMode(absl::string_view chars);

inline bool ArgparseOpen(absl::string_view name, absl::string_view mode,
                         ScopedFile* file) {
  auto* f = fopen(name.data(), mode.data());
  if (f == nullptr) return false;
  file->reset(f);
  return true;
}

inline bool ArgparseOpen(absl::string_view name, absl::string_view mode,
                         FILE** file) {
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
    absl::string_view name, absl::string_view mode, T* stream) {
  stream->open(name.data(), CharsToStreamMode(mode));
  return stream->is_open();
}

struct ProbeOpen {
  template <typename T>
  static auto Invoke(absl::string_view name, absl::string_view mode, T* file)
      -> absl::enable_if_t<
          std::is_same<bool, decltype(ArgparseOpen(name, mode, file))>::value,
          bool> {
    return ArgparseOpen(name, mode, file);
  }
};

template <typename T, typename = void>
struct IsOpenDefined : std::false_type {};

template <typename T>
struct IsOpenDefined<
    T, absl::void_t<decltype(ProbeOpen::Invoke(
           std::declval<absl::string_view>(), std::declval<absl::string_view>(),
           std::declval<T*>()))>> : std::true_type {};

template <typename T>
absl::enable_if_t<IsOpenDefined<T>::value, bool> Open(absl::string_view name,
                                                      absl::string_view mode,
                                                      T* file) {
  return ProbeOpen::Invoke(name, mode, file);
}

}  // namespace open_traits_internal

using open_traits_internal::IsOpenDefined;
using open_traits_internal::Open;

}  // namespace internal
}  // namespace argparse
