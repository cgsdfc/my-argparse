// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/argparse-builder.h"

namespace argparse {
namespace builder_internal {

Names::Names(std::string name) {
  if (name[0] == '-') {
    // This is in fact an option.
    std::vector<std::string> names{std::move(name)};
    this->SetObject(internal::NamesInfo::CreateOptional(std::move(names)));
    return;
  }
  ARGPARSE_CHECK_F(internal::IsValidPositionalName(name),
                   "Not a valid positional name: %s", name.c_str());
  this->SetObject(internal::NamesInfo::CreatePositional(std::move(name)));
}

Names::Names(std::initializer_list<std::string> names) {
  ARGPARSE_CHECK_F(names.size(), "At least one name must be provided");
  this->SetObject(internal::NamesInfo::CreateOptional(names));
}

}  // namespace builder_internal
}  // namespace argparse
