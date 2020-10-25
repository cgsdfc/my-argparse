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
  void AddArgument(std::unique_ptr<Argument> arg) {
    return container_->GetMainHolder()->AddArgument(std::move(arg));
  }
  ArgumentGroup* AddArgumentGroup(std::string title) {
    return container_->GetMainHolder()->AddArgumentGroup(std::move(title));
  }
  SubCommandGroup* AddSubCommandGroup(std::unique_ptr<SubCommandGroup> group) {
    return nullptr;
  }

  // Methods forwarded from ArgumentParser.
  ArgumentParser* GetOptionsListener() { return parser_.get(); }

  bool ParseKnownArgs(ArgArray args, std::vector<std::string>* out) {
    bool rv = parser_->Initialize(container_.get());
    ARGPARSE_DCHECK_F(rv, "Cannot initialize the ArgumentParser");
    return parser_->ParseKnownArgs(args, out);
  }

  // Clean all the memory of this object, after that no methods other than dtor
  // should be invoked.
  void Shutdown() {
    container_.reset();
    parser_.reset();
  }

 private:
  std::unique_ptr<ArgumentContainer> container_;
  std::unique_ptr<ArgumentParser> parser_;
};

}  // namespace internal
}  // namespace argparse
