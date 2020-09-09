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

#define DCHECK(expr) assert(expr)
#define DCHECK2(expr, msg) assert(expr&& msg)

// Perform a runtime check for user's error.
#define CHECK_USER(expr, format, ...) \
  CheckUserError(bool(expr), {__LINE__, __FILE__}, (format), ##__VA_ARGS__)

namespace argparse {

class Argument;
class ArgumentGroup;

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
using std::FILE;

struct SourceLocation {
  int line;
  const char* filename;
};

void CheckUserError(bool cond, SourceLocation loc, const char* fmt, ...);

// Try to use move, fall back to copy.
template <typename T>
T MoveOrCopyImpl(T* val, std::true_type) {
  return std::move(*val);
}
template <typename T>
T MoveOrCopyImpl(T* val, std::false_type) {
  return *val;
}
template <typename T>
T MoveOrCopy(T* val) {
  static_assert(std::is_copy_constructible<T>{} ||
                std::is_move_constructible<T>{});
  return MoveOrCopyImpl(val, std::is_move_constructible<T>{});
}

// Result<T> handles user' returned value and error using a union.
template <typename T>
class Result {
 public:
  // Default is empty (!has_value && !has_error).
  Result() { DCHECK(empty()); }
  // To hold a value.
  explicit Result(T&& val) : data_(std::move(val)) { DCHECK(has_value()); }
  explicit Result(const T& val) : data_(val) { DCHECK(has_value()); }

  // For now just use default. If T can't be moved, will it still work?
  Result(Result&&) = default;
  Result& operator=(Result&&) = default;

  bool has_value() const { return data_.index() == kValueIndex; }
  bool has_error() const { return data_.index() == kErrorMsgIndex; }

  void set_error(const std::string& msg) {
    data_.template emplace<kErrorMsgIndex>(msg);
    DCHECK(has_error());
  }
  void set_error(std::string&& msg) {
    data_.template emplace<kErrorMsgIndex>(std::move(msg));
    DCHECK(has_error());
  }
  void set_value(const T& val) {
    data_.template emplace<kValueIndex>(val);
    DCHECK(has_value());
  }
  void set_value(T&& val) {
    data_.template emplace<kValueIndex>(std::move(val));
    DCHECK(has_value());
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
    DCHECK(has_error());
    // We know that std::string is moveable.
    return std::get<kErrorMsgIndex>(std::move(data_));
  }
  const std::string& get_error() const {
    DCHECK(has_error());
    return std::get<kErrorMsgIndex>(data_);
  }

  // Release the value, Keep in value state.
  T release_value() {
    DCHECK(has_value());
    // But we don't know T is moveable or not.
    return std::get<kValueIndex>(MoveOrCopy(&data_));
  }
  const T& get_value() const {
    DCHECK(has_value());
    return std::get<kValueIndex>(data_);
  }
  // Goes back to empty state.
  void reset() {
    data_.template emplace<kEmptyIndex>(EmptyType{});
    DCHECK(empty());
  }

 private:
  bool empty() const { return kEmptyIndex == data_.index(); }
  enum Indices {
    kEmptyIndex,
    kErrorMsgIndex,
    kValueIndex,
  };
  struct EmptyType {};
  std::variant<EmptyType, std::string, T> data_;
};

// Wrapper of argp_state.
class ArgpState {
 public:
  ArgpState(argp_state* state) : state_(state) {}
  void PrintHelp(FILE* file, unsigned flags) {
    argp_state_help(state_, file, flags);
  }
  void PrintUsage() { argp_usage(state_); }

  template <typename... Args>
  void ErrorF(const char* fmt, Args... args) {
    return argp_error(state_, fmt, args...);
  }
  void Error(const std::string& msg) { return ErrorF("%s", msg.c_str()); }

  template <typename... Args>
  void FailureF(int status, int errnum, const char* fmt, Args... args) {
    return argp_failure(state_, status, errnum, fmt, args...);
  }
  void Failure(int status, int errnum, const std::string& msg) {
    return FailureF(status, errnum, "%s", msg.c_str());
  }
  argp_state* operator->() { return state_; }

 private:
  argp_state* state_;
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

  T ReleaseValue() { return MoveOrCopy(&value_); }
  const T& value() const { return value_; }

  static AnyImpl* FromAny(Any* any) {
    DCHECK(any && any->GetType() == typeid(T));
    return static_cast<AnyImpl*>(any);
  }
  static const AnyImpl& FromAny(const Any& any) {
    DCHECK(any.GetType() == typeid(T));
    return static_cast<const AnyImpl&>(any);
  }

 private:
  T value_;
};

template <typename T>
std::unique_ptr<Any> MakeAny(T&& val) {
  return std::make_unique<AnyImpl<T>>(std::forward<T>(val));
}

// Use it like const Any& val = MakeAnyOnStack(0);
template <typename T>
AnyImpl<T> MakeAnyOnStack(T&& val) {
  return AnyImpl<T>(std::forward<T>(val));
}

template <typename T>
T AnyCast(std::unique_ptr<Any> any) {
  DCHECK(any);
  return AnyImpl<T>::FromAny(any.get())->ReleaseValue();
}

template <typename T>
T AnyCast(const Any& any) {
  return AnyImpl<T>::FromAny(any).value();
}

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
    CHECK_USER(os.good(), "error formatting type %s: std::ostream failed",
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

enum class Actions {
  kNoAction,
  kStore,
  kStoreConst,
  kStoreTrue,
  kStoreFalse,
  kAppend,
  kAppendConst,
  kPrintHelp,
  kPrintUsage,
  kCustom,
};

enum class Types {
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
  kParse,
  kOpen,
};

inline constexpr std::size_t kMaxOpsKind = std::size_t(OpsKind::kOpen) + 1;

// File open mode.
enum Mode {
  kModeNoMode = 0x0,
  kModeRead = 1,
  kModeWrite = 2,
  kModeAppend = 4,
  kModeTruncate = 8,
  kModeBinary = 16,
};

Mode CharsToMode(const char* str);
std::string ModeToChars(Mode mode);

Mode StreamModeToMode(std::ios_base::openmode stream_mode);
std::ios_base::openmode ModeToStreamMode(Mode m);

// This traits indicates whether T supports append operation and if it does,
// tells us how to do the append.
template <typename T>
struct AppendTraits {
  static constexpr void* Run = nullptr;
};

// Extracted the bool value from AppendTraits.
template <typename T>
using IsAppendSupported = std::bool_constant<bool(AppendTraits<T>::Run)>;

// Get the value-type for a appendable, only use it when IsAppendSupported<T>.
template <typename T>
using ValueTypeOf = typename AppendTraits<T>::ValueType;

// For user's types, specialize AppendTraits<>, and if your type is
// standard-compatible, inherits from DefaultAppendTraits<>.

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
struct IsOpsSupported<OpsKind::kParse, T>
    : std::bool_constant<bool(ParseTraits<T>::Run)> {};

template <typename T>
struct IsOpsSupported<OpsKind::kOpen, T>
    : std::bool_constant<bool(OpenTraits<T>::Run)> {};

class DestPtr {
 public:
  template <typename T>
  explicit DestPtr(T* ptr) : type_(typeid(T)), ptr_(ptr) {}
  DestPtr() = default;

  // Copy content to out.
  template <typename T>
  const T& load() const {
    DCHECK(type_ == typeid(T));
    return *reinterpret_cast<const T*>(ptr_);
  }
  template <typename T>
  void load(T* out) const {
    *out = load<T>();
  }

  template <typename T>
  T* load_ptr() const {
    DCHECK(type_ == typeid(T));
    return reinterpret_cast<T*>(ptr_);
  }

  template <typename T>
  void load_ptr(T** ptr_out) const {
    *ptr_out = load_ptr<T>();
  }

  template <typename T>
  void store(T&& val) {
    using Type = std::remove_reference_t<T>;
    DCHECK(type_ == typeid(Type));
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
  struct NoneType {};
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
  virtual void Store(DestPtr dest, std::unique_ptr<Any> data) = 0;
  virtual void StoreConst(DestPtr dest, const Any& data) = 0;
  virtual void Append(DestPtr dest, std::unique_ptr<Any> data) = 0;
  virtual void AppendConst(DestPtr dest, const Any& data) = 0;

  virtual void Parse(const std::string& in, OpsResult* out) = 0;
  virtual void Open(const std::string& in, Mode, OpsResult* out) = 0;
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

// Perform data conversion..
class TypeCallback {
 public:
  virtual ~TypeCallback() {}
  virtual void Run(const std::string& in, OpsResult* out) {}
};

class ActionCallback {
 public:
  virtual ~ActionCallback() {}
  virtual void Run(DestPtr dest, std::unique_ptr<Any> data) = 0;
};

class CallbackRunner {
 public:
  // Communicate with outside when callback is fired.
  class Delegate { // should be called Client??
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

struct NamesInfo {
  bool is_option = false;
  std::vector<std::string> long_names;
  std::vector<char> short_names;
  std::string meta_var;

  // Positional.
  explicit NamesInfo(std::string name);
  // Option.
  explicit NamesInfo(std::vector<const char*> names);
};

struct NumArgsInfo {

};

struct DestInfo {
  DestPtr dest_ptr;
  std::unique_ptr<OpsFactory> ops_factory;
  std::unique_ptr<Operations> ops;

  DestInfo(DestPtr d, std::unique_ptr<OpsFactory> f)
      : dest_ptr(d), ops_factory(std::move(f)) {
    ops = ops_factory->Create();
  }
};

struct ActionInfo {
  Actions action_code = Actions::kNoAction;
  std::unique_ptr<ActionCallback> callback;
  std::unique_ptr<Operations> ops;

  ActionInfo() = default;
  explicit ActionInfo(Actions a) : action_code(a) {}
  explicit ActionInfo(std::unique_ptr<ActionCallback> cb)
      : action_code(Actions::kCustom), callback(std::move(cb)) {}
};

struct TypeInfo {
  Types type_code = Types::kNothing;
  std::unique_ptr<Operations> ops;
  std::unique_ptr<TypeCallback> callback;
  Mode mode = kModeNoMode;

  TypeInfo() = default;
  explicit TypeInfo(Mode m) : type_code(Types::kOpen), mode(m) {}
  explicit TypeInfo(std::unique_ptr<TypeCallback> cb)
      : type_code(Types::kCustom), callback(std::move(cb)) {}
  explicit TypeInfo(std::unique_ptr<Operations> ops)
      : type_code(Types::kParse), ops(std::move(ops)) {}
};

// Keep all info needed for running callback.
// This initializes an Argument.
class ArgumentInitializer {
 public:
  virtual ~ArgumentInitializer() {}

  virtual void SetRequired(bool required) = 0;
  virtual void SetHelpDoc(std::string help_doc) = 0;
  virtual void SetMetaVar(std::string meta_var) = 0;

  virtual void SetDest(std::unique_ptr<DestInfo> dest) = 0;
  virtual void SetType(std::unique_ptr<TypeInfo> info) = 0;
  virtual void SetAction(std::unique_ptr<ActionInfo> info) = 0;
  virtual void SetConstValue(std::unique_ptr<Any> value) = 0;
  virtual void SetDefaultValue(std::unique_ptr<Any> value) = 0;
};

enum class HelpFormatPolicy {
  kDefault,           // add nothing.
  kTypeHint,          // add (type: <type-hint>) to help doc.
  kDefaultValueHint,  // add (default: <default-value>) to help doc.
};

class Argument {
 public:
  // The initialization process of an Argument is:
  // 1. contructor is called, names and is_option is determined.
  // 2. ArgumentInitializer is created and its methods are called.
  // Different properties of an arg are set.
  // 3. Initialize() is called, this validates various aspects of an arg and
  // prepares it for running callbacks.
  // This initialization process can only happen once for an arg.
  virtual std::unique_ptr<ArgumentInitializer> CreateInitializer() = 0;
  virtual void Initialize(HelpFormatPolicy policy) = 0;
  virtual CallbackRunner* GetCallbackRunner() = 0;
  virtual const char* GetDoc() = 0;
  virtual bool IsRequired() = 0;
  virtual ArgumentGroup* GetGroup() = 0;

  // virtual int GetKey() const = 0;
  // virtual void FormatArgsDoc(std::ostream& os) const = 0;
  // virtual void CompileToArgpOptions(
  //     std::vector<argp_option>* options) const = 0;
  virtual bool Before(const Argument* that) const = 0;
  virtual const NamesInfo* GetNamesInfo() = 0;
  bool IsOption() { return GetNamesInfo()->is_option; }
  // const char* GetMetaVar() {
  //   const auto& mv = GetNamesInfo()->meta_var;
  //   return mv.empty() ? nullptr : mv.c_str();
  // }
  // const char * GetName() {
    
  // }

  virtual ~Argument() {}
};

// Return value of help filter function.
enum class HelpFilterResult {
  kKeep,
  kDrop,
  kReplace,
};

using HelpFilterCallback =
    std::function<HelpFilterResult(const Argument&, std::string* text)>;

using ProgramVersionCallback = void (*)(std::FILE*, argp_state*);

class ArgArray {
 public:
  ArgArray(int argc, const char** argv)
      : argc_(argc), argv_(const_cast<char**>(argv)) {}
  ArgArray(std::vector<const char*>* v) : ArgArray(v->size(), v->data()) {}
  // ArgArray(std::initializer_list<const char*> list)
  //     : ArgArray(list.size(), list.begin()) {}

  int argc() const { return argc_; }
  std::size_t size() const { return argc(); }

  char** argv() const { return argv_; }
  char* operator[](std::size_t i) {
    DCHECK(i < argc());
    return argv()[i];
  }

 private:
  int argc_;
  char** argv_;
};

// TODO: change this..
class ArgpParser {
 public:
  class Delegate {
   public:
    virtual Argument* FindOptionalArgument(int key) = 0;
    virtual Argument* FindPositionalArgument(int index) = 0;
    virtual std::size_t PositionalArgumentCount() = 0;
    virtual void CompileToArgpOptions(std::vector<argp_option>* options) = 0;
    virtual void GenerateArgsDoc(std::string* args_doc) = 0;
    virtual ~Delegate() {}
  };

  struct Options {
    int flags = 0;
    const char* program_version = {};
    const char* description = {};
    const char* after_doc = {};
    const char* domain = {};
    const char* email = {};
    ProgramVersionCallback program_version_callback;
    HelpFilterCallback help_filter;
  };

  // Initialize from a few options (user's options).
  virtual void Init(const Options& options) = 0;
  // Parse args, exit on errors.
  virtual void ParseArgs(ArgArray args) = 0;
  // Parse args, collect unknown args into rest, don't exit, report error via
  // Status.
  virtual bool ParseKnownArgs(ArgArray args, std::vector<std::string>* rest) {
    ParseArgs(args);
    return true;
  }
  virtual ~ArgpParser() {}
  static std::unique_ptr<ArgpParser> Create(Delegate* delegate);
};

template <typename T>
class PointerNullIterator {
 public:
  using value_type = T*;
  // Return the next value, or null if no next.
  virtual value_type Next() = 0;
  virtual ~PointerNullIterator() {}
};

class ArgumentGroup {
 public:
  virtual ~ArgumentGroup() {}
  virtual const char* GetHeader() = 0;
  // Visit each arg.
  virtual void ForEachArgument(std::function<void(Argument*)> callback) {}
  // Add an arg to this group.
  virtual Argument* AddArgument(std::unique_ptr<NamesInfo> names) = 0;
  virtual int GetArgumentCount() = 0;
};

// TODO: add lookup methods and iterate methods...
class ArgumentHolder {
 public:
  virtual ArgumentGroup* GetDefaultOptionGroup() = 0;
  virtual ArgumentGroup* GetDefaultPositionalGroup() = 0;
  virtual ArgumentGroup* AddArgumentGroup(const char* header) = 0;

  // Helper method to add arg to default group.
  Argument* AddArgument(std::unique_ptr<NamesInfo> names) {
    auto* group = names->is_option ? GetDefaultOptionGroup()
                                   : GetDefaultPositionalGroup();
    return group->AddArgument(std::move(names));
  }

  virtual void ForEachArgument(std::function<void(Argument*)> callback) = 0;
  virtual void ForEachGroup(std::function<void(ArgumentGroup*)> callback) = 0;
  virtual int GetArgumentCount() = 0;
  virtual ~ArgumentHolder() {}
};

struct OptionsInfo {
  int flags = 0;
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
  virtual bool ParseKnownArgs(ArgArray args, std::vector<std::string>* out) = 0;
};

class ParserFactory {
 public:
  // Interaction when creating parser.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual std::unique_ptr<OptionsInfo> GetOptions() = 0;
  };
  virtual ~ParserFactory() {}
  virtual std::unique_ptr<Parser> CreateParser(
      std::unique_ptr<Delegate> delegate) = 0;
};

// Combination of Holder and Parser. ArgumentParser should be impl'ed in terms
// of this.
class ArgumentController {
 public:
  virtual ~ArgumentController() {}
  // Parse args, if rest is null, exit on error. Otherwise put unknown ones into
  // rest and return status code.
  virtual bool ParseKnownArgs(ArgArray args,
                              std::vector<std::string>* rest) = 0;
  // Add arg to main holder.
  virtual Argument* AddArgument(std::unique_ptr<NamesInfo> names) = 0;
  // Add group to main holder.
  virtual ArgumentGroup* AddArgumentGroup(const char* header) = 0;
  virtual ArgumentHolder* GetMainHolder() = 0;
  virtual void SetOptions(std::unique_ptr<OptionsInfo> info) = 0;
  static std::unique_ptr<ArgumentController> Create();
};

// End of interfaces. Begin of Impls.

template <typename T>
void ConvertResults(Result<T>* in, OpsResult* out) {
  out->has_error = in->has_error();
  if (out->has_error) {
    out->errmsg = in->release_error();
  } else if (in->has_value()) {
    out->value = MakeAny(in->release_value());
  }
}

template <OpsKind Ops, typename T, bool Supported = IsOpsSupported<Ops, T>{}>
struct OpsImpl;

const char* OpsToString(OpsKind ops);

const char* TypeNameImpl(const std::type_info& type);

template <typename T>
const char* TypeName() {
  return TypeNameImpl(typeid(T));
}

template <OpsKind Ops, typename T>
struct OpsImpl<Ops, T, false> {
  template <typename... Args>
  static void Run(Args&&...) {
    CHECK_USER(
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
      dest.store(MoveOrCopy(&value));
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
      AppendTraits<T>::Run(ptr, MoveOrCopy(&value));
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
struct OpsImpl<OpsKind::kParse, T, true> {
  static void Run(const std::string& in, OpsResult* out) {
    Result<T> result;
    ParseTraits<T>::Run(in, &result);
    ConvertResults(&result, out);
  }
};

template <typename T>
struct OpsImpl<OpsKind::kOpen, T, true> {
  static void Run(const std::string& in, Mode mode, OpsResult* out) {
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
  DCHECK(index < kMaxOps);
  return kArray[index];
}

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
  void Parse(const std::string& in, OpsResult* out) override {
    OpsImpl<OpsKind::kParse, T>::Run(in, out);
  }
  void Open(const std::string& in, Mode mode, OpsResult* out) override {
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
std::unique_ptr<OpsFactory> CreateOperationsFactory() {
  return std::make_unique<OpsFactoryImpl<T>>();
}

// Holds all meta-info about an argument.
class ArgumentImpl : public Argument {
 public:
  ArgumentImpl(std::unique_ptr<NamesInfo> names, ArgumentGroup* group);

  std::unique_ptr<ArgumentInitializer> CreateInitializer() override;
  ArgumentGroup* GetGroup() override { return group_ptr_; }
  const NamesInfo* GetNamesInfo() override { return names_info_.get(); }
  bool IsRequired() override { return is_required(); }
  bool is_option() const { return names_info_->is_option; }
  bool is_required() const { return is_required_; }

  const std::string& help_doc() const { return help_doc_; }
  const char* GetDoc() override { return doc(); }
  const char* doc() const {
    return help_doc_.empty() ? nullptr : help_doc_.c_str();
  }

  const std::string& meta_var() const { return names_info_->meta_var; }
  const char* arg() const {
    DCHECK(!meta_var().empty());
    return meta_var().c_str();
  }

  // TODO: split the long_names, short_names from Names into name, key and
  // alias.
  const std::vector<std::string>& long_names() const {
    return names_info_->long_names;
  }
  const std::vector<char>& short_names() const {
    return names_info_->short_names;
  }

  const char* name() const {
    return long_names().empty() ? nullptr : long_names()[0].c_str();
  }

  CallbackRunner* GetCallbackRunner() override;

  // [--name|-n|-whatever=[value]] or output
  // void FormatArgsDoc(std::ostream& os) const override;

  void Initialize(HelpFormatPolicy policy) override;
  void ProcessHelpFormatPolicy(HelpFormatPolicy policy);

  // void CompileToArgpOptions(std::vector<argp_option>* options) const
  // override;

  bool Before(const Argument* that) const override {
    return CompareArguments(this, static_cast<const ArgumentImpl*>(that));
  }

 private:
  // enum Keys {
  //   kKeyForNothing = 0,
  //   kKeyForPositional = -1,
  // };

  class InitializerImpl;
  class CallbackInfo;

  static bool CompareArguments(const ArgumentImpl* a, const ArgumentImpl* b);

  // int key_ = kKeyForNothing;
  // int group_ = 0;
  std::unique_ptr<NamesInfo> names_info_;
  ArgumentGroup* group_ptr_;
  std::string help_doc_;
  bool is_required_ = false;
  std::unique_ptr<CallbackInfo> callback_info_;
};

// Callback relative things are put into this class.
class ArgumentImpl::CallbackInfo : public CallbackRunner {
 public:
  void Initialize();
  void FormatTypeHint(std::ostream& os) const;
  void FormatDefaultValue(std::ostream& os) const;

 private:
  friend class ArgumentImpl::InitializerImpl;

  void RunCallback(std::unique_ptr<Delegate> delegate) override;
  void InitAction();
  void InitType();
  void InitDefaultValue();
  void RunAction(std::unique_ptr<Any> data, Delegate*);
  void RunType(const std::string& in, OpsResult* out);
  // Helpers:
  const Any& const_value() const {
    DCHECK(const_value_);
    return *const_value_;
  }
  const DestPtr& dest_ptr() const {
    DCHECK(dest_info_);
    return dest_info_->dest_ptr;
  }

  std::unique_ptr<DestInfo> dest_info_;
  std::unique_ptr<ActionInfo> action_info_;
  std::unique_ptr<TypeInfo> type_info_;
  std::unique_ptr<NumArgsInfo> num_args_;
  std::unique_ptr<Any> const_value_;
  std::unique_ptr<Any> default_value_;
};

template <typename T>
class CustomTypeCallback : public TypeCallback {
 public:
  using CallbackType = std::function<TypeCallbackPrototype<T>>;
  explicit CustomTypeCallback(CallbackType cb) : callback_(std::move(cb)) {}

  void Run(const std::string& in, OpsResult* out) override {
    Result<T> result;
    std::invoke(callback_, in, &result);
    ConvertResults(&result, out);
  }

 private:
  CallbackType callback_;
};

// Provided by user's callable obj.
template <typename T, typename V>
class CustomActionCallback : public ActionCallback {
 public:
  using CallbackType = std::function<ActionCallbackPrototype<T, V>>;
  explicit CustomActionCallback(CallbackType cb) : callback_(std::move(cb)) {
    DCHECK(callback_);
  }

 private:
  void Run(DestPtr dest, std::unique_ptr<Any> data) override {
    Result<V> result(AnyCast<V>(std::move(data)));
    // UnwrapAny(std::move(data), &result);
    auto* obj = dest.template load_ptr<T>();
    std::invoke(callback_, obj, std::move(result));
  }

  CallbackType callback_;
};

template <typename Callback, typename T>
std::unique_ptr<TypeCallback> CreateTypeCallbackImpl(
    Callback&& cb,
    TypeCallbackPrototype<T>*) {
  return std::make_unique<CustomTypeCallback<T>>(std::forward<Callback>(cb));
}

template <typename Callback, typename T>
std::unique_ptr<TypeCallback> CreateTypeCallbackImpl(
    Callback&& cb,
    TypeCallbackPrototypeThrows<T>*) {
  return std::make_unique<CustomTypeCallback<T>>(
      [cb](const std::string& in, Result<T>* out) {
        try {
          *out = std::invoke(cb, in);
        } catch (const ArgumentError& e) {
          out->set_error(e.what());
        }
      });
}

template <typename Callback>
std::unique_ptr<TypeCallback> CreateTypeCallback(Callback&& cb) {
  return CreateTypeCallbackImpl(
      std::forward<Callback>(cb),
      (detail::function_signature_t<Callback>*)nullptr);
}

template <typename Callback, typename T, typename V>
std::unique_ptr<ActionCallback> CreateActionCallbackImpl(
    Callback&& cb,
    ActionCallbackPrototype<T, V>*) {
  return std::make_unique<CustomActionCallback<T, V>>(
      std::forward<Callback>(cb));
}

template <typename Callback>
std::unique_ptr<ActionCallback> CreateActionCallback(Callback&& cb) {
  return CreateActionCallbackImpl(
      std::forward<Callback>(cb),
      (detail::function_signature_t<Callback>*)nullptr);
}

class FileType {
 public:
  explicit FileType(const char* mode) : mode_(CharsToMode(mode)) {}
  explicit FileType(std::ios_base::openmode mode)
      : mode_(StreamModeToMode(mode)) {}
  Mode mode() const { return mode_; }

 private:
  Mode mode_;
};

struct Type {
  std::unique_ptr<TypeInfo> info;

  Type() = default;
  Type(Type&&) = default;

  // To explicitly request a type, use Type<double>().
  template <typename T>
  Type() : info(new TypeInfo(CreateOperations<T>())) {}

  // Use it to provide your own TypeCallback impl.
  Type(TypeCallback* cb)
      : info(new TypeInfo(std::unique_ptr<TypeCallback>(cb))) {}

  // Use FileType("rw") to request opening a file to read/write.
  Type(FileType file_type) : info(new TypeInfo(file_type.mode())) {}

  template <typename Callback,
            typename = std::enable_if_t<detail::is_callback<Callback>{}>>
  Type(Callback&& cb)
      : info(new TypeInfo(CreateTypeCallback(std::forward<Callback>(cb)))) {}
};

Actions StringToActions(const std::string& str);

struct Action {
  std::unique_ptr<ActionInfo> info;

  Action() = default;
  Action(Action&&) = default;

  // action("store_true").
  Action(const char* action_string)
      : info(new ActionInfo(StringToActions(action_string))) {}

  // action(new MyAction()).
  Action(ActionCallback* cb)
      : info(new ActionInfo(std::unique_ptr<ActionCallback>(cb))) {}

  // action([](T* out, Result<T> in) {})
  template <typename Callback,
            typename = std::enable_if_t<detail::is_callback<Callback>{}>>
  Action(Callback&& cb)
      : info(new ActionInfo(CreateActionCallback(std::forward<Callback>(cb)))) {
  }
};

struct Dest {
  std::unique_ptr<DestInfo> info;

  Dest() = default;
  template <typename T>
  Dest(T* ptr)
      : info(new DestInfo(DestPtr(ptr), CreateOperationsFactory<T>())) {
    CHECK_USER(ptr, "Pointer passed to dest() must not be null.");
  }
};

struct AnyValue {
  std::unique_ptr<Any> data;
  AnyValue(AnyValue&&) = delete;

  template <typename T>
  AnyValue(T&& val) : data(MakeAny(std::forward<T>(val))) {}
};

bool IsValidPositionalName(const char* name, std::size_t len);

// A valid option name is long or short option name and not '--', '-'.
// This is only checked once and true for good.
bool IsValidOptionName(const char* name, std::size_t len);

// These two predicates must be called only when IsValidOptionName() holds.
inline bool IsLongOptionName(const char* name, std::size_t len) {
  DCHECK(IsValidOptionName(name, len));
  return len > 2;
}

inline bool IsShortOptionName(const char* name, std::size_t len) {
  DCHECK(IsValidOptionName(name, len));
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

class ArgumentImpl::InitializerImpl : public ArgumentInitializer {
 public:
  InitializerImpl(ArgumentImpl* impl, CallbackInfo* cb_info)
      : impl_(impl), cb_info_(cb_info) {}

  void SetRequired(bool required) override { impl_->is_required_ = required; }
  void SetHelpDoc(std::string help_doc) override {
    impl_->help_doc_ = std::move(help_doc);
  }
  void SetMetaVar(std::string meta_var) override {
    impl_->names_info_->meta_var = std::move(meta_var);
  }
  void SetDest(std::unique_ptr<DestInfo> info) override {
    DCHECK(info);
    cb_info_->dest_info_ = std::move(info);
  }
  void SetType(std::unique_ptr<TypeInfo> info) override {
    DCHECK(info);
    cb_info_->type_info_ = std::move(info);
  }
  void SetAction(std::unique_ptr<ActionInfo> info) override {
    DCHECK(info);
    cb_info_->action_info_ = std::move(info);
  }
  void SetConstValue(std::unique_ptr<Any> value) override {
    DCHECK(value);
    cb_info_->const_value_ = std::move(value);
  }
  void SetDefaultValue(std::unique_ptr<Any> value) override {
    DCHECK(value);
    cb_info_->default_value_ = std::move(value);
  }

 private:
  ArgumentImpl* impl_;
  CallbackInfo* cb_info_;
};

inline std::unique_ptr<ArgumentInitializer> ArgumentImpl::CreateInitializer() {
  DCHECK(!callback_info_);
  callback_info_.reset(new CallbackInfo);
  return std::make_unique<InitializerImpl>(this, callback_info_.get());
}

class ArgumentBuilder {
 public:
  explicit ArgumentBuilder(std::unique_ptr<ArgumentInitializer> init)
      : init_(std::move(init)) {}

  ArgumentBuilder& dest(Dest d) {
    if (d.info)
      init_->SetDest(std::move(d.info));
    return *this;
  }
  ArgumentBuilder& action(Action a) {
    if (a.info)
      init_->SetAction(std::move(a.info));
    return *this;
  }
  ArgumentBuilder& type(Type t) {
    if (t.info)
      init_->SetType(std::move(t.info));
    return *this;
  }
  ArgumentBuilder& const_value(AnyValue val) {
    init_->SetConstValue(std::move(val.data));
    return *this;
  }
  ArgumentBuilder& default_value(AnyValue val) {
    init_->SetDefaultValue(std::move(val.data));
    return *this;
  }
  ArgumentBuilder& help(const char* h) {
    if (h)
      init_->SetHelpDoc(h);
    return *this;
  }
  ArgumentBuilder& required(bool b) {
    init_->SetRequired(b);
    return *this;
  }
  ArgumentBuilder& meta_var(const char* v) {
    if (v)
      init_->SetMetaVar(v);
    return *this;
  }

 private:
  std::unique_ptr<ArgumentInitializer> init_;
};

// This is an interface that provides add_argument() and other common things.
class ArgumentContainer {
 public:
  ArgumentBuilder add_argument(Names names,
                               Dest dest = {},
                               const char* help = {},
                               Type type = {},
                               Action action = {}) {
    DCHECK(names.info);
    auto* arg = AddArgument(std::move(names.info));
    ArgumentBuilder builder(arg->CreateInitializer());
    builder.dest(std::move(dest))
        .help(help)
        .type(std::move(type))
        .action(std::move(action));
    return builder;
  }

  virtual ~ArgumentContainer() {}

 private:
  virtual Argument* AddArgument(std::unique_ptr<NamesInfo> names) = 0;
};

inline void PrintArgpOptionArray(const std::vector<argp_option>& options) {
  for (const auto& opt : options) {
    printf("name=%s, key=%d, arg=%s, doc=%s, group=%d\n", opt.name, opt.key,
           opt.arg, opt.doc, opt.group);
  }
}

class ArgumentGroupProxy : public ArgumentContainer {
 public:
  ArgumentGroupProxy(ArgumentGroup* group) : group_(group) {}

 private:
  Argument* AddArgument(std::unique_ptr<NamesInfo> names) override {
    return group_->AddArgument(std::move(names));
  }

  ArgumentGroup* group_;
};

// TODO: this shouldn't inherit ArgumentContainer as it is a public interface.
class ArgumentHolderImpl : public ArgumentHolder {
 public:
  ArgumentHolderImpl();

  ArgumentGroup* AddArgumentGroup(const char* header) override;

  void ForEachArgument(std::function<void(Argument*)> callback) override {
    for (ArgumentImpl& arg : arguments_)
      callback(&arg);
  }
  void ForEachGroup(std::function<void(ArgumentGroup*)> callback) override {
    for (auto& group : groups_)
      callback(group.get());
  }

  ArgumentGroup* GetDefaultOptionGroup() override {
    return groups_[kOptionGroup].get();
  }
  ArgumentGroup* GetDefaultPositionalGroup() override {
    return groups_[kPositionalGroup].get();
  }

  int GetArgumentCount() override { return arguments_.size(); }

 private:
  // Add an arg to a specific group.
  Argument* AddArgumentToGroup(std::unique_ptr<NamesInfo> names, ArgumentGroup* group);

  enum GroupID {
    kOptionGroup = 0,
    kPositionalGroup = 1,
  };

  // ArgpParser::Delegate:
  // If there is a group, but it has no member, it will not be added to
  // argp_options. This class manages the logic above. It also frees the
  // Argument class from managing groups as well as option and positional.
  class GroupImpl;

  bool CheckNamesConflict(const NamesInfo& names);

  static constexpr unsigned kFirstArgumentKey = 128;

  // We have to explicitly manage group_id (instead of using 0 to inherit the
  // gid from the preivous entry) since the user can add option and positionals
  // in any order. Automatical inheriting gid will mess up.
  // unsigned next_group_id_ = kFirstUserGroup;
  // unsigned next_key_ = kFirstArgumentKey;
  // Control what extra info appear in the help doc.
  HelpFormatPolicy help_format_policy_ = HelpFormatPolicy::kDefault;
  // Hold the storage of all args.
  std::list<ArgumentImpl> arguments_;
  std::vector<std::unique_ptr<ArgumentGroup>> groups_;
  // Conflicts checking.
  std::set<std::string> name_set_;
};

class ArgumentHolderImpl::GroupImpl : public ArgumentGroup {
 public:
  GroupImpl(ArgumentHolderImpl* holder, const char* header)
      : holder_(holder), header_(header) {
    DCHECK(header_.size());
    if (header_.back() != ':')
      header_.push_back(':');
  }

  Argument* AddArgument(std::unique_ptr<NamesInfo> names) override {
    ++members_;
    return holder_->AddArgumentToGroup(std::move(names), this);
  }
  const char* GetHeader() override { return header_.c_str(); }

  int GetArgumentCount() override { return members_; }

 private:
  ArgumentHolderImpl* holder_;
  std::string header_;  // the text provided by user plus a ':'.
  int members_ = 0;
};

// This handles the argp_parser_t function and provide a bunch of context during
// the parsing.
class ArgpParserImpl : public ArgpParser {
 public:
  // When this is constructed, Delegate must have been added options.
  explicit ArgpParserImpl(ArgpParser::Delegate* delegate);

  void Init(const Options& options) override;
  // If any error happened, just exit the program. No need to check for return
  // value.
  void ParseArgs(ArgArray args) override;
  bool ParseKnownArgs(ArgArray args, std::vector<std::string>* rest) override;

 private:
  void set_doc(const char* doc) { argp_.doc = doc; }
  void set_argp_domain(const char* domain) { argp_.argp_domain = domain; }
  void set_args_doc(const char* args_doc) { argp_.args_doc = args_doc; }
  void AddFlags(int flags) { parser_flags_ |= flags; }

  // This is an internal class to communicate data/state between user's
  // callback.
  class Context;
  void RunCallback(Argument* arg, char* value, ArgpState state);

  error_t DoParse(int key, char* arg, ArgpState state);

  static error_t ArgpParserCallbackImpl(int key, char* arg, argp_state* state) {
    auto* self = reinterpret_cast<ArgpParserImpl*>(state->input);
    return self->DoParse(key, arg, state);
  }

  static char* ArgpHelpFilterCallbackImpl(int key,
                                          const char* text,
                                          void* input);

  unsigned positional_count() const { return positional_count_; }

  static constexpr unsigned kSpecialKeyMask = 0x1000000;

  int parser_flags_ = 0;
  unsigned positional_count_ = 0;
  ArgpParser::Delegate* delegate_;
  argp argp_ = {};
  std::string program_doc_;
  std::string args_doc_;
  std::vector<argp_option> argp_options_;
  HelpFilterCallback help_filter_;
};

class ArgpParserImpl::Context : public CallbackRunner::Delegate {
 public:
  Context(const Argument* argument, const char* value, ArgpState state);

  void OnCallbackError(const std::string& errmsg) override {
    return state_.ErrorF("error parsing argument: %s", errmsg.c_str());
  }

  void OnPrintUsage() override { return state_.PrintUsage(); }
  void OnPrintHelp() override { return state_.PrintHelp(stderr, help_flags_); }
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
  ArgpState state_;
  std::string value_;
  int help_flags_ = 0;
};

class ArgumentControllerImpl : public ArgumentController {
 public:
  explicit ArgumentControllerImpl(
      std::unique_ptr<ParserFactory> parser_factory);

  ArgumentHolder* GetMainHolder() override { return main_holder_.get(); }

  Argument* AddArgument(std::unique_ptr<NamesInfo> names) override {
    SetDirty(true);
    return GetMainHolder()->AddArgument(std::move(names));
  }

  ArgumentGroup* AddArgumentGroup(const char* header) override {
    SetDirty(true);
    return GetMainHolder()->AddArgumentGroup(header);
  }

  void SetOptions(std::unique_ptr<OptionsInfo> info) override {
    SetDirty(true);
    options_info_ = std::move(info);
  }

  bool ParseKnownArgs(ArgArray args, std::vector<std::string>* rest) override {
    return GetParser()->ParseKnownArgs(args, rest);
  }

 private:
  void SetDirty(bool dirty) { dirty_ = dirty; }
  bool dirty() const { return dirty_; }
  Parser* GetParser() {
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
};

// Compile Arguments to argp data and various things needed by the parser.
class ArgpCompiler {
 public:
  ArgpCompiler(ArgumentHolder* holder);

  void CompileOptions(std::vector<argp_option>* out);
  void CompileUsage(std::string* out);
  void CompileArgumentIndexes(std::map<int, Argument*>* optionals,
                              std::vector<Argument*>* positionals);

 private:
  void CompileGroup(ArgumentGroup* group, std::vector<argp_option>* out);
  void CompileArgument(Argument* arg, std::vector<argp_option>* out);
  int FindGroup(ArgumentGroup* g) { return group_to_id_[g]; }
  int FindArgument(Argument* a) { return argument_to_id_[a]; }

  ArgumentHolder* holder_;
  HelpFormatPolicy policy_;
  std::map<Argument*, int> argument_to_id_;
  std::map<ArgumentGroup*, int> group_to_id_;
};

// struct ArgpData {
//   int parser_flags = 0;
//   argp argp_info = {};
//   std::string program_doc;
//   std::string args_doc;
//   std::vector<argp_option> argp_options;
// };

class ParserImpl {
 public:
};

// Public flags user can use. These are corresponding to the ARGP_XXX flags
// passed to argp_parse().
enum Flags {
  kNoFlags = 0,            // The default.
  kNoHelp = ARGP_NO_HELP,  // Don't produce --help.
  kLongOnly = ARGP_LONG_ONLY,
  kNoExit = ARGP_NO_EXIT,
};

// Options to ArgumentParser constructor.
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

class ArgumentParser : public ArgumentContainer {
 public:
  ArgumentParser() : controller_(ArgumentController::Create()) {}
  explicit ArgumentParser(Options options) : ArgumentParser() {
    if (options.info)
      controller_->SetOptions(std::move(options.info));
  }

  ArgumentGroupProxy add_argument_group(const char* header) {
    return controller_->AddArgumentGroup(header);
  }

  void parse_args(ArgArray args) {
    DCHECK(controller_);
    controller_->ParseKnownArgs(args, nullptr);
  }

  // Helper for demo and testing.
  // void parse_args(std::initializer_list<const char*> args) {
  //   std::vector<const char*> args_copy(args.begin(), args.end());
  //   return parse_args(args.size(), args_copy.data());
  // }
  bool parse_known_args(ArgArray args, std::vector<std::string>* out) {
    DCHECK(out);
    return controller_->ParseKnownArgs(args, out);
  }

  const char* program_name() const { return program_invocation_name; }
  const char* program_short_name() const {
    return program_invocation_short_name;
  }
  const char* program_version() const { return argp_program_version; }
  const char* program_bug_address() const { return argp_program_bug_address; }

 private:
  Argument* AddArgument(std::unique_ptr<NamesInfo> names) override {
    return controller_->AddArgument(std::move(names));
  }
  std::unique_ptr<ArgumentController> controller_;
};

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
  static void Run(const std::string& in, Mode mode, Result<FILE*>* out);
};

template <typename T>
struct StreamOpenTraits {
  static void Run(const std::string& in, Mode mode, Result<T>* out) {
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

// For STL-compatible T, default is:
template <typename T>
struct DefaultAppendTraits {
  using ValueType = typename T::value_type;

  static void Run(T* obj, ValueType item) {
    // By default use the push_back() method of T.
    obj->push_back(item);
  }
};

// Specialized for STL containers.
template <typename T>
struct AppendTraits<std::vector<T>> : DefaultAppendTraits<std::vector<T>> {};
template <typename T>
struct AppendTraits<std::list<T>> : DefaultAppendTraits<std::list<T>> {};
template <typename T>
struct AppendTraits<std::deque<T>> : DefaultAppendTraits<std::deque<T>> {};
// std::string is not considered appendable, if you need that, use
// std::vector<char>

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
        DCHECK(false);
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

}  // namespace argparse
