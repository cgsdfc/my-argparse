// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-logging.h"

namespace argparse {
namespace internal {

// This enum is aimed to replace setter methods like
// `SetProgramVersion(std::string)` and friends to `SetOption(OptionKey key,
// std::string)`. When such API is abundant, this can save a lot of *typing*.
// Our codebase is currently of tiny size, so putting all those keys in one enum
// is ok.
enum class OptionKey {
  kDescription,
  kProgramVersion,
  kProgramName,
  kProgramUsage,
  kBugReportEmail,
};

// Use this in a default branch of a switch statement.
#define ARGPARSE_INTERNAL_INVALID_OPTION_KEY(option_key)   \
  ARGPARSE_INTERNAL_LOG(WARNING, "Invalid OptionKey '%d'", \
                        static_cast<int>((option_key)))

}  // namespace internal
}  // namespace argparse
