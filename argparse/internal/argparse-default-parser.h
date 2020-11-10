// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-argument-parser.h"

namespace argparse {
namespace internal {
namespace default_parser_internal {

class DefaultParser final : public ArgumentParser {
 public:
  void Initialize(ArgumentContainer* container) override {}
  bool ParseKnownArgs(ArgArray args,
                      std::vector<std::string>* unparsed_args) override {
    return false;
  }
};

}  // namespace default_parser_internal
}  // namespace internal
}  // namespace argparse
