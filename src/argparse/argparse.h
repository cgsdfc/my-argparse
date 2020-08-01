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
#include <stdexcept>  // to define ArgumentError.
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
  //  public:
  //   template <typename T>
  //   explicit Dest(T* ptr) : type_(typeid(*ptr)), ptr_(ptr) {}

  //  private:
  std::type_index type_;
  void* ptr_;
};

// template <typename T>
// Dest dest(T* ptr) {
//   return Dest(ptr);
// }

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
    DCHECK2(dest_ptr_, "Bind() must be called before Run() can be called!");
    return RunImpl(ctx);
    // TODO: How Run() link with RunImpl() ??
    //   try {
    //       return RunImpl(ctx);
    //   } catch (...) {

    //   }
  }

  // Bind to a Dest. Make sure type matches. Bind() must be called before Run()
  // can be called.
  bool Bind(const Dest& dest) {
    DCHECK2(!dest_ptr_, "A UserCallback cannot be bound twice");
    if (dest.type_ != type_)
      return false;
    DCHECK(dest.ptr_);
    dest_ptr_ = dest.ptr_;
    return true;
  }

  virtual ~UserCallback() {}

 protected:
  virtual Status RunImpl(const Context& ctx) = 0;

  explicit UserCallback(std::type_index type) : type_(type) {}

  std::type_index type_;
  // The typed-pruned pointer of a Dest.
  void* dest_ptr_ = nullptr;
};

template <typename T>
struct DefaultConverter {
  // Return typename of T.
  static const char* type_name();
  // Parse in, put the result into out, return error indicator.
  static bool Parse(const std::string& in, T* out);
};

// This will format an error string saying that:
// cannot parse `value' into `type_name'.
std::string ReportError(const std::string& value, const char* type_name);

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
  using CallbackMayThrow = CallbackMayThrow<T>;
  // Report error by returning status.
  using CallbackNoExcept = CallbackNoExcept<T>;

  explicit TypeUserCallback(CallbackMayThrow callback)
      : TypeUserCallback(MayThrowToNoExceptAdapter{std::move(callback)}) {}

  explicit TypeUserCallback(CallbackNoExcept callback)
      : TypeUserCallback(std::move(callback)) {}

 private:
  explicit TypeUserCallback(CallbackNoExcept&& callback)
      : UserCallback(typeid(std::declval<T>())),
        callback_(std::move(callback)) {}

  Status RunImpl(const Context& ctx) override {
    return callback_(ctx, reinterpret_cast<T*>(dest_ptr_));
  }

  struct MayThrowToNoExceptAdapter {
    CallbackMayThrow callback;
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
  CallbackNoExcept callback_;
};

// Finally, if the user wants to do arbitray things not limited by the pattern
// of converting and storing, he can provide an action, which is a callback with
// this signature: Status callback(const Context& ctx, T* out) Or void
// callback(const Context& ctx, T* out), which may throw an exception.
template <typename T>
class ActionUserCallback : public UserCallback {
 public:
  // Report error by throwing exception.
  using CallbackMayThrow = CallbackMayThrowVoid<T>;
  // Report error by returning status.
  using CallbackNoExcept = CallbackNoExcept<T>;
  /// XXX: It is worthy to provide two signatures?

  explicit ActionUserCallback(CallbackMayThrow callback)
      : ActionUserCallback(MayThrowToNoExceptAdapter{std::move(callback)}) {}

  explicit ActionUserCallback(CallbackNoExcept callback)
      : ActionUserCallback(std::move(callback)) {}

 private:
  explicit ActionUserCallback(CallbackNoExcept&& callback)
      : UserCallback(typeid(std::declval<T>())),
        callback_(std::move(callback)) {}

  Status RunImpl(const Context& ctx) override {
    return callback_(ctx, reinterpret_cast<T*>(dest_ptr_));
  }

  struct MayThrowToNoExceptAdapter {
    CallbackMayThrow callback;
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

  CallbackNoExcept callback_;
};

// template <typename Container>
// Status append(const Context& ctx, Container* out) {
//   using value_type = typename Container::value_type;
//   using Converter = DefaultConverter<value_type>;
//   value_type item;
//   bool rv = Converter::Parse(ctx.value, &item);
//   if (!rv)
//     return ReportError(ctx.value, Converter::type_name());
//   out->push_back(std::move(item));
//   return true;
// }

// Type-erasured
struct Action {
  std::unique_ptr<UserCallback> callback;
  Action() = default;
  template <typename T>
  /* implicit */ Action(CallbackNoExcept<T> cb)
      : callback(new ActionUserCallback<T>(std::move(cb))) {}
  template <typename T>
  /* implicit */ Action(CallbackMayThrowVoid<T> cb)
      : callback(new ActionUserCallback<T>(std::move(cb))) {}
};

struct Destination {
  std::optional<Dest> dest;
  std::unique_ptr<UserCallback> callback;
  Destination() = default;

  template <typename T>
  /* implicit */ Destination(T* ptr)
      : dest(ptr), callback(new DestUserCallback<T>()) {
    DCHECK2(ptr, "nullptr passed to Destination()!");
  }
};

struct Type {
  std::unique_ptr<UserCallback> callback;
  Type() = default;
  template <typename T>
  /* implicit */ Type(CallbackNoExcept<T> cb)
      : callback(new TypeUserCallback<T>(std::move(cb))) {}
  template <typename T>
  /* implicit */ Type(CallbackMayThrow<T> cb)
      : callback(new TypeUserCallback<T>(std::move(cb))) {}
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

inline bool IsLongOptionName(const std::string& name) {
  return IsLongOptionName(name.c_str(), name.size());
}

inline bool IsShortOptionName(const char* name, std::size_t len) {
  DCHECK(IsValidOptionName(name, len));
  return len == 2;
}

inline bool IsLongOptionName(const std::string& name);

struct Names {
  std::vector<std::string> long_names;
  std::vector<char> short_names;
  bool is_option;

  // For positinal argument, only one name is allowed.
  Names(std::string positional) {
    long_names.push_back(std::move(positional));
    is_option = false;
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
  }
};

class ArgumentHolder;

// Holds all meta-info about an argument.
class Argument {
 public:
  Argument(unsigned* key_generator) : key_generator_(key_generator) {}

  void SetDest(Destination dest) {
    dest_ = dest.dest;
    user_callback_ = std::move(dest.callback);
  }

  void SetAction(Action action) {
    if (action.callback)
      user_callback_ = std::move(action.callback);
  }

  void SetType(Type type) {
    if (type.callback)
      user_callback_ = std::move(type.callback);
  }

  void SetHelpDoc(std::string help_doc) {
      help_doc_ = std::move(help_doc);
  }

  void SetNames(Names names) {
    is_option_ = names.is_option;
    long_names_ = std::move(names.long_names);
    short_names_ = std::move(names.short_names);
    GenerateKey();
  }

  Status Finalize() {
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

 private:
  friend class ArgumentHolder;

  void GenerateKey() {
    DCHECK(key_ == -1);
    if (short_names_.size())
      key_ = short_names_[0];
    else
      key_ = *key_generator_++;
  }

  int key_ = -1;
  unsigned* key_generator_;

  std::optional<Dest> dest_;
  std::unique_ptr<UserCallback> user_callback_;
  std::string help_doc_;
  std::vector<std::string> long_names_;
  std::vector<char> short_names_;
  bool is_option_;
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

private:
 Argument* arg_;
};

class ArgumentHolder {
 public:
  ArgumentBuilder AddArgument(Names names,
                              Destination dest = {},
                              const char* help = {},
                              Type type = {},
                              Action action = {}) {
    // First check if this arg will conflict with existing ones.
    DCHECK2(CheckNamesConflict(names), "Names conflict with existing names!");
    Argument& arg = arguments_.emplace_back(&key_generator_);
    arg.SetDest(std::move(dest));
    arg.SetHelpDoc(help);
    arg.SetType(std::move(type));
    arg.SetAction(std::move(action));

    bool inserted = key_index_.emplace(arg.key_, &arg).second;
    DCHECK(inserted);
    return ArgumentBuilder(&arg);
  }

 private:
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
  unsigned key_generator_ = kFirstIntOutsideChar;
  std::list<Argument> arguments_;
  std::map<int, Argument*> key_index_;
  std::set<std::string> name_set_;
};

class ArgumentParser {};

}  // namespace argparse
