// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <gflags/gflags.h>

#include "argparse/internal/argparse-internal.h"

namespace argparse {
namespace internal {

bool IsGflagsSupportedType(std::type_index type);

class GflagsParserFactory : public ArgumentParser::Factory {
 public:
  std::unique_ptr<ArgumentParser> CreateParser() override;
};

}  // namespace internal
}  // namespace argparse
