// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-port.h"

namespace argparse {

// Result<T> handles user' returned value and error using a union.
template <typename T>
class Result {
 public:
  // Default is empty (!has_value && !has_error).
  Result() = default;

  template <typename ... Args>
  explicit Result(std::in_place_t, Args&&... args) {
    AdvanceToValueStateFromEmpty(std::in_place, std::forward<Args>(args)...);
  }
  // To hold a value.
  explicit Result(T&& val) { AdvanceToValueStateFromEmpty(std::move(val)); }
  explicit Result(const T& val) { AdvanceToValueStateFromEmpty(val); }

  // For now just use default. If T can't be moved, will it still work?
  Result(Result&&) = default;
  Result& operator=(Result&&) = default;

  bool empty() const { return IsEmptyState(); }
  bool has_value() const { return IsValueState(); }
  bool has_error() const { return IsErrorState(); }

  void SetError(const std::string& msg) { AdvanceToErrorState(msg); }
  void SetError(std::string&& msg) { AdvanceToErrorState(std::move(msg)); }
  void SetValue(const T& val) { AdvanceToValueState(val); }
  void SetValue(T&& val) { AdvanceToValueState(std::move(val)); }

  Result& operator=(const T& val) {
    SetValue(val);
    return *this;
  }
  Result& operator=(T&& val) {
    SetValue(std::move(val));
    return *this;
  }

  template <typename ... Args>
  void emplace(Args&&... args) {
    AdvanceToValueState(std::in_place, std::forward<Args>(args)...);
  }

  // Release the err-msg (if any). Go back to empty state.
  std::string ReleaseError() { return AdvanceToEmptyStateFromError(); }
  const std::string& error() const {
    ARGPARSE_DCHECK(has_error());
    return *error_;
  }

  // Release the value, go back to empty state.
  T ReleaseValue() { return AdvanceToEmptyStateFromValue(); }
  const T& value() const {
    ARGPARSE_DCHECK(has_value());
    return *value_;
  }
  // Goes back to empty state.
  void Reset() { AdvanceToEmptyState(); }

 private:
  void AdvanceToEmptyState() {
    ARGPARSE_DCHECK(IsValidState());
    error_.reset();
    value_.reset();
  }
  std::string AdvanceToEmptyStateFromError() {
    ARGPARSE_DCHECK(IsErrorState());
    std::string str(std::move(*error_));
    error_.reset();
    return str;
  }
  T AdvanceToEmptyStateFromValue() {
    ARGPARSE_DCHECK(IsValueState());
    T val(std::move_if_noexcept(*value_));
    value_.reset();
    return val;
  }
  void AdvanceToValueState(T value) {
    AdvanceToEmptyState();
    AdvanceToValueStateFromEmpty(std::move(value));
  }
  void AdvanceToValueStateFromEmpty(T value) {
    ARGPARSE_DCHECK(IsEmptyState());
    value_.reset(new T(std::move_if_noexcept(value)));
  }
  template <typename... Args>
  void AdvanceToValueState(std::in_place_t, Args&&... args) {
    AdvanceToEmptyState();
    AdvanceToValueStateFromEmpty(std::in_place, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void AdvanceToValueStateFromEmpty(std::in_place_t, Args&&... args) {
    ARGPARSE_DCHECK(IsEmptyState());
    value_.reset(new T{std::forward<Args>(args)...});
  }
  void AdvanceToErrorStateFromEmpty(std::string err) {
    ARGPARSE_DCHECK(IsEmptyState());
    error_.reset(new std::string(std::move(err)));
  }
  void AdvanceToErrorState(std::string err) {
    AdvanceToEmptyState();
    AdvanceToErrorStateFromEmpty(std::move(err));
  }
  bool IsValidState() const { return !(error_ && value_); }
  bool IsValueState() const {
    ARGPARSE_DCHECK(IsValidState());
    return value_ != nullptr;
  }
  bool IsErrorState() const {
    ARGPARSE_DCHECK(IsValidState());
    return error_ != nullptr;
  }
  bool IsEmptyState() const {
    ARGPARSE_DCHECK(IsValidState());
    return error_ == nullptr && value_ == nullptr;
  }

  std::unique_ptr<std::string> error_;
  std::unique_ptr<T> value_;
};

}  // namespace argparse
