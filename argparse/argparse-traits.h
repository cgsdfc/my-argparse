// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <deque>
#include <fstream>
#include <list>
#include <map>
#include <sstream>
#include <type_traits>
#include <vector>

#ifdef ARGPARSE_USE_FMTLIB
#include <fmt/core.h>
#endif

#include "argparse/argparse-conversion-result.h"
#include "argparse/internal/argparse-logging.h"
#include "argparse/internal/argparse-open-traits.h"
#include "argparse/internal/argparse-port.h"
#include "argparse/internal/argparse-std-parse.h"

// Defines various traits that users can specialize to meet their needs.
namespace argparse {

// The purpose of MetaTypes is to provide a mechanism to summerize types as
// metatype so that they can have the same typehint. For example, different
// types of file object, like C FILE* and C++ streams, can all have the same
// metatype -- file. And different types of integers can all be summerized as
// 'int'. Our policy is to:
// 1. If T is specialized, use TypeHintTraits<T>.
// 2. If T's metatype is not unknown, use MetaTypeHint.
// 3. fall back to demangle.
// Note: number such as float, double and int, long use their own typename as
// metatype, since it will confuse user if not.
// As user, you can:
// 1. Be pleasant with the default setting for your type, such std::string and
// bool.
// 2. Want to use a metatype, tell us by MetaTypeOf<T>.
// 3. Want a completey new type hint, tell us by TypeHintTraits<T>.
enum class MetaTypes {
  kString,
  kFile,
  kList,
  kNumber,
  kBool,
  kChar,
  kUnknown,
};

template <MetaTypes M>
using MetaTypeContant = std::integral_constant<MetaTypes, M>;

template <typename T, typename SFINAE = void>
struct MetaTypeOf : MetaTypeContant<MetaTypes::kUnknown> {};

template <typename T>
struct AppendTraits;

template <typename T>
using TypeCallbackPrototype = auto(absl::string_view, T*) -> bool;
template <typename T>
using ActionCallbackPrototype = auto(T) -> bool;

template <typename T>
using TypeCallback = std::function<TypeCallbackPrototype<T>>;
template <typename T>
using ActionCallback = std::function<ActionCallbackPrototype<T>>;

// Keep these impl here. This makes the code more coherent.
namespace internal {

namespace traits_internal {

/// Strip the class from a method type
template <typename T>
struct remove_class {};
template <typename C, typename R, typename... A>
struct remove_class<R (C::*)(A...)> {
  typedef R type(A...);
};
template <typename C, typename R, typename... A>
struct remove_class<R (C::*)(A...) const> {
  typedef R type(A...);
};

template <typename F>
struct strip_function_object {
  using type = typename remove_class<decltype(&F::operator())>::type;
};

// Extracts the function signature from a function, function pointer or lambda.
template <typename Function, typename F = absl::remove_reference_t<Function>>
using function_signature_t = absl::conditional_t<
    std::is_function<F>::value, F,
    typename absl::conditional_t<
        std::is_pointer<F>::value || std::is_member_pointer<F>::value,
        std::remove_pointer<F>, strip_function_object<F>>::type>;

}  // namespace traits_internal

template <typename T>
struct IsOpenSupported;
template <typename T>
struct IsAppendSupported;
template <typename T>
struct IsNumericType;
template <typename T>
std::string TypeHint();
// Helper typedef to get ValueType of AppendTraits.
template <typename T>
using ValueTypeOf = typename AppendTraits<T>::ValueType;

// TODO: use StrCat instead of streams.
template <typename T>
struct DummyFormatTraits {
  static std::string Run(const T& in) {
    std::ostringstream os;
    os << "<" << TypeName<T>() << " object>";
    return os.str();
  }
};

template <typename T>
struct StringStreamFormatTraits {
  static std::string Run(const T& in) {
    std::ostringstream os;
    os << in;
    ARGPARSE_CHECK_F(os.good(), "error formatting: std::ostream failed");
    return os.str();
  }
};

template <typename T, typename SFINAE = void>
struct DefaultFormatTraits;

template <>
struct DefaultFormatTraits<bool, void> {
  static std::string Run(bool in) { return in ? "true" : "false"; }
};

template <>
struct DefaultFormatTraits<char, void> {
  // For char, this is 'c'.
  static std::string Run(char in) { return std::string{'\'', in, '\''}; }
};

template <typename T, typename SFINAE = void>
struct has_insert_operator : std::false_type {};
template <typename T>
struct has_insert_operator<T,
                           absl::void_t<decltype(std::declval<std::ostream&>()
                                                 << std::declval<const T&>())>>
    : std::true_type {};

#if ARGPARSE_USE_FMTLIB
template <typename T>
struct FmtlibFormatTraits {
  static std::string Run(const T& in) { return fmt::format("{}", in); }
};
// Handled by fmtlib completely.
template <typename T, typename SFINAE>
struct DefaultFormatTraits : FmtlibFormatTraits<T> {};
#else
// Default version is dummy..
template <typename T, typename SFINAE>
struct DefaultFormatTraits : DummyFormatTraits<T> {};
template <typename T>
struct DefaultFormatTraits<T, absl::enable_if_t<has_insert_operator<T>{}>>
    : StringStreamFormatTraits<T> {};
#endif
// Handling for file obj..

// The default impl for the types we know (bulitin-types like int).
// This traits shouldn't be overriden by users.
template <typename T, typename SFINAE = void>
struct DefaultParseTraits {
  static constexpr bool Run = false;
};

template <>
struct DefaultParseTraits<std::string> {
  static ConversionResult Run(absl::string_view in) {
    return ConversionSuccess(in);
  }
};

// char is an unquoted single character.
template <>
struct DefaultParseTraits<char> {
  // static bool Run(absl::string_view in, char* out);
  static ConversionResult Run(absl::string_view in);
};

template <>
struct DefaultParseTraits<bool> {
  static ConversionResult Run(absl::string_view in);
};

// TODO: use absl strings numbers, which is much faster.
template <typename T>
struct DefaultParseTraits<T, absl::enable_if_t<internal::IsNumericType<T>{}>> {
  static ConversionResult Run(absl::string_view in) {
    try {
      return ConversionSuccess(internal::StdParse<T>(in));
    } catch (std::invalid_argument&) {
      return ConversionFailure("invalid numeric format");
    } catch (std::out_of_range&) {
      return ConversionFailure("numeric value out of range");
    }
  }
};

// Default is the rules impl'ed by us:
// 1. fall back to TypeName() -- demanged name of T.
// 2. MetaTypeHint, for file, string and list[T], general types..
template <typename T, typename SFINAE = void>
struct DefaultTypeHint {
  static std::string Run() { return static_cast<std::string>(TypeName<T>()); }
};

// If you get unhappy with this default handling, for example,
// you want number to be "number", you can specialize this.
template <typename T, MetaTypes M = MetaTypeOf<T>{}>
struct MetaTypeHint {
  static std::string Run() {
    switch (M) {
      case MetaTypes::kFile:
        return "file";
      case MetaTypes::kString:
        return "string";
      case MetaTypes::kBool:
        return "bool";
      case MetaTypes::kChar:
        return "char";
      case MetaTypes::kNumber:
        return std::string(TypeName<T>());
      default:
        // List
        ARGPARSE_DCHECK(false);
    }
  }
};

template <typename T>
struct MetaTypeHint<T, MetaTypes::kList> {
  static std::string Run() {
    return "list[" + TypeHint<ValueTypeOf<T>>() + "]";
  }
};

template <typename T>
struct DefaultTypeHint<
    T, absl::enable_if_t<MetaTypes::kUnknown != MetaTypeOf<T>{}>>
    : MetaTypeHint<T> {};

}  // namespace internal

// This traits indicates whether T supports append operation and if it does,
// tells us how to do the append.
// For user's types, specialize AppendTraits<>, and if your type is
// standard-compatible, inherits from DefaultAppendTraits<>.
template <typename T>
struct AppendTraits {
  // Run(T* obj, ValueType val) should perform the append operation.
  static constexpr bool Run = false;
  // The value type of your appendable type.
  using ValueType = void;
};

// Default impl of AppendTraits. Use it if your type is standard-compatible.
template <typename T>
struct DefaultAppendTraits {
  // By default use T's member typedef.
  using ValueType = typename T::value_type;
  // By default use T's push_back.
  static void Run(T* obj, ValueType item) {
    obj->push_back(std::move_if_noexcept(item));
  }
};

// Partial Specialized for STL containers.
// Note: std::string is not considered appendable, if you need that, use
// std::vector<char>
template <typename T>
struct AppendTraits<std::vector<T>> : DefaultAppendTraits<std::vector<T>> {};
template <typename T>
struct AppendTraits<std::list<T>> : DefaultAppendTraits<std::list<T>> {};
template <typename T>
struct AppendTraits<std::deque<T>> : DefaultAppendTraits<std::deque<T>> {};

// The rules for FormatTraits are:
// 1. If fmtlib is found, use its functionality.
// 2. If no fmtlib, but operator<<(std::ostream&, const T&) is defined for T,
// use that. Specially, std::boolalpha is used.
// 3. Fall back to a format: <Type object>.
template <typename T>
struct FormatTraits : internal::DefaultFormatTraits<T> {};

// By default, use the traits defined by the library for builtin types.
// The user can specialize this to provide traits for their custom types
// or override global (existing) types.
template <typename T>
struct ParseTraits : internal::DefaultParseTraits<T> {};

// String.
template <>
struct MetaTypeOf<std::string, void> : MetaTypeContant<MetaTypes::kString> {};

// Bool.
template <>
struct MetaTypeOf<bool, void> : MetaTypeContant<MetaTypes::kBool> {};

// Char.
template <>
struct MetaTypeOf<char, void> : MetaTypeContant<MetaTypes::kChar> {};

// File.
template <typename T>
struct MetaTypeOf<T, absl::enable_if_t<internal::IsOpenSupported<T>{}>>
    : MetaTypeContant<MetaTypes::kFile> {};

// List.
template <typename T>
struct MetaTypeOf<T, absl::enable_if_t<internal::IsAppendSupported<T>{}>>
    : MetaTypeContant<MetaTypes::kList> {};

// Number.
template <typename T>
struct MetaTypeOf<T, absl::enable_if_t<internal::IsNumericType<T>{}>>
    : MetaTypeContant<MetaTypes::kNumber> {};

template <typename T>
struct TypeHintTraits : internal::DefaultTypeHint<T> {};

namespace internal {
template <typename T>
std::string TypeHint() {
  return TypeHintTraits<T>::Run();
}
template <typename T>
std::string FormatValue(const T& value) {
  return FormatTraits<absl::decay_t<T>>::Run(value);
}

}  // namespace internal
}  // namespace argparse
