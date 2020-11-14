// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-info.h"

namespace argparse {
namespace internal {

class ArgumentGroup;
class ArgumentBuilder;

class Argument final : public SupportUserData {
 public:
  ArgumentGroup* GetGroup() const { return group_; }

  DestInfo* GetDest() const { return dest_info_.get(); }
  TypeInfo* GetType() const { return type_info_.get(); }
  ActionInfo* GetAction() const { return action_info_.get(); }
  NumArgsInfo* GetNumArgs() const { return num_args_.get(); }
  NamesInfo* GetNames() const { return names_info_.get(); }

  const Any* GetConstValue() const { return const_value_.get(); }
  const Any* GetDefaultValue() const { return default_value_.get(); }

  absl::string_view GetMetaVar() const { return meta_var_; }
  absl::string_view GetHelpDoc() const { return help_doc_; }

  bool IsRequired() const { return is_required_; }
  bool IsOptional() const { return GetNames()->IsOptional(); }
  bool IsPositional() const { return GetNames()->IsPositional(); }

  // TODO: fix this.
  // Flag is an option that only has short names.
  bool IsFlag() { return false; }

  // TODO: fix this.
  // For positional, this will be PosName. For Option, this will be
  // the first long name or first short name (if no long name).
  absl::string_view GetName() { return {}; }

  // If a typehint exists, return true and set out.
  bool AppendTypeHint(std::string* out);

  // Append the string form of the default value.
  bool AppendDefaultValueAsString(std::string* out);

  // Return true if `lhs` should appear before `rhs` in a usage message.
  static bool BeforeInUsage(Argument* lhs, Argument* rhs);

  static std::unique_ptr<Argument> Create();

 private:
  void SetNames(std::unique_ptr<NamesInfo> info) {
    names_info_ = std::move(info);
  }
  void SetRequired(bool required) { is_required_ = required; }
  void SetHelpDoc(absl::string_view value) {
    help_doc_ = static_cast<std::string>(value);
  }
  void SetMetaVar(absl::string_view value) {
    meta_var_ = static_cast<std::string>(value);
  }
  void SetDest(std::unique_ptr<DestInfo> info) {
    if (info) dest_info_ = std::move(info);
  }
  void SetType(std::unique_ptr<TypeInfo> info) {
    if (info) type_info_ = std::move(info);
  }
  void SetAction(std::unique_ptr<ActionInfo> info) {
    if (info) action_info_ = std::move(info);
  }
  void SetConstValue(std::unique_ptr<Any> value) {
    if (value) const_value_ = std::move(value);
  }
  void SetDefaultValue(std::unique_ptr<Any> value) {
    if (value) default_value_ = std::move(value);
  }
  void SetGroup(ArgumentGroup* group) {
    ARGPARSE_DCHECK(group);
    group_ = group;
  }
  void SetNumArgs(std::unique_ptr<NumArgsInfo> info) {
    if (info) num_args_ = std::move(info);
  }

  // Only ArgumentBuilder can access the setters.
  friend class ArgumentBuilder;

  ArgumentGroup* group_ = nullptr;
  std::string help_doc_;
  std::string meta_var_;
  bool is_required_ = false;
  std::unique_ptr<NamesInfo> names_info_;
  std::unique_ptr<DestInfo> dest_info_;
  std::unique_ptr<ActionInfo> action_info_;
  std::unique_ptr<TypeInfo> type_info_;
  std::unique_ptr<NumArgsInfo> num_args_;
  std::unique_ptr<Any> const_value_;
  std::unique_ptr<Any> default_value_;
};

}  // namespace internal
}  // namespace argparse
