#pragma once

#include <argp.h>
#include <algorithm>
#include <any>
#include <cassert>
#include <cstdlib>  // malloc()
#include <cstring>  // strlen()
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

class DestPtr;
class StoreOps;
class OpenFileOps;
class ParseOps;
class AppendOps;

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

// Our version of any.
class Any {
 public:
  virtual ~Any() {}
  virtual std::type_index GetType() = 0;
};

template <typename T>
class AnyImpl : public Any {
 public:
  explicit AnyImpl(T&& val) : value_(std::move(val)) {}
  explicit AnyImpl(const T& val) : value_(val) {}
  ~AnyImpl() override {}
  std::type_index GetType() override { return typeid(T); }

  T ReleaseValue() { return MoveOrCopy(&value_); }

  static AnyImpl* FromAny(Any* any) {
    DCHECK(any && any->GetType() == typeid(T));
    return static_cast<AnyImpl*>(any);
  }

 private:
  T value_;
};

template <typename T>
void WrapAny(T&& in, std::unique_ptr<Any>* out) {
  using Type = std::remove_cv_t<std::remove_reference_t<T>>;
  out->reset(new AnyImpl<Type>(std::forward<T>(in)));
}

// Wrap T into Any.
template <typename T>
void WrapAny(Result<T>* result, std::unique_ptr<Any>* out) {
  if (result->has_value())
    out->reset(new AnyImpl<T>(result->release_value()));
  // else
  //   out->reset();
}

// Steal the T from Any and store into Result<T>.
template <typename T>
void UnwrapAny(std::unique_ptr<Any> any, Result<T>* out) {
  if (any) {
    auto* any_impl = AnyImpl<T>::FromAny(any.get());
    out->set_value(any_impl->ReleaseValue());
  } else {
    out->reset();
  }
}

template <typename T>
void UnwrapAny(std::unique_ptr<Any> any, T* out) {
  if (any) {
    auto* any_impl = AnyImpl<T>::FromAny(any.get());
    *out = any_impl->ReleaseValue();
  }
}

// This is an internal class to communicate data/state between user's callback.
struct Context {
  Context(const Argument* argument, const char* value, ArgpState state)
      : has_value(bool(value)), argument(argument), state(state) {
    if (has_value)
      this->value.assign(value);
  }

  const bool has_value;
  const Argument* argument;
  ArgpState state;
  std::string value;
};

// The default impl for the types we know (bulitin-types like int).
// This traits shouldn't be overriden by users.
template <typename T, typename SFINAE = void>
struct DefaultParserTraits {
  // This is selected when user use a custom type without specializing
  // TypeCallbackTraits.
  static void Run(const std::string&, Result<T>* out) {
    DCHECK2(false, "Please specialize ParserTraits<T> for your type");
  }
};

// By default, use the traits defined by the library for builtin types.
// The user can specialize this to provide traits for their custom types
// or override global (existing) types.
template <typename T>
struct ParserTraits : DefaultParserTraits<T> {};

inline bool AnyHasType(const std::any& val, std::type_index type) {
  return type == val.type();
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
struct AppendTraits : std::false_type {};  // Not supported.

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
  kStoreTrue,
  kStoreFalse,
  kAppend,
  kAppendConst,
  kPrintHelp,
  kPrintUsage,
  kCustom,
};

// class CallbackFactory {
//  public:
//   virtual ActionCallback* CreateActionCallback() = 0;
//   virtual TypeCallback* CreateTypeCallback() = 0;
//   virtual ~CallbackFactory() {}
// };

template <typename Ops, typename T>
struct IsOpsSupported : std::false_type {};

// This is a meta-function that tests if A can be performed on T.
// template <typename T, Actions A>
// struct IsActionSupported : std::false_type {};

// Store will copy or move the value produced by type() into dest.
// template <typename T>
// struct IsActionSupported<T, Actions::kStore>
//     : std::bool_constant<std::is_copy_assignable<T>{} ||
//                          std::is_move_assignable<T>{}> {};

template <typename T>
struct IsOpsSupported<StoreOps, T>
    : std::bool_constant<std::is_copy_assignable<T>{} ||
                         std::is_move_assignable<T>{}> {};

template <typename T>
struct IsOpsSupported<AppendOps, T> : IsAppendSupported<T> {};

template <typename T>
struct IsFileLike : std::false_type {};

template <typename T>
struct IsOpsSupported<OpenFileOps, T> : IsFileLike<T> {};

// Store Const will copy the const into dest. Since the action may be taken many
// times, T must be copy-assignable.
// template <typename T>
// struct IsActionSupported<T, Actions::kStoreConst>
//     : std::is_copy_constructible<T> {};

// template <typename T>
// struct IsActionSupported<T, Actions::kAppend> : IsAppendSupported<T> {};

// template <typename T>
// struct IsActionSupported<T, Actions::kAppendConst>
//     : std::bool_constant<IsAppendSupported<T>{} &&
//                          std::is_copy_constructible<T>{}> {};

// This is a meta-function that handles static-runtime mix of selecting action
// factory.
// template <typename T, Actions A, bool Supported = IsActionSupported<T, A>{}>
// struct CallbackFactorySelector;

// template <typename T, Actions A>
// struct CallbackFactorySelector<T, A, false> /* Not supported */ {
//   static CallbackFactory* Run() { return nullptr; }
// };

// class OpsFactory {
//  public:
//   virtual ~OpsFactory() {}
//   virtual StoreOps* CreateStoreOps() = 0;
//   virtual AppendOps* CreateAppendOps() = 0;
//   virtual OpenFileOps* CreateOpenFileOps() = 0;
//   virtual ParseOps* CreateParseOps() = 0;
//   virtual ParseOps* CreateParseOpsForValueType() = 0;
// };

// template <typename Ops,
//           template <typename>
//           class OpsImpl,
//           typename T,
//           bool ok = IsOpsSupported<Ops, T>{}>
// struct OpsFactoryHelper;

// template <typename Ops, template <typename> class OpsImpl, typename T>
// struct OpsFactoryHelper<Ops, OpsImpl, T, false> {
//   static Ops* Create() { return nullptr; }
// };
// template <typename Ops, template <typename> class OpsImpl, typename T>
// struct OpsFactoryHelper<Ops, OpsImpl, T, true> {
//   static Ops* Create() { return new OpsImpl<T>(); }
// };

class DestPtr {
 public:
  template <typename T>
  explicit DestPtr(T* ptr) : type_(typeid(T)), ptr_(ptr) {}
  DestPtr() = default;

  // Copy content to out.
  template <typename T>
  void load(T* out) const {
    DCHECK(type_ == typeid(T));
    *out = *reinterpret_cast<T*>(ptr_);
  }

  template <typename T>
  void load_ptr(T** ptr_out) const {
    DCHECK(type_ == typeid(T));
    *ptr_out = reinterpret_cast<T*>(ptr_);
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

// File open mode.
enum class Mode : unsigned {
  kRead = 0x0,
  kWrite = 0x1,
  kAppend = 0x2,
  kTruncate = 0x4,
};

// # Ops classes.
// Ops classes is a finer-grained erasure of different operations supported by
// different types. The factory of Ops is DestInfo, which creates various known
// subclasses of them to support both type-callback and action-callback. Ops
// aims to be the common mechanism underlying both type and action. By moving
// the type-operation logic into ops, type and action are allows to be more
// complicated (not just as a simple type eraser). Subclasses of them now can
// contain customized data and require non-default ctors to support states.
// This also enable a cleaner design, by adding one layer between the upper one
// (type and action) the lower one (the operations defined by a T) and thus
// reuse code, like the StoreConst and Store both use the StoreOps, etc.
// DestInfo now becomes a factory of Ops.
// Ops only defines the functionality naturally supported by a T (deducted from
// T). User's functor is not supported. Think of Ops as a builtin set of
// minimal function offered by the lib.

class DestInfo {
//  public:
//   template <typename T>
//   explicit DestInfo(T* ptr)
//       : ops_factory_(new OpsFactoryImpl<T>), dest_ptr_(ptr) {}

//   OpsFactory* GetOpsFactory() { return ops_factory_.get(); }
//   DestPtr GetDestPtr() { return dest_ptr_; }

//  private:
//   std::unique_ptr<OpsFactory> ops_factory_;
//   DestPtr dest_ptr_;
};

// The function pointer table for Ops impl.
// For each type used by user, generate a vtable for all available operations.
// Null entries in the table means this op is not supported (or banned) for this
// type. For example, it is banned to open a file and turn it into a double, but
// it might be true that an opened file can be turned into an int (fd).
// The table for each type is shared once created.
class Operations {
 public:
  struct OpsResult {
    std::unique_ptr<Any> value;  // null if error.
    std::string errmsg;
  };

  virtual void Store(DestPtr dest, std::unique_ptr<Any> data) = 0;
  virtual void StoreConst(DestPtr dest, const Any& data) = 0;
  virtual void Append(DestPtr dest, std::unique_ptr<Any> data) = 0;
  virtual void AppendConst(DestPtr dest, const Any& data) = 0;

  virtual void Parse(const std::string& in, OpsResult* out) = 0;
  virtual void Open(const std::string& in, Mode, OpsResult* out) = 0;

  virtual Operations* ValueTypeOps() = 0;
  virtual ~Operations() {}
};

class OpsFactory;
class OpsTypeMap;

class OpsTypeMap {
 public:
  // If not found, return null.
  virtual Operations* Find(std::type_index index) = 0;
  // If not found, create a new one with factory.
  virtual Operations* SetDefault(std::type_index index,
                                 OpsFactory* factory) = 0;
  virtual ~OpsTypeMap() {}
};

// How to create a vtable?
class OpsFactory {
 public:
  virtual std::unique_ptr<Operations> Create(OpsTypeMap* map) = 0;
  virtual ~OpsFactory() {}
};

template <typename T>
class OperationsImpl : public Operations {
 public:
  void Store(DestPtr dest, std::unique_ptr<Any> data);
  void StoreConst(DestPtr dest, const Any& data);
  void Append(DestPtr dest, std::unique_ptr<Any> data);
  void AppendConst(DestPtr dest, const Any& data);

  void Parse(const std::string& in, OpsResult* out);
  void Open(const std::string& in, Mode, OpsResult* out);

  Operations* ValueTypeOps();
};

class OpsTypeMapImpl : public OpsTypeMap {
 public:
  Operations* Find(std::type_index index) override { return nullptr; }
  Operations* SetDefault(std::type_index index, OpsFactory* factory) override {
    return nullptr;
  }

 private:
  std::map<std::type_index, std::unique_ptr<Operations>> map_;
};

// template <ty
  // For defining actual functions.
  // enum Kind {
  //   kStore,
  //   kAppend,
  //   kParse,
  //   kParseValueType,
  //   kOpenFile,
  // };

// template <typename T, Operations::Kind OpsKind>
// struct IsOpsEnabled : std::false_type {};

// template <typename T,
//           Operations::Kind OpsKind,
//           bool Enabled = IsOpsEnabled<T, OpsKind>{}>
// struct OpsImpl;

// template <typename T>
// struct OpsImpl<T, Operations::kStore, false> {
//   static void Run(DestPtr, std::unique_ptr<Any>) {
//     DCHECK2(false, "StoreOps is not supported by this type");
//   }
// };
// template <typename T>
// struct OpsImpl<T, Operations::kStore, true> {
//   static void Run(DestPtr dest_ptr, std::unique_ptr<Any> data) {
//     // DCHECK2(false, "StoreOps is not supported by this type");
//   }
// };

template <typename T>
using TypeCallbackPrototype = void(const std::string&, Result<T>*);

// There is an alternative for those using exception.
// You convert string into T and throw ArgumentError if something bad happened.
template <typename T>
using TypeCallbackPrototypeThrows = T(const std::string&);

// The prototype for action. An action normally does not report errors.
template <typename T, typename V>
using ActionCallbackPrototype = void(T*, Result<V>);

// Perform data conversion..
class TypeCallback {
 public:
  struct ConversionResult {
    bool has_error = false;
    std::unique_ptr<Any> value;
    std::string msg;
  };
  virtual ~TypeCallback() {}
  virtual void Run(const std::string& in, ConversionResult* out) {}
};

class DefaultTypeCallback : public TypeCallback {
 public:
  explicit DefaultTypeCallback(Operations* ops) : ops_(ops) {}

  void Run(const std::string& in, ConversionResult* out) override {
    Operations::OpsResult ops_result;
    ops_->Parse(in, &ops_result);
    out->has_error = !ops_result.value;
    out->value = std::move(ops_result.value);
    out->msg = std::move(ops_result.errmsg);
  }

 private:
  Operations* ops_;
};

class FileTypeCallback : public TypeCallback {
 public:
  explicit FileTypeCallback(Operations* ops) : ops_(ops) {}
  void SetMode(Mode mode) { mode_ = mode; }

  void Run(const std::string& in, ConversionResult* out) override {
    Operations::OpsResult ops_result;
    ops_->Open(in, mode_, &ops_result);

    out->has_error = !ops_result.value;
    out->value = std::move(ops_result.value);
    out->msg = std::move(ops_result.errmsg);
  }

 private:
  Operations* ops_;
  Mode mode_;
};

template <typename T>
class CustomTypeCallback : public TypeCallback {
 public:
  using CallbackType = std::function<TypeCallbackPrototype<T>>;
  explicit CustomTypeCallback(CallbackType cb) : callback_(std::move(cb)) {}

  void Run(const std::string& in, ConversionResult* out) override {
    Result<T> user_result;
    std::invoke(callback_, in, &user_result);
    out->has_error = user_result.has_error();
    if (out->has_error)
      out->msg = user_result.release_error();
    else if (user_result.has_value())
      WrapAny(&user_result, &out->value);
  }

 private:
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
  return new CustomTypeCallback<T>([cb](const std::string& in, Result<T>* out) {
    try {
      *out = std::invoke(cb, in);
    } catch (const ArgumentError& e) {
      out->set_error(e.what());
    }
  });
}

template <typename Callback>
TypeCallback* CreateCustomTypeCallback(Callback&& cb) {
  return CreateCustomTypeCallbackImpl(
      std::forward<Callback>(cb),
      (detail::function_signature_t<Callback>*)nullptr);
}

class ActionCallback {
 public:
  virtual ~ActionCallback() {}
  virtual void Run(std::unique_ptr<Any> any) = 0;

  void SetDest(DestPtr dest_ptr) {
    DCHECK2(dest_ptr, "dest_ptr should not be null");
    dest_ptr_ = dest_ptr;
  }

  // Set an const value to this action (must be a valid value.)
  void SetConstValue(std::unique_ptr<Any> val) {
    DCHECK2(val, "const_value should not be null");
    const_value_ = std::move(val);
  }

  const DestPtr& dest() const { return dest_ptr_; }
  const Any& const_value() const { return *const_value_; }

 private:
  DestPtr dest_ptr_;
  std::unique_ptr<Any> const_value_;
};

class DefaultActionCallback : public ActionCallback {
 public:
  DefaultActionCallback(Operations* ops, Actions action)
      : ops_(ops), action_(action) {}

  void Run(std::unique_ptr<Any> data) override {
    switch (action_) {
      case Actions::kStore:
        ops_->Store(dest(), std::move(data));
        break;
      case Actions::kStoreConst:
        ops_->StoreConst(dest(), const_value());
        break;
      case Actions::kStoreTrue:
        ops_->StoreConst(dest(), AnyImpl<bool>(true));
        break;
      case Actions::kStoreFalse:
        ops_->StoreConst(dest(), AnyImpl<bool>(false));
        break;
      case Actions::kAppend:
        ops_->Append(dest(), std::move(data));
        break;
      case Actions::kAppendConst:
        ops_->AppendConst(dest(), const_value());
        break;
    }
  }

 private:
  Operations* ops_;
  Actions action_;
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
  void Run(std::unique_ptr<Any> data) override {
    // std::invoke(callback_, this->dest(), std::move(data));
  }

  CallbackType callback_;
};

template <typename Callback, typename T, typename V>
ActionCallback* CreateCustomActionCallbackImpl(Callback&& cb,
                                               void (*)(T*, Result<V>)) {
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
    virtual void HandleCallbackError(Context* ctx, const std::string& msg) = 0;
  };
  virtual void Run(Context* ctx, Delegate* delegate) = 0;
  virtual ~CallbackRunner() {}
};

// Used for no dest, no type no action.
// class DummyCallbackRunner : public CallbackRunner {
//  public:
//   void Run(Context*, Delegate*) override {}
//   ~DummyCallbackRunner() override {}
// };

// XXX: this schema is kept.
// Run TypeCallback and ActionCallback.
class CallbackRunnerImpl : public CallbackRunner {
 public:
  CallbackRunnerImpl(std::unique_ptr<TypeCallback> type,
                     std::unique_ptr<ActionCallback> action)
      : type_(std::move(type)), action_(std::move(action)) {}

  void Run(Context* ctx, Delegate* delegate) override;

 private:
  std::unique_ptr<TypeCallback> type_;
  std::unique_ptr<ActionCallback> action_;
};

// // Instruction for how to run TypeCallback.
// enum class TypeCallbackKind {
//   kDoNothing,
//   kParse,
//   kParseValueType,
//   kOpenFile,
//   kCustom,
// };

// CallbackRunner as an interpreter.
// Instead of using various subclasses of ActionCallback and TypeCallback to
// impl builtin actions and types, as well as custom versions, we can use
// something like interpreter -- all states in one class and do a switch() based
// on instructions (enum value). Then Action/TypeCallback is for wrapping user's
// callback, as their names note.
// class OpsCallbackRunner : public CallbackRunner {
//  public:
//   void Run(Context* ctx, Delegate* delegate) {}

//  private:
//   void RunTypeCallback(const std::string& in, Operations::OpsResult* out) {
//     switch (type_kind_) {
//       case TypeCallbackKind::kParse:
//         ops_->parse(in, out);
//         break;
//       case TypeCallbackKind::kParseValueType:
//         ops_->parse_value_type(in, out);
//         break;
//       case TypeCallbackKind::kOpenFile:
//         ops_->open_file(in, open_mode_, out);
//         break;
//       case TypeCallbackKind::kCustom:
//         // TODO:
//         custom_type_->Run(in, nullptr);
//         break;
//       case TypeCallbackKind::kDoNothing:
//         break;
//     }
//   }

//   void RunActionCallback(std::unique_ptr<Any> data) {
//     switch (action_) {
//       case Actions::kCustom:
//         // custom_action_->Run()
//         break;
//       default:
//         break;
//     }
//   }

//   Actions action_;
//   TypeCallbackKind type_kind_;
//   Mode open_mode_;  // For FileType.
//   const Operations* ops_;
//   DestPtr dest_ptr_;
//   std::unique_ptr<Any> const_value_;  // copyable.
//   std::unique_ptr<ActionCallback> custom_action_;
//   std::unique_ptr<TypeCallback> custom_type_;
// };

template <typename T>
class DestInfoImpl : public DestInfo {
 public:
 private:
  // std::unique_ptr<OpsFactory> ops_factory_;
  // DestPtr
  // explicit DestInfoImpl(T* ptr) : DestInfo(ptr) {}
  // std::type_index GetDestType() override { return typeid(T); }

  // CallbackFactory* CreateFactory(Actions action) override {
  //   switch (action) {
  //     case Actions::kStore:
  //       return CallbackFactorySelector<T, Actions::kStore>::Run();
  //     case Actions::kAppend:
  //       return CallbackFactorySelector<T, Actions::kAppend>::Run();
  //     case Actions::kStoreConst:
  //       return CallbackFactorySelector<T, Actions::kStoreConst>::Run();
  //     case Actions::kAppendConst:
  //       return CallbackFactorySelector<T, Actions::kAppendConst>::Run();
  //     default:
  //       return nullptr;
  //   }
  // }
};

// template <typename ActionCallbackT, typename TypeCallbackT>
// struct CallbackFactoryGenerator {
//   static_assert(std::is_base_of<ActionCallback, ActionCallbackT>{});
//   static_assert(std::is_base_of<TypeCallback, TypeCallbackT>{});

//   static CallbackFactory* Run() {
//     class FactoryImpl : public CallbackFactory {
//      public:
//       ActionCallback* CreateActionCallback() override {
//         return new ActionCallbackT();
//       }
//       TypeCallback* CreateTypeCallback() override {
//         return new TypeCallbackT();
//       }
//     };
//     return new FactoryImpl();
//   }
// };

// template <typename T>
// struct CallbackFactorySelector<T, Actions::kStore, true>
//     : CallbackFactoryGenerator<StoreActionCallback<T>,
//     DefaultTypeCallback<T>> {
// };

// template <typename T>
// struct CallbackFactorySelector<T, Actions::kAppend, true>
//     : CallbackFactoryGenerator<AppendActionCallback<T>,
//                                DefaultTypeCallback<ValueTypeOf<T>>> {};

// template <typename T>
// struct CallbackFactorySelector<T, Actions::kStoreConst, true>
//     : CallbackFactoryGenerator<StoreConstActionCallback<T>, NullTypeCallback> {
// };

// template <typename T>
// struct CallbackFactorySelector<T, Actions::kAppendConst, true>
//     : CallbackFactoryGenerator<AppendConstActionCallback<T>, NullTypeCallback> {
// };

struct Type {
  std::unique_ptr<TypeCallback> callback;
  Type() = default;
  Type(Type&&) = default;

  Type(TypeCallback* cb) : callback(cb) {}

  template <typename Callback>
  /* implicit */ Type(Callback&& cb) {
    callback.reset(CreateCustomTypeCallback(std::forward<Callback>(cb)));
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
    dest_info.reset(new DestInfo(ptr));
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

  CallbackRunner* CreateCallbackRunner() override;

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

  void InitOptions(std::initializer_list<const char*> names);
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
  virtual int GetKey() const = 0;
  virtual void FormatArgsDoc(std::ostream& os) const = 0;
  virtual void CompileToArgpOptions(
      std::vector<argp_option>* options) const = 0;
  virtual bool AppearsBefore(const Argument* that) const = 0;

  virtual ~Argument() {}
};

// Holds all meta-info about an argument.
class ArgumentImpl : public Argument {
 public:
  ArgumentImpl(Delegate* delegate, const Names& names, int group);

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
  int GetKey() const override { return key(); }

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
  void FormatArgsDoc(std::ostream& os) const override;

  void InitCallback() override {
    DCHECK(!callback_runner_ && callback_resolver_);
    callback_runner_.reset(callback_resolver_->CreateCallbackRunner());
    callback_resolver_.reset();
  }

  void CompileToArgpOptions(std::vector<argp_option>* options) const override;

  bool AppearsBefore(const Argument* that) const override {
    return CompareArguments(this, static_cast<const ArgumentImpl*>(that));
  }

 private:
  enum Keys {
    kKeyForNothing = 0,
    kKeyForPositional = -1,
  };

  // Fill in members to do with names.
  void InitNames(Names names);

  // Initialize the key member. Must be called after InitNames().
  void InitKey(bool is_option);

  static bool CompareArguments(const ArgumentImpl* a, const ArgumentImpl* b);

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
  // template <typename T>
  // ArgumentBuilder& type() {
  //   resolver_->SetType(new DefaultTypeCallback<T>());
  //   return *this;
  // }
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

using ProgramVersionCallback = void (*)(std::FILE*, argp_state*);

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

// This is an interface that provides add_argument() and other common things.
class ArgumentContainer {
 public:
  ArgumentBuilder add_argument(Names names,
                               Dest dest = {},
                               const char* help = {},
                               Action action = {}) {
    auto builder = ArgumentBuilder(AddArgument(std::move(names)));
    return builder.dest(std::move(dest)).help(help).action(std::move(action));
  }

  virtual ~ArgumentContainer() {}

 private:
  virtual Argument* AddArgument(Names names) = 0;
};

inline void PrintArgpOptionArray(const std::vector<argp_option>& options) {
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
  ArgumentHolderImpl();

  // Add an arg to a specific group.
  Argument* AddArgumentToGroup(Names names, int group) override;

  // TODO: since in most cases, parse_args() is only called once, we may
  // compile all the options in one shot before parse_args() is called and
  // throw the options array away after using it.
  Argument* AddArgument(Names names) override;

  ArgumentGroup AddArgumentGroup(const char* header) override;

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

  void CompileToArgpOptions(std::vector<argp_option>* options) override;
  void GenerateArgsDoc(std::string* args_doc) override;
  Argument* FindOptionalArgument(int key) override;
  Argument* FindPositionalArgument(int index) override;

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
    void IncRef() { ++members_; }
    void CompileToArgpOption(std::vector<argp_option>* options) const;

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

  void OnArgumentCreated(Argument* arg) override;

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
  explicit ArgpParserImpl(ArgpParser::Delegate* delegate);

  void Init(const Options& options) override;
  void ParseArgs(ArgArray args) override {
    // If any error happened, just exit the program. No need to check for return
    // value.
    argp_parse(&argp_, args.argc(), args.argv(), parser_flags_, nullptr, this);
  }

  bool ParseKnownArgs(ArgArray args, std::vector<std::string>* rest) override;

 private:
  void set_doc(const char* doc) { argp_.doc = doc; }
  void set_argp_domain(const char* domain) { argp_.argp_domain = domain; }
  void set_args_doc(const char* args_doc) { argp_.args_doc = args_doc; }
  void AddFlags(int flags) { parser_flags_ |= flags; }

  // TODO: Change this scheme.
  void RunCallback(Argument* arg, char* value, ArgpState state) {
    Context ctx(arg, value, state);
    arg->GetCallbackRunner()->Run(&ctx, this);
  }

  // CallbackRunner::Delegate:
  void HandleCallbackError(Context* ctx, const std::string& msg) override {
    // ctx->state.ErrorF("error parsing argument %s: %s", )
  }

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
  Options& version(const char* v) {
    options.program_version = v;
    return *this;
  }
  Options& version(ProgramVersionCallback callback) {
    options.program_version_callback = callback;
    return *this;
  }
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
  Options& email(const char* b) {
    options.email = b;
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
    std::vector<const char*> args_copy(args.begin(), args.end());
    return parse_args(args.size(), args_copy.data());
  }
  // TODO: parse_known_args()

  const char* program_name() const { return program_invocation_name; }
  const char* program_short_name() const {
    return program_invocation_short_name;
  }
  const char* program_version() const { return argp_program_version; }
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
struct DefaultParserTraits<std::string> {
  static void Run(const std::string& in, Result<std::string>* out) {
    *out = in;
  }
};
// char is an unquoted single character.
template <>
struct DefaultParserTraits<char> {
  static void Run(const std::string& in, Result<char>* out) {
    if (in.size() != 1)
      return out->set_error("char must be exactly one character");
    if (!std::isprint(in[0]))
      return out->set_error("char must be printable");
    *out = in[0];
  }
};
template <>
struct DefaultParserTraits<bool> {
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
struct DefaultParserTraits<T, std::enable_if_t<has_stl_number_parser_t<T>{}>> {
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

// Default handling of FILE* is to open it for reading.
template <>
struct DefaultParserTraits<FILE*> {
  static void Run(const std::string& in, Result<FILE*>* out) {}
};

// Handling of FileType.
// User can give FileType(mode) in type() to indicate he want to open a file.
// But the actual opening operation depends on dest.
// Thus we define a FileOpener interface to erase the opening logic once dest is
// given. And FileTypeCallback uses a FileOpener to do type-independent opening.

template <typename T>
struct EnumOpTraits {
  using U = std::underlying_type_t<T>;
  static T And(T a, T b) {
    return static_cast<T>(static_cast<U>(a) & static_cast<U>(b));
  }
  static T Or(T a, T b) {
    return static_cast<T>(static_cast<U>(a) | static_cast<U>(b));
  }
};

inline Mode operator&(Mode a, Mode b) {
  return EnumOpTraits<Mode>::And(a, b);
}
inline Mode operator|(Mode a, Mode b) {
  return EnumOpTraits<Mode>::Or(a, b);
}

// class FileOpener {
//  public:
//   struct OpenResult {
//     std::unique_ptr<Any> file;  // null if error.
//     std::string errmsg;
//   };
//   virtual ~FileOpener() {}
//   virtual void Open(const char* filename, Mode mode, OpenResult* result) = 0;
// };

// class CFileOpener : public FileOpener {
//  public:
//   void Open(const char* filename, Mode mode, OpenResult* result) override {
//     TranslateMode(mode);
//     FILE* f = std::fopen(filename, mode_chars_.c_str());
//     if (f) {
//       WrapAny(f, &result->file);
//       return;
//     }
//     // handling..
//     result->errmsg = "Failed to open file";
//   }

//  private:
//   void TranslateMode(Mode mode);
//   std::string mode_chars_;
// };

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