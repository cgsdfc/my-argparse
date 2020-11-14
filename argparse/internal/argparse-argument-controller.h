// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-argument-container.h"
#include "argparse/internal/argparse-argument-parser.h"

namespace argparse {
namespace internal {

// This combines the functionality of ArgumentContainer and ArgumentParser and
// connects them. It exposes an interface that is directly usable by the wrapper
// layers.
class ArgumentController final {
 public:
  ArgumentController();

  // Methods forwarded from ArgumentContainer.
  void AddArgument(std::unique_ptr<Argument> arg);

  ArgumentGroup* AddArgumentGroup(absl::string_view title);

  SubCommandGroup* AddSubCommandGroup(std::unique_ptr<SubCommandGroup> group) {
    return nullptr;
  }

  // Forward to ArgumentParser.

  void SetOption(ParserOptions key, absl::string_view value);

  // TODO: make API more clear.
  bool ParseKnownArgs(ArgArray args, std::vector<std::string>* out);

  // Clean all the memory of this object, after that no methods other than dtor
  // should be invoked.
  void Shutdown();

 private:
  enum State {
    kActiveState,    // In this state, arguments can be added to us.
    kFrozenState,    // In this state, no argument can be added, but parse() can
                     // be called.
    kShutDownState,  // In this state, nothing can be do with it, waiting for
                     // dtor call.
  };

  void EnsureInFrozenState();

  State state_ = kActiveState;
  std::unique_ptr<ArgumentContainer> container_;
  std::unique_ptr<ArgumentParser> parser_;
};

}  // namespace internal
}  // namespace argparse
