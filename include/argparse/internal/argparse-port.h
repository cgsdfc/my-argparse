// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <iostream>
#include <memory>
#include <string>

#include "absl/base/attributes.h"
#include "absl/memory/memory.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"
#include "absl/utility/utility.h"

#define ARGPARSE_SOURCE_LOCATION_CURRENT() \
  (internal::SourceLocation{__LINE__, __FILE__, __func__})

#define ARGPARSE_CHECK_IMPL(condition, format, ...)                         \
  do {                                                                      \
    if (!static_cast<bool>(condition))                                      \
      ::argparse::internal::CheckFailed(ARGPARSE_SOURCE_LOCATION_CURRENT(), \
                                        (format), ##__VA_ARGS__);           \
  } while (0)

// Perform a runtime check for user's error.
#define ARGPARSE_CHECK_F(expr, format, ...) \
  ARGPARSE_CHECK_IMPL((expr), (format), ##__VA_ARGS__)

// If no format, use the stringified expr.
#define ARGPARSE_CHECK(expr) ARGPARSE_CHECK_IMPL((expr), "%s", #expr)

#ifdef NDEBUG  // Not debug
#define ARGPARSE_DCHECK(expr) ((void)(expr))
#define ARGPARSE_DCHECK_F(expr, format, ...) ((void)(expr))
#else
#define ARGPARSE_DCHECK(expr) ARGPARSE_CHECK(expr)
#define ARGPARSE_DCHECK_F(expr, format, ...) \
  ARGPARSE_CHECK_F(expr, format, ##__VA_ARGS__)
#endif

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

// TODO: integrate with absl's debugging.
struct SourceLocation {
  int line;
  const char* filename;
  const char* function;
};

ABSL_ATTRIBUTE_NORETURN void CheckFailed(SourceLocation loc, const char* fmt,
                                         ...) ABSL_PRINTF_ATTRIBUTE(2, 3);

bool IsValidPositionalName(const std::string& name);

// A valid option name is long or short option name and not '--', '-'.
// This is only checked once and true for good.
bool IsValidOptionName(const std::string& name);

// These two predicates must be called only when IsValidOptionName() holds.
inline bool IsLongOptionName(const std::string& name) {
  ARGPARSE_DCHECK(IsValidOptionName(name));
  return name.size() > 2;
}

inline bool IsShortOptionName(const std::string& name) {
  ARGPARSE_DCHECK(IsValidOptionName(name));
  return name.size() == 2;
}

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

}  // namespace internal
}  // namespace argparse
