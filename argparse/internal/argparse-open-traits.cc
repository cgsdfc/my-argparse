// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-open-traits.h"

namespace argparse {
namespace internal {
namespace open_traits_internal {

StdIosBaseOpenMode CharsToStreamMode(absl::string_view chars) {
  using StdIosBase = std::ios_base;
  StdIosBaseOpenMode result{};

  for (char ch : chars) {
    switch (ch) {
      case 'w':
        result |= StdIosBase::out;
        break;
      case 'r':
        result |= StdIosBase::in;
        break;
      case 'b':
        result |= StdIosBase::binary;
        break;
      case 'a':
        result |= StdIosBase::app;
        break;
      case '+':
        if (result & StdIosBase::in) result |= StdIosBase::out;
        if (result & StdIosBase::out) result |= StdIosBase::in;
        if (result & StdIosBase::app) result |= StdIosBase::in;
        break;
      default:
        break;
    }
  }

  return result;
}

}  // namespace open_traits_internal
}  // namespace internal
}  // namespace argparse
