#pragma once

#include <argp.h>
#include <algorithm>
#include <cassert>
#include <cstring>
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

#define DCHECK(expr) assert(expr)
#define DCHECK2(expr, msg) assert(expr&& msg)

namespace argparse {

// Throw this exception will cause an error msg to be printed (via what()).
class ArgumentError final : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct Dest {
  template <typename T>
  explicit Dest(T* ptr) : type(typeid(*ptr)), ptr(ptr) {}

  std::type_index type;
  void* ptr;
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

  explicit operator bool() const { return success_; }
  const std::string& message() const { return message_; }

 private:
  bool success_;
  std::string message_;
};

// Primary info about an argument.
class Argument;

struct Context {
  // The Argument being parsed.
  const Argument* argument;
  // The command-line value to this argument.
  std::string value;
  // More fields may follow.
};

// When an argument is parsed, a UserCallback is fired.
// User can execute their code to collect infomation into their Dest.
class UserCallback {
 public:
  // Run this callback within the Context and return a status.
  Status Run(const Context& ctx) {
    // DCHECK2(dest_ptr_, "Bind() must be called before Run() can be called!");
    // dest is optional, if no dest given, the callback will be run with a
    // nullptr, if the user wishes so. This mostly happens with action, the user
    // may use void as T to avoid inconvenience.
    DCHECK2(dest_ptr_ || type_ == typeid(void),
            "If no dest was provided, you must use void as T");
    return RunImpl(ctx);
  }

  // Bind to a Dest. Make sure type matches.
  bool Bind(const Dest& dest) {
    DCHECK2(!dest_ptr_, "A UserCallback cannot be bound twice");
    if (dest.type != type_)
      return false;
    DCHECK(dest.ptr);
    dest_ptr_ = dest.ptr;
    return true;
  }

  virtual ~UserCallback() {}

 protected:
  virtual Status RunImpl(const Context& ctx) = 0;

  // subclass must call this.
  explicit UserCallback(std::type_index type) : type_(type) {}

  std::type_index type_;
  // The typed-pruned pointer of a Dest.
  void* dest_ptr_ = nullptr;
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

// When the user merely provides a dest, we will infer from the type of the
// pointer and provide this callback, which parses the string into the value of
// the type and store into the user's pointer.
template <typename T>
class DestUserCallback : public UserCallback {
 public:
  DestUserCallback() : UserCallback(typeid(std::declval<T>())) {}

 private:
  Status RunImpl(const Context& ctx) override {
    using Converter = DefaultConverter<T>;
    bool rv = Converter::Parse(ctx.value, reinterpret_cast<T*>(dest_ptr_));
    if (rv)
      return true;
    // Error reporting.
    return ReportError(ctx.value, Converter::type_name());
  }
};

// Report error by returning status.
template <typename T>
using CallbackNoExcept = std::function<Status(const Context&, T*)>;

template <typename T>
using CallbackMayThrow = std::function<T(const Context&)>;

// Report error by throwing exception.
template <typename T>
using CallbackMayThrowVoid = std::function<void(const Context&, T*)>;

// Optionally, user may parse user-defined types by providing a type argument.
// It can have two forms of signature:
// 1. T callback(const Context& ctx) throw(ArgumentError);
// 2. Status callback(const Context& ctx, T* out) noexcept;
template <typename T>
class TypeUserCallback : public UserCallback {
 public:
  // Report error by throwing exception.
  using TypeCallbackMayThrow = CallbackMayThrow<T>;
  // Report error by returning status.
  using TypeCallbackNoExcept = CallbackNoExcept<T>;

  explicit TypeUserCallback(TypeCallbackMayThrow callback)
      : TypeUserCallback(MayThrowToNoExceptAdapter{std::move(callback)}) {}

  explicit TypeUserCallback(TypeCallbackNoExcept callback)
      : UserCallback(typeid(std::declval<T>())),
        callback_(std::move(callback)) {}

private:
  Status RunImpl(const Context& ctx) override {
    return callback_(ctx, reinterpret_cast<T*>(dest_ptr_));
  }

  struct MayThrowToNoExceptAdapter {
    TypeCallbackMayThrow callback;
    Status operator()(const Context& ctx, T* out) {
      try {
        *out = callback(ctx);
        return true;
      } catch (const ArgumentError& e) {
        return Status(e.what());
      } catch (...) {
        // If some random thing gets thrown, report it.
        return Status("Unknown thing got thrown from UserCallback");
      }
    }
  };

  // We default use non-throwing callback.
  TypeCallbackNoExcept callback_;
};

// Finally, if the user wants to do arbitray things not limited by the pattern
// of converting and storing, he can provide an action, which is a callback with
// this signature: Status callback(const Context& ctx, T* out) Or void
// callback(const Context& ctx, T* out), which may throw an exception.
template <typename T>
class ActionUserCallback : public UserCallback {
 public:
  // Report error by throwing exception.
  using ActionCallbackMayThrow = CallbackMayThrowVoid<T>;
  // Report error by returning status.
  using ActionCallbackNoExcept = CallbackNoExcept<T>;
  /// XXX: It is worthy to provide two signatures?

  explicit ActionUserCallback(ActionCallbackMayThrow callback)
      : ActionUserCallback(ActionCallbackNoExcept(
            MayThrowToNoExceptAdapter{std::move(callback)})) {}

  explicit ActionUserCallback(ActionCallbackNoExcept callback)
      : UserCallback(typeid(std::declval<T>())),
        callback_(std::move(callback)) {}

 private:
  Status RunImpl(const Context& ctx) override {
    return callback_(ctx, reinterpret_cast<T*>(dest_ptr_));
  }

  struct MayThrowToNoExceptAdapter {
    ActionCallbackMayThrow callback;
    Status operator()(const Context& ctx, T* out) {
      try {
        callback(ctx, out);
        return true;
      } catch (const ArgumentError& e) {
        return Status(e.what());
      } catch (...) {
        // If some random thing gets thrown, report it.
        return Status("Unknown thing got thrown from UserCallback");
      }
    }
  };

  ActionCallbackNoExcept callback_;
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

// For a function type, extract its T.
template <typename Func>
struct extract_target_type;

template <typename T>
struct extract_target_type<T(const Context&)> {
  using type = T;
};

template <typename T>
struct extract_target_type<Status(const Context&, T*)> {
  using type = T;
};

template <typename T>
struct extract_target_type<void(const Context&, T*)> {
  using type = T;
};

template <typename T>
using extract_target_type_t = typename extract_target_type<T>::type;

}  // namespace detail

// Type-erasured
struct Action {
  std::unique_ptr<UserCallback> callback;
  Action() = default;
  Action(Action&&) = default;

  template <typename Func>
  /* implicit */ Action(Func&& func) {
    using Signature = detail::function_signature_t<Func>;
    using T = detail::extract_target_type_t<Signature>;
    callback.reset(new ActionUserCallback<T>(
        std::function<Signature>(std::forward<Func>(func))));
  }
};

struct Destination {
  std::optional<Dest> dest;
  std::unique_ptr<UserCallback> callback;
  Destination() = default;
  template <typename T>
  /* implicit */ Destination(T* ptr)
      : dest(Dest(ptr)), callback(new DestUserCallback<T>()) {
    DCHECK2(ptr, "nullptr passed to Destination()!");
  }
};

struct Type {
  std::unique_ptr<UserCallback> callback;
  Type() = default;
  Type(Type&&) = default;

  template <typename Func>
  /* implicit */ Type(Func&& func) {
    using Signature = detail::function_signature_t<Func>;
    using T = detail::extract_target_type_t<Signature>;
    callback.reset(new TypeUserCallback<T>(
        std::function<Signature>(std::forward<Func>(func))));
  }
};

// A valid option name is long or short option name and not '--', '-'.
// This is only checked once and true for good.
inline bool IsValidOptionName(const char* name, std::size_t len) {
  if (!name || len < 2 || name[0] != '-')
    return false;
  if (len == 2)  // This rules out -?, -* -@ -=
    return std::isalnum(name[1]);
  // check for long-ness.
  for (name += 2; *name; ++name) {
    if (*name == '-' || *name == '_' || std::isalnum(*name))
      continue;
    return false;
  }
  return true;
}

// inline bool IsValidOptionName(const std::string& name);

inline bool IsLongOptionName(const char* name, std::size_t len) {
  DCHECK(IsValidOptionName(name, len));
  return len > 2;
}

// inline bool IsLongOptionName(const std::string& name) {
//   return IsLongOptionName(name.c_str(), name.size());
// }

inline bool IsShortOptionName(const char* name, std::size_t len) {
  DCHECK(IsValidOptionName(name, len));
  return len == 2;
}

// inline bool IsLongOptionName(const std::string& name) {
//   return Is
// }

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

  // For positinal argument, only one name is allowed.
  Names(const char* positional) {
    is_option = false;
    std::string name(positional);
    meta_var = ToUpper(name);
    long_names.push_back(std::move(name));
  }

  // For optional argument, a couple of names are allowed, including alias.
  Names(std::initializer_list<const char*> names) {
    DCHECK2(names.size(), "At least one name must be provided");
    is_option = true;

    for (auto name : names) {
      std::size_t len = std::strlen(name);
      DCHECK2(IsValidOptionName(name, len), "Not a valid option name!");
      if (IsLongOptionName(name, len))
        long_names.emplace_back(name + 2, len - 2);
      else
        short_names.emplace_back(*name++);
    }
    if (long_names.size())
      meta_var = ToUpper(long_names[0]);
    else
      meta_var = ToUpper({&short_names[0], 1});
  }
};

class ArgumentHolder;

// Holds all meta-info about an argument.
class Argument {
 public:
  void SetDest(Destination dest) {
    if (dest.callback) {
      dest_ = dest.dest;
      user_callback_ = std::move(dest.callback);
    }
  }

  void SetAction(Action action) {
    if (action.callback)
      user_callback_ = std::move(action.callback);
  }

  void SetType(Type type) {
    if (type.callback)
      user_callback_ = std::move(type.callback);
  }

  void SetHelpDoc(std::string help_doc) { help_doc_ = std::move(help_doc); }

  void SetNames(Names names) {
    is_option_ = names.is_option;
    long_names_ = std::move(names.long_names);
    short_names_ = std::move(names.short_names);
    meta_var_ = std::move(names.meta_var);
  }

  void SetKey(int key) {
    DCHECK2(is_option_, "Only option can be SetKey()");
    DCHECK(short_names_.empty() || short_names_[0] == key);
    key_ = key;
  }

  void SetRequired(bool required) { is_required_ = required; }
  void SetMetaVar(const char* meta_var) { meta_var_ = meta_var; }

  int key() const { return key_; }
  bool is_option() const { return is_option_; }
  bool is_required() const { return is_required_; }
  UserCallback* user_callback() const { return user_callback_.get(); }

  const std::string& help_doc() const { return help_doc_; }
  const char* doc() const {
    return help_doc_.empty() ? nullptr : help_doc_.c_str();
  }

  const std::string& meta_var() const { return meta_var_; }
  const char* arg() const {
    DCHECK(!meta_var_.empty());
    return meta_var_.c_str();
  }

  const std::vector<std::string>& long_names() const { return long_names_; }
  const char* name() const {
    return long_names_.empty() ? nullptr : long_names_[0].c_str();
  }

 private:
  friend class ArgumentHolder;

  Status Finalize() {
    // No dest provided, but still can have UserCallback. No need to Bind().
    if (!dest_.has_value())
      return true;
    if (!user_callback_)
      return Status(
          "There should be at least one UserCallback given dest was set!");
    if (!user_callback_->Bind(dest_.value()))
      return Status(
          "The type of dest is inconsistent with that of UserCallback!");
    return true;
  }

  // For positional, this is -1.
  int key_ = -1;
  std::optional<Dest> dest_;                     // Maybe null.
  std::unique_ptr<UserCallback> user_callback_;  // Maybe null.
  std::string help_doc_;
  std::vector<std::string> long_names_;
  std::vector<char> short_names_;
  std::string meta_var_;
  bool is_option_ = false;
  bool is_required_ = false;
};

class ArgumentBuilder {
 public:
  explicit ArgumentBuilder(Argument* arg) : arg_(arg) {}

  ArgumentBuilder& dest(Destination d) {
    arg_->SetDest(std::move(d));
    return *this;
  }
  ArgumentBuilder& action(Action a) {
    arg_->SetAction(std::move(a));
    return *this;
  }
  ArgumentBuilder& type(Type t) {
    arg_->SetType(std::move(t));
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

 private:
  Argument* arg_;
};

using ArgpOption = ::argp_option;

class ArgumentHolder {
 public:
  ArgumentBuilder add_argument(Names names,
                               Destination dest = {},
                               const char* help = {},
                               Type type = {},
                               Action action = {}) {
    // First check if this arg will conflict with existing ones.
    DCHECK2(CheckNamesConflict(names), "Names conflict with existing names!");
    Argument& arg = arguments_.emplace_back();
    arg.SetDest(std::move(dest));
    arg.SetHelpDoc(help);
    arg.SetType(std::move(type));
    arg.SetAction(std::move(action));

    // option/positional handling.
    if (arg.is_option()) {
      arg.SetKey(NextKey(names));
      bool inserted = key_index_.emplace(arg.key_, &arg).second;
      DCHECK(inserted);
    } else {
      positionals_.push_back(&arg);
    }

    return ArgumentBuilder(&arg);
  }

  Status Compile(std::vector<ArgpOption>* out) {
    for (auto& arg : arguments_) {
      auto status = arg.Finalize();
      if (!status)
        return status;
      // positional isn't managed by argp.
      if (!arg.is_option())
        continue;

      ArgpOption opt{};
      opt.key = arg.key();
      opt.name = arg.name();
      opt.doc = arg.doc();
      opt.arg = arg.arg();
      if (!arg.is_required())
        opt.flags |= OPTION_ARG_OPTIONAL;
      out->push_back(opt);

      // Handle alias.
      const auto& long_names = arg.long_names();
      for (auto iter = long_names.begin() + 1, end = long_names.end();
           iter != end; ++iter) {
        ArgpOption opt{};
        opt.name = iter->c_str();
        opt.flags |= OPTION_ALIAS;
        out->push_back(opt);
      }
    }
    // Ends with an empty option.
    out->push_back(ArgpOption{});
    return true;
  }

 private:
  int NextKey(const Names& names) {
    return names.short_names.empty() ? next_key_++ : names.short_names[0];
  }

  bool CheckNamesConflict(const Names& names) {
    for (auto&& long_name : names.long_names)
      if (!name_set_.insert(long_name).second)
        return false;
    for (char short_name : names.short_names)
      if (!name_set_.insert(std::string(&short_name, 1)).second)
        return false;
    return true;
  }

  static constexpr int kFirstIntOutsideChar = 128;
  unsigned next_key_ = kFirstIntOutsideChar;
  // Hold the storage of all args.
  std::list<Argument> arguments_;
  // indexed by their define-order.
  std::vector<Argument*> positionals_;
  // indexed by their key.
  std::map<int, Argument*> key_index_;
  std::set<std::string> name_set_;
};

class ArgumentParser {};

}  // namespace argparse
