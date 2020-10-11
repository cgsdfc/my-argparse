// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-any.h"
#include "argparse/internal/argparse-port.h"

namespace argparse {
namespace internal {

// Result is a union of a arbitrary value (type-erased) and an error message.
class Result {
public:

private:
    std::unique_ptr<Any> value_;
};

}  // namespace internal
}  // namespace argparse
