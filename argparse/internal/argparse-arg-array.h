// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <vector>

#include "absl/types/span.h"

namespace argparse {
namespace internal {

using ArgVector = std::vector<const char*>;

class ArgArray final : private absl::Span<const char*> {
  using Base = absl::Span<const char*>;

 public:
  // Constructed from a pair.
  ArgArray(int argc, const char** argv) : Base(absl::MakeSpan(argv, argc)) {}

  ArgArray(ArgVector& vector) : Base(absl::MakeSpan(vector)) {}

  ArgArray(const ArgArray&) = default;
  ArgArray& operator=(const ArgArray&) = default;

  int GetArgc() const { return static_cast<int>(Base::size()); }
  char** GetArgv() const { return const_cast<char**>(Base::data()); }

  using Base::begin;
  using Base::end;
  using Base::operator[];
};

}  // namespace internal
}  // namespace argparse
