// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <gflags/gflags.h>

#include "argparse/internal/argparse-argument-parser.h"

namespace argparse {
namespace internal {
namespace gflags_parser_internal {

class GflagsArgument {
 public:
  explicit GflagsArgument(Argument* arg);

  template <typename FlagType>
  void Register() {
    gflags::FlagRegisterer(name_,                             // name
                           help_,                             // help
                           filename_,                         // filename
                           dest_ptr_.Cast<FlagType>(),        // current_storage
                           AnyCast<FlagType>(default_value_)  // defval_storage
    );
  }

 private:
  const char* name_;
  const char* help_;
  const char* filename_;
  OpaquePtr dest_ptr_;
  Any* default_value_;
};

using GflagRegisterFunc = void (*)(GflagsArgument*);
using GflagsRegisterMap = std::map<std::type_index, GflagRegisterFunc>;

template <typename FlagType>
void RegisterGlagsArgument(GflagsArgument* arg) {
  return arg->Register<FlagType>();
}

class GflagsParser final : public ArgumentParser {
 public:
  GflagsParser();

  void SetProgramVersion(std::string val) override {
    gflags::SetVersionString(val);
  }
  void SetDescription(std::string val) override {
    gflags::SetUsageMessage(val);
  }

  bool Initialize(ArgumentContainer* container) override;
  bool ParseKnownArgs(ArgArray args,
                      std::vector<std::string>* unparsed_args) override;

  ~GflagsParser() override;

 private:
  const GflagsRegisterMap register_map_;
};

}  // namespace gflags_parser_internal
}  // namespace internal
}  // namespace argparse
