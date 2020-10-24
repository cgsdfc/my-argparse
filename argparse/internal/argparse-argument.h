// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-info.h"

namespace argparse {
namespace internal {
class ArgumentGroup;

class Argument {
 public:
  virtual bool IsRequired() = 0;
  virtual absl::string_view GetHelpDoc() = 0;
  virtual absl::string_view GetMetaVar() = 0;
  virtual ArgumentGroup* GetGroup() = 0;
  virtual NamesInfo* GetNamesInfo() = 0;
  virtual DestInfo* GetDest() = 0;
  virtual TypeInfo* GetType() = 0;
  virtual ActionInfo* GetAction() = 0;
  virtual NumArgsInfo* GetNumArgs() = 0;
  virtual const Any* GetConstValue() = 0;
  virtual const Any* GetDefaultValue() = 0;

  virtual void SetNames(std::unique_ptr<NamesInfo> info) = 0;
  virtual void SetRequired(bool required) = 0;
  virtual void SetHelpDoc(std::string help_doc) = 0;
  virtual void SetMetaVar(std::string meta_var) = 0;
  virtual void SetDest(std::unique_ptr<DestInfo> dest) = 0;
  virtual void SetType(std::unique_ptr<TypeInfo> info) = 0;
  virtual void SetAction(std::unique_ptr<ActionInfo> info) = 0;
  virtual void SetConstValue(std::unique_ptr<Any> value) = 0;
  virtual void SetDefaultValue(std::unique_ptr<Any> value) = 0;
  virtual void SetGroup(ArgumentGroup* group) = 0;
  virtual void SetNumArgs(std::unique_ptr<NumArgsInfo> info) = 0;

  // non-virtual helpers.
  bool IsOption() { return GetNamesInfo()->IsOption(); }
  // Flag is an option that only has short names.
  bool IsFlag() {
    auto* names = GetNamesInfo();
    return names->IsOption() && 0 == names->GetLongNamesCount();
  }

  // For positional, this will be PosName. For Option, this will be
  // the first long name or first short name (if no long name).
  absl::string_view GetName() {
    ARGPARSE_DCHECK(GetNamesInfo());
    return GetNamesInfo()->GetName();
  }

  // If a typehint exists, return true and set out.
  bool AppendTypeHint(std::string* out);

  // Append the string form of the default value.
  bool AppendDefaultValueAsString(std::string* out);

  // Return true if `lhs` should appear before `rhs` in a usage message.
  static bool BeforeInUsage(Argument* lhs, Argument* rhs);

  virtual ~Argument() {}
  static std::unique_ptr<Argument> Create();
};

}  // namespace internal
}  // namespace argparse
