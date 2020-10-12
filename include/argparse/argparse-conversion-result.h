// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-result.h"

namespace argparse {

class ConversionResult {
 public:
  explicit ConversionResult(internal::Result result)
      : result_(std::move(result)) {}

 private:
  internal::Result result_;
};

inline ConversionResult ConversionFailure(std::string error) {
  return ConversionResult(internal::Result(std::move(error)));
}

template <typename T>
ConversionResult ConversionSuccess(T&& value) {
  auto any = internal::MakeAny<portability::decay_t<T>>(std::forward<T>(value));
  return ConversionResult(internal::Result(std::move(any)));
}

}  // namespace argparse
