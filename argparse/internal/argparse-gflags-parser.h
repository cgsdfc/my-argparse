// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <gflags/gflags.h>

#include "argparse/internal/argparse-argument-parser.h"
#include "argparse/internal/argparse-argument.h"

namespace argparse {
namespace internal {
namespace gflags_parser_internal {

// TODO: move this to statically-init global data.
using GflagRegisterFunc = void (*)(Argument*);
using GflagsRegisterMap = std::map<std::type_index, GflagRegisterFunc>;

class GflagsParser final : public ArgumentParser {
 public:
  GflagsParser();

  void SetProgramVersion(std::string val) override {
    gflags::SetVersionString(val);
  }
  void SetDescription(std::string val) override {
    gflags::SetUsageMessage(val);
  }

  void Initialize(ArgumentContainer* container) override;
  bool ParseKnownArgs(ArgArray args,
                      std::vector<std::string>* unparsed_args) override;

  ~GflagsParser() override;

 private:
  const GflagsRegisterMap register_map_;
};

}  // namespace gflags_parser_internal
}  // namespace internal
}  // namespace argparse
