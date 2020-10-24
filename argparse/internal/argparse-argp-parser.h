// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-argument-parser.h"

namespace argparse {
namespace internal {

namespace argp_parser_internal {

class ArgpArgumentParser : public ArgumentParser {
 public:
  OptionsListener* GetOptionsListener() override {}
  void AddArgument(Argument* arg, SubCommand* cmd) override {}
  void AddArgumentGroup(ArgumentGroup* group) override {}
  bool ParseKnownArgs(ArgArray args, std::vector<std::string>* out) override {}
};

}  // namespace argp_parser_internal
}  // namespace internal

}  // namespace argparse
