#pragma once

#include <argp.h>
#include <algorithm>
#include <any>
#include <cassert>
#include <cstring>  // strlen()
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

 private:
  bool success_;
  std::string message_;
};

// TODO: make it a class.
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
  // Whether a value is passed to this arg.
  bool has_value() const { return has_value_; }
  // TODO: impl this by making Argument an interface.
  bool is_option() const;  // Whether this arg is an option.

  Context(const Argument* argument, const char* value, ArgpState state)
      : has_value_(bool(value)), argument_(argument), state_(state) {
    if (has_value())
      value_.assign(value);
  }

  // For checking if user's call succeeded.
  Status TakeStatus() { return std::move(status_); }

 private:
  // The Argument being parsed.
  bool has_value_;
  const Argument* argument_;
  ArgpState state_;
  // The command-line value to this argument.
  std::string value_;
  Status status_;  // User's error() call is saved here.
};

// Why we need a default version?
// During type-erasure of dest, a DestUserCallback will always be created, which
// makes use of this struct. When user actually use type or action, this isn't
// needed logically but needed syntatically.
template <typename T>
struct DefaultConverter {
  // Return typename of T.
  static const char* type_name() { return nullptr; }
  // Parse in, put the result into out, return error indicator.
  static bool Parse(const std::string& in, T* out) { return true; }
};

// This will format an error string saying that:
// cannot parse `value' into `type_name'.
inline std::string ReportError(const std::string& value,
                               const char* type_name) {
  std::ostringstream os;
  os << "Cannot convert `" << value << "' into a value of type " << type_name;
  return os.str();
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

template <typename T>
using ValueTypeOf = typename T::value_type;

// struct ValueTypeOf {
//   using type =
// };

// This struct instructs us how to append Value to T.
template <typename T, typename Value = typename T::value_type>
struct AppendTraits {
  using value_type = Value;

  static void Append(T* obj, value_type&& item) {
    // By default use the push_back() method of T.
    obj->push_back(std::forward<value_type>(item));
  }
};

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
struct ActionIsSupported : std::false_type {};

// Store is always supported if T is copy-assignable.
template <typename T>
struct ActionIsSupported<T, Actions::kStore, void>
    : std::is_copy_assignable<T> {};

// Store Const is supported only if Store is supported.
template <typename T>
struct ActionIsSupported<T, Actions::kStoreConst, void>
    : ActionIsSupported<T, Actions::kStore, void> {};

// Append is supported if AppendTraits<T>::Append() is valid.
template <typename T>
struct ActionIsSupported<T,
                         Actions::kAppend,
                         std::void_t<decltype(
                             // static_cast<void (*)(T*, typename
                             // AppendTraits<T>::value_type&&)>(
                             &AppendTraits<T>::Append)>> : std::true_type {};

// This is a meta-function that handles static-runtime mix of selecting action
// factory.
template <typename T, Actions A, bool Supported = ActionIsSupported<T, A>{}>
struct CallbackFactorySelector;

template <typename T, Actions A>
struct CallbackFactorySelector<T, A, false> /* Not supported */ {
  static CallbackFactory* Select() { return nullptr; }
};

class DestInfo {
 public:
  // Create a factory that needs no args.
  virtual CallbackFactory* CreateFactory(Actions action) = 0;
  // Create a factory with a piece of data.
  virtual CallbackFactory* CreateFactoryWithValue(Actions action,
                                                  std::any value) = 0;
  virtual std::type_index GetDestType() = 0;

  void* ptr() const { return ptr_; }

 protected:
  explicit DestInfo(void* ptr) : ptr_(ptr) {}
  void* ptr_ = nullptr;
};

// Perform data conversion..
class TypeCallback {
 public:
  class Delegate {
   public:
    virtual void OnTypeCallbackInvoked(Status status, std::any data) {}
    virtual ~Delegate() {}
  };
  void Run(Context* ctx, Delegate* delegate) {
    auto data = RunImpl(ctx);
    delegate->OnTypeCallbackInvoked(ctx->TakeStatus(), std::move(data));
  }
  virtual ~TypeCallback() {}
  virtual std::type_index GetValueType() = 0;

 private:
  virtual std::any RunImpl(Context* ctx) = 0;
};

// Impl a callback with the signature:
// void action(T* dest, std::optional<V> val);
// T is called the DestType. V is called the ValueType.
class ActionCallback {
 public:
  void Run(std::any data) { RunImpl(std::move(data)); }
  void BindToDest(DestInfo* dest_info) {
    DCHECK(GetDestType() == dest_info->GetDestType());
    dest_ = dest_info->ptr();
  }
  virtual ~ActionCallback() {}
  // For performing runtime type check.
  virtual std::type_index GetDestType() = 0;
  virtual std::type_index GetValueType() = 0;

 private:
  virtual void RunImpl(std::any data) = 0;

 protected:
  void* dest_ = nullptr;
};

// A subclass that always return nullptr. Used for actions without a need of
// value.
class NullTypeCallback : public TypeCallback {
 public:
  NullTypeCallback() = default;
  std::type_index GetValueType() override { return typeid(void); }

 private:
  std::any RunImpl(Context*) override { return nullptr; }
};

// A subclass that parses string into value using a Traits.
template <typename T>
class DefaultTypeCallback : public TypeCallback {
 public:
  std::type_index GetValueType() override { return typeid(T); }

 private:
  std::any RunImpl(Context* ctx) override {}
};

// A subclass that always return a const value. Used for actions like
// store-const and append-const.
template <typename T>
class ConstTypeCallback : public TypeCallback {
 public:
  explicit ConstTypeCallback(T value) : value_(value) {}
  std::type_index GetValueType() override { return typeid(T); }

 private:
  std::any RunImpl(Context* ctx) override { return value_; }
  std::any value_;
};

template <typename T>
class CustomTypeCallback : public TypeCallback {
 public:
  using CallbackType = std::function<void(Context*, T*)>;
  explicit CustomTypeCallback(CallbackType cb) : callback_(std::move(cb)) {}

 private:
  std::any RunImpl(Context* ctx) override {
    T value;
    std::invoke(callback_, ctx, &value);
    return std::make_any(std::move(value));
  }

  CallbackType callback_;
};

template <typename Callback>
std::unique_ptr<TypeCallback> CreateCustomTypeCallback(Callback&& cb) {}

// A helper subclass that impl dest-type and value-type.
template <typename T, typename V>
class ActionCallbackBase : public ActionCallback {
 public:
  using DestType = T;
  using ValueType = V;

  // explicit ActionCallbackBase(T* dest) : ActionCallback(dest) {}
  std::type_index GetDestType() override { return typeid(DestType); }
  std::type_index GetValueType() override { return typeid(ValueType); }

 protected:
  // Helpers for subclass.
  DestType* dest() { return reinterpret_cast<DestType*>(dest_); }
  ValueType ValueOf(std::any data) {
    DCHECK(data.has_value());
    DCHECK(std::type_index(data.type()) == GetValueType());
    return std::any_cast<ValueType>(data);
  }
};

// This impls store and store-const.
template <typename T, typename V = T>
class StoreActionCallback : public ActionCallbackBase<T, V> {
 private:
  void RunImpl(std::any data) override {
    if (data.has_value()) {
      *(this->dest()) = this->ValueOf(data);
    }
  }
};

template <typename T, typename V = ValueTypeOf<T>>
class AppendActionCallback : public ActionCallbackBase<T, V> {
 private:
  using Traits = AppendTraits<T, V>;
  void RunImpl(std::any data) override {
    if (data.has_value()) {
      Traits::Append(this->dest(), this->ValueOf(data));
    }
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
    std::optional<V> value;
    if (data.has_value())
      value.emplace(this->ValueOf(data));
    DCHECK(callback_);
    std::invoke(callback_, this->dest(), std::move(value));
  }

  CallbackType callback_;
};

template <typename Callback>
std::unique_ptr<ActionCallback> CreateCustomActionCallback(Callback&& cb) {}

class CallbackRunner {
 public:
  virtual Status Run(Context* ctx) = 0;
  virtual ~CallbackRunner() {}
};

// Used for no dest, no type no action.
class DummyCallbackRunner : public CallbackRunner {
 public:
  Status Run(Context*) override { return true; }
  ~DummyCallbackRunner() override {}
};

// Run TypeCallback and ActionCallback.
class CallbackRunnerImpl : public CallbackRunner,
                           private TypeCallback::Delegate {
 public:
  CallbackRunnerImpl(CallbackFactory* factory, DestInfo* dest)
      : type_(factory->CreateTypeCallback()),
        action_(factory->CreateActionCallback()) {
    DCHECK(dest->GetDestType() == action_->GetDestType());
    DCHECK(type_->GetValueType() == action_->GetValueType());
    action_->BindToDest(dest);
  }

  Status Run(Context* ctx) override {
    type_->Run(ctx, this);
    return std::move(status_);
  }
  ~CallbackRunnerImpl() override {}

 private:
  void OnTypeCallbackInvoked(Status status, std::any data) override {
    if (status) {
      action_->Run(std::move(data));
    } else {
      status_ = std::move(status);
    }
  }
  Status status_;
  std::unique_ptr<TypeCallback> type_;
  std::unique_ptr<ActionCallback> action_;
};

// A DestInfo without T*, which is suitable for actions like print_help or
// print_usage.
class VoidDestInfo : public DestInfo {
 public:
  std::type_index GetDestType() override { return typeid(void); }
  CallbackFactory* CreateFactory(Actions action) override;
};

template <typename T>
class DestInfoImpl : public DestInfo {
 public:
  explicit DestInfoImpl(T* ptr) : DestInfo(ptr) {}
  std::type_index GetDestType() override { return typeid(T); }

  CallbackFactory* CreateFactory(Actions action) override {
    switch (action) {
      case Actions::kStore:
        return CallbackFactorySelector<T, Actions::kStore>::Select();
      case Actions::kAppend:
        return CallbackFactorySelector<T, Actions::kAppend>::Select();
      default:
        return nullptr;
    }
  }

  CallbackFactory* CreateFactoryWithValue(Actions action,
                                          std::any value) override {
    switch (action) {
      case Actions::kStoreConst:
        return CallbackFactorySelector<T, Actions::kStore>::Select(value);
      case Actions::kAppendConst:
        return CallbackFactorySelector<T, Actions::kAppend>::Select(value);
      default:
        return nullptr;
    }
  }

  // T* ptr() { return reinterpret_cast<T*>(ptr_); }
};

template <typename T>
struct CallbackFactorySelector<T, Actions::kStore, true> {
  static CallbackFactory* Select() {
    class FactoryImpl : public CallbackFactory {
     public:
      ~FactoryImpl() override {}
      ActionCallback* CreateActionCallback() override {
        return new StoreActionCallback<T>();
      }
      TypeCallback* CreateTypeCallback() override {
        return new DefaultTypeCallback<T>();
      }
    };
    return new FactoryImpl();
  }
};

template <typename T>
struct CallbackFactorySelector<T, Actions::kAppend, true> {
  static CallbackFactory* Select() {
    class FactoryImpl : public CallbackFactory {
     public:
      ~FactoryImpl() override {}
      ActionCallback* CreateActionCallback() override {
        return new AppendActionCallback<T>();
      }
      TypeCallback* CreateTypeCallback() override {
        return new DefaultTypeCallback<ValueTypeOf<T>>();
      }
    };
    return new FactoryImpl();
  }
};

template <typename T>
struct CallbackFactorySelector<T, Actions::kStoreConst, true> {
  class FactoryImpl : public CallbackFactory {
   public:
    explicit FactoryImpl(std::any value) : value_(std::any_cast<T>(value)) {}
    ~FactoryImpl() override {}
    ActionCallback* CreateActionCallback() override {
      return new StoreActionCallback<T>();
    }
    TypeCallback* CreateTypeCallback() override {
      return new ConstTypeCallback<T>(value_);
    }

   private:
    T value_;
  };

  static CallbackFactory* Select(std::any value) {
    return new FactoryImpl(std::move(value));
  }
};

struct Type {
  std::unique_ptr<TypeCallback> callback;
  Type() = default;
  // Type(Type&&) = default;

  // type(int()) or type(float())
  template <typename T>
  Type(T&&) {
    callback.reset(new DefaultTypeCallback<T>());
  }

  template <typename Callback,
            typename Enable = detail::function_signature_t<Callback>>
  /* implicit */ Type(Callback&& cb) {
    callback = CreateCustomTypeCallback(std::forward<Callback>(cb));
  }
};

inline Actions StringToActions(const std::string& str) {
  static const std::map<std::string, Actions> map{
      {"store", Actions::kStore},
      {"store_const", Actions::kStoreConst},
      {"append", Actions::kAppend},
      {"append_const", Actions::kAppendConst},
  };
  auto iter = map.find(str);
  DCHECK2(iter != map.end(), "Unknown action string passed in");
  return iter->second;
}

// Type-erasured
struct Action {
  std::unique_ptr<ActionCallback> callback;
  Actions action = Actions::kNoAction;

  Action() = default;

  Action(const char* action_string) {
    DCHECK(action_string);
    action = StringToActions(action_string);
  }

  // Action(Action&&) = default;

  // TODO:: restrict signature.
  template <typename Callback,
            typename Enable = detail::function_signature_t<Callback>>
  /* implicit */ Action(Callback&& cb) {
    action = Actions::kCustom;
    callback = CreateCustomActionCallback(std::forward<Callback>(cb));
  }

  bool Check() const { return action != Actions::kCustom || callback; }
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
  virtual void SetAction(Action action) = 0;
  virtual CallbackRunner* CreateCallbackRunner() = 0;
  virtual ~CallbackResolver() {}
};

class CallbackResolverImpl : public CallbackResolver {
 public:
  void SetDest(Dest dest) override {
    DCHECK(dest.dest_info);
    dest_ = std::move(dest.dest_info);
  }
  void SetType(Type type) override {
    DCHECK(type.callback);
    custom_type_ = std::move(type.callback);
  }
  void SetAction(Action action) override {
    DCHECK(action.Check());
    action_ = action.action;
    custom_action_ = std::move(action.callback);
  }
  CallbackRunner* CreateCallbackRunner() override {

  }

 private:
  Actions action_ = Actions::kNoAction;
  std::unique_ptr<DestInfo> dest_;
  std::unique_ptr<TypeCallback> custom_type_;
  std::unique_ptr<ActionCallback> custom_action_;
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

  // TODO: this should allow a single option.
  // For positinal argument, only one name is allowed.
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

  // For optional argument, a couple of names are allowed, including alias.
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

class ArgumentInterace {
 public:
  virtual bool IsOption() = 0;
  virtual void FormatArgsDoc(std::ostream& os) = 0;
};

// A delegate used by the Argument class.
class ArgumentDelegate {
 public:
  // Generate a key for an option.
  virtual int NextOptionKey() = 0;
  // Call when an Arg is fully constructed.
  virtual void OnArgumentCreated(Argument*) = 0;
  virtual ~ArgumentDelegate() {}
};

// Holds all meta-info about an argument.
// For now this is a union of Option, Positional and Group.
// In the future this can be three classes.
class Argument {
 public:
  Argument(ArgumentDelegate* delegate, const Names& names, int group)
      : delegate_(delegate), group_(group) {
    InitNames(names);
    InitKey();
    delegate_->OnArgumentCreated(this);
  }

  void SetHelpDoc(const char* help_doc) {
    if (help_doc)
      help_doc_ = help_doc;
  }

  void SetRequired(bool required) { is_required_ = required; }
  void SetMetaVar(const char* meta_var) { meta_var_ = meta_var; }

  bool initialized() const { return key_ != 0; }
  int key() const { return key_; }
  int group() const { return group_; }
  bool is_option() const { return key_ != kKeyForPositional; }
  bool is_required() const { return is_required_; }
  // UserCallback* user_callback() const { return user_callback_.get(); }

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

  CallbackResolver* GetCallbackResolver() { return callback_resolver_.get(); }

  // [--name|-n|-whatever=[value]] or output
  void FormatArgsDoc(std::ostringstream& os) const {
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

  void CompileToArgpOptions(std::vector<ArgpOption>* options) const {
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
  void InitKey() {
    if (!is_option()) {
      key_ = kKeyForPositional;
      return;
    }
    key_ = short_names_.empty() ? delegate_->NextOptionKey() : short_names_[0];
  }

  Status Finalize() {
    DCHECK(!callback_runner_ && callback_resolver_);
    callback_runner_.reset(callback_resolver_->CreateCallbackRunner());
    callback_resolver_.reset();
    return true;
  }

  // Put here to record some metics.
  Status RunUserCallback(const Context& ctx) {
    if (!user_callback_)  // User doesn't provide any.
      return true;
    return user_callback_->Run(ctx);
  }

  // For extension.
  ArgumentDelegate* delegate_;
  // For positional, this is -1, for group-header, this is -2.
  int key_ = kKeyForNothing;
  int group_ = 0;
  // std::optional<Dest> dest_;                     // Maybe null.
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
  ArgumentBuilder& help(const char* h) {
    arg_->SetHelpDoc(h);
    return *this;
  }
  ArgumentBuilder& required(bool b) {
    arg_->SetRequired(b);
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

using HelpFilterCallback = std::function<
    HelpFilterResult(const Argument&, const std::string&, std::string* repl)>;

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
  // XXX: what is the use of help_filter?

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

// TODO: this shouldn't inherit ArgumentContainer as it is a public interface.
class ArgumentHolder : public ArgumentContainer,
                       public ArgumentDelegate,
                       public ArgpParser::Delegate {
 public:
  ArgumentHolder() {
    AddGroup("optional arguments");
    AddGroup("positional arguments");
  }

  // Create a new group.
  int AddGroup(const char* header) {
    int group = groups_.size() + 1;
    groups_.emplace_back(group, header);
    return group;
  }

  // Add an arg to a specific group.
  Argument* AddArgumentToGroup(Names names, int group) {
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

  ArgumentGroup add_argument_group(const char* header);

  const std::map<int, Argument*>& optional_arguments() const {
    return optional_arguments_;
  }
  const std::vector<Argument*>& positional_arguments() const {
    return positional_arguments_;
  }

  // ArgpParser::Delegate:
  static constexpr int kAverageAliasCount = 4;

  void CompileToArgpOptions(std::vector<ArgpOption>* options) override {
    if (arguments_.empty())
      return;

    const unsigned option_size =
        arguments_.size() + groups_.size() + kAverageAliasCount + 1;
    options->reserve(option_size);

    for (const Group& group : groups_) {
      group.CompileToArgpOption(options);
    }

    // TODO: if there is not pos/opt at all but there are two groups, will argp
    // still print these empty groups?
    for (const Argument& arg : arguments_) {
      arg.CompileToArgpOptions(options);
    }
    // Only when at least one opt/pos presents should we generate their groups.
    options->push_back({});

    // PrintArgpOptionArray(*options);
  }

  static bool CompareArguments(const Argument* a, const Argument* b) {
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

  void GenerateArgsDoc(std::string* args_doc) override {
    std::vector<Argument*> args(arguments_.size());
    std::transform(arguments_.begin(), arguments_.end(), args.begin(),
                   [](Argument& arg) { return &arg; });
    std::sort(args.begin(), args.end(), &CompareArguments);

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

 private:
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

  Group* GroupFromID(int group) {
    DCHECK(group <= groups_.size());
    return &groups_[group - 1];
  }

  // ArgumentDelegate:
  int NextOptionKey() override { return next_key_++; }

  void OnArgumentCreated(Argument* arg) override {
    DCHECK(arg->initialized());
    GroupFromID(arg->group())->AddMember();
    if (arg->is_option()) {
      bool inserted = optional_arguments_.emplace(arg->key(), arg).second;
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
  std::list<Argument> arguments_;
  // indexed by their define-order.
  std::vector<Argument*> positional_arguments_;
  // indexed by their key.
  std::map<int, Argument*> optional_arguments_;
  // groups must be random-accessed.
  std::vector<Group> groups_;
  // Conflicts checking.
  std::set<std::string> name_set_;
};

// Impl add_group() call.
class ArgumentGroup : public ArgumentContainer {
 private:
  Argument* AddArgument(Names names) override {
    return holder_->AddArgumentToGroup(std::move(names), group_);
  }

  ArgumentGroup(ArgumentHolder* holder, int group)
      : holder_(holder), group_(group) {}

  // Only ArgumentHolder can create this.
  friend class ArgumentHolder;
  int group_;
  ArgumentHolder* holder_;
};

ArgumentGroup ArgumentHolder::add_argument_group(const char* header) {
  int group = AddGroup(header);
  return ArgumentGroup(this, group);
}

// This handles the argp_parser_t function and provide a bunch of context during
// the parsing.
class ArgpParserImpl : public ArgpParser {
 public:
  // When this is constructed, Delegate must have been added options.
  explicit ArgpParserImpl(Delegate* delegate) : delegate_(delegate) {
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
    auto status = arg->RunUserCallback(ctx);
    // If the user said no, just die with a msg.
    if (status)
      return;
    // state.Error("%s", status.message().c_str());
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
    if (!input)
      return (char*)text;
    auto* self = reinterpret_cast<ArgpParserImpl*>(input);
    DCHECK2(self->help_filter_,
            "should only be called if user install help filter!");
    auto* arg = self->delegate_->FindOptionalArgument(key);
    DCHECK2(arg, "argp calls us with unknown key!");

    std::string repl;
    HelpFilterResult result =
        std::invoke(self->help_filter_, *arg, text, &repl);
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
  Delegate* delegate_;
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

class ArgumentParser : private ArgumentHolder {
 public:
  ArgumentParser() = default;

  explicit ArgumentParser(const Options& options) : user_options_(options) {}

  Options& options() { return user_options_; }

  using ArgumentHolder::add_argument;
  using ArgumentHolder::add_argument_group;

  // argp wants a char**, but most user don't expect argv being changed. So
  // cheat them.
  void parse_args(int argc, const char** argv) {
    auto parser = ArgpParser::Create(this);
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
  Options user_options_;
};

}  // namespace argparse
