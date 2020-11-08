// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <sstream>

#include "absl/meta/type_traits.h"
#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "argparse/internal/argparse-std-parse.h"

// This file implements the default parser for various basic types.
namespace argparse {
namespace internal {

namespace parse_traits_internal {

// Default implementation of ArgparseParse().

bool ArgparseParse(absl::string_view str, bool* out) {
  return absl::SimpleAtob(str, out);
}

bool ArgparseParse(absl::string_view str, std::string* out) {
  *out = std::string(str);
  return true;
}

bool ArgparseParse(absl::string_view str, char* out) {
  if (str.size() == 1 && absl::ascii_isprint(str.front())) {
    *out = str.front();
    return true;
  }
  return false;
}

bool ArgparseParse(absl::string_view str, float* out) {
  return absl::SimpleAtof(str, out);
}

bool ArgparseParse(absl::string_view str, double* out) {
  return absl::SimpleAtod(str, out);
}

template <typename int_type>
absl::enable_if_t<std::is_integral<int_type>::value &&
                      (sizeof(int_type) == 4 || sizeof(int_type) == 8),
                  bool> // Ensure `int_type` is a 4 or 8 bytes integer type.
ArgparseParse(absl::string_view str, int_type* out) {
  return absl::SimpelAtoi(str, out);
}

// Select a proper Parse() function for type `T`.
class ParseSelect {
 private:
  // ArgparseParse()
  struct ArgparseParseProbe {
    template <typename T>
    static auto Invoke(absl::string_view str, T* out) -> absl::enable_if_t<
        std::is_same<bool, decltype(ArgparseParse(str, out))>::value, bool> {
      return ArgparseParse(str, out);
    }
  };

  // std::stox.
  struct StdParseProbe {
    template <typename T>
    static auto Invoke(absl::string_view str, T* out)
        -> absl::enable_if_t<IsStdParseDefined<T>::value, bool> {
      return StdParse(str, out);
    }
  };

  // is >> value.
  struct ExtractorOpProbe {
    template <typename T>
    static auto Invoke(absl::string_view str, T* out) -> absl::enable_if_t<
        std::is_same<std::istream&, decltype(std::declval<std::istream&>() >>
                                             std::declval<T&>())>::value,
        bool> {
      std::istringstream iss(std::string(str));
      iss >> *out;
      return iss.good();
    }
  };

  template <typename Parse, typename T>
  struct Probe : Parse {
   private:
    template <typename>
    std::false_type Test(char);

    template <typename P,
              typename = decltype(P::Invoke(std::declval<absl::string_view>(),
                                            std::declval<T*>()))>
    std::true_type Test(int);

   public:
    static constexpr bool value = decltype(Test<Parse>(0))::value;
  };

 public:
  template <typename T>
  using Apply =
      absl::disjunction<Probe<ArgparseParseProbe, T>, Probe<StdParseProbe, T>,
                        Probe<ExtractorOpProbe, T>, std::false_type>;
};

template <typename T>
struct IsParseSupported
    : std::integral_constant<bool, ParseSelect::template Apply<T>::value> {};

template <typename T>
absl::enable_if_t<IsParseSupported<T>::value, bool> Parse(absl::string_view str,
                                                          T* out) {
  return ParseSelect::template Apply<T>::Invoke(str, out);
}

}  // namespace parse_traits_internal

using parse_traits_internal::IsParseSupported;
using parse_traits_internal::Parse;

}  // namespace internal
}  // namespace argparse
