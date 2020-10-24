#pragma once

#include <argp.h>

#include "argparse/internal/argparse-argument-parser.h"

namespace argparse {
namespace internal {

namespace argp_parser_internal {
class ArgpArgumentParser : public ArgumentParser {
 public:
  // void AddArgument(Argument* arg) override {}
};

}  // namespace argp_parser_internal
}  // namespace internal

}  // namespace argparse
