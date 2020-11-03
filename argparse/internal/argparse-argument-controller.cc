// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-argument-controller.h"

namespace argparse {
namespace internal {

ArgumentController::ArgumentController()
    : container_(new ArgumentContainer),
      parser_(ArgumentParser::CreateDefault()) {}

void ArgumentController::EnsureInActiveState(const char* func) const {
  ARGPARSE_DCHECK_F(state_ == kActiveState,
                    "Method %s must be called in Active state", func);
}

void ArgumentController::EnsureInFrozenState() {
  ARGPARSE_DCHECK_F(
      state_ != kShutDownState,
      "No method other than destructor should be called after shutdown");
  if (state_ == kFrozenState) return;
  TransmitToFrozenState();
}

void ArgumentController::TransmitToFrozenState() {
  EnsureInActiveState(__func__);
   parser_->Initialize(container_.get());
  state_ = kFrozenState;
}

void ArgumentController::AddArgument(std::unique_ptr<Argument> arg) {
  EnsureInActiveState(__func__);
  return container_->GetMainHolder()->AddArgument(std::move(arg));
}

ArgumentGroup* ArgumentController::AddArgumentGroup(std::string title) {
  EnsureInActiveState(__func__);
  return container_->GetMainHolder()->AddArgumentGroup(std::move(title));
}

bool ArgumentController::ParseKnownArgs(ArgArray args,
                                        std::vector<std::string>* out) {
  EnsureInFrozenState();
  return parser_->ParseKnownArgs(args, out);
}

void ArgumentController::Shutdown() {
  if (state_ == kShutDownState) return;
  // Must delete container first.
  container_.reset();
  parser_.reset();
}

}  // namespace internal
}  // namespace argparse
