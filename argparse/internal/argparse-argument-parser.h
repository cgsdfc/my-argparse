// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-arg-array.h"
#include "argparse/internal/argparse-argument.h"

namespace argparse {
namespace internal {
class SubCommand;

// ArgumentParser has some standard options to tune its behaviours.
class OptionsListener {
 public:
  virtual ~OptionsListener() {}
  virtual void SetProgramVersion(std::string val) = 0;
  virtual void SetDescription(std::string val) = 0;
  virtual void SetBugReportEmail(std::string val) = 0;
  virtual void SetProgramName(std::string val) = 0;
  virtual void SetProgramUsage(std::string usage) = 0;
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
  virtual OptionsListener* GetOptionsListener() = 0;
  virtual void AddArgument(Argument* arg, SubCommand* cmd) = 0;
  virtual void AddArgumentGroup(ArgumentGroup* group) = 0;
  //   virtual void AddSubCommand(SubCommand* cmd) = 0;
  //   virtual void AddSubCommandGroup(SubCommandGroup* group) = 0;
  // Parse args, if rest is null, exit on error. Otherwise put unknown ones into
  // rest and return status code.
  virtual bool ParseKnownArgs(ArgArray args, std::vector<std::string>* out) = 0;
  static std::unique_ptr<ArgumentParser> CreateDefault();
};

}  // namespace internal
}  // namespace argparse
