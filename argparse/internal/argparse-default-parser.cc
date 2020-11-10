// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-default-parser.h"

#include "argparse/internal/argparse-argument-container.h"
#include "argparse/internal/argparse-argument.h"

namespace argparse {
namespace internal {
namespace default_parser_internal {


}  // namespace default_parser_internal

std::unique_ptr<ArgumentParser> ArgumentParser::CreateDefault() {
  return absl::make_unique<default_parser_internal::DefaultParser>();
}

}  // namespace internal
}  // namespace argparse
