// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-argument-controller.h"

namespace argparse {
namespace internal {

ArgumentController::ArgumentController()
    : container_(new ArgumentContainer),
      parser_(ArgumentParser::CreateDefault()) {}

}  // namespace internal
}  // namespace argparse
