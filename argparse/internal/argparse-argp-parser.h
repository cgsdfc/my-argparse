// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <argp.h>

#include "argparse/internal/argparse-argument-parser.h"

namespace argparse {
namespace internal {
namespace argp_parser_internal {

struct ArgumentData {
  int id;
  std::string help;
};

struct GroupData {
  int id;
};

// Maybe option listener should be AP itself.
class ArgpArgumentParser : public ArgumentParser{
 public:
  bool Initialize(ArgumentContainer* container) override;
  bool ParseKnownArgs(ArgArray args, std::vector<std::string>* out) override;

  private:
    using OptionVector = std::vector<struct argp_option>;

    struct argp parser_;
    OptionVector option_vector_;
};

}  // namespace argp_parser_internal
}  // namespace internal
}  // namespace argparse
