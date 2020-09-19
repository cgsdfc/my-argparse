#pragma once

#include <argp.h>
#include <algorithm>
#include <cassert>
#include <deque>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>  // to define ArgumentError.
#include <type_traits>
#include <typeindex>  // We use type_index since it is copyable.
#include <utility>
#include <variant>
#include <vector>

#ifdef ARGPARSE_USE_FMTLIB
#include <fmt/core.h>
#endif

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

class Argument;
class ArgumentGroup;

struct SourceLocation {
  int line;
  const char* filename;
  const char* function;
};

// When an meaningless type is needed.
struct NoneType {};

[[noreturn]] void CheckFailed(SourceLocation loc, const char* fmt, ...);

// Result<T> handles user' returned value and error using a union.
template <typename T>
class Result {
 public:
  // Default is empty (!has_value && !has_error).
  Result() { ARGPARSE_DCHECK(empty()); }
  // To hold a value.
  explicit Result(T&& val) : data_(std::move(val)) {
    ARGPARSE_DCHECK(has_value());
  }
  explicit Result(const T& val) : data_(val) { ARGPARSE_DCHECK(has_value()); }

  // For now just use default. If T can't be moved, will it still work?
  Result(Result&&) = default;
  Result& operator=(Result&&) = default;

  bool has_value() const { return data_.index() == kValueIndex; }
  bool has_error() const { return data_.index() == kErrorMsgIndex; }

  void set_error(const std::string& msg) {
    data_.template emplace<kErrorMsgIndex>(msg);
    ARGPARSE_DCHECK(has_error());
  }
  void set_error(std::string&& msg) {
    data_.template emplace<kErrorMsgIndex>(std::move(msg));
    ARGPARSE_DCHECK(has_error());
  }
  void set_value(const T& val) {
    data_.template emplace<kValueIndex>(val);
    ARGPARSE_DCHECK(has_value());
  }
  void set_value(T&& val) {
    data_.template emplace<kValueIndex>(std::move(val));
    ARGPARSE_DCHECK(has_value());
  }

  Result& operator=(const T& val) {
    set_value(val);
    return *this;
  }
  Result& operator=(T&& val) {
    set_value(std::move(val));
    return *this;
  }

  // Release the err-msg (if any). Keep in error state.
  std::string release_error() {
    ARGPARSE_DCHECK(has_error());
    // We know that std::string is moveable.
    return std::get<kErrorMsgIndex>(std::move(data_));
  }
  const std::string& get_error() const {
    ARGPARSE_DCHECK(has_error());
    return std::get<kErrorMsgIndex>(data_);
  }

  // Release the value, Keep in value state.
  T release_value() {
    ARGPARSE_DCHECK(has_value());
    // But we don't know T is moveable or not.
    return std::get<kValueIndex>(std::move_if_noexcept(data_));
  }
  const T& get_value() const {
    ARGPARSE_DCHECK(has_value());
    return std::get<kValueIndex>(data_);
  }
  // Goes back to empty state.
  void reset() {
    data_.template emplace<kEmptyIndex>(NoneType{});
    ARGPARSE_DCHECK(empty());
  }

 private:
  bool empty() const { return kEmptyIndex == data_.index(); }
  enum Indices {
    kEmptyIndex,
    kErrorMsgIndex,
    kValueIndex,
  };
  std::variant<NoneType, std::string, T> data_;
};

// Throw this exception will cause an error msg to be printed (via what()).
class ArgumentError final : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// Our version of any.
class Any {
 public:
  virtual ~Any() {}
  virtual std::type_index GetType() const = 0;
};

template <typename T>
class AnyImpl : public Any {
 public:
  explicit AnyImpl(T&& val) : value_(std::move(val)) {}
  explicit AnyImpl(const T& val) : value_(val) {}
  ~AnyImpl() override {}
  std::type_index GetType() const override { return typeid(T); }

  T ReleaseValue() { return std::move_if_noexcept(value_); }
  const T& value() const { return value_; }

  static AnyImpl* FromAny(Any* any) {
    ARGPARSE_DCHECK(any && any->GetType() == typeid(T));
    return static_cast<AnyImpl*>(any);
  }
  static const AnyImpl& FromAny(const Any& any) {
    ARGPARSE_DCHECK(any.GetType() == typeid(T));
    return static_cast<const AnyImpl&>(any);
  }

 private:
  T value_;
};

template <typename T, typename... Args>
std::unique_ptr<Any> MakeAny(Args&&... args) {
  return std::make_unique<AnyImpl<T>>(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
AnyImpl<T> MakeAnyOnStack(Args&&... args) {
  return AnyImpl<T>(std::forward<Args>(args)...);
}

template <typename T>
T AnyCast(std::unique_ptr<Any> any) {
  ARGPARSE_DCHECK(any);
  return AnyImpl<T>::FromAny(any.get())->ReleaseValue();
}

template <typename T>
T AnyCast(const Any& any) {
  return AnyImpl<T>::FromAny(any).value();
}

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

enum class ActionKind {
  kNoAction,
  kStore,
  kStoreConst,
  kStoreTrue,
  kStoreFalse,
  kAppend,
  kAppendConst,
  kCount,
  kPrintHelp,
  kPrintUsage,
  kCustom,
};

enum class TypeKind {
  kNothing,
  kParse,
  kOpen,
  kCustom,
};

enum class OpsKind {
  kStore,
  kStoreConst,
  kAppend,
  kAppendConst,
  kCount,
  kParse,
  kOpen,
};

inline constexpr std::size_t kMaxOpsKind = std::size_t(OpsKind::kOpen) + 1;

// File open mode. This is not enum class since we do & | on it.
enum OpenMode {
  kModeNoMode = 0x0,
  kModeRead = 1,
  kModeWrite = 2,
  kModeAppend = 4,
  kModeTruncate = 8,
  kModeBinary = 16,
};

OpenMode CharsToMode(const char* str);
std::string ModeToChars(OpenMode mode);

OpenMode StreamModeToMode(std::ios_base::openmode stream_mode);
std::ios_base::openmode ModeToStreamMode(OpenMode m);

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

template <typename T>
struct TypeHintTraits;

template <typename T>
std::string TypeHint() {
  return TypeHintTraits<T>::Run();
}

template <typename T>
const char* TypeName();

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
    ARGPARSE_CHECK_F(os.good(), "error formatting type %s: std::ostream failed",
                     TypeName<T>());
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
struct DefaultFormatTraits<T, std::enable_if_t<has_insert_operator<T>{}>>
    : StringStreamFormatTraits<T> {};
#endif
// Handling for file obj..

// The rules for FormatTraits are:
// 1. If fmtlib is found, use its functionality.
// 2. If no fmtlib, but operator<<(std::ostream&, const T&) is defined for T,
// use that. Specially, std::boolalpha is used.
// 3. Fall back to a format: <Type object>.
template <typename T>
struct FormatTraits : DefaultFormatTraits<T> {};

// Helper function:
template <typename T>
std::string Format(const T& in) {
  return FormatTraits<T>::Run(in);
}

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
using IsAppendSupported = std::bool_constant<bool(AppendTraits<T>::Run)>;

// Get the value-type for a appendable, only use it when IsAppendSupported<T>.
template <typename T>
using ValueTypeOf = typename AppendTraits<T>::ValueType;

constexpr const char kDefaultOpenFailureMsg[] = "Failed to open file";

template <typename T>
struct OpenTraits {
  static constexpr void* Run = nullptr;
};

template <OpsKind Ops, typename T>
struct IsOpsSupported : std::false_type {};

template <typename T>
struct IsOpsSupported<OpsKind::kStore, T>
    : std::bool_constant<std::is_copy_assignable<T>{} ||
                         std::is_move_assignable<T>{}> {};

template <typename T>
struct IsOpsSupported<OpsKind::kStoreConst, T> : std::is_copy_assignable<T> {};

template <typename T>
struct IsOpsSupported<OpsKind::kAppend, T> : IsAppendSupported<T> {};

// 2 level spec here..
template <typename T, bool = IsAppendSupported<T>{}>
struct IsAppendConstSupported;
template <typename T>
struct IsAppendConstSupported<T, false> : std::false_type {};
template <typename T>
struct IsAppendConstSupported<T, true>
    : std::is_copy_assignable<ValueTypeOf<T>> {};

template <typename T>
struct IsOpsSupported<OpsKind::kAppendConst, T> : IsAppendConstSupported<T> {};

template <typename T>
struct IsOpsSupported<OpsKind::kCount, T> : std::is_integral<T> {};

template <typename T>
struct IsOpsSupported<OpsKind::kParse, T>
    : std::bool_constant<bool(ParseTraits<T>::Run)> {};

template <typename T>
struct IsOpsSupported<OpsKind::kOpen, T>
    : std::bool_constant<bool(OpenTraits<T>::Run)> {};
// wstring is not supported now.

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

struct CFileOpenTraits {
  static void Run(const std::string& in, OpenMode mode, Result<FILE*>* out);
};

template <typename T>
struct StreamOpenTraits {
  static void Run(const std::string& in, OpenMode mode, Result<T>* out) {
    auto ios_mode = ModeToStreamMode(mode);
    T stream(in, ios_mode);
    if (stream.is_open())
      return out->set_value(std::move(stream));
    out->set_error(kDefaultOpenFailureMsg);
  }
};

template <>
struct OpenTraits<FILE*> : CFileOpenTraits {};
template <>
struct OpenTraits<std::fstream> : StreamOpenTraits<std::fstream> {};
template <>
struct OpenTraits<std::ifstream> : StreamOpenTraits<std::ifstream> {};
template <>
struct OpenTraits<std::ofstream> : StreamOpenTraits<std::ofstream> {};

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

// Default is the rules impl'ed by us:
// 1. fall back to TypeName() -- demanged name of T.
// 2. MetaTypeHint, for file, string and list[T], general types..
template <typename T, typename SFINAE = void>
struct DefaultTypeHint {
  static std::string Run() { return TypeName<T>(); }
};

// TypeHint() is always supported..
template <typename T>
struct TypeHintTraits : DefaultTypeHint<T> {};

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
struct MetaTypeOf<T, std::enable_if_t<IsOpsSupported<OpsKind::kOpen, T>{}>>
    : MetaTypeContant<MetaTypes::kFile> {};

// List.
template <typename T>
struct MetaTypeOf<T, std::enable_if_t<IsOpsSupported<OpsKind::kAppend, T>{}>>
    : MetaTypeContant<MetaTypes::kList> {};

// Number.
template <typename T>
struct MetaTypeOf<T, std::enable_if_t<has_stl_number_parser_t<T>{}>>
    : MetaTypeContant<MetaTypes::kNumber> {};

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
        return TypeName<T>();
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
struct DefaultTypeHint<T,
                       std::enable_if_t<MetaTypes::kUnknown != MetaTypeOf<T>{}>>
    : MetaTypeHint<T> {};

// This is a type-erased type-safe void* wrapper.
class DestPtr {
 public:
  template <typename T>
  explicit DestPtr(T* ptr) : type_(typeid(T)), ptr_(ptr) {}
  DestPtr() = default;

  // Copy content to out.
  template <typename T>
  const T& load() const {
    ARGPARSE_DCHECK(type_ == typeid(T));
    return *reinterpret_cast<const T*>(ptr_);
  }
  template <typename T>
  void load(T* out) const {
    *out = load<T>();
  }

  template <typename T>
  T* load_ptr() const {
    ARGPARSE_DCHECK(type_ == typeid(T));
    return reinterpret_cast<T*>(ptr_);
  }
  template <typename T>
  void load_ptr(T** ptr_out) const {
    *ptr_out = load_ptr<T>();
  }

  template <typename T>
  void store(T&& val) {
    using Type = std::remove_reference_t<T>;
    ARGPARSE_DCHECK(type_ == typeid(Type));
    *reinterpret_cast<T*>(ptr_) = std::forward<T>(val);
  }

  template <typename T>
  void reset(T* ptr) {
    DestPtr that(ptr);
    swap(that);
  }

  void swap(DestPtr& that) {
    std::swap(this->type_, that.type_);
    std::swap(this->ptr_, that.ptr_);
  }

  explicit operator bool() const { return !!ptr_; }

  std::type_index type() const { return type_; }
  void* ptr() const { return ptr_; }

 private:
  std::type_index type_ = typeid(NoneType);
  void* ptr_ = nullptr;
};

struct OpsResult {
  bool has_error = false;
  std::unique_ptr<Any> value;  // null if error.
  std::string errmsg;
};

// A handle to the function table.
class Operations {
 public:
  // For actions:
  virtual void Store(DestPtr dest, std::unique_ptr<Any> data) = 0;
  virtual void StoreConst(DestPtr dest, const Any& data) = 0;
  virtual void Append(DestPtr dest, std::unique_ptr<Any> data) = 0;
  virtual void AppendConst(DestPtr dest, const Any& data) = 0;
  virtual void Count(DestPtr dest) = 0;
  // For types:
  virtual void Parse(const std::string& in, OpsResult* out) = 0;
  virtual void Open(const std::string& in, OpenMode, OpsResult* out) = 0;
  virtual bool IsSupported(OpsKind ops) = 0;
  virtual const char* GetTypeName() = 0;
  virtual std::string GetTypeHint() = 0;
  virtual const std::type_info& GetTypeInfo() = 0;
  virtual std::string FormatValue(const Any& val) = 0;
  virtual ~Operations() {}
};

// How to create a vtable?
class OpsFactory {
 public:
  virtual std::unique_ptr<Operations> Create() = 0;
  // If this type has a concept of value_type, create a handle.
  virtual std::unique_ptr<Operations> CreateValueTypeOps() = 0;
  virtual ~OpsFactory() {}
};

template <typename T>
using TypeCallbackPrototype = void(const std::string&, Result<T>*);

// There is an alternative for those using exception.
// You convert string into T and throw ArgumentError if something bad happened.
template <typename T>
using TypeCallbackPrototypeThrows = T(const std::string&);

// The prototype for action. An action normally does not report errors.
template <typename T, typename V>
using ActionCallbackPrototype = void(T*, Result<V>);

// Can only take a Result without dest.
// template <typename T, typename V>
// using ActionCallbackPrototypeNoDest = void(Result<V>);

class CallbackRunner {
 public:
  // Communicate with outside when callback is fired.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual bool GetValue(std::string* out) = 0;
    virtual void OnCallbackError(const std::string& errmsg) = 0;
    virtual void OnPrintUsage() = 0;
    virtual void OnPrintHelp() = 0;
  };
  // Before the callback is run, allow default value to be set.
  virtual void InitCallback() {}
  virtual void RunCallback(std::unique_ptr<Delegate> delegate) = 0;
  virtual ~CallbackRunner() {}
};

// This is the type eraser of user's callback to the type() option.
class TypeCallback {
 public:
  virtual ~TypeCallback() {}
  virtual void Run(const std::string& in, OpsResult* out) = 0;
  virtual std::string GetTypeHint() = 0;
};

// Similar to TypeCallback, but is for action().
class ActionCallback {
 public:
  virtual ~ActionCallback() {}
  virtual void Run(DestPtr dest, std::unique_ptr<Any> data) = 0;
};

class NamesInfo {
 public:
  virtual ~NamesInfo() {}
  virtual bool IsOption() {}
  virtual std::string GetDefaultMetaVar() {}

  enum NameKind {
    kLongName,
    kShortName,
    kAllNames,
  };

  // Visit each name of the optional argument.
  virtual void ForEachName(NameKind name_kind,
                           std::function<void(const std::string&)> callback) {}
  virtual const char* GetPositionalName() {}

  static std::unique_ptr<NamesInfo> CreateOptional(const std::string& in);
  static std::unique_ptr<NamesInfo> CreatePositional(
      const std::vector<std::string>& in);
};

class NumArgsInfo {
 public:
  virtual ~NumArgsInfo() {}
  // Run() checks if num is valid by returning bool.
  // If invalid, error msg will be set.
  virtual bool Run(unsigned num, std::string* errmsg) = 0;
  static std::unique_ptr<NumArgsInfo> CreateFromFlag(char flag);
  static std::unique_ptr<NumArgsInfo> CreateFromNum(int num);
};

class DestInfo {
 public:
  virtual ~DestInfo() {}
  // For action.
  virtual DestPtr GetDestPtr() = 0;
  // For default value formatting.
  virtual std::string FormatValue(const Any& in) = 0;
  // For providing default ops for type and action.
  virtual OpsFactory* GetOpsFactory() = 0;

  template <typename T>
  static std::unique_ptr<DestInfo> CreateFromPtr(T* ptr);
};

class CallbackClient {
 public:
  virtual ~CallbackClient() {}
  virtual std::unique_ptr<Any> GetData() = 0;
  virtual DestPtr GetDestPtr() = 0;
  virtual const Any* GetConstValue() = 0;
  virtual void PrintHelp() = 0;
  virtual void PrintUsage() = 0;
};

class ActionInfo {
 public:
  virtual ~ActionInfo() {}

  virtual void Run(CallbackClient* client) = 0;

  static std::unique_ptr<ActionInfo> CreateDefault(
      ActionKind action_kind,
      std::unique_ptr<Operations> ops);

  static std::unique_ptr<ActionInfo> CreateFromCallback(
      std::unique_ptr<ActionCallback> cb);
};

class TypeInfo {
 public:
  virtual ~TypeInfo() {}
  virtual void Run(const std::string& in, OpsResult* out) = 0;
  virtual std::string GetTypeHint() = 0;

  // Default version: parse a single string into value.
  static std::unique_ptr<TypeInfo> CreateDefault(
      std::unique_ptr<Operations> ops);
  // Open a file.
  static std::unique_ptr<TypeInfo> CreateFileType(
      std::unique_ptr<Operations> ops,
      OpenMode mode);
  // Invoke user's callback.
  static std::unique_ptr<TypeInfo> CreateFromCallback(
      std::unique_ptr<TypeCallback> cb);
};

// Control whether some extra info appear in the help doc.
enum class HelpFormatPolicy {
  kDefault,           // add nothing.
  kTypeHint,          // add (type: <type-hint>) to help doc.
  kDefaultValueHint,  // add (default: <default-value>) to help doc.
};

class Argument {
 public:
  virtual bool IsRequired() = 0;
  virtual const char* GetHelpDoc() = 0;
  virtual const char* GetMetaVar() = 0;
  virtual ArgumentGroup* GetGroup() = 0;
  virtual NamesInfo* GetNamesInfo() = 0;
  virtual DestInfo* GetDest() = 0;
  virtual TypeInfo* GetType() = 0;
  virtual ActionInfo* GetAction() = 0;
  virtual const Any* GetConstValue() = 0;
  virtual const Any* GetDefaultValue() = 0;

  virtual void SetRequired(bool required) = 0;
  virtual void SetHelpDoc(std::string help_doc) = 0;
  virtual void SetMetaVar(std::string meta_var) = 0;
  virtual void SetDest(std::unique_ptr<DestInfo> dest) = 0;
  virtual void SetType(std::unique_ptr<TypeInfo> info) = 0;
  virtual void SetAction(std::unique_ptr<ActionInfo> info) = 0;
  virtual void SetConstValue(std::unique_ptr<Any> value) = 0;
  virtual void SetDefaultValue(std::unique_ptr<Any> value) = 0;
  virtual void SetGroup(ArgumentGroup* group) = 0;
  virtual void SetNumArgs(std::unique_ptr<NumArgsInfo> info) = 0;

  // non-virtual helpers.
  bool IsOption() { return GetNamesInfo()->IsOption(); }
  // If a typehint exists, return true and set out.
  bool GetTypeHint(std::string* out) {
    if (auto* type = GetType()) {
      *out = type->GetTypeHint();
      return true;
    }
    return false;
  }
  // If a default-value exists, return true and set out.
  bool FormatDefaultValue(std::string* out) {
    if (GetDefaultValue() && GetDest()) {
      *out = GetDest()->FormatValue(*GetDefaultValue());
      return true;
    }
    return false;
  }

  virtual bool Before(const Argument* that) const = 0;

  virtual ~Argument() {}
  static std::unique_ptr<Argument> Create(std::unique_ptr<NamesInfo> info);
};

// This class handles Argument creation.
// It understands user' options and tries to create an argument correctly.
// Its necessity originates from the fact that some computation is unavoidable
// between creating XXXInfos and getting what user gives us. For example, user's
// action only tells us some string, but the actual performing of the action
// needs an Operation, which can only be found from DestInfo. Meanwhile, impl
// can choose to ignore some of user's options if the parser don't support it
// and create their own impl of Argument to fit their parser. In a word, this
// abstraction is right needed.
class ArgumentFactory {
 public:
  virtual ~ArgumentFactory() {}

  // Accept things from argument.

  // names
  virtual void SetNames(std::unique_ptr<NamesInfo> info) = 0;

  // dest(&obj)
  virtual void SetDest(std::unique_ptr<DestInfo> info) = 0;

  // action("store")
  virtual void SetActionString(const char* str) = 0;

  // action(<lambda>)
  virtual void SetActionCallback(std::unique_ptr<ActionCallback> cb) = 0;

  // type<int>()
  virtual void SetTypeOperations(std::unique_ptr<Operations> ops) = 0;

  // type(<lambda>)
  virtual void SetTypeCallback(std::unique_ptr<TypeCallback> cb) = 0;

  // type(FileType())
  virtual void SetTypeFileType(OpenMode mode) = 0;

  // nargs('+')
  virtual void SetNumArgsFlag(char flag) = 0;

  // nargs(42)
  virtual void SetNumArgsNumber(int num) = 0;

  // const_value(...)
  virtual void SetConstValue(std::unique_ptr<Any> val) = 0;

  // default_value(...)
  virtual void SetDefaultValue(std::unique_ptr<Any> val) = 0;

  // required(false)
  virtual void SetRequired(bool req) = 0;

  // help(xxx)
  virtual void SetHelp(std::string val) = 0;

  // meta_var(xxx)
  virtual void SetMetaVar(std::string val) = 0;

  // Finally..
  virtual std::unique_ptr<Argument> Create() = 0;

  static std::unique_ptr<ArgumentFactory> CreateDefault();
};

// Return value of help filter function.
enum class HelpFilterResult {
  kKeep,
  kDrop,
  kReplace,
};

using HelpFilterCallback =
    std::function<HelpFilterResult(const Argument&, std::string* text)>;

// XXX: this depends on argp and is not general.
// In fact, people only need to pass in a std::string.
using ProgramVersionCallback = void (*)(std::FILE*, argp_state*);

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

class ArgumentGroup {
 public:
  virtual ~ArgumentGroup() {}
  virtual const char* GetHeader() = 0;
  // Visit each arg.
  // TODO: impl this.
  virtual void ForEachArgument(std::function<void(Argument*)> callback) {}
  // Add an arg to this group.
  virtual void AddArgument(std::unique_ptr<Argument> arg) = 0;
  virtual int GetArgumentCount() = 0;
};

class ArgumentHolder {
 public:
  // Notify outside some event.
  class Listener {
   public:
    virtual void OnAddArgument(Argument* arg) {}
    virtual void OnAddArgumentGroup(ArgumentGroup* group) {}
    virtual ~Listener() {}
  };

  virtual void SetListener(std::unique_ptr<Listener> listener) {}
  virtual ArgumentGroup* AddArgumentGroup(const char* header) = 0;
  virtual void ForEachArgument(std::function<void(Argument*)> callback) = 0;
  virtual void ForEachGroup(std::function<void(ArgumentGroup*)> callback) = 0;
  virtual int GetArgumentCount() = 0;
  // method to add arg to default group.
  virtual void AddArgument(std::unique_ptr<Argument> arg) = 0;
  virtual ~ArgumentHolder() {}
  static std::unique_ptr<ArgumentHolder> Create();
};

struct SubCommandInfo {
  std::string name;
  std::string help_doc;
  std::vector<std::string> aliases;

  SubCommandInfo(const char* name_,
                 const char* help,
                 std::vector<std::string> alias) {
    ARGPARSE_DCHECK(name_);
    name.assign(name_);
    if (help)
      help_doc.assign(help);
    aliases = std::move(alias);
  }
};

class SubCommand {
 public:
  virtual ~SubCommand() {}
  virtual ArgumentHolder* GetHolder() = 0;
  virtual SubCommandInfo* GetInfo() = 0;
  // virtual const char* GetName() = 0;
  // virtual const char* GetHelpDoc() = 0;
  // virtual void ForEachAlias(std::function<void(const char*)> callback) {}
};

// The basic information of this group.
struct SubCommandGroupInfo {};

// A group of SubCommands, which can have things like description...
class SubCommandGroup {
 public:
  virtual ~SubCommandGroup() {}
  virtual SubCommand* AddSubCommand(std::unique_ptr<SubCommandInfo> info) = 0;
  virtual SubCommandGroupInfo* GetInfo() = 0;
};

// Like ArgumentHolder, but holds subcommands.
class SubCommandHolder {
 public:
  class Listener {
   public:
    virtual ~Listener() {}
    virtual void OnAddSubCommandGroup(SubCommandGroup* group) {}
    virtual void OnAddSubCommand(SubCommand* sub) {}
  };

  virtual ~SubCommandHolder() {}
  virtual SubCommandGroup* AddSubCommandGroup(
      std::unique_ptr<SubCommandGroupInfo> info) = 0;
  virtual void ForEachSubCommand(std::function<void(SubCommand*)> callback) = 0;
  virtual void ForEachSubCommandGroup(
      std::function<void(SubCommandGroup*)> callback) = 0;
  virtual void SetListener(std::unique_ptr<Listener> listener) = 0;
  static std::unique_ptr<SubCommandHolder> Create();
};

// Main options passed to the parser.
struct OptionsInfo {
  int flags = 0;
  // TODO: may change some of these to std::string to allow dynamic generated
  // content.
  const char* program_version = {};
  const char* description = {};
  const char* after_doc = {};
  const char* domain = {};
  const char* email = {};
  ProgramVersionCallback program_version_callback;
  HelpFilterCallback help_filter;
};

// Parser contains everythings it needs to parse arguments.
class Parser {
 public:
  virtual ~Parser() {}
  // Parse args, if rest is null, exit on error. Otherwise put unknown ones into
  // rest and return status code.
  virtual bool ParseKnownArgs(ArgArray args, std::vector<std::string>* out) = 0;
};

class ParserFactory {
 public:
  // Interaction when creating parser.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual std::unique_ptr<OptionsInfo> GetOptions() = 0;
    virtual ArgumentHolder* GetMainHolder() = 0;
    virtual SubCommandHolder* GetSubCommandHolder() = 0;
  };

  virtual ~ParserFactory() {}
  virtual std::unique_ptr<Parser> CreateParser(
      std::unique_ptr<Delegate> delegate) = 0;

  using Callback = std::unique_ptr<ParserFactory> (*)();
  static void RegisterCallback(Callback callback);
};

// Combination of Holder and Parser. ArgumentParser should be impl'ed in terms
// of this.
class ArgumentController {
 public:
  virtual ~ArgumentController() {}
  virtual ArgumentHolder* GetMainHolder() = 0;
  virtual SubCommandHolder* GetSubCommandHolder() = 0;
  virtual void SetOptions(std::unique_ptr<OptionsInfo> info) = 0;
  virtual Parser* GetParser() = 0;
  static std::unique_ptr<ArgumentController> Create();
};

////////////////////////////////////////
// End of interfaces. Begin of Impls. //
////////////////////////////////////////

// TODO: make these info class abstract.
struct NamesInfoImpl : public NamesInfo {
  bool is_option = false;
  std::vector<std::string> long_names;
  std::vector<char> short_names;
  std::string meta_var;

  // Positional.
  explicit NamesInfoImpl(std::string name);
  // Option.
  explicit NamesInfoImpl(std::vector<const char*> names);
};

class NumArgsInfoImpl : public NumArgsInfo {
 public:
  explicit NumArgsInfoImpl(int num) : lower_bound_(num), upper_bound_(num) {}
  explicit NumArgsInfoImpl(char flag) {
    switch (flag) {
      case '+':
        lower_bound_ = 1;
        upper_bound_ = kMax;
        break;
      case '?':
        lower_bound_ = 0;
        upper_bound_ = 1;
      case '*':
        lower_bound_ = 0;
        upper_bound_ = kMax;
      default:
        ARGPARSE_DCHECK(false);
        break;
    }
  }

  bool Run(unsigned num, std::string* errmsg) override { return false; }

 private:
  static constexpr unsigned kMax = std::numeric_limits<unsigned>::max();
  unsigned lower_bound_;
  unsigned upper_bound_;
};

inline std::unique_ptr<NumArgsInfo> NumArgsInfo::CreateFromFlag(char flag) {
  return std::make_unique<NumArgsInfoImpl>(flag);
}

inline std::unique_ptr<NumArgsInfo> NumArgsInfo::CreateFromNum(int num) {
  return std::make_unique<NumArgsInfoImpl>(num);
}

class DestInfoImpl : public DestInfo {
 public:
  DestInfoImpl(DestPtr d, std::unique_ptr<OpsFactory> f)
      : dest_ptr_(d), ops_factory_(std::move(f)) {
    ops_ = ops_factory_->Create();
  }

  DestPtr GetDestPtr() override { return dest_ptr_; }
  OpsFactory* GetOpsFactory() override { return ops_factory_.get(); }
  std::string FormatValue(const Any& in) override {
    return ops_->FormatValue(in);
  }

 private:
  DestPtr dest_ptr_;
  std::unique_ptr<OpsFactory> ops_factory_;
  std::unique_ptr<Operations> ops_;
};

// ActionInfo for builtin actions like store and append.
class DefaultActionInfo : public ActionInfo {
 public:
  DefaultActionInfo(ActionKind action_kind, std::unique_ptr<Operations> ops)
      : action_kind_(action_kind), ops_(std::move(ops)) {}

  void Run(CallbackClient* client) override;

 private:
  // Since kind of action is too much, we use a switch instead of subclasses.
  ActionKind action_kind_;
  std::unique_ptr<Operations> ops_;
};

// class TypeLessActionInfo : public 

// Adapt an ActionCallback to ActionInfo.
class ActionCallbackInfo : public ActionInfo {
 public:
  explicit ActionCallbackInfo(std::unique_ptr<ActionCallback> cb)
      : action_callback_(std::move(cb)) {}

  void Run(CallbackClient* client) override {
    return action_callback_->Run(client->GetDestPtr(), client->GetData());
  }

 private:
  std::unique_ptr<ActionCallback> action_callback_;
};

inline std::unique_ptr<ActionInfo> ActionInfo::CreateDefault(
    ActionKind action_kind,
    std::unique_ptr<Operations> ops) {
  return std::make_unique<DefaultActionInfo>(action_kind, std::move(ops));
}

inline std::unique_ptr<ActionInfo> ActionInfo::CreateFromCallback(
    std::unique_ptr<ActionCallback> cb) {
  return std::make_unique<ActionCallbackInfo>(std::move(cb));
}

// The default of TypeInfo: parse a single string into a value
// using ParseTraits.
class DefaultTypeInfo : public TypeInfo {
 public:
  explicit DefaultTypeInfo(std::unique_ptr<Operations> ops)
      : ops_(std::move(ops)) {}

  void Run(const std::string& in, OpsResult* out) override {
    return ops_->Parse(in, out);
  }

  std::string GetTypeHint() override { return ops_->GetTypeHint(); }

 private:
  std::unique_ptr<Operations> ops_;
};

// TypeInfo that opens a file according to some mode.
class FileTypeInfo : public TypeInfo {
 public:
  // TODO: set up cache of Operations objs..
  FileTypeInfo(std::unique_ptr<Operations> ops, OpenMode mode)
      : ops_(std::move(ops)), mode_(mode) {
    ARGPARSE_DCHECK(mode != kModeNoMode);
  }

  void Run(const std::string& in, OpsResult* out) override {
    return ops_->Open(in, mode_, out);
  }

  std::string GetTypeHint() override { return ops_->GetTypeHint(); }

 private:
  std::unique_ptr<Operations> ops_;
  OpenMode mode_;
};

// TypeInfo that runs user's callback.
class TypeCallbackInfo : public TypeInfo {
 public:
  explicit TypeCallbackInfo(std::unique_ptr<TypeCallback> cb)
      : type_callback_(std::move(cb)) {}

  void Run(const std::string& in, OpsResult* out) override {
    return type_callback_->Run(in, out);
  }

  std::string GetTypeHint() override { return type_callback_->GetTypeHint(); }

 private:
  std::unique_ptr<TypeCallback> type_callback_;
};

inline std::unique_ptr<TypeInfo> TypeInfo::CreateDefault(
    std::unique_ptr<Operations> ops) {
  return std::make_unique<DefaultTypeInfo>(std::move(ops));
}
inline std::unique_ptr<TypeInfo> TypeInfo::CreateFileType(
    std::unique_ptr<Operations> ops,
    OpenMode mode) {
  return std::make_unique<FileTypeInfo>(std::move(ops), mode);
}
// Invoke user's callback.
inline std::unique_ptr<TypeInfo> TypeInfo::CreateFromCallback(
    std::unique_ptr<TypeCallback> cb) {
  return std::make_unique<TypeCallbackInfo>(std::move(cb));
}

ActionKind StringToActions(const std::string& str);

class ArgumentFactoryImpl : public ArgumentFactory {
 public:
  void SetNames(std::unique_ptr<NamesInfo> info) override {
    ARGPARSE_DCHECK_F(!arg_, "SetNames should only be called once");
    arg_ = Argument::Create(std::move(info));
  }

  void SetDest(std::unique_ptr<DestInfo> info) override {
    arg_->SetDest(std::move(info));
  }

  void SetActionString(const char* str) override {
    action_kind_ = StringToActions(str);
  }

  void SetTypeOperations(std::unique_ptr<Operations> ops) override {
    arg_->SetType(TypeInfo::CreateDefault(std::move(ops)));
  }

  void SetTypeCallback(std::unique_ptr<TypeCallback> cb) override {
    arg_->SetType(TypeInfo::CreateFromCallback(std::move(cb)));
  }

  void SetTypeFileType(OpenMode mode) override { open_mode_ = mode; }

  void SetNumArgsFlag(char flag) override {
    arg_->SetNumArgs(NumArgsInfo::CreateFromFlag(flag));
  }

  void SetNumArgsNumber(int num) override {
    arg_->SetNumArgs(NumArgsInfo::CreateFromNum(num));
  }

  void SetConstValue(std::unique_ptr<Any> val) override {
    arg_->SetConstValue(std::move(val));
  }

  void SetDefaultValue(std::unique_ptr<Any> val) override {
    arg_->SetDefaultValue(std::move(val));
  }

  void SetMetaVar(std::string val) override {
    meta_var_ = std::make_unique<std::string>(std::move(val));
  }

  void SetRequired(bool val) override {
    ARGPARSE_DCHECK(arg_);
    arg_->SetRequired(val);
  }

  void SetHelp(std::string val) override {
    ARGPARSE_DCHECK(arg_);
    arg_->SetHelpDoc(std::move(val));
  }

  void SetActionCallback(std::unique_ptr<ActionCallback> cb) override {
    arg_->SetAction(ActionInfo::CreateFromCallback(std::move(cb)));
  }

  std::unique_ptr<Argument> Create() override;

 private:
  // Some options are directly fed into arg.
  std::unique_ptr<Argument> arg_;
  // If not given, use default from NamesInfo.
  std::unique_ptr<std::string> meta_var_;
  ActionKind action_kind_ = ActionKind::kNoAction;
  OpenMode open_mode_ = kModeNoMode;
};

template <typename T>
void ConvertResults(Result<T>* in, OpsResult* out) {
  out->has_error = in->has_error();
  if (out->has_error) {
    out->errmsg = in->release_error();
  } else if (in->has_value()) {
    out->value = MakeAny<T>(in->release_value());
  }
}

const char* OpsToString(OpsKind ops);
const char* TypeNameImpl(const std::type_info& type);
template <typename T>
const char* TypeName() {
  return TypeNameImpl(typeid(T));
}

template <OpsKind Ops, typename T, bool Supported = IsOpsSupported<Ops, T>{}>
struct OpsImpl;

template <OpsKind Ops, typename T>
struct OpsImpl<Ops, T, false> {
  template <typename... Args>
  static void Run(Args&&...) {
    ARGPARSE_CHECK_F(
        false,
        "Operation %s is not supported by type %s. Please specialize one of "
        "AppendTraits, ParseTraits and OpenTraits, or pass in a callback.",
        OpsToString(Ops), TypeName<T>());
  }
};

template <typename T>
struct OpsImpl<OpsKind::kStore, T, true> {
  static void Run(DestPtr dest, std::unique_ptr<Any> data) {
    if (data) {
      auto value = AnyCast<T>(std::move(data));
      dest.store(std::move_if_noexcept(value));
    }
  }
};

template <typename T>
struct OpsImpl<OpsKind::kStoreConst, T, true> {
  static void Run(DestPtr dest, const Any& data) {
    dest.store(AnyCast<T>(data));
  }
};

template <typename T>
struct OpsImpl<OpsKind::kAppend, T, true> {
  static void Run(DestPtr dest, std::unique_ptr<Any> data) {
    if (data) {
      auto* ptr = dest.load_ptr<T>();
      auto value = AnyCast<ValueTypeOf<T>>(std::move(data));
      AppendTraits<T>::Run(ptr, std::move_if_noexcept(value));
    }
  }
};

template <typename T>
struct OpsImpl<OpsKind::kAppendConst, T, true> {
  static void Run(DestPtr dest, const Any& data) {
    auto* ptr = dest.load_ptr<T>();
    auto value = AnyCast<ValueTypeOf<T>>(data);
    AppendTraits<T>::Run(ptr, value);
  }
};

template <typename T>
struct OpsImpl<OpsKind::kCount, T, true> {
  static void Run(DestPtr dest) {
    auto* ptr = dest.load_ptr<T>();
    ++(*ptr);
  }
};

template <typename T>
struct OpsImpl<OpsKind::kParse, T, true> {
  static void Run(const std::string& in, OpsResult* out) {
    Result<T> result;
    ParseTraits<T>::Run(in, &result);
    ConvertResults(&result, out);
  }
};

template <typename T>
struct OpsImpl<OpsKind::kOpen, T, true> {
  static void Run(const std::string& in, OpenMode mode, OpsResult* out) {
    Result<T> result;
    OpenTraits<T>::Run(in, mode, &result);
    ConvertResults(&result, out);
  }
};

template <typename T, std::size_t... OpsIndices>
bool OpsIsSupportedImpl(OpsKind ops, std::index_sequence<OpsIndices...>) {
  static constexpr std::size_t kMaxOps = sizeof...(OpsIndices);
  static constexpr bool kArray[kMaxOps] = {
      (IsOpsSupported<static_cast<OpsKind>(OpsIndices), T>{})...};
  auto index = std::size_t(ops);
  ARGPARSE_DCHECK(index < kMaxOps);
  return kArray[index];
}

template <typename T>
class OperationsImpl : public Operations {
 public:
  void Store(DestPtr dest, std::unique_ptr<Any> data) override {
    OpsImpl<OpsKind::kStore, T>::Run(dest, std::move(data));
  }
  void StoreConst(DestPtr dest, const Any& data) override {
    OpsImpl<OpsKind::kStoreConst, T>::Run(dest, data);
  }
  void Append(DestPtr dest, std::unique_ptr<Any> data) override {
    OpsImpl<OpsKind::kAppend, T>::Run(dest, std::move(data));
  }
  void AppendConst(DestPtr dest, const Any& data) override {
    OpsImpl<OpsKind::kAppendConst, T>::Run(dest, data);
  }
  void Count(DestPtr dest) override { OpsImpl<OpsKind::kCount, T>::Run(dest); }
  void Parse(const std::string& in, OpsResult* out) override {
    OpsImpl<OpsKind::kParse, T>::Run(in, out);
  }
  void Open(const std::string& in, OpenMode mode, OpsResult* out) override {
    OpsImpl<OpsKind::kOpen, T>::Run(in, mode, out);
  }
  bool IsSupported(OpsKind ops) override {
    return OpsIsSupportedImpl<T>(ops, std::make_index_sequence<kMaxOpsKind>{});
  }
  const char* GetTypeName() override { return TypeName<T>(); }
  std::string GetTypeHint() override { return TypeHint<T>(); }
  std::string FormatValue(const Any& val) override {
    return FormatTraits<T>::Run(AnyCast<T>(val));
  }
  const std::type_info& GetTypeInfo() override { return typeid(T); }
};

template <typename T>
std::unique_ptr<Operations> CreateOperations() {
  return std::make_unique<OperationsImpl<T>>();
}

template <typename T, bool = IsAppendSupported<T>{}>
struct CreateValueTypeOpsImpl;

template <typename T>
struct CreateValueTypeOpsImpl<T, false> {
  static std::unique_ptr<Operations> Run() { return nullptr; }
};
template <typename T>
struct CreateValueTypeOpsImpl<T, true> {
  static std::unique_ptr<Operations> Run() {
    return CreateOperations<ValueTypeOf<T>>();
  }
};

template <typename T>
class OpsFactoryImpl : public OpsFactory {
 public:
  std::unique_ptr<Operations> Create() override {
    return CreateOperations<T>();
  }
  std::unique_ptr<Operations> CreateValueTypeOps() override {
    return CreateValueTypeOpsImpl<T>::Run();
  }
};

template <typename T>
std::unique_ptr<OpsFactory> CreateOpsFactory() {
  return std::make_unique<OpsFactoryImpl<T>>();
}

template <typename T>
std::unique_ptr<DestInfo> DestInfo::CreateFromPtr(T* ptr) {
  ARGPARSE_CHECK_F(ptr, "Pointer passed to dest() must not be null.");
  return std::make_unique<DestInfoImpl>(DestPtr(ptr), CreateOpsFactory<T>());
}

// Holds all meta-info about an argument.
class ArgumentImpl : public Argument {
 public:
  explicit ArgumentImpl(std::unique_ptr<NamesInfo> names)
      : names_info_(std::move(names)) {}

  DestInfo* GetDest() override { return dest_info_.get(); }
  TypeInfo* GetType() override { return type_info_.get(); }
  ActionInfo* GetAction() override { return action_info_.get(); }
  const Any* GetConstValue() override { return const_value_.get(); }
  const Any* GetDefaultValue() override { return default_value_.get(); }
  const char* GetMetaVar() override { return meta_var_.c_str(); }
  ArgumentGroup* GetGroup() override { return group_; }
  NamesInfo* GetNamesInfo() override { return names_info_.get(); }
  bool IsRequired() override { return is_required_; }
  const char* GetHelpDoc() override { return help_doc_.c_str(); }
  void SetRequired(bool required) override { is_required_ = required; }
  void SetHelpDoc(std::string help_doc) override {
    help_doc_ = std::move(help_doc);
  }
  void SetMetaVar(std::string meta_var) override {
    meta_var_ = std::move(meta_var);
  }
  void SetDest(std::unique_ptr<DestInfo> info) override {
    ARGPARSE_DCHECK(info);
    dest_info_ = std::move(info);
  }
  void SetType(std::unique_ptr<TypeInfo> info) override {
    ARGPARSE_DCHECK(info);
    type_info_ = std::move(info);
  }
  void SetAction(std::unique_ptr<ActionInfo> info) override {
    ARGPARSE_DCHECK(info);
    action_info_ = std::move(info);
  }
  void SetConstValue(std::unique_ptr<Any> value) override {
    ARGPARSE_DCHECK(value);
    const_value_ = std::move(value);
  }
  void SetDefaultValue(std::unique_ptr<Any> value) override {
    ARGPARSE_DCHECK(value);
    default_value_ = std::move(value);
  }
  void SetGroup(ArgumentGroup* group) override {
    ARGPARSE_DCHECK(group);
    group_ = group;
  }
  void SetNumArgs(std::unique_ptr<NumArgsInfo> info) override {
    ARGPARSE_DCHECK(info);
    num_args_ = std::move(info);
  }

  bool Before(const Argument* that) const override {
    return CompareArguments(this, static_cast<const ArgumentImpl*>(that));
  }

 private:
  static bool CompareArguments(const ArgumentImpl* a, const ArgumentImpl* b);

  ArgumentGroup* group_ = nullptr;
  std::string help_doc_;
  std::string meta_var_;
  bool is_required_ = false;

  std::unique_ptr<NamesInfo> names_info_;
  std::unique_ptr<DestInfo> dest_info_;
  std::unique_ptr<ActionInfo> action_info_;
  std::unique_ptr<TypeInfo> type_info_;
  std::unique_ptr<NumArgsInfo> num_args_;
  std::unique_ptr<Any> const_value_;
  std::unique_ptr<Any> default_value_;
};

inline std::unique_ptr<Argument> Argument::Create(
    std::unique_ptr<NamesInfo> info) {
  ARGPARSE_DCHECK(info);
  return std::make_unique<ArgumentImpl>(std::move(info));
}

class ArgumentHolderImpl : public ArgumentHolder {
 public:
  ArgumentHolderImpl();

  ArgumentGroup* AddArgumentGroup(const char* header) override;

  void AddArgument(std::unique_ptr<Argument> arg) override {
    auto* group =
        arg->IsOption() ? GetDefaultOptionGroup() : GetDefaultPositionalGroup();
    return group->AddArgument(std::move(arg));
  }

  void ForEachArgument(std::function<void(Argument*)> callback) override {
    for (auto& arg : arguments_)
      callback(arg.get());
  }
  void ForEachGroup(std::function<void(ArgumentGroup*)> callback) override {
    for (auto& group : groups_)
      callback(group.get());
  }

  int GetArgumentCount() override { return arguments_.size(); }

  void SetListener(std::unique_ptr<Listener> listener) override {
    listener_ = std::move(listener);
  }

 private:
  enum GroupID {
    kOptionGroup = 0,
    kPositionalGroup = 1,
  };

  class GroupImpl;

  // Add an arg to a specific group.
  void AddArgumentToGroup(std::unique_ptr<Argument> arg, ArgumentGroup* group);
  ArgumentGroup* GetDefaultOptionGroup() const {
    return groups_[kOptionGroup].get();
  }
  ArgumentGroup* GetDefaultPositionalGroup() const {
    return groups_[kPositionalGroup].get();
  }

  bool CheckNamesConflict(const NamesInfo& names);

  std::unique_ptr<Listener> listener_;
  // Hold the storage of all args.
  std::vector<std::unique_ptr<Argument>> arguments_;
  std::vector<std::unique_ptr<ArgumentGroup>> groups_;
  // Conflicts checking.
  std::set<std::string> name_set_;
};

class ArgumentControllerImpl : public ArgumentController {
 public:
  explicit ArgumentControllerImpl(
      std::unique_ptr<ParserFactory> parser_factory);

  ArgumentHolder* GetMainHolder() override { return main_holder_.get(); }
  SubCommandHolder* GetSubCommandHolder() override {
    return subcmd_holder_.get();
  }

  void SetOptions(std::unique_ptr<OptionsInfo> info) override {
    SetDirty(true);
    options_info_ = std::move(info);
  }

 private:
  // Listen to events of argumentholder and subcommand holder.
  class ListenerImpl;

  void SetDirty(bool dirty) { dirty_ = dirty; }
  bool dirty() const { return dirty_; }
  Parser* GetParser() override {
    if (dirty() || !parser_) {
      SetDirty(false);
      parser_ = parser_factory_->CreateParser(nullptr);
    }
    return parser_.get();
  }

  bool dirty_ = false;
  std::unique_ptr<ParserFactory> parser_factory_;
  std::unique_ptr<Parser> parser_;
  std::unique_ptr<OptionsInfo> options_info_;
  std::unique_ptr<ArgumentHolder> main_holder_;
  std::unique_ptr<SubCommandHolder> subcmd_holder_;
};

class ArgumentControllerImpl::ListenerImpl : public ArgumentHolder::Listener,
                                             public SubCommandHolder::Listener {
 public:
  explicit ListenerImpl(ArgumentControllerImpl* impl) : impl_(impl) {}

 private:
  void MarkDirty() { impl_->SetDirty(true); }
  void OnAddArgument(Argument*) override { MarkDirty(); }
  void OnAddArgumentGroup(ArgumentGroup*) override { MarkDirty(); }
  void OnAddSubCommand(SubCommand*) override { MarkDirty(); }
  void OnAddSubCommandGroup(SubCommandGroup*) override { MarkDirty(); }

  ArgumentControllerImpl* impl_;
};

class SubCommandImpl : public SubCommand {
 public:
  ArgumentHolder* GetHolder() override { return holder_.get(); }
  SubCommandInfo* GetInfo() override { return info_.get(); }

  explicit SubCommandImpl(SubCommandGroup* group,
                          std::unique_ptr<SubCommandInfo> info);
  // : group_(group),
  //   info_(std::move(info)),
  //   holder_(new ArgumentHolderImpl()) {}

 private:
  SubCommandGroup* group_;
  // Option given by user.
  std::unique_ptr<SubCommandInfo> info_;
  std::unique_ptr<ArgumentHolder> holder_;
};

class SubCommandHolderImpl : public SubCommandHolder {
 public:
  void ForEachSubCommand(std::function<void(SubCommand*)> callback) override {
    for (auto& sub : subcmds_)
      callback(sub.get());
  }
  void ForEachSubCommandGroup(
      std::function<void(SubCommandGroup*)> callback) override {
    for (auto& group : groups_)
      callback(group.get());
  }

  SubCommandGroup* AddSubCommandGroup(
      std::unique_ptr<SubCommandGroupInfo> info) override {}

  void SetListener(std::unique_ptr<Listener> listener) override {
    listener_ = std::move(listener);
  }

 private:
  class GroupImpl;

  SubCommand* AddSubCommandToGroup(SubCommandGroup* group,
                                   std::unique_ptr<SubCommandInfo> info) {
    auto* sub = new SubCommandImpl(group, std::move(info));
    if (listener_)
      listener_->OnAddSubCommand(sub);
    subcmds_.emplace_back(sub);
    return sub;
  }

  std::unique_ptr<Listener> listener_;
  std::vector<std::unique_ptr<SubCommand>> subcmds_;
  std::vector<std::unique_ptr<SubCommandGroup>> groups_;
};

class SubCommandHolderImpl::GroupImpl : public SubCommandGroup {
 public:
  SubCommand* AddSubCommand(std::unique_ptr<SubCommandInfo> info) override {
    return impl_->AddSubCommandToGroup(this, std::move(info));
  }

 private:
  SubCommandHolderImpl* impl_;
};

template <typename T>
class TypeCallbackImpl : public TypeCallback {
 public:
  using CallbackType = std::function<TypeCallbackPrototype<T>>;
  explicit TypeCallbackImpl(CallbackType cb) : callback_(std::move(cb)) {}

  void Run(const std::string& in, OpsResult* out) override {
    Result<T> result;
    std::invoke(callback_, in, &result);
    ConvertResults(&result, out);
  }

  std::string GetTypeHint() override { return TypeHint<T>(); }

 private:
  CallbackType callback_;
};

// Provided by user's callable obj.
template <typename T, typename V>
class CustomActionCallback : public ActionCallback {
 public:
  using CallbackType = std::function<ActionCallbackPrototype<T, V>>;
  explicit CustomActionCallback(CallbackType cb) : callback_(std::move(cb)) {
    ARGPARSE_DCHECK(callback_);
  }

 private:
  void Run(DestPtr dest_ptr, std::unique_ptr<Any> data) override {
    Result<V> result(AnyCast<V>(std::move(data)));
    auto* obj = dest_ptr.template load_ptr<T>();
    std::invoke(callback_, obj, std::move(result));
  }

  CallbackType callback_;
};

template <typename Callback, typename T>
std::unique_ptr<TypeCallback> MakeTypeCallbackImpl(Callback&& cb,
                                                   TypeCallbackPrototype<T>*) {
  return std::make_unique<TypeCallbackImpl<T>>(std::forward<Callback>(cb));
}

template <typename Callback, typename T>
std::unique_ptr<TypeCallback> MakeTypeCallbackImpl(
    Callback&& cb,
    TypeCallbackPrototypeThrows<T>*) {
  return std::make_unique<TypeCallbackImpl<T>>(
      [cb](const std::string& in, Result<T>* out) {
        try {
          *out = std::invoke(cb, in);
        } catch (const ArgumentError& e) {
          out->set_error(e.what());
        }
      });
}

template <typename Callback>
std::unique_ptr<TypeCallback> MakeTypeCallback(Callback&& cb) {
  return MakeTypeCallbackImpl(std::forward<Callback>(cb),
                              (detail::function_signature_t<Callback>*)nullptr);
}

template <typename Callback, typename T, typename V>
std::unique_ptr<ActionCallback> MakeActionCallbackImpl(
    Callback&& cb,
    ActionCallbackPrototype<T, V>*) {
  return std::make_unique<CustomActionCallback<T, V>>(
      std::forward<Callback>(cb));
}

template <typename Callback>
std::unique_ptr<ActionCallback> MakeActionCallback(Callback&& cb) {
  return MakeActionCallbackImpl(
      std::forward<Callback>(cb),
      (detail::function_signature_t<Callback>*)nullptr);
}

class FileType {
 public:
  explicit FileType(const char* mode) : mode_(CharsToMode(mode)) {}
  explicit FileType(std::ios_base::openmode mode)
      : mode_(StreamModeToMode(mode)) {}
  OpenMode mode() const { return mode_; }

 private:
  OpenMode mode_;
};

bool IsValidPositionalName(const char* name, std::size_t len);

// A valid option name is long or short option name and not '--', '-'.
// This is only checked once and true for good.
bool IsValidOptionName(const char* name, std::size_t len);

// These two predicates must be called only when IsValidOptionName() holds.
inline bool IsLongOptionName(const char* name, std::size_t len) {
  ARGPARSE_DCHECK(IsValidOptionName(name, len));
  return len > 2;
}

inline bool IsShortOptionName(const char* name, std::size_t len) {
  ARGPARSE_DCHECK(IsValidOptionName(name, len));
  return len == 2;
}

inline std::string ToUpper(const std::string& in) {
  std::string out(in);
  std::transform(in.begin(), in.end(), out.begin(), ::toupper);
  return out;
}

struct Names {
  std::unique_ptr<NamesInfo> info;
  Names(const char* name);
  Names(std::initializer_list<const char*> names);
};

// TODO: remove this..
using ::argp;
using ::argp_error;
using ::argp_failure;
using ::argp_help;
using ::argp_parse;
using ::argp_parser_t;
using ::argp_program_bug_address;
using ::argp_program_version;
using ::argp_program_version_hook;
using ::argp_state;
using ::argp_state_help;
using ::argp_usage;
using ::error_t;
using ::program_invocation_name;
using ::program_invocation_short_name;

// All the data elements needs by argp.
// Created by Compiler and kept alive by Parser.
class GNUArgpContext {
 public:
  using ParserCallback = argp_parser_t;
  virtual ~GNUArgpContext() {}
  virtual void SetParserCallback(ParserCallback cb) = 0;
  virtual int GetParseFlags() = 0;
  virtual Argument* FindOption(int key) = 0;
  virtual Argument* FindPositional(int pos) = 0;
  virtual argp* GetArgpStruct() = 0;
};

// Fast lookup of arguments based on their key or position.
// This is generated by Compiler mostly by looking at the Arguments and Options.
class ArgpIndexesInfo {
 public:
  Argument* FindOption(int key) const;
  Argument* FindPositional(int pos) const;

  std::map<int, Argument*> optionals;
  std::vector<Argument*> positionals;
};

class GNUArgpCompiler {
 public:
  virtual ~GNUArgpCompiler() {}
  virtual std::unique_ptr<GNUArgpContext> Compile(
      std::unique_ptr<ParserFactory::Delegate> delegate) = 0;
};

// Compile Arguments to argp data and various things needed by the parser.
class ArgpCompiler {
 public:
  ArgpCompiler(ArgumentHolder* holder) : holder_(holder) { Initialize(); }

  void CompileOptions(std::vector<argp_option>* out);
  void CompileUsage(std::string* out);
  void CompileArgumentIndexes(ArgpIndexesInfo* out);

 private:
  void Initialize();
  void CompileGroup(ArgumentGroup* group, std::vector<argp_option>* out);
  void CompileArgument(Argument* arg, std::vector<argp_option>* out);
  void CompileUsageFor(Argument* arg, std::ostream& os);
  int FindGroup(ArgumentGroup* g) { return group_to_id_[g]; }
  int FindArgument(Argument* a) { return argument_to_id_[a]; }
  void InitGroup(ArgumentGroup* group);
  void InitArgument(Argument* arg);

  static constexpr unsigned kFirstArgumentKey = 128;

  ArgumentHolder* holder_;
  int next_arg_key_ = kFirstArgumentKey;
  int next_group_id_ = 1;
  HelpFormatPolicy policy_;
  std::map<Argument*, int> argument_to_id_;
  std::map<ArgumentGroup*, int> group_to_id_;
};

// This class focuses on parsing (efficiently).. Any other things are handled by
// Compiler..
class GNUArgpParser : public Parser {
 public:
  explicit GNUArgpParser(std::unique_ptr<GNUArgpContext> context)
      : context_(std::move(context)) {
    context_->SetParserCallback(&ParserCallbackImpl);
  }

  bool ParseKnownArgs(ArgArray args, std::vector<std::string>* rest) override {
    int flags = context_->GetParseFlags();
    if (rest)
      flags |= ARGP_NO_EXIT;
    int arg_index = 0;
    auto err = argp_parse(context_->GetArgpStruct(), args.argc(), args.argv(),
                          flags, &arg_index, this);
    if (rest) {
      for (int i = arg_index; i < args.argc(); ++i)
        rest->push_back(args[i]);
      return !err;
    }
    // Should not get here...
    return true;
  }

 private:
  // This is an internal class to communicate data/state between user's
  // callback.
  class Context;
  void RunCallback(Argument* arg, char* value, argp_state* state);

  error_t DoParse(int key, char* arg, argp_state* state);

  static error_t ParserCallbackImpl(int key, char* arg, argp_state* state) {
    auto* self = reinterpret_cast<GNUArgpParser*>(state->input);
    return self->DoParse(key, arg, state);
  }

  static char* HelpFilterCallbackImpl(int key, const char* text, void* input);

  // unsigned positional_count() const { return positional_count_; }

  static constexpr unsigned kSpecialKeyMask = 0x1000000;

  // std::unique_ptr<ArgpIndexesInfo> index_info_;
  std::unique_ptr<GNUArgpContext> context_;
  // int parser_flags_ = 0;
  // unsigned positional_count_ = 0;
  // argp argp_ = {};
  // std::string program_doc_;
  // std::string args_doc_;
  // std::vector<argp_option> argp_options_;
  // HelpFilterCallback help_filter_;
};

class GNUArgpParser::Context : public CallbackRunner::Delegate {
 public:
  Context(const Argument* argument, const char* value, argp_state* state);

  void OnCallbackError(const std::string& errmsg) override {
    return argp_error(state_, "error parsing argument: %s", errmsg.c_str());
  }

  void OnPrintUsage() override { return argp_usage(state_); }
  void OnPrintHelp() override {
    return argp_state_help(state_, stderr, help_flags_);
  }

  bool GetValue(std::string* out) override {
    if (has_value_) {
      *out = value_;
      return true;
    }
    return false;
  }

 private:
  const bool has_value_;
  const Argument* arg_;
  argp_state* state_;
  std::string value_;
  int help_flags_ = 0;
};

// struct ArgpData {
//   int parser_flags = 0;
//   argp argp_info = {};
//   std::string program_doc;
//   std::string args_doc;
//   std::vector<argp_option> argp_options;
// };

// Public flags user can use. These are corresponding to the ARGP_XXX flags
// passed to argp_parse().
enum Flags {
  kNoFlags = 0,            // The default.
  kNoHelp = ARGP_NO_HELP,  // Don't produce --help.
  kLongOnly = ARGP_LONG_ONLY,
  kNoExit = ARGP_NO_EXIT,
};

// Options to ArgumentParser constructor.
// TODO: rename to OptionsBuilder and typedef.
struct Options {
  // Only the most common options are listed in this list.
  Options() : info(new OptionsInfo) {}
  Options& version(const char* v) {
    info->program_version = v;
    return *this;
  }
  Options& version(ProgramVersionCallback callback) {
    info->program_version_callback = callback;
    return *this;
  }
  Options& description(const char* d) {
    info->description = d;
    return *this;
  }
  Options& after_doc(const char* a) {
    info->after_doc = a;
    return *this;
  }
  Options& domain(const char* d) {
    info->domain = d;
    return *this;
  }
  Options& email(const char* b) {
    info->email = b;
    return *this;
  }
  Options& flags(Flags f) {
    info->flags |= f;
    return *this;
  }
  Options& help_filter(HelpFilterCallback cb) {
    info->help_filter = std::move(cb);
    return *this;
  }

  std::unique_ptr<OptionsInfo> info;
};

class AddArgumentHelper;

class ArgumentBuilder {
 public:
  explicit ArgumentBuilder(Names names)
      : factory_(ArgumentFactory::CreateDefault()) {
    ARGPARSE_DCHECK(names.info);
    factory_->SetNames(std::move(names.info));
  }

  // TODO: Fix the typeinfo/actioninfo deduction.
  template <typename T>
  ArgumentBuilder& dest(T* ptr) {
    factory_->SetDest(DestInfo::CreateFromPtr(ptr));
    return *this;
  }
  ArgumentBuilder& action(const char* str) {
    factory_->SetActionString(str);
    return *this;
  }
  template <typename Callback>
  ArgumentBuilder& action(Callback&& cb) {
    factory_->SetActionCallback(MakeActionCallback(std::forward<Callback>(cb)));
    return *this;
  }
  template <typename Callback>
  ArgumentBuilder& type(Callback&& cb) {
    factory_->SetTypeCallback(MakeTypeCallback(std::forward<Callback>(cb)));
    return *this;
  }
  template <typename T>
  ArgumentBuilder& type() {
    factory_->SetTypeOperations(CreateOperations<T>());
    return *this;
  }
  ArgumentBuilder& type(FileType file_type) {
    factory_->SetTypeFileType(file_type.mode());
    return *this;
  }
  template <typename T>
  ArgumentBuilder& const_value(T&& val) {
    factory_->SetConstValue(MakeAny<T>(std::forward<T>(val)));
    return *this;
  }
  template <typename T>
  ArgumentBuilder& default_value(T&& val) {
    factory_->SetDefaultValue(MakeAny<T>(std::forward<T>(val)));
    return *this;
  }
  ArgumentBuilder& help(std::string val) {
    factory_->SetHelp(std::move(val));
    return *this;
  }
  ArgumentBuilder& required(bool val) {
    factory_->SetRequired(val);
    return *this;
  }
  ArgumentBuilder& meta_var(std::string val) {
    factory_->SetMetaVar(std::move(val));
    return *this;
  }
  ArgumentBuilder& nargs(int num) {
    factory_->SetNumArgsNumber(num);
    return *this;
  }
  ArgumentBuilder& nargs(char flag) {
    factory_->SetNumArgsFlag(flag);
    return *this;
  }

 private:
  friend class AddArgumentHelper;
  std::unique_ptr<Argument> CreateArgument() { return factory_->Create(); }

  std::unique_ptr<ArgumentFactory> factory_;
};

// Helper alias.
using argument = ArgumentBuilder;

// This is a helper that provides add_argument().
class AddArgumentHelper {
 public:
  void add(ArgumentBuilder& builder) {
    AddArgumentImpl(builder.CreateArgument());
  }
  virtual ~AddArgumentHelper() {}

 private:
  virtual void AddArgumentImpl(std::unique_ptr<Argument> arg) {}
};

class argument_group : public AddArgumentHelper {
 public:
  explicit argument_group(ArgumentGroup* group) : group_(group) {}

 private:
  void AddArgumentImpl(std::unique_ptr<Argument> arg) override {
    return group_->AddArgument(std::move(arg));
  }

  ArgumentGroup* group_;
};

// If we can do add_argument_group(), add_argument() is always possible.
class AddArgumentGroupHelper : public AddArgumentHelper {
 public:
  argument_group add_argument_group(const char* header) {
    ARGPARSE_DCHECK(header);
    return argument_group(AddArgumentGroupImpl(header));
  }

 private:
  virtual ArgumentGroup* AddArgumentGroupImpl(const char* header) = 0;
};

class SubParser : public AddArgumentGroupHelper {
 public:
  explicit SubParser(SubCommand* sub) : sub_(sub) {}

 private:
  void AddArgumentImpl(std::unique_ptr<Argument> arg) override {
    return sub_->GetHolder()->AddArgument(std::move(arg));
  }
  ArgumentGroup* AddArgumentGroupImpl(const char* header) override {
    return sub_->GetHolder()->AddArgumentGroup(header);
  }
  SubCommand* sub_;
};

class SubParserGroup;

// Support add(parser("something").aliases({...}).help("..."))
class parser {
 public:
  explicit parser(const char* name);
  parser& aliases(std::vector<std::string> als);
  parser& help(const char* h);

 private:
  friend class SubParserGroup;
  std::unique_ptr<SubCommandInfo> Release() {
    ARGPARSE_DCHECK(info_);
    return std::move(info_);
  }
  std::unique_ptr<SubCommandInfo> info_;
};

class SubParserGroup {
 public:
  explicit SubParserGroup(SubCommandGroup* group) : group_(group) {}

  // TODO: More precise signature.
  SubParser add_parser(const char* name,
                       const char* help = {},
                       std::vector<std::string> aliases = {}) {
    auto info =
        std::make_unique<SubCommandInfo>(name, help, std::move(aliases));
    auto* sub = group_->AddSubCommand(std::move(info));
    return SubParser(sub);
  }
  SubParser add(parser& p) {
    auto info = p.Release();
    auto* sub = group_->AddSubCommand(std::move(info));
    return SubParser(sub);
  }

 private:
  SubCommandGroup* group_;
};

// Support add(subparsers(...))
class subparsers {};

// Interface of ArgumentParser.
class MainParserHelper : public AddArgumentGroupHelper {
 public:
  void parse_args(int argc, const char** argv) {
    ParseArgsImpl(ArgArray(argc, argv), nullptr);
  }
  void parse_args(std::vector<const char*> args) {
    ParseArgsImpl(ArgArray(args), nullptr);
  }
  bool parse_known_args(int argc,
                        const char** argv,
                        std::vector<std::string>* out) {
    return ParseArgsImpl(ArgArray(argc, argv), out);
  }
  bool parse_known_args(std::vector<const char*> args,
                        std::vector<std::string>* out) {
    return ParseArgsImpl(args, out);
  }

  SubParserGroup add(subparsers& sub) {}
  // TODO: More precise signature.
  SubParserGroup add_subparsers() {
    return SubParserGroup(AddSubParsersImpl(nullptr));
  }

 private:
  virtual bool ParseArgsImpl(ArgArray args, std::vector<std::string>* out) = 0;
  virtual SubCommandGroup* AddSubParsersImpl(
      std::unique_ptr<SubCommandGroupInfo> info) = 0;
};

class ArgumentParser : public MainParserHelper {
 public:
  ArgumentParser() : controller_(ArgumentController::Create()) {}

  explicit ArgumentParser(Options options) : ArgumentParser() {
    if (options.info)
      controller_->SetOptions(std::move(options.info));
  }

 private:
  bool ParseArgsImpl(ArgArray args, std::vector<std::string>* out) override {
    ARGPARSE_DCHECK(out);
    return controller_->GetParser()->ParseKnownArgs(args, out);
  }
  void AddArgumentImpl(std::unique_ptr<Argument> arg) override {
    return controller_->GetMainHolder()->AddArgument(std::move(arg));
  }
  ArgumentGroup* AddArgumentGroupImpl(const char* header) override {
    return controller_->GetMainHolder()->AddArgumentGroup(header);
  }
  SubCommandGroup* AddSubParsersImpl(
      std::unique_ptr<SubCommandGroupInfo> info) override {
    return controller_->GetSubCommandHolder()->AddSubCommandGroup(
        std::move(info));
  }

  std::unique_ptr<ArgumentController> controller_;
};

}  // namespace argparse
