// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/argparse-traits.h"

#include "absl/base/internal/strerror.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"

namespace argparse {
namespace internal {

ConversionResult DefaultParseTraits<char>::Run(absl::string_view in) {
  if (in.size() != 1)
    return ConversionFailure("char must be exactly one character");
  if (!absl::ascii_isprint(in[0]))
    return ConversionFailure("char must be printable");
  return ConversionSuccess(in.front());
}

ConversionResult DefaultParseTraits<bool>::Run(absl::string_view in) {
  bool val;
  if (absl::SimpleAtob(in, &val)) return ConversionSuccess(val);
  return ConversionFailure("not a valid bool value");
}

}  // namespace internal
}  // namespace argparse
