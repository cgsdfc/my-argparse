#pragma once

#include <memory>
#include <typeindex>
#include <utility>

#include "argparse/base/common.h"

namespace argparse {

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
  explicit AnyImpl(std::in_place_type_t<T>, Args&&... args)
      : value_(std::forward<Args>(args)...) {}

  ~AnyImpl() override {}
  std::type_index GetType() const override { return typeid(T); }

  T ReleaseValue() { return std::move_if_noexcept(value_); }
  const T& value() const { return value_; }

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
  return std::make_unique<AnyImpl<T>>(std::in_place_type<T>,
                                      std::forward<Args>(args)...);
}

// template <typename T, typename... Args>
// AnyImpl<T> MakeAnyOnStack(Args&&... args) {
//   return AnyImpl<T>(std::forward<Args>(args)...);
// }

template <typename T>
T AnyCast(std::unique_ptr<Any> any) {
  ARGPARSE_DCHECK(any);
  return AnyImpl<T>::FromAny(any.get())->ReleaseValue();
}

template <typename T>
T AnyCast(const Any& any) {
  return AnyImpl<T>::FromAny(any).value();
}

}  // namespace argparse
