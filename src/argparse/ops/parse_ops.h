#pragma once

namespace argparse {

// The default impl for the types we know (bulitin-types like int).
// This traits shouldn't be overriden by users.
template <typename T, typename SFINAE = void>
struct DefaultParseTraits {
  static constexpr bool Run = false;
};

// By default, use the traits defined by the library for builtin types.
// The user can specialize this to provide traits for their custom types
// or override global (existing) types.
template <typename T>
struct ParseTraits : DefaultParseTraits<T> {};

template <>
struct DefaultParseTraits<std::string> {
  static void Run(const std::string& in, Result<std::string>* out) {
    *out = in;
  }
};
// char is an unquoted single character.
template <>
struct DefaultParseTraits<char> {
  static void Run(const std::string& in, Result<char>* out) {
    if (in.size() != 1)
      return out->set_error("char must be exactly one character");
    if (!std::isprint(in[0]))
      return out->set_error("char must be printable");
    *out = in[0];
  }
};
template <>
struct DefaultParseTraits<bool> {
  static void Run(const std::string& in, Result<bool>* out) {
    static const std::map<std::string, bool> kStringToBools{
        {"true", true},   {"True", true},   {"1", true},
        {"false", false}, {"False", false}, {"0", false},
    };
    auto iter = kStringToBools.find(in);
    if (iter == kStringToBools.end())
      return out->set_error("not a valid bool value");
    *out = iter->second;
  }
};

// For std::stof,stod,stold.
template <typename T, T (*func)(const std::string&, std::size_t*)>
using stl_floating_point_parser_t =
    std::integral_constant<decltype(func), func>;

// For std::stoi,stol,stoll,etc.
template <typename T, T (*func)(const std::string&, std::size_t*, int)>
using stl_integral_parser_t = std::integral_constant<decltype(func), func>;

template <typename T>
struct stl_number_parser : std::false_type {};

template <>
struct stl_number_parser<float>
    : stl_floating_point_parser_t<float, std::stof> {};
template <>
struct stl_number_parser<double>
    : stl_floating_point_parser_t<double, std::stod> {};
template <>
struct stl_number_parser<long double>
    : stl_floating_point_parser_t<long double, std::stold> {};

template <>
struct stl_number_parser<int> : stl_integral_parser_t<int, std::stoi> {};
template <>
struct stl_number_parser<long> : stl_integral_parser_t<long, std::stol> {};
template <>
struct stl_number_parser<long long>
    : stl_integral_parser_t<long long, std::stoll> {};

template <>
struct stl_number_parser<unsigned long>
    : stl_integral_parser_t<unsigned long, std::stoul> {};
template <>
struct stl_number_parser<unsigned long long>
    : stl_integral_parser_t<unsigned long long, std::stoull> {};

template <typename T>
using has_stl_number_parser_t =
    std::bool_constant<bool(stl_number_parser<T>{})>;

template <typename T, typename stl_number_parser<T>::value_type func>
T StlParseNumberImpl(const std::string& in, std::false_type) {
  return func(in, nullptr, 0);
}
template <typename T, typename stl_number_parser<T>::value_type func>
T StlParseNumberImpl(const std::string& in, std::true_type) {
  return func(in, nullptr);
}
template <typename T>
T StlParseNumber(const std::string& in) {
  static_assert(has_stl_number_parser_t<T>{});
  return StlParseNumberImpl<T, stl_number_parser<T>{}>(
      in, std::is_floating_point<T>{});
}

template <typename T>
struct DefaultParseTraits<T, std::enable_if_t<has_stl_number_parser_t<T>{}>> {
  static void Run(const std::string& in, Result<T>* out) {
    try {
      *out = StlParseNumber<T>(in);
    } catch (std::invalid_argument&) {
      out->set_error("invalid numeric format");
    } catch (std::out_of_range&) {
      out->set_error("numeric value out of range");
    }
  }
};

}  // namespace argparse
