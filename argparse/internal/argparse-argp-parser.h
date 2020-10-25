// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-argument-parser.h"

namespace argparse {
namespace internal {

namespace argp_parser_internal {

// Maybe option listener should be AP itself.
class ArgpArgumentParser : public ArgumentParser{
 public:
  bool Initialize(ArgumentContainer* container) override {}
  bool ParseKnownArgs(ArgArray args, std::vector<std::string>* out) override {}

  private:
};

}  // namespace argp_parser_internal
}  // namespace internal

}  // namespace argparse
