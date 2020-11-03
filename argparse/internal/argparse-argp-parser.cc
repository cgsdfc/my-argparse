// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-argp-parser.h"

namespace argparse {
namespace internal {
namespace argp_parser_internal {

void ArgpArgumentParser::Initialize(ArgumentContainer* container) {}

bool ArgpArgumentParser::ParseKnownArgs(ArgArray args,
                                        std::vector<std::string>* out) {return false;}
}

std::unique_ptr<ArgumentParser> ArgumentParser::CreateDefault() {
  using argp_parser_internal::ArgpArgumentParser;
  return absl::make_unique<ArgpArgumentParser>();
}

}  // namespace internal
}  // namespace argparse
