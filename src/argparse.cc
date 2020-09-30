#include "argparse/argparse.h"

// #include <cxxabi.h>  // demangle().
// #include <cstdlib>   // malloc()
// #include <cstring>   // strlen()

namespace argparse {

using internal::NamesInfo;

Names::Names(std::string name) {
  if (name[0] == '-') {
    // This is in fact an option.
    std::vector<std::string> names{std::move(name)};
    info = NamesInfo::CreateOptional(std::move(names));
    return;
  }
  ARGPARSE_CHECK_F(IsValidPositionalName(name),
                   "Not a valid positional name: %s", name.c_str());
  info = NamesInfo::CreatePositional(std::move(name));
}

Names::Names(std::initializer_list<std::string> names)
    : info(NamesInfo::CreateOptional(names)) {
  ARGPARSE_CHECK_F(names.size(), "At least one name must be provided");
}

}  // namespace argparse
