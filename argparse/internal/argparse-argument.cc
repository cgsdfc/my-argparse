// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-argument.h"

namespace argparse {
namespace internal {

namespace {

// Holds all meta-info about an argument.
class ArgumentImpl : public Argument {
 public:
  ArgumentImpl() = default;

  DestInfo* GetDest() override { return dest_info_.get(); }
  TypeInfo* GetType() override { return type_info_.get(); }
  ActionInfo* GetAction() override { return action_info_.get(); }
  NumArgsInfo* GetNumArgs() override { return num_args_.get(); }
  const Any* GetConstValue() override { return const_value_.get(); }
  const Any* GetDefaultValue() override { return default_value_.get(); }
  absl::string_view GetMetaVar() override { return meta_var_; }
  ArgumentGroup* GetGroup() override { return group_; }
  NamesInfo* GetNamesInfo() override { return names_info_.get(); }
  bool IsRequired() override { return is_required_; }
  absl::string_view GetHelpDoc() override { return help_doc_; }
  void SetNames(std::unique_ptr<NamesInfo> info) override {
    names_info_ = std::move(info);
  }
  void SetRequired(bool required) override { is_required_ = required; }
  void SetHelpDoc(std::string help_doc) override {
    help_doc_ = std::move(help_doc);
  }
  void SetMetaVar(std::string meta_var) override {
    meta_var_ = std::move(meta_var);
  }
  void SetDest(std::unique_ptr<DestInfo> info) override {
    if (info) dest_info_ = std::move(info);
  }
  void SetType(std::unique_ptr<TypeInfo> info) override {
    if (info) type_info_ = std::move(info);
  }
  void SetAction(std::unique_ptr<ActionInfo> info) override {
    if (info) action_info_ = std::move(info);
  }
  void SetConstValue(std::unique_ptr<Any> value) override {
    if (value) const_value_ = std::move(value);
  }
  void SetDefaultValue(std::unique_ptr<Any> value) override {
    if (value) default_value_ = std::move(value);
  }
  void SetGroup(ArgumentGroup* group) override {
    ARGPARSE_DCHECK(group);
    group_ = group;
  }
  void SetNumArgs(std::unique_ptr<NumArgsInfo> info) override {
    if (info) num_args_ = std::move(info);
  }

 private:
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

}  // namespace

std::unique_ptr<Argument> Argument::Create() {
  return absl::make_unique<ArgumentImpl>();
}

bool Argument::AppendTypeHint(std::string* out) {
  if (auto* type = GetType()) {
    out->append(type->GetTypeHint());
    return true;
  }
  return false;
}

bool Argument::AppendDefaultValueAsString(std::string* out) {
  if (GetDefaultValue() && GetDest()) {
    auto str = GetDest()->GetOperations()->FormatValue(*GetDefaultValue());
    out->append(std::move(str));
    return true;
  }
  return false;
}

bool Argument::BeforeInUsage(Argument* a, Argument* b) {
  // options go before positionals.
  if (a->IsOptional() != b->IsOptional()) return a->IsOptional();

  // positional compares on their names.
  if (!a->IsOptional() && !b->IsOptional()) {
    return a->GetName() < b->GetName();
  }

  // required option goes first.
  if (a->IsRequired() != b->IsRequired()) return a->IsRequired();

  // // short-only option (flag) goes before the rest.
  if (a->IsFlag() != b->IsFlag()) return a->IsFlag();

  // a and b are both long options or both flags.
  return a->GetName() < b->GetName();
}

}  // namespace internal
}  // namespace argparse
