// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <typeindex>

#include "argparse/internal/argparse-logging.h"

namespace argparse {
namespace internal {

//  A type-erased type-safe void* wrapper.
class OpaquePtr {
 public:
  template <typename T>
  explicit OpaquePtr(T* ptr) : type_(typeid(T)), ptr_(ptr) {
    ARGPARSE_DCHECK(ptr);
  }

  OpaquePtr() = default;
  OpaquePtr(std::nullptr_t) : OpaquePtr() {}
  OpaquePtr(const OpaquePtr&) = default;
  OpaquePtr& operator=(const OpaquePtr&) = default;

  bool operator==(const OpaquePtr& that) const {
    return raw_value() == that.raw_value();
  }
  bool operator!=(const OpaquePtr& that) const {
    return !(*this == that);
  }

  template <typename T>
  T* Cast() const {
    ARGPARSE_DCHECK(type() == typeid(T));
    return reinterpret_cast<T*>(raw_value());
  }

  template <typename T>
  const T& GetValue() const {
    return *Cast<T>();
  }
  template <typename T>
  T& GetValue() {
    return *Cast<T>();
  }

  template <typename T>
  void PutValue(T&& val) {
    GetValue<absl::decay_t<T>>() = std::forward<T>(val);
  }

  template <typename T>
  void Reset(T* ptr) {
    OpaquePtr that(ptr);
    Swap(that);
  }
  void Reset(std::nullptr_t) {
    OpaquePtr null;
    Swap(null);
  }

  void Swap(OpaquePtr& that) {
    std::swap(this->type_, that.type_);
    std::swap(this->ptr_, that.ptr_);
  }

  explicit operator bool() const { return !!raw_value(); }

  std::type_index type() const { return type_; }
  void* raw_value() const { return ptr_; }

 private:
  // The type of *ptr_.
  std::type_index type_ = typeid(void);
  void* ptr_ = nullptr;
};

}  // namespace internal
}  // namespace argparse
