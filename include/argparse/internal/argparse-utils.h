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


// Control whether some extra info appear in the help doc.
enum class HelpFormatPolicy {
  kDefault,           // add nothing.
  kTypeHint,          // add (type: <type-hint>) to help doc.
  kDefaultValueHint,  // add (default: <default-value>) to help doc.
};

namespace detail {
}  // namespace detail

template <typename T, typename SFINAE = void>
struct has_prefix_plus_plus : std::false_type {};
template <typename T>
struct has_prefix_plus_plus<T, std::void_t<decltype(++std::declval<T&>())>>
    : std::true_type {};

}  // namespace argparse
