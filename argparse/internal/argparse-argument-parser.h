// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <memory>

#include "absl/strings/string_view.h"
#include "argparse/internal/argparse-arg-array.h"

namespace argparse {
namespace internal {

class SubCommand;
class ArgumentContainer;

enum class ParserOptions {
  kDescription,
  kProgramVersion,
  kProgramName,
  kProgramUsage,
  kBugReportEmail,
};

// internal::ArgumentParser is the analogy of argparse::ArgumentParser,
// except that its methods take internal objects as inputs.
// ArgumentController exposes ArgumentContainer to receive user's input,
// and feeds the notification of ArgumentContainer to ArgumentParser through
// adapter so that the latter can build its data-structure that is optimized for
// parsing arguments.
class ArgumentParser {
 public:
  virtual ~ArgumentParser() {}
  // Receive various options from user.
  virtual void SetOption(ParserOptions key, absl::string_view value) {}

  // Read the content of the ArgumentContainer and prepare for parsing.
  // The container is guaranteed to have longer lifetime than the parser.
  virtual void Initialize(ArgumentContainer* container) = 0;
  // Parse args, if rest is null, exit on error. Otherwise put unknown ones into
  // rest and return status code.
  virtual bool ParseKnownArgs(ArgArray args, std::vector<std::string>* out) = 0;
  static std::unique_ptr<ArgumentParser> CreateDefault();
};

}  // namespace internal
}  // namespace argparse
