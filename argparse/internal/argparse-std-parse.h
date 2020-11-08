// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <string>

#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"

namespace argparse {
namespace internal {

namespace stl_parse_internal {

// For std::stof,stod,stold.
template <typename T, T (*func)(const std::string&, std::size_t*)>
struct ParseFloat : std::integral_constant<decltype(func), func> {
  static T Invoke(const std::string& str) { return func(str, nullptr); }
};

// For std::stoi,stol,stoll,etc.
template <typename T, T (*func)(const std::string&, std::size_t*, int)>
struct ParseInt : std::integral_constant<decltype(func), func> {
  static T Invoke(const std::string& str) { return func(str, 0, nullptr); }
};

template <typename T>
struct ParseNumber : std::false_type {};

template <>
struct ParseNumber<float> : ParseFloat<float, std::stof> {};
template <>
struct ParseNumber<double> : ParseFloat<double, std::stod> {};
template <>
struct ParseNumber<long double> : ParseFloat<long double, std::stold> {};

template <>
struct ParseNumber<int> : ParseInt<int, std::stoi> {};
template <>
struct ParseNumber<long> : ParseInt<long, std::stol> {};
template <>
struct ParseNumber<long long> : ParseInt<long long, std::stoll> {};

template <>
struct ParseNumber<unsigned long> : ParseInt<unsigned long, std::stoul> {};
template <>
struct ParseNumber<unsigned long long>
    : ParseInt<unsigned long long, std::stoull> {};

template <typename T>
struct IsStdParseDefined
    : std::integral_constant<bool, static_cast<bool>(ParseNumber<T>::value)> {};

// Throwing version:
template <typename T>
absl::enable_if_t<IsStdParseDefined<T>::value, T> StdParse(
    absl::string_view in) {
  return ParseNumber<T>::Invoke(std::string(in));
}

template <typename T>
absl::enable_if_t<IsStdParseDefined<T>::value, bool> StdParse(
    absl::string_view in, T* out) {
  try {
    *out = StdParse<T>(in);
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace stl_parse_internal

using stl_parse_internal::IsStdParseDefined;
using stl_parse_internal::StdParse;

}  // namespace internal
}  // namespace argparse
