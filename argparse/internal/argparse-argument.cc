// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-argument.h"

namespace argparse {
namespace internal {

std::unique_ptr<Argument> Argument::Create() {
  return absl::make_unique<Argument>();
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
