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

ConversionResult DefaultParseTraits<char>::Run(const std::string& in) {
  if (in.size() != 1)
    return ConversionFailure("char must be exactly one character");
  if (!absl::ascii_isprint(in[0]))
    return ConversionFailure("char must be printable");
  return ConversionSuccess(in.front());
}

ConversionResult DefaultParseTraits<bool>::Run(const std::string& in) {
  bool val;
  if (absl::SimpleAtob(in, &val)) return ConversionSuccess(val);
  return ConversionFailure("not a valid bool value");
}

ConversionResult CFileOpenTraits::Run(const std::string& in, OpenMode mode) {
  auto mode_str = ModeToChars(mode);
  auto* file = std::fopen(in.c_str(), mode_str.c_str());
  if (file) return ConversionSuccess(file);
  auto error = absl::base_internal::StrError(errno);
  return ConversionFailure(std::move(error));
}

void CFileOpenTraits::Run(const std::string& in, OpenMode mode,
                          Result<FILE*>* out) {
  auto mode_str = ModeToChars(mode);
  auto* file = std::fopen(in.c_str(), mode_str.c_str());
  if (file) return out->SetValue(file);
  if (int e = errno) {
    errno = 0;
    return out->SetError(std::strerror(e));
  }
  out->SetError(kDefaultOpenFailureMsg);
}


}  // namespace internal
}  // namespace argparse
