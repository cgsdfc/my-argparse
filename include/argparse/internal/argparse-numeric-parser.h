// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <string>

// About how to parse a string into a numeric value.
namespace argparse {
namespace internal {

// For std::stof,stod,stold.
template <typename T, T (*func)(const std::string&, std::size_t*)>
using stl_floating_point_parser_t =
    std::integral_constant<decltype(func), func>;

// For std::stoi,stol,stoll,etc.
template <typename T, T (*func)(const std::string&, std::size_t*, int)>
using stl_integral_parser_t = std::integral_constant<decltype(func), func>;

template <typename T>
struct stl_numeric_parser : std::false_type {};

template <>
struct stl_numeric_parser<float>
    : stl_floating_point_parser_t<float, std::stof> {};
template <>
struct stl_numeric_parser<double>
    : stl_floating_point_parser_t<double, std::stod> {};
template <>
struct stl_numeric_parser<long double>
    : stl_floating_point_parser_t<long double, std::stold> {};

template <>
struct stl_numeric_parser<int> : stl_integral_parser_t<int, std::stoi> {};
template <>
struct stl_numeric_parser<long> : stl_integral_parser_t<long, std::stol> {};
template <>
struct stl_numeric_parser<long long>
    : stl_integral_parser_t<long long, std::stoll> {};

template <>
struct stl_numeric_parser<unsigned long>
    : stl_integral_parser_t<unsigned long, std::stoul> {};
template <>
struct stl_numeric_parser<unsigned long long>
    : stl_integral_parser_t<unsigned long long, std::stoull> {};

template <typename T>
using has_stl_number_parser_t =
    std::bool_constant<bool(stl_numeric_parser<T>{})>;

template <typename T, typename stl_numeric_parser<T>::value_type func>
T StlParseNumberImpl(const std::string& in, std::false_type) {
  return func(in, nullptr, 0);
}
template <typename T, typename stl_numeric_parser<T>::value_type func>
T StlParseNumberImpl(const std::string& in, std::true_type) {
  return func(in, nullptr);
}
template <typename T>
T StlParseNumber(const std::string& in) {
  static_assert(has_stl_number_parser_t<T>{});
  return StlParseNumberImpl<T, stl_numeric_parser<T>{}>(
      in, std::is_floating_point<T>{});
}

}  // namespace internal
}  // namespace argparse
