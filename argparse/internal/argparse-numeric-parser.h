// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <string>

#include "argparse/internal/argparse-port.h"

// About how to parse a string into a numeric value.
namespace argparse {
namespace internal {

// For std::stof,stod,stold.
template <typename T, T (*func)(const std::string&, std::size_t*)>
using STLFloatingPointParseFunc = std::integral_constant<decltype(func), func>;

// For std::stoi,stol,stoll,etc.
template <typename T, T (*func)(const std::string&, std::size_t*, int)>
using STLIntegralParseFunc = std::integral_constant<decltype(func), func>;

template <typename T>
struct STLNumericParseFunc : std::false_type {};

template <>
struct STLNumericParseFunc<float>
    : STLFloatingPointParseFunc<float, std::stof> {};
template <>
struct STLNumericParseFunc<double>
    : STLFloatingPointParseFunc<double, std::stod> {};
template <>
struct STLNumericParseFunc<long double>
    : STLFloatingPointParseFunc<long double, std::stold> {};

template <>
struct STLNumericParseFunc<int> : STLIntegralParseFunc<int, std::stoi> {};
template <>
struct STLNumericParseFunc<long> : STLIntegralParseFunc<long, std::stol> {};
template <>
struct STLNumericParseFunc<long long>
    : STLIntegralParseFunc<long long, std::stoll> {};

template <>
struct STLNumericParseFunc<unsigned long>
    : STLIntegralParseFunc<unsigned long, std::stoul> {};
template <>
struct STLNumericParseFunc<unsigned long long>
    : STLIntegralParseFunc<unsigned long long, std::stoull> {};

template <typename T>
using HasSTLNumericParseFunc =
    portability::bool_constant<bool(STLNumericParseFunc<T>{})>;

template <typename T, typename STLNumericParseFunc<T>::value_type func>
T STLParseNumericImpl(absl::string_view in, std::false_type) {
  return func(in, nullptr, 0);
}
template <typename T, typename STLNumericParseFunc<T>::value_type func>
T STLParseNumericImpl(absl::string_view in, std::true_type) {
  return func(in, nullptr);
}
template <typename T>
T STLParseNumeric(absl::string_view in) {
  static_assert(HasSTLNumericParseFunc<T>{}, "Must be a numeric type");
  return STLParseNumericImpl<T, STLNumericParseFunc<T>{}>(
      in, std::is_floating_point<T>{});
}

}  // namespace internal
}  // namespace argparse
