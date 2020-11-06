// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-argument-builder.h"

namespace argparse {
namespace internal {

namespace {

// TODO: move them to info.
bool ActionNeedsBool(ActionKind in) {
  return in == ActionKind::kStoreFalse || in == ActionKind::kStoreTrue;
}

bool ActionNeedsValueType(ActionKind in) {
  return in == ActionKind::kAppend || in == ActionKind::kAppendConst;
}

}  // namespace

ActionKind ArgumentBuilder::StringToActions(absl::string_view str) {
  static const std::map<absl::string_view, ActionKind> kStringToActions{
      {"store", ActionKind::kStore},
      {"store_const", ActionKind::kStoreConst},
      {"store_true", ActionKind::kStoreTrue},
      {"store_false", ActionKind::kStoreFalse},
      {"append", ActionKind::kAppend},
      {"append_const", ActionKind::kAppendConst},
      {"count", ActionKind::kCount},
  };
  auto iter = kStringToActions.find(str);
  ARGPARSE_CHECK_F(iter != kStringToActions.end(),
                   "Unknown action string: '%s'", str.data());
  return iter->second;
}

std::unique_ptr<Argument> ArgumentBuilder::Build() {
  ARGPARSE_DCHECK(arg_);
  arg_->SetMetaVar(meta_var_ ? std::move(*meta_var_)
                             : arg_->GetNames()->GetDefaultMetaVar());

  // Put a bool if needed.
  if (ActionNeedsBool(action_kind_)) {
    const bool kStoreTrue = action_kind_ == ActionKind::kStoreTrue;
    arg_->SetDefaultValue(MakeAny<bool>(!kStoreTrue));
    arg_->SetConstValue(MakeAny<bool>(kStoreTrue));
  }

  // Important phrase..
  auto* dest = arg_->GetDest();

  if (!arg_->GetAction()) {
    // We assume a default store action but only if has dest.
    if (action_kind_ == ActionKind::kNoAction && dest) {
      action_kind_ = ActionKind::kStore;
    }
    // Some action don't need an ops, like print_help, we perhaps need to
    // distinct that..
    arg_->SetAction(ActionInfo::CreateBuiltinAction(action_kind_, dest,
                                                    arg_->GetConstValue()));
  }

  if (!arg_->GetType()) {
    Operations* ops = nullptr;
    if (dest)
      ops = ActionNeedsValueType(action_kind_) ? dest->GetValueTypeOps()
                                               : dest->GetOperations();
    auto info = open_mode_ == kModeNoMode
                    ? TypeInfo::CreateDefault(ops)
                    : TypeInfo::CreateFileType(ops, open_mode_);
    arg_->SetType(std::move(info));
  }

  return std::move(arg_);
}

std::unique_ptr<ArgumentBuilder> ArgumentBuilder::Create() {
  return absl::make_unique<ArgumentBuilder>();
}

}  // namespace internal
}  // namespace argparse
