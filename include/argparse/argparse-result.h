// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <variant>

#include "argparse/internal/argparse-port.h"

namespace argparse {

// Result<T> handles user' returned value and error using a union.
template <typename T>
class Result {
 public:
  // Default is empty (!has_value && !has_error).
  Result() = default;
  // To hold a value.
  explicit Result(T&& val) : data_(std::move(val)) {
    ARGPARSE_DCHECK(has_value());
  }
  explicit Result(const T& val) : data_(val) { ARGPARSE_DCHECK(has_value()); }

  // For now just use default. If T can't be moved, will it still work?
  Result(Result&&) = default;
  Result& operator=(Result&&) = default;

  bool empty() const { return kEmptyIndex == data_.index(); }
  bool has_value() const { return data_.index() == kValueIndex; }
  bool has_error() const { return data_.index() == kErrorMsgIndex; }

  void set_error(const std::string& msg) {
    data_.template emplace<kErrorMsgIndex>(msg);
    ARGPARSE_DCHECK(has_error());
  }
  void set_error(std::string&& msg) {
    data_.template emplace<kErrorMsgIndex>(std::move(msg));
    ARGPARSE_DCHECK(has_error());
  }
  void set_value(const T& val) {
    data_.template emplace<kValueIndex>(val);
    ARGPARSE_DCHECK(has_value());
  }
  void set_value(T&& val) {
    data_.template emplace<kValueIndex>(std::move(val));
    ARGPARSE_DCHECK(has_value());
  }

  Result& operator=(const T& val) {
    set_value(val);
    return *this;
  }
  Result& operator=(T&& val) {
    set_value(std::move(val));
    return *this;
  }

  // Release the err-msg (if any). Go back to empty state.
  std::string release_error() {
    ARGPARSE_DCHECK(has_error());
    auto str = std::get<kErrorMsgIndex>(std::move(data_));
    reset();
    return str;
  }
  const std::string& get_error() const {
    ARGPARSE_DCHECK(has_error());
    return std::get<kErrorMsgIndex>(data_);
  }

  // Release the value, go back to empty state.
  T release_value() {
    ARGPARSE_DCHECK(has_value());
    auto val = std::get<kValueIndex>(std::move_if_noexcept(data_));
    reset();
    return val;
  }
  const T& get_value() const {
    ARGPARSE_DCHECK(has_value());
    return std::get<kValueIndex>(data_);
  }
  // Goes back to empty state.
  void reset() {
    data_.template emplace<kEmptyIndex>(internal::NoneType{});
    ARGPARSE_DCHECK(empty());
  }

 private:
  enum Indices {
    kEmptyIndex,
    kErrorMsgIndex,
    kValueIndex,
  };
  std::variant<internal::NoneType, std::string, T> data_;
};

}
