// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once
#include "argparse/internal/argparse-argument-holder.h"
#include "argparse/internal/argparse-argument-parser.h"

namespace argparse {
namespace internal {

class SubCommand;
class SubCommandGroup;

// ArgumentContainer contains everything user plugs into us, namely,
// Arguments, ArgumentGroups, SubCommands, SubCommandGroups, etc.
// It keeps all these objects alive as long as it is alive.
// It also sends out notifications of events of the insertion of these objects.
// It's main role is to receive and hold things, providing iteration methods,
// etc.
// It should be directly allocated.
class ArgumentContainer final {
 public:
  ArgumentContainer();
  ArgumentHolder* GetMainHolder() { return &main_holder_; }

 private:
  ArgumentHolder main_holder_;
};

}  // namespace internal
}  // namespace argparse
