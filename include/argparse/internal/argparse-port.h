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

// File open mode. This is not enum class since we do & | on it.
enum OpenMode {
  kModeNoMode = 0x0,
  kModeRead = 1,
  kModeWrite = 2,
  kModeAppend = 4,
  kModeTruncate = 8,
  kModeBinary = 16,
};

namespace internal {

class Any;

// When an meaningless type is needed.
struct NoneType {};

struct SourceLocation {
  int line;
  const char* filename;
  const char* function;
};

ABSL_ATTRIBUTE_NORETURN void CheckFailed(SourceLocation loc, const char* fmt,
                                         ...);

OpenMode CharsToMode(const char* str);
std::string ModeToChars(OpenMode mode);

OpenMode StreamModeToMode(std::ios_base::openmode stream_mode);
std::ios_base::openmode ModeToStreamMode(OpenMode m);

struct OpsResult {
  bool has_error = false;
  std::unique_ptr<Any> value;  // null if error.
  std::string errmsg;
};

// Throw this exception will cause an error msg to be printed (via what()).
class ArgumentError final : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// Like std::string_view, but may be more suit our needs.
// class absl::string_view {
//  public:
//   absl::string_view(const absl::string_view&) = default;
//   absl::string_view& operator=(const absl::string_view&) = default;

//   absl::string_view() = delete;
//   absl::string_view(std::string&&) = delete;

//   // data should be non-null and null-terminated.
//   absl::string_view(const char* data);

//   absl::string_view(const std::string& in) : absl::string_view(in.data(),
//   in.size()) {}

//   // Should be selected if data is a string literal.
//   template <std::size_t N>
//   absl::string_view(const char (&data)[N]) : absl::string_view(data, N - 1)
//   {}

//   // data should be non-null and null-terminated.
//   absl::string_view(const char* data, std::size_t size);

//   std::size_t size() const { return size_; }
//   bool empty() const { return 0 == size(); }
//   const char* data() const {
//     ARGPARSE_DCHECK(data_);
//     return data_;
//   }

//   std::string ToString() const { return std::string(data_, size_); }
//   std::unique_ptr<char[]> ToCharArray() const;

//   static int Compare(const absl::string_view& a, const absl::string_view& b);
//   bool operator<(const absl::string_view& that) const {
//     return Compare(*this, that) < 0;
//   }
//   bool operator==(const absl::string_view& that) const {
//     return Compare(*this, that) == 0;
//   }
//   operator std::string() const { return ToString(); }

//  private:
//   // Not default-constructible.
//   const char* data_;
//   std::size_t size_;
// };

// std::ostream& operator<<(std::ostream& os, const absl::string_view& in);

const char* TypeNameImpl(const std::type_info& type);

template <typename T>
absl::string_view TypeName() {
  return TypeNameImpl(typeid(T));
}

template <typename...>
struct TypeList {};

}  // namespace internal
}  // namespace argparse
