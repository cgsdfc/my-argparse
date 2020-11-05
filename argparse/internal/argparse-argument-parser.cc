// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "absl/memory/memory.h"

#if ARGPARSE_USE_GFLAGS
#include "argparse/internal/argparse-gflags-parser.h"
#elif ARGPARSE_USE_ARGP
#include "argparse/internal/argparse-argp-parser.h"
#else
#error no parser available.
#endif

// This file implements the ArgumentParser::CreateDefault().
namespace argparse {
namespace internal {

std::unique_ptr<ArgumentParser> ArgumentParser::CreateDefault() {
#if ARGPARSE_USE_GFLAGS
  using ParserType = gflags_parser_internal::GflagsParser;
#elif ARGPARSE_USE_ARGP
  using ParserType = argp_parser_internal::ArgpArgumentParser;
#else
#error no parser available.
#endif
  return absl::make_unique<ParserType>();
}

}  // namespace internal
}  // namespace argparse
