#pragma once

#include <deque>
#include <list>
#include <type_traits>  // bool_constant
#include <utility>  // move_if_noexcept()
#include <vector>

namespace argparse {
// This traits indicates whether T supports append operation and if it does,
// tells us how to do the append.
// For user's types, specialize AppendTraits<>, and if your type is
// standard-compatible, inherits from DefaultAppendTraits<>.
template <typename T>
struct AppendTraits {
  static constexpr bool Run = false;
};

// Extracted the bool value from AppendTraits.
template <typename T>
struct IsAppendSupported : std::bool_constant<bool(AppendTraits<T>::Run)> {};

// Get the value-type for a appendable, only use it when IsAppendSupported<T>.
template <typename T>
using ValueTypeOf = typename AppendTraits<T>::ValueType;

// 2 level spec here..
// template <typename T, bool = IsAppendSupported<T>{}>
// struct IsAppendConstSupported;
template <typename T>
struct IsAppendConstSupported<T, false> : std::false_type {};
template <typename T>
struct IsAppendConstSupported<T, true>
    : std::is_copy_assignable<ValueTypeOf<T>> {};

// For STL-compatible T, by default use the push_back() method of T.
template <typename T>
struct DefaultAppendTraits {
  using ValueType = ValueTypeOf<T>;
  static void Run(T* obj, ValueType item) {
    obj->push_back(std::move_if_noexcept(item));
  }
};

// Specialized for STL containers.
// std::string is not considered appendable, if you need that, use
// std::vector<char>
template <typename T>
struct AppendTraits<std::vector<T>> : DefaultAppendTraits<std::vector<T>> {};
template <typename T>
struct AppendTraits<std::list<T>> : DefaultAppendTraits<std::list<T>> {};
template <typename T>
struct AppendTraits<std::deque<T>> : DefaultAppendTraits<std::deque<T>> {};

}  // namespace argparse
