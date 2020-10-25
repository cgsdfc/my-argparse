// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-argument-container.h"

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

  ArgumentGroup* AddArgumentGroup(std::string title);

  SubCommandGroup* AddSubCommandGroup(std::unique_ptr<SubCommandGroup> group) {
    EnsureInActiveState(__func__);
    return nullptr;
  }

  // TODO: use OptionsListenr.
  ArgumentParser* GetOptionsListener() {
    EnsureInActiveState(__func__);
    return parser_.get();
  }

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

  void EnsureInActiveState(const char* func) const;
  void EnsureInFrozenState();
  void TransmitToFrozenState();

  State state_ = kActiveState;
  std::unique_ptr<ArgumentContainer> container_;
  std::unique_ptr<ArgumentParser> parser_;
};

}  // namespace internal
}  // namespace argparse
