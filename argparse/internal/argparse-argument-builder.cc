// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-argument-builder.h"
#include "argparse/internal/argparse-internal.h"

// Implementation of internal::ArgumentBuilder.
namespace argparse {
namespace internal {

namespace {

ActionKind StringToActions(const std::string& str) {
  static const std::map<std::string, ActionKind> kStringToActions{
      {"store", ActionKind::kStore},
      {"store_const", ActionKind::kStoreConst},
      {"store_true", ActionKind::kStoreTrue},
      {"store_false", ActionKind::kStoreFalse},
      {"append", ActionKind::kAppend},
      {"append_const", ActionKind::kAppendConst},
      {"count", ActionKind::kCount},
      {"print_help", ActionKind::kPrintHelp},
      {"print_usage", ActionKind::kPrintUsage},
  };
  auto iter = kStringToActions.find(str);
  ARGPARSE_CHECK_F(iter != kStringToActions.end(),
                   "Unknown action string '%s' passed in", str.c_str());
  return iter->second;
}

bool ActionNeedsBool(ActionKind in) {
  return in == ActionKind::kStoreFalse || in == ActionKind::kStoreTrue;
}

bool ActionNeedsValueType(ActionKind in) {
  return in == ActionKind::kAppend || in == ActionKind::kAppendConst;
}

class ArgumentBuilderImpl : public ArgumentBuilder {
 public:
  ArgumentBuilderImpl() : arg_(Argument::Create()) {}

  void SetNames(std::unique_ptr<NamesInfo> info) override {
    arg_->SetNames(std::move(info));
  }

  void SetDest(std::unique_ptr<DestInfo> info) override {
    arg_->SetDest(std::move(info));
  }

  void SetActionString(const char* str) override {
    action_kind_ = StringToActions(str);
  }

  void SetTypeInfo(std::unique_ptr<TypeInfo> info) override {
    if (info) arg_->SetType(std::move(info));
  }

  void SetActionInfo(std::unique_ptr<ActionInfo> info) override {
    if (info) arg_->SetAction(std::move(info));
  }

  void SetTypeFileType(OpenMode mode) override { open_mode_ = mode; }

  void SetNumArgs(std::unique_ptr<NumArgsInfo> info) override {
    if (info) arg_->SetNumArgs(std::move(info));
  }

  void SetConstValue(std::unique_ptr<Any> val) override {
    arg_->SetConstValue(std::move(val));
  }

  void SetDefaultValue(std::unique_ptr<Any> val) override {
    arg_->SetDefaultValue(std::move(val));
  }

  void SetMetaVar(std::string val) override {
    meta_var_ = absl::make_unique<std::string>(std::move(val));
  }

  void SetRequired(bool val) override {
    ARGPARSE_DCHECK(arg_);
    arg_->SetRequired(val);
  }

  void SetHelp(std::string val) override {
    ARGPARSE_DCHECK(arg_);
    arg_->SetHelpDoc(std::move(val));
  }

  std::unique_ptr<Argument> CreateArgument() override;

 private:
  // Some options are directly fed into arg.
  std::unique_ptr<Argument> arg_;
  // If not given, use default from NamesInfo.
  std::unique_ptr<std::string> meta_var_;
  ActionKind action_kind_ = ActionKind::kNoAction;
  OpenMode open_mode_ = kModeNoMode;
};

std::unique_ptr<Argument> ArgumentBuilderImpl::CreateArgument() {
  ARGPARSE_DCHECK(arg_);
  arg_->SetMetaVar(meta_var_ ? std::move(*meta_var_)
                             : arg_->GetNamesInfo()->GetDefaultMetaVar());

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

}  // namespace

std::unique_ptr<ArgumentBuilder> ArgumentBuilder::Create() {
  return absl::make_unique<ArgumentBuilderImpl>();
}

}  // namespace internal
}  // namespace argparse
