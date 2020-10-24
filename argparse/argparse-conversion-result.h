// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-any.h"
#include "argparse/internal/argparse-port.h"

namespace argparse {

namespace internal {
struct OpsResult;
}  // namespace internal

// A new API that will replace the legacy TypeCallback by directly erasing type
// using ConversionResult.
class ConversionResult {
 public:
  // Use ConversionFailure() or ConversionSuccess() instead.
  explicit ConversionResult(std::string error)
      : error_(absl::make_unique<std::string>(std::move(error))) {}
  explicit ConversionResult(std::unique_ptr<internal::Any> value)
      : value_(std::move(value)) {}

  bool HasValue() const { return value_ != nullptr; }
  bool HasError() const { return error_ != nullptr; }

  template <typename T>
  const T& GetValue() const {
    ARGPARSE_DCHECK(HasValue());
    return internal::AnyCast<T>(*value_);
  }

  absl::string_view GetError() const {
    ARGPARSE_DCHECK(HasError());
    return *error_;
  }

  template <typename T>
  ABSL_MUST_USE_RESULT T TakeValue() {
    return internal::AnyCast<T>(ReleaseValue());
  }

  // Template conversion operator does not work?
  // template <typename T>
  // explicit operator const T&() const {
  //   return GetValue<T>();
  // }

 private:
  friend class internal::OpsResult;

  std::unique_ptr<internal::Any> ReleaseValue() {
    ARGPARSE_DCHECK(HasValue());
    return std::move(value_);
  }
  std::string ReleaseError() {
    ARGPARSE_DCHECK(HasError());
    auto str = std::move(*error_);
    error_.reset();
    return str;
  }

  std::unique_ptr<internal::Any> value_;
  std::unique_ptr<std::string> error_;
};

// Indicate a conversion failure. Optionally an error message can be supplied.
inline ConversionResult ConversionFailure(std::string error = {}) {
  return ConversionResult(std::move(error));
}

// Indicate a conversion success by wrapping a value into an instance of
// ConversionResult. The T type argument is made explicit to avoid
// human-errors.
template <typename T, typename... Args>
ConversionResult ConversionSuccess(Args&&... args) {
  return ConversionResult(internal::MakeAny<T>(std::forward<Args>(args)...));
}
// Single-value version of ConversionResult, which allows simple expression like
// ConversionSuccess(1);
template <typename T>
ConversionResult ConversionSuccess(T&& value) {
  return ConversionSuccess<absl::decay_t<T>>(std::forward<T>(value));
}

}  // namespace argparse
