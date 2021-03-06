// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-argument-controller.h"

#ifndef NDEBUG
#define ARGPARSE_ARGUMENT_CONTROLLER_CHECK_STATE(expected_state)               \
  do {                                                                         \
    if (state_ != (expected_state)) {                                          \
      ARGPARSE_INTERNAL_LOG(FATAL, "Method '%s' must be called in '%s' state", \
                            __func__, #expected_state);                        \
    }                                                                          \
  } while (0)
#else
#define ARGPARSE_ARGUMENT_CONTROLLER_CHECK_STATE(expected_state)
#endif

namespace argparse {
namespace internal {

ArgumentController::ArgumentController()
    : container_(new ArgumentContainer),
      parser_(ArgumentParser::CreateDefault()) {}

void ArgumentController::EnsureInFrozenState() {
  if (state_ == kShutDownState) {
    ARGPARSE_INTERNAL_LOG(
        FATAL,
        "No method other than destructor should be called after shutdown");
    return;
  }
  if (state_ == kFrozenState) return;

  ARGPARSE_INTERNAL_DCHECK(state_ == kActiveState, "");
  state_ = kFrozenState;
  parser_->Initialize(container_.get());
}

void ArgumentController::AddArgument(std::unique_ptr<Argument> arg) {
  // EnsureInActiveState(__func__);
  ARGPARSE_ARGUMENT_CONTROLLER_CHECK_STATE(kActiveState);
  return container_->GetMainHolder()->AddArgument(std::move(arg));
}

ArgumentGroup* ArgumentController::AddArgumentGroup(absl::string_view title) {
  ARGPARSE_ARGUMENT_CONTROLLER_CHECK_STATE(kActiveState);
  return container_->GetMainHolder()->AddArgumentGroup(title);
}

bool ArgumentController::ParseKnownArgs(ArgArray args,
                                        std::vector<std::string>* out) {
  EnsureInFrozenState();
  return parser_->ParseKnownArgs(args, out);
}

void ArgumentController::Shutdown() {
  if (state_ == kShutDownState) return;
  state_ = kShutDownState;
  // Must delete container first.
  container_.reset();
  parser_.reset();
}

void ArgumentController::SetOption(ParserOptions key, absl::string_view value) {
  ARGPARSE_ARGUMENT_CONTROLLER_CHECK_STATE(kActiveState);
  parser_->SetOption(key, value);
}

}  // namespace internal
}  // namespace argparse
