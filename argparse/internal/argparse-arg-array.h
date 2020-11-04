// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <vector>

#include "absl/types/span.h"
#include "argparse/internal/argparse-port.h"

namespace argparse {
namespace internal {

using ArgVector = std::vector<const char*>;

using ArgArray = absl::Span<const char*>;

inline ArgArray MakeArgArray(ArgVector& vector) {
  return absl::MakeSpan(vector);
}

inline ArgArray MakeArgArray(int argc, const char** argv) {
  return absl::MakeSpan(argv, argc);
}

inline char** ArgArrayGetArgv(const ArgArray& args) {
  return const_cast<char**>(args.data());
}

inline int ArgArrayGetArgc(const ArgArray& args) {
  return static_cast<int>(args.size());
}

}  // namespace internal
}  // namespace argparse
