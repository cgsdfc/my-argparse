// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-argument-parser.h"

namespace argparse {
namespace internal {
namespace gflags_parser_internal {

struct RegisterParams {
  const char* name;
  const char* help;
  const char* filename;
  internal::OpaquePtr current_value;
  internal::Any* default_value;
};

using GflagRegisterFunc = void (*)(const RegisterParams&);
using GflagsRegisterMap = std::map<std::type_index, GflagRegisterFunc>;

class GflagsParser final : public ArgumentParser {
 public:
  GflagsParser();

  void SetProgramVersion(std::string val) override;
  void SetDescription(std::string val) override;

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
