#include "argparse/argparse.h"

namespace argparse {

using internal::NamesInfo;

Names::Names(std::string name) {
  if (name[0] == '-') {
    // This is in fact an option.
    std::vector<std::string> names{std::move(name)};
    this->SetObject(NamesInfo::CreateOptional(std::move(names)));
    return;
  }
  ARGPARSE_CHECK_F(internal::IsValidPositionalName(name),
                   "Not a valid positional name: %s", name.c_str());
  this->SetObject(NamesInfo::CreatePositional(std::move(name)));
}

Names::Names(std::initializer_list<std::string> names){
  ARGPARSE_CHECK_F(names.size(), "At least one name must be provided");
  this->SetObject(NamesInfo::CreateOptional(names));
}

}  // namespace argparse
