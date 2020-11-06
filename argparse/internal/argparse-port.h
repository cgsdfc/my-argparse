// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <type_traits>
#include <typeinfo>
#include <memory>

#include "absl/strings/string_view.h"

namespace argparse {

// Abseil has already done a great job on portability, but still there is some
// corner to cover, such as std::bool_constant.
namespace portability {
// For now we don't what standard/compiler has bool_constant, so we always use
// this one.
template <bool B>
using bool_constant = std::integral_constant<bool, B>;
}  // namespace portability

namespace internal {


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
