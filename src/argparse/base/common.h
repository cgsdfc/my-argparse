#pragma once

#define ARGPARSE_CHECK_IMPL(condition, format, ...)                         \
  do {                                                                      \
    if (!static_cast<bool>(condition))                                      \
      CheckFailed({__LINE__, __FILE__, __func__}, (format), ##__VA_ARGS__); \
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
// When an meaningless type is needed.
struct NoneType {};

[[noreturn]] void CheckFailed(SourceLocation loc, const char* fmt, ...);

struct SourceLocation {
  int line;
  const char* filename;
  const char* function;
};

// File open mode. This is not enum class since we do & | on it.
enum OpenMode {
  kModeNoMode = 0x0,
  kModeRead = 1,
  kModeWrite = 2,
  kModeAppend = 4,
  kModeTruncate = 8,
  kModeBinary = 16,
};

namespace detail {
// clang-format off

// Copied from pybind11.
/// Strip the class from a method type
template <typename T> struct remove_class { };
template <typename C, typename R, typename... A> struct remove_class<R (C::*)(A...)> { typedef R type(A...); };
template <typename C, typename R, typename... A> struct remove_class<R (C::*)(A...) const> { typedef R type(A...); };

template <typename F> struct strip_function_object {
    using type = typename remove_class<decltype(&F::operator())>::type;
};

// Extracts the function signature from a function, function pointer or lambda.
template <typename Func, typename F = std::remove_reference_t<Func>>
using function_signature_t = std::conditional_t<
    std::is_function<F>::value,
    F,
    typename std::conditional_t<
        std::is_pointer<F>::value || std::is_member_pointer<F>::value,
        std::remove_pointer<F>,
        strip_function_object<F>
    >::type
>;
// clang-format on

template <typename T>
struct is_function_pointer : std::is_function<std::remove_pointer_t<T>> {};

template <typename T, typename SFINAE = void>
struct is_functor : std::false_type {};

// Note: this will fail on auto lambda and overloaded operator().
// But you should not use these as input to callback.
template <typename T>
struct is_functor<T, std::void_t<decltype(&T::operator())>> : std::true_type {};

template <typename Func, typename F = std::decay_t<Func>>
struct is_callback
    : std::bool_constant<is_function_pointer<F>{} || is_functor<F>{}> {};

}  // namespace detail

template <typename T>
const char* TypeName();

const char* TypeNameImpl(const std::type_info& type);
template <typename T>
const char* TypeName() {
  return TypeNameImpl(typeid(T));
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

}  // namespace argparse