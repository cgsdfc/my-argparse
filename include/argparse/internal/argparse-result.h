// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <memory>
#include <string>

#include "argparse/internal/argparse-any.h"
#include "argparse/internal/argparse-port.h"

namespace argparse {
namespace internal {

// Result is a union of a arbitrary value (type-erased) and an error message.
class Result {
 public:
  explicit Result(std::unique_ptr<Any> value) : value_(std::move(value)) {}
  explicit Result(std::string error)
      : error_(absl::make_unique<std::string>(std::move(error))) {}

  Result() = default;

  bool HasValue() const { return !!value_; }
  bool HasError() const { return !!error_; }
  bool IsEmpty() const { return !HasValue() && !HasError(); }

  const std::string& error() const{
    ARGPARSE_DCHECK(HasError());
    return *error_;
  }

  const Any* value() const {
    ARGPARSE_DCHECK(HasValue());
    return value_.get();
  }

  std::unique_ptr<Any> TakeValue() {
    ARGPARSE_DCHECK(HasValue());
    return std::move(value_);
  }

  std::unique_ptr<std::string> TakeError() {
    ARGPARSE_DCHECK(HasError());
    return std::move(error_);
  }

 private:
  std::unique_ptr<Any> value_;
  std::unique_ptr<std::string> error_;
};

}  // namespace internal
}  // namespace argparse
