// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <memory>
#include <typeindex>
#include <utility>

#include "argparse/internal/argparse-port.h"

namespace argparse {
namespace internal {

// Our version of any.
class Any {
 public:
  virtual ~Any() {}
  virtual std::type_index GetType() const = 0;
};

template <typename T>
class AnyImpl : public Any {
 public:
  explicit AnyImpl(T&& val) : value_(std::move(val)) {}
  explicit AnyImpl(const T& val) : value_(val) {}
  template <typename... Args>
  explicit AnyImpl(absl::in_place_t, Args&&... args)
      : value_(std::forward<Args>(args)...) {}

  ~AnyImpl() override {}
  std::type_index GetType() const override { return typeid(T); }

  T ReleaseValue() { return std::move_if_noexcept(value_); }
  const T& value() const { return value_; }
  T& value() { return value_; }

  static AnyImpl* FromAny(Any* any) {
    ARGPARSE_DCHECK(any && any->GetType() == typeid(T));
    return static_cast<AnyImpl*>(any);
  }
  static const AnyImpl& FromAny(const Any& any) {
    ARGPARSE_DCHECK(any.GetType() == typeid(T));
    return static_cast<const AnyImpl&>(any);
  }

 private:
  T value_;
};

template <typename T, typename... Args>
std::unique_ptr<Any> MakeAny(Args&&... args) {
  return absl::make_unique<AnyImpl<T>>(absl::in_place,
                                              std::forward<Args>(args)...);
}

template <typename T>
T AnyCast(std::unique_ptr<Any> any) {
  ARGPARSE_DCHECK(any);
  return AnyImpl<T>::FromAny(any.get())->ReleaseValue();
}

template <typename T>
const T& AnyCast(const Any& any) {
  return AnyImpl<T>::FromAny(any).value();
}

template <typename T>
const T* AnyCast(const Any* any) {
  return &AnyCast<T>(*any);
}

template <typename T>
T* AnyCast(Any* any) {
  return &AnyImpl<T>::FromAny(any)->value();
}

}  // namespace internal
}  // namespace argparse