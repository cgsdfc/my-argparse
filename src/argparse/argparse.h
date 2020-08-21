#pragma once

#include <argp.h>
#include <algorithm>
#include <any>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>  // strlen()
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>  // to define ArgumentError.
#include <type_traits>
#include <typeindex>  // We use type_index since it is copyable.
#include <utility>
#include <vector>

#define DCHECK(expr) assert(expr)
#define DCHECK2(expr, msg) assert(expr&& msg)

namespace argparse {

class Argument;
class ArgumentHolder;
class ArgumentGroup;
class ArgumentBuilder;
class ArgumentParser;
class Options;

class ActionCallback;
class TypeCallback;

using ArgpOption = ::argp_option;
using ArgpProgramVersionCallback = decltype(::argp_program_version_hook);
using ::argp;
using ::argp_error;
using ::argp_failure;
using ::argp_help;
using ::argp_parse;
using ::argp_parser_t;
using ::argp_program_bug_address;
using ::argp_program_version;
using ::argp_state;
using ::argp_state_help;
using ::argp_usage;
using ::error_t;
using ::program_invocation_name;
using ::program_invocation_short_name;

// Wrapper of argp_state.
class ArgpState {
 public:
  ArgpState(argp_state* state) : state_(state) {}
  void Help(FILE* file, unsigned flags) {
    argp_state_help(state_, file, flags);
  }
  void Usage() { argp_usage(state_); }

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

class Status {
 public:
  // If succeeded, just return true, or if failed but nothing to say, just
  // return false.
  /* implicit */ Status(bool success) : success_(success) {}

  // If failed with a msg, just say the msg.
  /* implicit */ Status(const std::string& message) : Status(false, message) {}

  // This is needed since const char* => bool is higher than const char* =>
  // std::string.
  Status(const char* message) : Status(false, message) {}

  // Or you prefer to do it canonically, use this.
  Status(bool success, const std::string& message)
      : success_(success), message_(message) {}

  // Default to success.
  Status() : Status(true) {}

  Status(Status&& that) = default;

  Status(const Status& that) = default;
  Status& operator=(const Status& that) = default;

  explicit operator bool() const { return success_; }
  const std::string& message() const { return message_; }

  void set_error(const std::string& msg) { message_ = msg; }

 private:
  bool success_;
  std::string message_;
};

// This is an internal class to communicate data/state between user's callback.
class Context {
 public:
  // Issue an error.
  void error(const std::string& msg) {
    state_.Error(msg);
    status_ = Status(msg);
  }

  void print_usage() { return state_.Usage(); }
  void print_help() { return state_.Help(stderr, 0); }

  std::string& value() { return value_; }
  const std::string& value() const { return value_; }

  bool status_ok() const { return static_cast<bool>(status_); }
  Status* status() { return &status_; }

  void set_result(std::any data) { result_ = std::move(data); }
  std::any* result() { return &result_; }
  std::any TakeResult() { return std::move(result_); }

  // Whether a value is passed to this arg.
  bool has_value() const { return has_value_; }
  // TODO: impl this by making Argument an interface.
  const Argument& argument() const { return *argument_; }

  Context(const Argument* argument, const char* value, ArgpState state)
      : has_value_(bool(value)), argument_(argument), state_(state) {
    if (has_value())
      value_.assign(value);
  }

 private:
  bool has_value_;
  const Argument* argument_;
  ArgpState state_;
  std::string value_;
  Status status_;
  std::any result_;
};

// The default impl for the types we know (bulitin-types like int).
// This traits shouldn't be overriden by users.
template <typename T, typename SFINAE = void>
struct DefaultTypeCallbackTraits {
  // This is selected when user use a custom type without specializing
  // TypeCallbackTraits.
  static void Run(const std::string&, T*, Status*) {
    DCHECK2(false, "Please specialize TypeCallbackTrait<T> for your type");
  }
};

// By default, use the traits defined by the library for builtin types.
// The user can specialize this to provide traits for their custom types
// or override global (existing) types.
template <typename T>
struct TypeCallbackTraits : DefaultTypeCallbackTraits<T> {};

// This will format an error string saying that:
// cannot parse `value' into `type_name'.
inline std::string ReportError(const std::string& value,
                               const char* type_name) {
  std::ostringstream os;
  os << "Cannot convert `" << value << "' into a value of type " << type_name;
  return os.str();
}

inline bool AnyHasType(const std::any& val, std::type_index type) {
  return type == val.type();
}

// template <typename T>
// using InternalUserCallback = std::function<Status(const Context&, T*)>;

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

}  // namespace detail

// For STL-compatible T, default is:
template <typename T>
struct DefaultAppendTraits : std::true_type {
  using ValueType = typename T::value_type;

  static void Append(T* obj, ValueType&& item) {
    // By default use the push_back() method of T.
    obj->push_back(std::forward<ValueType>(item));
  }
};

// This traits indicates whether T supports append operation and if it does,
// tells us how to do the append.
template <typename T>
struct AppendTraits : std::false_type {}; // Not supported.

// Extracted the bool value from AppendTraits.
template <typename T>
using IsAppendSupported = std::bool_constant<AppendTraits<T>{}>;

// Get the value-type for a appendable, only use it when IsAppendSupported<T>.
template <typename T>
using ValueTypeOf = typename AppendTraits<T>::ValueType;

// Specialized for STL containers.
template <typename T>
struct AppendTraits<std::vector<T>> : DefaultAppendTraits<std::vector<T>> {};
template <typename T>
struct AppendTraits<std::list<T>> : DefaultAppendTraits<std::list<T>> {};
template <typename T>
struct AppendTraits<std::deque<T>> : DefaultAppendTraits<std::deque<T>> {};
// std::string is not considered appendable, if you need that, use
// std::vector<char>

// For user's types, specialize AppendTraits<>, and if your type is
// standard-compatible, inherits from DefaultAppendTraits<>.

// A more complete module to handle user's callbacks.
enum class Actions {
  kNoAction,
  kStore,
  kStoreConst,
  kAppend,
  kAppendConst,
  kCustom,
};

class CallbackFactory {
 public:
  virtual ActionCallback* CreateActionCallback() = 0;
  virtual TypeCallback* CreateTypeCallback() = 0;
  virtual ~CallbackFactory() {}
};

// This is a meta-function that tests if A can be performed on T.
template <typename T, Actions A, typename Enable = void>
struct IsActionSupported : std::false_type {};

// Store is always supported if T is copy-assignable.
template <typename T>
struct IsActionSupported<T, Actions::kStore, void>
    : std::is_copy_assignable<T> {};

// Store Const is supported only if Store is supported.
template <typename T>
struct IsActionSupported<T, Actions::kStoreConst, void>
    : IsActionSupported<T, Actions::kStore, void> {};

template <typename T>
struct IsActionSupported<T,
                         Actions::kAppend,
                         std::enable_if_t<IsAppendSupported<T>{}>>
    : std::true_type {};

// This is a meta-function that handles static-runtime mix of selecting action
// factory.
template <typename T, Actions A, bool Supported = IsActionSupported<T, A>{}>
struct CallbackFactorySelector;

template <typename T, Actions A>
struct CallbackFactorySelector<T, A, false> /* Not supported */ {
  static CallbackFactory* Run() { return nullptr; }
};

class DestInfo {
 public:
  virtual CallbackFactory* CreateFactory(Actions action) = 0;
  virtual std::type_index GetDestType() = 0;
  virtual ~DestInfo() {}
  void* ptr() const { return ptr_; }

 protected:
  explicit DestInfo(void* ptr) : ptr_(ptr) {}
  void* ptr_ = nullptr;
};

// The most consice prototype of a TypeCallback.
// User read the string, write the conversion result to T* and if failed, report
// error using Status*.
// The reason why the return value is not Status is for lambda, if you mix
// return true and return "errmsg", the compiler can't deduct return type, which
// forces you to write ->Status. Very ugly. And to indicate success, you must
// return true, Very verbose. The Status* scheme, however, allow no tailing
// return type and silence implies ok pattern. If you don't touch Status*, you
// imply success. Only when you use it does it reports an error.
// Like:
// @code
// [](const std::string& in, int* out, Status* status) { try { *out =
// std::stoi(in); } catch (const std::exception& e) {
// status->set_error(e.what());}
// @endcode

template <typename T>
using TypeCallbackPrototype = void(const std::string&, T*, Status*);

// There is an alternative for those using exception.
// You convert string into T and throw ArgumentError if something bad happened.
template <typename T>
using TypeCallbackPrototypeThrows = T(const std::string&);

// The prototype for action. An action normally does not report errors.
template <typename T, typename V>
using ActionCallbackPrototype = void(T*, std::optional<V>);

// Perform data conversion..
class TypeCallback {
 public:
  virtual ~TypeCallback() {}
  virtual std::type_index GetValueType() = 0;

  void Run(Context* ctx) {
    DCHECK(ctx->has_value());
    ctx->set_result(RunImpl(ctx->value(), ctx->status()));
  }

 private:
  virtual std::any RunImpl(const std::string& in, Status* status) = 0;
};

// Impl a callback with the signature:
// void action(T* dest, std::optional<V> val);
// T is called the DestType. V is called the ValueType.
class ActionCallback {
 public:
  virtual ~ActionCallback() {}
  // For performing runtime type check.
  virtual std::type_index GetDestType() = 0;
  virtual std::type_index GetValueType() = 0;

  bool WorksWith(DestInfo* dest, TypeCallback* type) {
    // return GetDestType() == dest->GetDestType() &&
    //        GetValueType() == type->GetValueType();
    // TODO: check this at runtime.
    return true;
  }

  void Run(Context* ctx) {
    DCHECK(ctx->status_ok());
    RunImpl(ctx->TakeResult());
  }

  void SetDest(DestInfo* dest_info) {
    DCHECK(GetDestType() == dest_info->GetDestType());
    dest_ = dest_info->ptr();
  }

  // Set an const value to this action (must be a valid value.)
  void SetConstValue(std::any value) {
    DCHECK(value.has_value());
    DCHECK(AnyHasType(value, GetValueType()));
    const_value_ = std::move(value);
  }

 private:
  virtual void RunImpl(std::any data) = 0;

 protected:
  void* dest_ = nullptr;
  std::any const_value_;
};

// A subclass that always return nullptr. Used for actions without a need of
// value.
class NullTypeCallback : public TypeCallback {
 public:
  NullTypeCallback() = default;
  std::type_index GetValueType() override { return typeid(void); }

 private:
  std::any RunImpl(const std::string& in, Status* status) override {
    return {};
  }
};

// A subclass that parses string into value using a Traits.
template <typename T>
class DefaultTypeCallback : public TypeCallback {
 public:
  std::type_index GetValueType() override { return typeid(T); }

 private:
  std::any RunImpl(const std::string& in, Status* status) override {
    T value;
    TypeCallbackTraits<T>::Run(in, &value, status);
    return value;
  }
};

template <typename T>
class CustomTypeCallback : public TypeCallback {
 public:
  using CallbackType = std::function<TypeCallbackPrototype<T>>;
  explicit CustomTypeCallback(CallbackType cb) : callback_(std::move(cb)) {}

 private:
  std::any RunImpl(const std::string& in, Status* status) override {
    T value;
    std::invoke(callback_, in, &value, status);
    return value;
  }

  CallbackType callback_;
};

template <typename Callback, typename T>
TypeCallback* CreateCustomTypeCallbackImpl(Callback&& cb,
                                           TypeCallbackPrototype<T>*) {
  return new CustomTypeCallback<T>(std::forward<Callback>(cb));
}

template <typename Callback, typename T>
TypeCallback* CreateCustomTypeCallbackImpl(Callback&& cb,
                                           TypeCallbackPrototypeThrows<T>*) {
  auto adapter = [cb](const std::string& in, T* out, Status* status) {
    try {
      *out = std::invoke(cb, in);
    } catch (const ArgumentError& e) {
      *status = e.what();
    }
  };
  return new CustomTypeCallback<T>(adapter);
}

template <typename Callback>
TypeCallback* CreateCustomTypeCallback(Callback&& cb) {
  return CreateCustomTypeCallbackImpl(
      std::forward<Callback>(cb),
      (detail::function_signature_t<Callback>*)nullptr);
}

// A helper subclass that impl dest-type and value-type.
template <typename T, typename V>
class ActionCallbackBase : public ActionCallback {
 public:
  using DestType = T;
  using ValueType = V;

  std::type_index GetDestType() override { return typeid(DestType); }
  std::type_index GetValueType() override { return typeid(ValueType); }

 protected:
  // Helpers for subclass.
  // Access the dest (must be set).
  DestType* dest() {
    DCHECK(dest_);
    return reinterpret_cast<DestType*>(dest_);
  }
  // Cast data into the correct type.
  ValueType ValueOf(std::any data) {
    DCHECK(data.has_value());
    DCHECK(AnyHasType(data, GetValueType()));
    return std::any_cast<ValueType>(std::move(data));
  }
  // Access the const value.
  ValueType ConstValue() {
    DCHECK(const_value_.has_value());
    return ValueOf(const_value_);
  }
};

// This impls store and store-const.
template <typename T, typename V = T>
class StoreActionCallback : public ActionCallbackBase<T, V> {
 private:
  void RunImpl(std::any data) override {
    if (data.has_value())
      *(this->dest()) = this->ValueOf(data);
  }
};

template <typename T, typename V = T>
class StoreConstActionCallback : public ActionCallbackBase<T, V> {
 private:
  void RunImpl(std::any) override { *(this->dest()) = this->ConstValue(); }
};

template <typename T, typename Traits = AppendTraits<T>>
class AppendActionCallback
    : public ActionCallbackBase<T, typename Traits::ValueType> {
 private:
  void RunImpl(std::any data) override {
    if (data.has_value())
      Traits::Append(this->dest(), this->ValueOf(data));
  }
};

template <typename T, typename Traits = AppendTraits<T>>
class AppendConstActionCallback
    : public ActionCallbackBase<T, typename Traits::ValueType> {
 private:
  void RunImpl(std::any) override {
    Traits::Append(this->dest(), this->ConstValue());
  }
};

// Provided by user's callable obj.
template <typename T, typename V>
class CustomActionCallback : public ActionCallbackBase<T, V> {
 public:
  using CallbackType = std::function<void(T*, std::optional<V>)>;
  explicit CustomActionCallback(CallbackType cb) : callback_(std::move(cb)) {}

 private:
  void RunImpl(std::any data) override {
    // User needs an optional.
    std::optional<V> value;
    if (data.has_value())
      value.emplace(this->ValueOf(std::move(data)));
    DCHECK(callback_);
    std::invoke(callback_, this->dest(), std::move(value));
  }

  CallbackType callback_;
};

template <typename Callback, typename T, typename V>
ActionCallback* CreateCustomActionCallbackImpl(Callback&& cb,
                                               void (*)(T*, std::optional<V>)) {
  return new CustomActionCallback<T, V>(std::forward<Callback>(cb));
}

template <typename Callback>
ActionCallback* CreateCustomActionCallback(Callback&& cb) {
  return CreateCustomActionCallbackImpl(
      std::forward<Callback>(cb),
      (detail::function_signature_t<Callback>*)nullptr);
}

class CallbackRunner {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void HandleError(Context* ctx) = 0;
  };
  virtual void Run(Context* ctx, Delegate* delegate) = 0;
  virtual ~CallbackRunner() {}
};

// Used for no dest, no type no action.
class DummyCallbackRunner : public CallbackRunner {
 public:
  void Run(Context*, Delegate*) override {}
  ~DummyCallbackRunner() override {}
};

// Run TypeCallback and ActionCallback.
class CallbackRunnerImpl : public CallbackRunner {
 public:
  CallbackRunnerImpl(std::unique_ptr<TypeCallback> type,
                     std::unique_ptr<ActionCallback> action)
      : type_(std::move(type)), action_(std::move(action)) {}

  void Run(Context* ctx, Delegate* delegate) override {
    if (ctx->has_value())
      type_->Run(ctx);
    if (ctx->status_ok())
      action_->Run(ctx);
    else
      delegate->HandleError(ctx);
  }

 private:
  std::unique_ptr<TypeCallback> type_;
  std::unique_ptr<ActionCallback> action_;
};

template <typename T>
class DestInfoImpl : public DestInfo {
 public:
  explicit DestInfoImpl(T* ptr) : DestInfo(ptr) {}
  std::type_index GetDestType() override { return typeid(T); }

  CallbackFactory* CreateFactory(Actions action) override {
    switch (action) {
      case Actions::kStore:
        return CallbackFactorySelector<T, Actions::kStore>::Run();
      case Actions::kAppend:
        return CallbackFactorySelector<T, Actions::kAppend>::Run();
      case Actions::kStoreConst:
        return CallbackFactorySelector<T, Actions::kStoreConst>::Run();
      case Actions::kAppendConst:
        return CallbackFactorySelector<T, Actions::kAppendConst>::Run();
      default:
        return nullptr;
    }
  }
};

template <typename ActionCallbackT, typename TypeCallbackT>
struct CallbackFactoryGenerator {
  static_assert(std::is_base_of<ActionCallback, ActionCallbackT>{});
  static_assert(std::is_base_of<TypeCallback, TypeCallbackT>{});

  static CallbackFactory* Run() {
    class FactoryImpl : public CallbackFactory {
     public:
      ActionCallback* CreateActionCallback() override {
        return new ActionCallbackT();
      }
      TypeCallback* CreateTypeCallback() override {
        return new TypeCallbackT();
      }
    };
    return new FactoryImpl();
  }
};

template <typename T>
struct CallbackFactorySelector<T, Actions::kStore, true>
    : CallbackFactoryGenerator<StoreActionCallback<T>, DefaultTypeCallback<T>> {
};

template <typename T>
struct CallbackFactorySelector<T, Actions::kAppend, true>
    : CallbackFactoryGenerator<AppendActionCallback<T>,
                               DefaultTypeCallback<ValueTypeOf<T>>> {};

template <typename T>
struct CallbackFactorySelector<T, Actions::kStoreConst, true>
    : CallbackFactoryGenerator<StoreConstActionCallback<T>, NullTypeCallback> {
};

template <typename T>
struct CallbackFactorySelector<T, Actions::kAppendConst, true>
    : CallbackFactoryGenerator<AppendConstActionCallback<T>, NullTypeCallback> {
};

struct Type {
  std::unique_ptr<TypeCallback> callback;
  Type() = default;
  Type(Type&&) = default;

  Type(TypeCallback* cb) : callback(cb) {}

  template <typename Callback>
  /* implicit */ Type(Callback&& cb) {
    callback = CreateCustomTypeCallback(std::forward<Callback>(cb));
  }
};

inline Actions StringToActions(const std::string& str) {
  static const std::map<std::string, Actions> kStringToActions{
      {"store", Actions::kStore},
      {"store_const", Actions::kStoreConst},
      {"append", Actions::kAppend},
      {"append_const", Actions::kAppendConst},
  };
  auto iter = kStringToActions.find(str);
  DCHECK2(iter != kStringToActions.end(), "Unknown action string passed in");
  return iter->second;
}

// Type-erasured
struct Action {
  Actions action = Actions::kNoAction;
  std::unique_ptr<ActionCallback> callback;

  Action() = default;
  Action(Action&&) = default;

  Action(const char* action_string) {
    DCHECK(action_string);
    action = StringToActions(action_string);
  }

  Action(ActionCallback* cb) : action(Actions::kCustom), callback(cb) {}

  // TODO:: restrict signature.
  template <typename Callback>
  /* implicit */ Action(Callback&& cb)
      : Action(CreateCustomActionCallback(std::forward<Callback>(cb))) {}
};

struct Dest {
  std::unique_ptr<DestInfo> dest_info;
  Dest() = default;
  template <typename T>
  /* implicit */ Dest(T* ptr) {
    DCHECK2(ptr, "nullptr passed to Dest()");
    dest_info.reset(new DestInfoImpl<T>(ptr));
  }
};

// This creates correct types of two callbacks (and catch potential bugs of
// users) from user's configuration.
class CallbackResolver {
 public:
  virtual void SetDest(Dest dest) = 0;
  virtual void SetType(Type type) = 0;
  virtual void SetValue(std::any value) = 0;
  virtual void SetAction(Action action) = 0;
  virtual CallbackRunner* CreateCallbackRunner() = 0;
  virtual ~CallbackResolver() {}
};

class CallbackResolverImpl : public CallbackResolver {
 public:
  void SetDest(Dest dest) override { dest_ = std::move(dest.dest_info); }
  void SetType(Type type) override { custom_type_ = std::move(type.callback); }
  void SetValue(std::any value) override { value_ = std::move(value); }
  void SetAction(Action action) override {
    action_ = action.action;
    custom_action_ = std::move(action.callback);
  }

  CallbackRunner* CreateCallbackRunner() override {
    if (!dest_) {
      // everything is null.
      if (!custom_action_)  // The user don't give any action and dest, the type
                            // is discarded if given.
        return new DummyCallbackRunner();
      if (!custom_type_) {
        DCHECK(custom_action_);
        // The user migh just want to print something.
        custom_type_.reset(new NullTypeCallback());
      }
      return new CallbackRunnerImpl(std::move(custom_type_),
                                    std::move(custom_action_));
    }

    if (action_ == Actions::kNoAction)
      action_ = Actions::kStore;

    // The factory is used as a fall-back when user don't specify.
    auto* factory = dest_->CreateFactory(action_);
    // if we need the factory (action_ or type_ is null), but it is invalid.
    DCHECK2((custom_action_ && custom_type_) || factory,
            "The provided action is not supported by the type of dest");

    if (!custom_action_) {
      // user does not provide an action callback, we need to infer.
      custom_action_.reset(factory->CreateActionCallback());
    }

    // If there is a dest, send it to action anyway (may not be needed by the
    // action, but we generally don't know that.).
    custom_action_->SetDest(dest_.get());

    if (value_.has_value()) {
      // If user gave us a value, send it to the action.
      custom_action_->SetConstValue(std::move(value_));
    }

    if (!custom_type_) {
      custom_type_.reset(factory->CreateTypeCallback());
    }
    DCHECK2(custom_action_->WorksWith(dest_.get(), custom_type_.get()),
            "The provide dest, action and type are not compatible");
    return new CallbackRunnerImpl(std::move(custom_type_),
                                  std::move(custom_action_));
  }

 private:
  Actions action_ = Actions::kNoAction;
  std::unique_ptr<DestInfo> dest_;
  std::unique_ptr<TypeCallback> custom_type_;
  std::unique_ptr<ActionCallback> custom_action_;
  std::any value_;
};

inline bool IsValidPositionalName(const char* name, std::size_t len) {
  if (!name || !len || !std::isalpha(name[0]))
    return false;
  for (++name, --len; len > 0; ++name, --len) {
    if (std::isalnum(*name) || *name == '-' || *name == '_')
      continue;  // allowed.
    return false;
  }
  return true;
}

// A valid option name is long or short option name and not '--', '-'.
// This is only checked once and true for good.
inline bool IsValidOptionName(const char* name, std::size_t len) {
  if (!name || len < 2 || name[0] != '-')
    return false;
  if (len == 2)  // This rules out -?, -* -@ -= --
    return std::isalnum(name[1]);
  // check for long-ness.
  DCHECK2(name[1] == '-',
          "Single-dash long option (i.e., -jar) is not supported, please use "
          "GNU-style long option (double-dash)");

  for (name += 2; *name; ++name) {
    if (*name == '-' || *name == '_' || std::isalnum(*name))
      continue;
    return false;
  }
  return true;
}

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
  std::vector<std::string> long_names;

  // TODO: Ban short name alias.
  std::vector<char> short_names;
  bool is_option;
  std::string meta_var;

  Names(const char* name) {
    if (name[0] == '-') {
      InitOptions({name});
      return;
    }
    auto len = std::strlen(name);
    DCHECK2(IsValidPositionalName(name, len), "Not a valid positional name!");
    is_option = false;
    std::string positional(name, len);
    meta_var = ToUpper(positional);
    long_names.push_back(std::move(positional));
  }

  Names(std::initializer_list<const char*> names) { InitOptions(names); }

  void InitOptions(std::initializer_list<const char*> names) {
    DCHECK2(names.size(), "At least one name must be provided");
    is_option = true;

    for (auto name : names) {
      std::size_t len = std::strlen(name);
      DCHECK2(IsValidOptionName(name, len), "Not a valid option name!");
      if (IsLongOptionName(name, len)) {
        // Strip leading '-' at most twice.
        for (int i = 0; *name == '-' && i < 2; ++i) {
          ++name;
          --len;
        }
        long_names.emplace_back(name, len);
      } else {
        short_names.push_back(name[1]);
      }
    }
    if (long_names.size())
      meta_var = ToUpper(long_names[0]);
    else
      meta_var = ToUpper({&short_names[0], 1});
  }
};

class Argument {
 public:
  class Delegate {
   public:
    // Generate a key for an option.
    virtual int NextOptionKey() = 0;
    // Call when an Arg is fully constructed.
    virtual void OnArgumentCreated(Argument*) = 0;
    virtual ~Delegate() {}
  };

  virtual void SetHelpDoc(const char* help_doc) = 0;
  virtual void SetRequired(bool required) = 0;
  virtual void SetMetaVar(const char* meta_var) = 0;
  virtual CallbackResolver* GetCallbackResolver() = 0;
  virtual void InitCallback() = 0;
  virtual CallbackRunner* GetCallbackRunner() = 0;

  virtual bool IsOption() const = 0;
  virtual bool IsInitialized() const = 0;
  virtual int GetKey() const = 0;
  virtual int GetGroup() const = 0;
  virtual void FormatArgsDoc(std::ostream& os) const = 0;
  virtual void CompileToArgpOptions(std::vector<ArgpOption>* options) const = 0;
  virtual bool AppearsBefore(const Argument* that) const = 0;

  virtual ~Argument() {}
};

// Holds all meta-info about an argument.
class ArgumentImpl : public Argument {
 public:
  ArgumentImpl(Delegate* delegate, const Names& names, int group)
      : callback_resolver_(new CallbackResolverImpl()),
        delegate_(delegate),
        group_(group) {
    InitNames(names);
    InitKey(names.is_option);
    delegate_->OnArgumentCreated(this);
  }

  void SetHelpDoc(const char* help_doc) override {
    DCHECK(help_doc);
    help_doc_ = help_doc;
  }

  void SetRequired(bool required) override { is_required_ = required; }
  void SetMetaVar(const char* meta_var) override {
    DCHECK(meta_var);
    meta_var_ = meta_var;
  }
  bool IsOption() const override { return is_option(); }
  bool IsInitialized() const override { return initialized(); }
  int GetKey() const override { return key(); }
  int GetGroup() const override { return group(); }

  bool initialized() const { return key_ != 0; }
  int key() const { return key_; }
  int group() const { return group_; }
  bool is_option() const { return key_ != kKeyForPositional; }
  bool is_required() const { return is_required_; }

  const std::string& help_doc() const { return help_doc_; }
  const char* doc() const {
    return help_doc_.empty() ? nullptr : help_doc_.c_str();
  }

  const std::string& meta_var() const { return meta_var_; }
  const char* arg() const {
    DCHECK(!meta_var_.empty());
    return meta_var_.c_str();
  }

  // TODO: split the long_names, short_names from Names into name, key and
  // alias.
  const std::vector<std::string>& long_names() const { return long_names_; }
  const std::vector<char>& short_names() const { return short_names_; }

  const char* name() const {
    return long_names_.empty() ? nullptr : long_names_[0].c_str();
  }

  CallbackResolver* GetCallbackResolver() override {
    DCHECK(callback_resolver_);
    return callback_resolver_.get();
  }
  CallbackRunner* GetCallbackRunner() override {
    DCHECK(callback_runner_);
    return callback_runner_.get();
  }
  // [--name|-n|-whatever=[value]] or output
  void FormatArgsDoc(std::ostream& os) const override {
    if (!is_option()) {
      os << meta_var();
      return;
    }
    os << '[';
    std::size_t i = 0;
    const auto size = long_names_.size() + short_names_.size();

    for (; i < size; ++i) {
      if (i < long_names_.size()) {
        os << "--" << long_names_[i];
      } else {
        os << '-' << short_names_[i - long_names_.size()];
      }
      if (i < size - 1)
        os << '|';
    }

    if (!is_required())
      os << '[';
    os << '=' << meta_var();
    if (!is_required())
      os << ']';
    os << ']';
  }

  void InitCallback() override {
    DCHECK(!callback_runner_ && callback_resolver_);
    callback_runner_.reset(callback_resolver_->CreateCallbackRunner());
    callback_resolver_.reset();
  }

  void CompileToArgpOptions(std::vector<ArgpOption>* options) const override {
    ArgpOption opt{};
    opt.doc = doc();
    opt.group = group();
    opt.name = name();
    if (!is_option()) {
      // positional means none-zero in only doc and name, and flag should be
      // OPTION_DOC.
      opt.flags = OPTION_DOC;
      return options->push_back(opt);
    }
    opt.arg = arg();
    opt.key = key();
    options->push_back(opt);
    // TODO: handle alias correctly. Add all aliases.
    for (auto first = long_names_.begin() + 1, last = long_names_.end();
         first != last; ++first) {
      ArgpOption opt_alias;
      std::memcpy(&opt_alias, &opt, sizeof(ArgpOption));
      opt.name = first->c_str();
      opt.flags = OPTION_ALIAS;
      options->push_back(opt_alias);
    }
  }

  bool AppearsBefore(const Argument* that) const override {
    return CompareArguments(this, static_cast<const ArgumentImpl*>(that));
  }

 private:
  friend class ArgpParserImpl;

  enum Keys {
    kKeyForNothing = 0,
    kKeyForPositional = -1,
  };

  // Fill in members to do with names.
  void InitNames(Names names) {
    long_names_ = std::move(names.long_names);
    short_names_ = std::move(names.short_names);
    meta_var_ = std::move(names.meta_var);
  }

  // Initialize the key member. Must be called after InitNames().
  void InitKey(bool is_option) {
    if (!is_option) {
      key_ = kKeyForPositional;
      return;
    }
    key_ = short_names_.empty() ? delegate_->NextOptionKey() : short_names_[0];
  }

  static bool CompareArguments(const ArgumentImpl* a, const ArgumentImpl* b) {
    // options go before positionals.
    if (a->is_option() != b->is_option())
      return int(a->is_option()) > int(b->is_option());

    // positional compares on their names.
    if (!a->is_option() && !b->is_option()) {
      DCHECK(a->name() && b->name());
      return std::strcmp(a->name(), b->name()) < 0;
    }

    // required option goes first.
    if (a->is_required() != b->is_required())
      return int(a->is_required()) > int(b->is_required());

    // short-only option goes before the rest.
    if (bool(a->name()) != bool(b->name()))
      return bool(a->name()) < bool(b->name());

    // a and b are both short-only option.
    if (!a->name() && !b->name())
      return a->key() < b->key();

    // a and b are both long option.
    DCHECK(a->name() && b->name());
    return std::strcmp(a->name(), b->name()) < 0;
  }

  // For extension.
  Delegate* delegate_;
  // For positional, this is -1, for group-header, this is -2.
  int key_ = kKeyForNothing;
  int group_ = 0;
  std::unique_ptr<CallbackResolver> callback_resolver_;
  std::unique_ptr<CallbackRunner> callback_runner_;  // Maybe null.
  std::string help_doc_;
  std::vector<std::string> long_names_;
  std::vector<char> short_names_;
  std::string meta_var_;
  bool is_required_ = false;
};

class ArgumentBuilder {
 public:
  explicit ArgumentBuilder(Argument* arg)
      : arg_(arg), resolver_(arg->GetCallbackResolver()) {}

  ArgumentBuilder& dest(Dest d) {
    resolver_->SetDest(std::move(d));
    return *this;
  }
  ArgumentBuilder& action(Action a) {
    resolver_->SetAction(std::move(a));
    return *this;
  }
  ArgumentBuilder& type(Type t) {
    resolver_->SetType(std::move(t));
    return *this;
  }
  template <typename T>
  ArgumentBuilder& type() {
    resolver_->SetType(new DefaultTypeCallback<T>());
    return *this;
  }
  ArgumentBuilder& value(std::any val) {
    resolver_->SetValue(std::move(val));
    return *this;
  }
  ArgumentBuilder& help(const char* h) {
    if (h)
      arg_->SetHelpDoc(h);
    return *this;
  }
  ArgumentBuilder& required(bool b) {
    arg_->SetRequired(b);
    return *this;
  }
  ArgumentBuilder& meta_var(const char* v) {
    if (v)
      arg_->SetMetaVar(v);
    return *this;
  }

  // for test:
  Argument* arg() const { return arg_; }

 private:
  Argument* arg_;
  CallbackResolver* resolver_;
};

// Return value of help filter function.
enum class HelpFilterResult {
  kKeep,
  kDrop,
  kReplace,
};

using HelpFilterCallback =
    std::function<HelpFilterResult(const Argument&, std::string* text)>;

class ArgArray {
 public:
  ArgArray(int argc, const char** argv)
      : argc_(argc), argv_(const_cast<char**>(argv)) {}
  ArgArray(std::vector<const char*>* v) : ArgArray(v->size(), v->data()) {}

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

class ArgpParser {
 public:
  class Delegate {
   public:
    virtual Argument* FindOptionalArgument(int key) = 0;
    virtual Argument* FindPositionalArgument(int index) = 0;
    virtual std::size_t PositionalArgumentCount() = 0;
    virtual void CompileToArgpOptions(std::vector<ArgpOption>* options) = 0;
    virtual void GenerateArgsDoc(std::string* args_doc) = 0;
    virtual ~Delegate() {}
  };

  struct Options {
    const char* program_version = {};
    const char* description = {};
    const char* after_doc = {};
    const char* domain = {};
    const char* bug_address = {};
    ArgpProgramVersionCallback program_version_callback = {};
    HelpFilterCallback help_filter;
    int flags = 0;
  };

  // Initialize from a few options (user's options).
  virtual void Init(const Options& options) = 0;
  // Parse args, exit on errors.
  virtual void ParseArgs(ArgArray args) = 0;
  // Parse args, collect unknown args into rest, don't exit, report error via
  // Status.
  virtual Status ParseKnownArgs(ArgArray args, std::vector<std::string>* rest) {
    ParseArgs(args);
    return true;
  }
  virtual ~ArgpParser() {}
  static std::unique_ptr<ArgpParser> Create(Delegate* delegate);
};

// This is an interface that provides add_argument() and other common things.
class ArgumentContainer {
 public:
  ArgumentBuilder add_argument(Names names,
                               Dest dest = {},
                               const char* help = {},
                               Type type = {}) {
    auto builder = ArgumentBuilder(AddArgument(std::move(names)));
    return builder.dest(std::move(dest)).help(help).type(std::move(type));
  }

  virtual ~ArgumentContainer() {}

 private:
  virtual Argument* AddArgument(Names names) = 0;
};

inline void PrintArgpOptionArray(const std::vector<ArgpOption>& options) {
  for (const auto& opt : options) {
    printf("name=%s, key=%d, arg=%s, doc=%s, group=%d\n", opt.name, opt.key,
           opt.arg, opt.doc, opt.group);
  }
}

class ArgumentHolder {
 public:
  virtual Argument* AddArgumentToGroup(Names names, int group) = 0;
  virtual Argument* AddArgument(Names names) = 0;
  virtual ArgumentGroup AddArgumentGroup(const char* header) = 0;
  virtual std::unique_ptr<ArgpParser> CreateParser() = 0;
  virtual ~ArgumentHolder() {}
};

// Impl add_group() call.
class ArgumentGroup : public ArgumentContainer {
 public:
  ArgumentGroup(ArgumentHolder* holder, int group)
      : holder_(holder), group_(group) {}

 private:
  Argument* AddArgument(Names names) override {
    return holder_->AddArgumentToGroup(std::move(names), group_);
  }

  int group_;
  ArgumentHolder* holder_;
};

// TODO: this shouldn't inherit ArgumentContainer as it is a public interface.
class ArgumentHolderImpl : public ArgumentHolder,
                           public Argument::Delegate,
                           public ArgpParser::Delegate {
 public:
  ArgumentHolderImpl() {
    AddGroup("optional arguments");
    AddGroup("positional arguments");
  }

  // Add an arg to a specific group.
  Argument* AddArgumentToGroup(Names names, int group) override {
    // First check if this arg will conflict with existing ones.
    DCHECK2(CheckNamesConflict(names), "Names conflict with existing names!");
    DCHECK(group <= groups_.size());
    Argument& arg = arguments_.emplace_back(this, names, group);
    return &arg;
  }

  // TODO: since in most cases, parse_args() is only called once, we may
  // compile all the options in one shot before parse_args() is called and
  // throw the options array away after using it.
  Argument* AddArgument(Names names) override {
    int group = names.is_option ? kOptionGroup : kPositionalGroup;
    return AddArgumentToGroup(std::move(names), group);
  }

  ArgumentGroup AddArgumentGroup(const char* header) override {
    int group = AddGroup(header);
    return ArgumentGroup(this, group);
  }

  std::unique_ptr<ArgpParser> CreateParser() override {
    return ArgpParser::Create(this);
  }

  const std::map<int, Argument*>& optional_arguments() const {
    return optional_arguments_;
  }
  const std::vector<Argument*>& positional_arguments() const {
    return positional_arguments_;
  }

 private:
  // ArgpParser::Delegate:
  static constexpr int kAverageAliasCount = 4;

  void CompileToArgpOptions(std::vector<ArgpOption>* options) override {
    if (arguments_.empty())
      return options->push_back({});

    const unsigned option_size =
        arguments_.size() + groups_.size() + kAverageAliasCount + 1;
    options->reserve(option_size);

    for (const Group& group : groups_) {
      group.CompileToArgpOption(options);
    }

    // TODO: if there is not pos/opt at all but there are two groups, will argp
    // still print these empty groups?
    for (Argument& arg : arguments_) {
      arg.InitCallback();
      arg.CompileToArgpOptions(options);
    }
    // Only when at least one opt/pos presents should we generate their groups.
    options->push_back({});

    // PrintArgpOptionArray(*options);
  }

  void GenerateArgsDoc(std::string* args_doc) override {
    std::vector<Argument*> args(arguments_.size());
    std::transform(arguments_.begin(), arguments_.end(), args.begin(),
                   [](ArgumentImpl& arg) { return &arg; });
    std::sort(args.begin(), args.end(),
              [](Argument* a, Argument* b) { return a->AppearsBefore(b); });

    // join the dump of each arg with a space.
    std::ostringstream os;
    for (std::size_t i = 0, size = args.size(); i < size; ++i) {
      args[i]->FormatArgsDoc(os);
      if (i != size - 1)
        os << ' ';
      args_doc->assign(os.str());
    }
  }

  Argument* FindOptionalArgument(int key) override {
    auto iter = optional_arguments_.find(key);
    return iter == optional_arguments_.end() ? nullptr : iter->second;
  }

  Argument* FindPositionalArgument(int index) override {
    return (0 <= index && index < positional_arguments_.size())
               ? positional_arguments_[index]
               : nullptr;
  }

  std::size_t PositionalArgumentCount() override {
    return positional_arguments_.size();
  }

  // If there is a group, but it has no member, it will not be added to
  // argp_options. This class manages the logic above. It also frees the
  // Argument class from managing groups as well as option and positional.
  class Group {
   public:
    Group(int group, const char* header) : group_(group), header_(header) {
      DCHECK(group_ > 0);
      DCHECK(header_.size());
      if (header_.back() != ':')
        header_.push_back(':');
    }

    void AddMember() { ++members_; }

    void CompileToArgpOption(std::vector<ArgpOption>* options) const {
      if (!members_)
        return;
      ArgpOption opt{};
      opt.group = group_;
      opt.doc = header_.c_str();
      return options->push_back(opt);
    }

   private:
    unsigned group_;        // The group id.
    std::string header_;    // the text provided by user plus a ':'.
    unsigned members_ = 0;  // If this is 0, no header will be gen'ed.
  };

  // Create a new group.
  int AddGroup(const char* header) {
    int group = groups_.size() + 1;
    groups_.emplace_back(group, header);
    return group;
  }

  Group* GroupFromID(int group) {
    DCHECK(group <= groups_.size());
    return &groups_[group - 1];
  }

  // Argument::Delegate:
  int NextOptionKey() override { return next_key_++; }

  void OnArgumentCreated(Argument* arg) override {
    DCHECK(arg->IsInitialized());
    GroupFromID(arg->GetGroup())->AddMember();
    if (arg->IsOption()) {
      bool inserted = optional_arguments_.emplace(arg->GetKey(), arg).second;
      DCHECK(inserted);
    } else {
      positional_arguments_.push_back(arg);
    }
  }

  bool CheckNamesConflict(const Names& names) {
    for (auto&& long_name : names.long_names)
      if (!name_set_.insert(long_name).second)
        return false;
    // May not use multiple short names.
    for (char short_name : names.short_names)
      if (!name_set_.insert(std::string(&short_name, 1)).second)
        return false;
    return true;
  }

  static constexpr unsigned kFirstArgumentKey = 128;

  // // Gid for two builtin groups.
  enum GroupID {
    kOptionGroup = 1,
    kPositionalGroup = 2,
  };

  // We have to explicitly manage group_id (instead of using 0 to inherit the
  // gid from the preivous entry) since the user can add option and positionals
  // in any order. Automatical inheriting gid will mess up.
  // unsigned next_group_id_ = kFirstUserGroup;
  unsigned next_key_ = kFirstArgumentKey;

  // Hold the storage of all args.
  std::list<ArgumentImpl> arguments_;
  // indexed by their define-order.
  std::vector<Argument*> positional_arguments_;
  // indexed by their key.
  std::map<int, Argument*> optional_arguments_;
  // groups must be random-accessed.
  std::vector<Group> groups_;
  // Conflicts checking.
  std::set<std::string> name_set_;
};

// This handles the argp_parser_t function and provide a bunch of context during
// the parsing.
class ArgpParserImpl : public ArgpParser, private CallbackRunner::Delegate {
 public:
  // When this is constructed, Delegate must have been added options.
  explicit ArgpParserImpl(ArgpParser::Delegate* delegate)
      : delegate_(delegate) {
    argp_.parser = &ArgpParserImpl::Callback;
    positional_count_ = delegate_->PositionalArgumentCount();
    delegate_->CompileToArgpOptions(&argp_options_);
    argp_.options = argp_options_.data();
    delegate_->GenerateArgsDoc(&args_doc_);
    argp_.args_doc = args_doc_.c_str();
  }

  void Init(const Options& options) override {
    if (options.program_version)
      ::argp_program_version = options.program_version;
    if (options.program_version_callback)
      ::argp_program_version_hook = options.program_version_callback;
    if (options.bug_address)
      ::argp_program_bug_address = options.bug_address;
    if (options.help_filter) {
      argp_.help_filter = &HelpFilterImpl;
      help_filter_ = options.help_filter;
    }

    // TODO: may check domain?
    set_argp_domain(options.domain);
    AddFlags(options.flags);

    // Generate the program doc.
    if (options.description) {
      program_doc_ = options.description;
    }
    if (options.after_doc) {
      program_doc_.append({'\v'});
      program_doc_.append(options.after_doc);
    }

    if (!program_doc_.empty())
      set_doc(program_doc_.c_str());
  }

  void ParseArgs(ArgArray args) override {
    // If any error happened, just exit the program. No need to check for return
    // value.
    argp_parse(&argp_, args.argc(), args.argv(), parser_flags_, nullptr, this);
  }

  Status ParseKnownArgs(ArgArray args,
                        std::vector<std::string>* rest) override {
    int arg_index;
    error_t error = argp_parse(&argp_, args.argc(), args.argv(), parser_flags_,
                               &arg_index, this);
    if (!error)
      return true;
    for (int i = arg_index; i < args.argc(); ++i) {
      rest->emplace_back(args[i]);
    }
    // TODO: may just return bool. ctx.error() just call argp_error, don't store
    // status.
    return false;
  }

 private:
  void set_doc(const char* doc) { argp_.doc = doc; }
  void set_argp_domain(const char* domain) { argp_.argp_domain = domain; }
  void set_args_doc(const char* args_doc) { argp_.args_doc = args_doc; }
  void AddFlags(int flags) { parser_flags_ |= flags; }

  // TODO: Change this scheme.
  void InvokeUserCallback(Argument* arg, char* value, ArgpState state) {
    Context ctx(arg, value, state);
    arg->GetCallbackRunner()->Run(&ctx, this);
  }

  // CallbackRunner::Delegate:
  void HandleError(Context* ctx) override {
    // TODO:
  }

  static constexpr unsigned kSpecialKeyMask = 0x1000000;

  error_t ParseImpl(int key, char* arg, ArgpState state) {
    // Positional argument.
    if (key == ARGP_KEY_ARG) {
      const int arg_num = state->arg_num;
      if (Argument* argument = delegate_->FindPositionalArgument(arg_num)) {
        InvokeUserCallback(argument, arg, state);
        return 0;
      }
      // Too many arguments.
      if (state->arg_num >= positional_count())
        state.ErrorF("Too many positional arguments. Expected %d, got %d",
                     (int)positional_count(), (int)state->arg_num);
      return ARGP_ERR_UNKNOWN;
    }

    // Next most frequent handling is options.
    if ((key & kSpecialKeyMask) == 0) {
      // This isn't a special key, but rather an option.
      Argument* argument = delegate_->FindOptionalArgument(key);
      if (!argument)
        return ARGP_ERR_UNKNOWN;
      InvokeUserCallback(argument, arg, state);
      return 0;
    }

    // No more commandline args, do some post-processing.
    if (key == ARGP_KEY_END) {
      // No enough args.
      if (state->arg_num < positional_count())
        state.ErrorF("No enough positional arguments. Expected %d, got %d",
                     (int)positional_count(), (int)state->arg_num);
    }

    // Remaining args (not parsed). Collect them or turn it into an error.
    if (key == ARGP_KEY_ARGS) {
      return 0;
    }

    return 0;
  }

  static error_t Callback(int key, char* arg, argp_state* state) {
    auto* self = reinterpret_cast<ArgpParserImpl*>(state->input);
    return self->ParseImpl(key, arg, state);
  }

  static char* HelpFilterImpl(int key, const char* text, void* input) {
    if (!input || !text)
      return (char*)text;
    auto* self = reinterpret_cast<ArgpParserImpl*>(input);
    DCHECK2(self->help_filter_,
            "should only be called if user install help filter!");
    auto* arg = self->delegate_->FindOptionalArgument(key);
    DCHECK2(arg, "argp calls us with unknown key!");

    std::string repl(text);
    HelpFilterResult result = std::invoke(self->help_filter_, *arg, &repl);
    switch (result) {
      case HelpFilterResult::kKeep:
        return const_cast<char*>(text);
      case HelpFilterResult::kDrop:
        return nullptr;
      case HelpFilterResult::kReplace: {
        char* s = (char*)std::malloc(1 + repl.size());
        return std::strcpy(s, repl.c_str());
      }
    }
  }

  unsigned positional_count() const { return positional_count_; }

  int parser_flags_ = 0;
  unsigned positional_count_ = 0;
  ArgpParser::Delegate* delegate_;
  Status status_;
  argp argp_ = {};
  std::string program_doc_;
  std::string args_doc_;
  std::vector<ArgpOption> argp_options_;
  HelpFilterCallback help_filter_;
};

inline std::unique_ptr<ArgpParser> ArgpParser::Create(Delegate* delegate) {
  return std::make_unique<ArgpParserImpl>(delegate);
}

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
  Options(const char* version = nullptr, const char* description = nullptr) {
    options.program_version = version;
    options.description = description;
  }
  // Options() = default;

  Options& version(const char* v) {
    options.program_version = v;
    return *this;
  }
  // Options& version(ArgpProgramVersionCallback callback) {
  //   program_version_callback_ = callback;
  //   return *this;
  // }
  Options& description(const char* d) {
    options.description = d;
    return *this;
  }
  Options& after_doc(const char* a) {
    options.after_doc = a;
    return *this;
  }
  Options& domain(const char* d) {
    options.domain = d;
    return *this;
  }
  Options& bug_address(const char* b) {
    options.bug_address = b;
    return *this;
  }
  Options& flags(Flags f) {
    options.flags |= f;
    return *this;
  }
  Options& help_filter(HelpFilterCallback cb) {
    options.help_filter = std::move(cb);
    return *this;
  }

  ArgpParser::Options options;
};

class ArgumentParser : public ArgumentContainer {
 public:
  explicit ArgumentParser(const Options& options = {})
      : holder_(new ArgumentHolderImpl()), user_options_(options) {}

  Options& options() { return user_options_; }

  ArgumentGroup add_argument_group(const char* header) {
    return holder_->AddArgumentGroup(header);
  }

  // argp wants a char**, but most user don't expect argv being changed. So
  // cheat them.
  void parse_args(int argc, const char** argv) {
    auto parser = holder_->CreateParser();
    parser->Init(user_options_.options);
    return parser->ParseArgs(ArgArray(argc, argv));
  }

  // Helper for demo and testing.
  void parse_args(std::initializer_list<const char*> args) {
    // init-er is not mutable.
    std::vector<const char*> args_copy(args.begin(), args.end());
    // args_copy.push_back(nullptr);
    return parse_args(args.size(), args_copy.data());
  }
  // TODO: parse_known_args()

  const char* program_name() const { return program_invocation_name; }
  const char* program_short_name() const {
    return program_invocation_short_name;
  }
  const char* program_version() const { return argp_program_version; }
  // TODO: rename to email.
  const char* program_bug_address() const { return argp_program_bug_address; }

 private:
  Argument* AddArgument(Names names) override {
    return holder_->AddArgument(std::move(names));
  }
  Options user_options_;
  std::unique_ptr<ArgumentHolder> holder_;
};

// wstring is not supported now.
template <>
struct DefaultTypeCallbackTraits<std::string> {
  static void Run(const std::string& in, std::string* out, Status*) {
    *out = in;
  }
};
// char is an unquoted single character.
template <>
struct DefaultTypeCallbackTraits<char> {
  static void Run(const std::string& in, char* out, Status* status) {
    if (in.size() != 1)
      return status->set_error("char must be exactly one character");
    if (!std::isprint(in[0]))
      return status->set_error("char must be printable");
    *out = in[0];
  }
};
template <>
struct DefaultTypeCallbackTraits<bool> {
  static void Run(const std::string& in, bool* out, Status* status) {
    static const std::map<std::string, bool> kStringToBools{
        {"true", true},   {"True", true},   {"1", true},
        {"false", false}, {"False", false}, {"0", false},
    };
    auto iter = kStringToBools.find(in);
    if (iter == kStringToBools.end())
      return status->set_error("not a valid bool value");
    *out= iter->second;
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
void StlParseNumber(const std::string& in, T* out) {
  static_assert(has_stl_number_parser_t<T>{});
  *out = StlParseNumberImpl<T, stl_number_parser<T>{}>(
      in, std::is_floating_point<T>{});
}

template <typename T>
struct STLNumberTypeCallbackTraits {
  static void Run(const std::string& in, T* out, Status* status) {
    try {
      StlParseNumber(in, out);
    } catch (std::invalid_argument&) {
      status->set_error("invalid numeric format");
    } catch (std::out_of_range&) {
      status->set_error("numeric value out of range");
    }
  }
};

template <typename T>
struct DefaultTypeCallbackTraits<T,
                                 std::enable_if_t<has_stl_number_parser_t<T>{}>>
    : STLNumberTypeCallbackTraits<T> {};

}  // namespace argparse

// TODO: about error handling.
// User's callback should be very concise and contains the very only
// info user needs.
// For type-callback, user performs a conversion (only if a arg is present), or
// tell us error. For action-callback, user performs an action (only if
// type-callback succeeded) and maybe tell us error. That's enough. User don't
// print help or usage, it 's what we should do -- catch user's error and do
// error handling. We allow user to specify error handling policy, deciding what
// to print, whether to exit, exit with what code... etc.