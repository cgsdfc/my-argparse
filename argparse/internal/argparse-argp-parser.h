// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <argp.h>

#include "absl/container/flat_hash_map.h"
#include "argparse/internal/argparse-argument-parser.h"

namespace argparse {
namespace internal {
class Argument;
class ArgumentGroup;

namespace argp_parser_internal {

class ArgpParser final : public ArgumentParser {
 public:
  ArgpParser();
  void Initialize(ArgumentContainer* container) override;
  bool ParseKnownArgs(ArgArray args, std::vector<std::string>* out) override;

  void SetDescription(std::string value) override;
  void SetProgramVersion(std::string value) override;
  void SetBugReportEmail(std::string value) override;

  ~ArgpParser();

 private:
  // Actual handling of each argument.
  error_t Parse(int key, char* arg, struct argp_state* state);

  void AppendGroupOption(ArgumentGroup* group);
  void AppendArgument(Argument* arg);
  void AppendPositionalArgument(Argument* arg);
  void AppendOptionalArgument(Argument* arg);

  using OptionVector = std::vector<struct argp_option>;

  unsigned next_option_id_;
  unsigned next_group_id_;
  std::string description_;
  std::string program_version_;
  std::string program_name_;
  std::string bug_address_;
  OptionVector options_;
  absl::flat_hash_map<unsigned, Argument*> optional_args_;
  std::vector<Argument*> positional_args_;
  struct argp parser_;
};

}  // namespace argp_parser_internal
}  // namespace internal
}  // namespace argparse
