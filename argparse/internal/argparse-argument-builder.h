// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-argument.h"

namespace argparse {
namespace internal {

// This class handles Argument creation.
// It understands user' options and tries to create an argument correctly.
// Its necessity originates from the fact that some computation is unavoidable
// between creating XXXInfos and getting what user gives us. For example, user's
// action only tells us some string, but the actual performing of the action
// needs an Operation, which can only be found from DestInfo. Meanwhile, impl
// can choose to ignore some of user's options if the parser don't support it
// and create their own impl of Argument to fit their parser. In a word, this
// abstraction is right needed.
class ArgumentBuilder {
 public:
  ArgumentBuilder() : arg_(Argument::Create()) {}

  void SetNames(std::unique_ptr<NamesInfo> info) {
    if (info) arg_->SetNames(std::move(info));
  }

  void SetDest(std::unique_ptr<DestInfo> info) {
    if (info) arg_->SetDest(std::move(info));
  }

  void SetActionString(absl::string_view str) {
    action_kind_ = StringToActions(str);
  }

  void SetTypeInfo(std::unique_ptr<TypeInfo> info) {
    if (info) arg_->SetType(std::move(info));
  }

  void SetActionInfo(std::unique_ptr<ActionInfo> info) {
    if (info) arg_->SetAction(std::move(info));
  }

  void SetTypeFileType(OpenMode mode) { open_mode_ = mode; }

  void SetNumArgs(std::unique_ptr<NumArgsInfo> info) {
    if (info) arg_->SetNumArgs(std::move(info));
  }

  void SetConstValue(std::unique_ptr<Any> val) {
    arg_->SetConstValue(std::move(val));
  }

  void SetDefaultValue(std::unique_ptr<Any> val) {
    arg_->SetDefaultValue(std::move(val));
  }

  void SetMetaVar(std::string val) {
    meta_var_ = absl::make_unique<std::string>(std::move(val));
  }

  void SetRequired(bool val) { arg_->SetRequired(val); }

  void SetHelp(std::string val) { arg_->SetHelpDoc(std::move(val)); }

  std::unique_ptr<Argument> Build();

  static std::unique_ptr<ArgumentBuilder> Create();

 private:
  ActionKind StringToActions(absl::string_view str);

  // Some options are directly fed into arg.
  std::unique_ptr<Argument> arg_;
  // If not given, use default from NamesInfo.
  std::unique_ptr<std::string> meta_var_;
  ActionKind action_kind_ = ActionKind::kNoAction;
  OpenMode open_mode_ = kModeNoMode;
};

}  // namespace internal
}  // namespace argparse
