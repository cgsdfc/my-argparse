// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <iostream>
#include <memory>
#include <set>
#include <string>

#include "absl/base/attributes.h"
#include "absl/container/inlined_vector.h"
#include "absl/memory/memory.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"
#include "absl/utility/utility.h"
#include "argparse/internal/argparse-logging.h"

namespace argparse {

// Abseil has already done a great job on portability, but still there is some
// corner to cover, such as std::bool_constant.
namespace portability {

// For now we don't what standard/compiler has bool_constant, so we always use
// this one.
template <bool B>
using bool_constant = std::integral_constant<bool, B>;

#define ARGPARSE_STATIC_ASSERT(const_expr) \
  static_assert((const_expr), #const_expr)

}  // namespace portability

namespace internal {

class Any;

// When an meaningless type is needed.
struct NoneType {};

// TODO: forbit exception..
// Throw this exception will cause an error msg to be printed (via what()).
class ArgumentError final : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// TODO: find some replacement.
const char* TypeNameImpl(const std::type_info& type);

template <typename T>
absl::string_view TypeName() {
  return TypeNameImpl(typeid(T));
}

template <typename...>
struct TypeList {};

// Support holding a piece of opaque data by subclass.
class SupportUserData {
 public:
  struct UserData {
    virtual ~UserData() {}
  };

  UserData* GetUserData() const { return data_.get(); }
  void SetUserData(std::unique_ptr<UserData> data) { data_ = std::move(data); }

  SupportUserData() = default;

 private:
  std::unique_ptr<UserData> data_;
};

}  // namespace internal
}  // namespace argparse
