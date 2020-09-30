// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <functional>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <variant>

// Basic things
namespace argparse {

template <typename T>
std::string TypeHint();

const char* TypeNameImpl(const std::type_info& type);


// Control whether some extra info appear in the help doc.
enum class HelpFormatPolicy {
  kDefault,           // add nothing.
  kTypeHint,          // add (type: <type-hint>) to help doc.
  kDefaultValueHint,  // add (default: <default-value>) to help doc.
};

namespace detail {
}  // namespace detail

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

inline std::string ToUpper(const std::string& in) {
  std::string out(in);
  std::transform(in.begin(), in.end(), out.begin(), ::toupper);
  return out;
}

template <typename T, typename SFINAE = void>
struct has_insert_operator : std::false_type {};
template <typename T>
struct has_insert_operator<T,
                           std::void_t<decltype(std::declval<std::ostream&>()
                                                << std::declval<const T&>())>>
    : std::true_type {};

template <typename T, typename SFINAE = void>
struct has_prefix_plus_plus : std::false_type {};
template <typename T>
struct has_prefix_plus_plus<T, std::void_t<decltype(++std::declval<T&>())>>
    : std::true_type {};

class ArgArray {
 public:
  ArgArray(int argc, const char** argv)
      : argc_(argc), argv_(const_cast<char**>(argv)) {}
  ArgArray(std::vector<const char*>& args)
      : ArgArray(args.size(), args.data()) {}

  int argc() const { return argc_; }
  std::size_t size() const { return argc(); }

  char** argv() const { return argv_; }
  char* operator[](std::size_t i) {
    ARGPARSE_DCHECK(i < argc());
    return argv()[i];
  }

 private:
  int argc_;
  char** argv_;
};

// Like std::string_view, but may be more suit our needs.
class StringView {
 public:
  StringView(const StringView&) = default;
  StringView& operator=(const StringView&) = default;

  StringView() = delete;
  StringView(std::string&&) = delete;

  // data should be non-null and null-terminated.
  StringView(const char* data);

  StringView(const std::string& in) : StringView(in.data(), in.size()) {}

  // Should be selected if data is a string literal.
  template <std::size_t N>
  StringView(const char (&data)[N]) : StringView(data, N - 1) {}

  std::size_t size() const { return size_; }
  bool empty() const { return 0 == size(); }
  const char* data() const {
    ARGPARSE_DCHECK(data_);
    return data_;
  }

  std::string ToString() const { return std::string(data_, size_); }
  std::unique_ptr<char[]> ToCharArray() const;

  static int Compare(const StringView& a, const StringView& b);
  bool operator<(const StringView& that) const {
    return Compare(*this, that) < 0;
  }
  bool operator==(const StringView& that) const {
    return Compare(*this, that) == 0;
  }

 private:
  // data should be non-null and null-terminated.
  StringView(const char* data, std::size_t size);

  // Not default-constructible.
  const char* data_;
  std::size_t size_;
};

std::ostream& operator<<(std::ostream& os, const StringView& in);

template <typename T>
StringView TypeName() {
  return TypeNameImpl(typeid(T));
}

}  // namespace argparse
