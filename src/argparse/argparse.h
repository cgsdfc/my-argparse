#pragma once

#include <argp.h>
#include <algorithm>
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
class ArgpParser;

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

  // Default to success.
  Status() : Status(true) {}

  explicit operator bool() const { return success_; }
  const std::string& message() const { return message_; }

 private:
  bool success_;
  std::string message_;
};

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

template <typename T>
using InternalUserCallback = std::function<Status(const Context&, T*)>;

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
std::type_index TypeIndex() {
  return typeid(std::declval<T>());
}

// When the user merely provides a dest, we will infer from the type of the
// pointer and provide this callback, which parses the string into the value of
// the type and store into the user's pointer.
template <typename T>
class DefaultUserCallback : public UserCallback {
 public:
  DefaultUserCallback() : UserCallback(TypeIndex<T>()) {}

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

template <typename T>
class CustomUserCallback : public UserCallback {
 public:
  using Callback = InternalUserCallback<T>;
  explicit CustomUserCallback(Callback cb)
      : UserCallback(TypeIndex<T>()), callback_(std::move(cb)) {}

 private:
  Status RunImpl(const Context& ctx) override {
    return std::invoke(callback_, ctx, reinterpret_cast<T*>(dest_ptr_));
  }
  Callback callback_;
};

// introduce a layer to generally adapt user's callback to our internal
// signature. To obtain an adapter, you use
// UserCallbackAdapter<decltype(cb)>::type. Upon this layer we use
// std::function<Status(const Context&, T*)> as the actual storage and further
// erase its T using UserCallback.

enum CallbackSupportMask {
  kSupportedByAction = 0x1,
  kSupportedByType = 0x2,
  kSupportedByAll = 0xffff,
};

// Policy layer: for non-internal format, define a Policy function doing the
// actual adaption.
template <typename Callback,
          typename Signature = detail::function_signature_t<Callback>>
struct UserCallbackTraits;

template <typename Callback, typename T>
struct UserCallbackTraits<Callback, Status(const Context&, T*) noexcept> {
  using type = T;
  static constexpr CallbackSupportMask kMask = kSupportedByAll;
  static Status RunCallback(const Context& ctx, T* out, Callback&& cb) {
    return cb(ctx, out);
  }
};

// If the user have the right signature (but may throw), we still catch any
// exception.
template <typename Callback, typename T>
struct UserCallbackTraits<Callback, Status(const Context&, T*)> {
  using type = T;
  static constexpr CallbackSupportMask kMask = kSupportedByAll;
  static Status RunCallback(const Context& ctx, T* out, Callback&& cb) {
    try {
      return cb(ctx, out);
    } catch (const ArgumentError& e) {
      return Status(e.what());
    }
  }
};

// If the user just convert things, we store his result
// and catch any exception.
template <typename Callback, typename T>
struct UserCallbackTraits<Callback, T(const Context&)> {
  using type = T;
  static constexpr CallbackSupportMask kMask = kSupportedByType;
  static Status RunCallback(const Context& ctx, T* out, Callback&& cb) {
    try {
      *out = cb(ctx);
      return true;
    } catch (const ArgumentError& e) {
      return Status(e.what());
    }
  }
};

// If the user just convert things and force noexcept, we store his result
// assuming no error can happen.
template <typename Callback, typename T>
struct UserCallbackTraits<Callback, void(const Context&, T*)> {
  static constexpr CallbackSupportMask kMask = kSupportedByAction;
  using type = T;
  static Status RunCallback(const Context& ctx, T* out, Callback&& cb) {
    try {
      cb(ctx, out);
      return true;
    } catch (const ArgumentError& e) {
      return Status(e.what());
    }
  }
};

// If the user just convert things and force noexcept, we store his result
// assuming no error can happen.
template <typename Callback, typename T>
struct UserCallbackTraits<Callback, T(const Context&) noexcept> {
  static constexpr CallbackSupportMask kMask = kSupportedByType;
  using type = T;
  static Status RunCallback(const Context& ctx, T* out, Callback&& cb) {
    *out = cb(ctx);
    return true;
  }
};

// Adapter layer: If the signature is not standard, call the policy layer.
template <typename Callback,
          typename Signature = detail::function_signature_t<Callback>>
struct UserCallbackAdapter {
  using Traits = UserCallbackTraits<Callback>;
  using type = typename Traits::type;
  struct Helper {
    // Store a copy of Callback.
    std::decay_t<Callback> cb_;

    Status operator()(const Context& ctx, type* out) {
      return Traits::RunCallback(ctx, out, std::forward<Callback>(cb_));
    }
  };
  static InternalUserCallback<type> Adapt(Callback&& cb) {
    return Helper{std::forward<Callback>(cb)};
  }
};

// If the user have the right signature, do nothing.
template <typename Callback, typename T>
struct UserCallbackAdapter<Callback, Status(const Context&, T*) noexcept> {
  using type = T;
  static Callback&& Adapt(Callback&& cb) { return std::forward<Callback>(cb); }
};

template <typename Callback>
std::unique_ptr<UserCallback> CreateCustomUserCallback(Callback&& cb) {
  using Adapter = UserCallbackAdapter<Callback>;
  using type = typename Adapter::type;
  return std::make_unique<CustomUserCallback<type>>(
      Adapter::Adapt(std::forward<Callback>(cb)));
}

// Check if Callback is supported by Whom.
template <typename Callback, CallbackSupportMask kMask>
struct CallbackIsSupported
    : std::bool_constant<(UserCallbackTraits<Callback>::kMask & kMask) != 0> {};

// Type-erasured
struct Action {
  std::unique_ptr<UserCallback> callback;
  Action() = default;
  Action(Action&&) = default;

  // TODO:: restrict signature.
  template <typename Callback>
  /* implicit */ Action(Callback&& cb) {
    static_assert(CallbackIsSupported<Callback, kSupportedByAction>{},
                  "Callback was not supported by Action");
    callback = CreateCustomUserCallback(std::forward<Callback>(cb));
  }
};

struct Destination {
  std::optional<Dest> dest;
  std::unique_ptr<UserCallback> callback;
  Destination() = default;
  template <typename T>
  /* implicit */ Destination(T* ptr)
      : dest(Dest(ptr)), callback(new DefaultUserCallback<T>()) {
    DCHECK2(ptr, "nullptr passed to Destination()!");
  }
};

struct Type {
  std::unique_ptr<UserCallback> callback;
  Type() = default;
  Type(Type&&) = default;

  template <typename Callback>
  /* implicit */ Type(Callback&& cb) {
    static_assert(CallbackIsSupported<Callback, kSupportedByType>{},
                  "Callback was not supported by Type");
    callback = CreateCustomUserCallback(std::forward<Callback>(cb));
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
    FlushCompile();
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

  ArgumentGroup add_argument_group(const char* header);

  const std::map<int, Argument*>& key_index() const { return key_index_; }
  const std::vector<Argument*>& positionals() const { return positionals_; }

  class FrozenScope {
   public:
    explicit FrozenScope(ArgumentHolder* holder) : holder_(holder) {
      holder_->EnterFrozenScope();
    }
    ~FrozenScope() { holder_->LeaveFrozenScope(); }

   private:
    ArgumentHolder* holder_;
  };

  const ArgpOption* frozen_options() const {
    DCHECK(frozen_);
    return options_.data();
  }

 private:
  void EnterFrozenScope() {
    if (frozen_)
      return;
    frozen_ = true;
    // If nothing added..
    if (arguments_.empty()) {
      DCHECK(options_.data() == nullptr);
      return;
    }
    // Compile all args.
    FlushCompile();
    options_.push_back(ArgpOption{});
  }

  void LeaveFrozenScope() {
    frozen_ = false;
    if (!options_.empty())
      options_.pop_back();
  }

  Status CompileSingle(Argument* arg) {
    auto status = arg->Finalize();
    if (!status)
      return status;
    // positional isn't managed by argp.
    if (!arg->is_option())
      return true;

    ArgpOption opt{};
    opt.key = arg->key();
    opt.name = arg->name();
    opt.doc = arg->doc();
    opt.arg = arg->arg();
    if (!arg->is_required())
      opt.flags |= OPTION_ARG_OPTIONAL;
    options_.push_back(opt);

    // Handle alias.
    const auto& long_names = arg->long_names();
    for (auto iter = long_names.begin() + 1, end = long_names.end();
         iter != end; ++iter) {
      ArgpOption opt{};
      opt.name = iter->c_str();
      opt.flags |= OPTION_ALIAS;
      options_.push_back(opt);
    }
    return true;
  }

  // Compile args [next_to_compile_, limit) to options_.
  Status CompileUntil(int limit) {
    if (next_to_compile_ >= limit || limit > arguments_.size())
      return true;
    // TODO: list isn't random access.
    auto iter = std::next(arguments_.begin(), next_to_compile_);
    for (; next_to_compile_ < limit; ++next_to_compile_, ++iter) {
      auto status = CompileSingle(&(*iter));
      if (!status)
        return status;
    }
    return true;
  }

  void FlushCompile() {
    // Compile as much as possible.
    auto status = CompileUntil(arguments_.size());
    DCHECK2(status, "CompileUtil() failed when add_argument()");
  }

  void CompileGroup(const char* header) {
    ArgpOption opt{};
    opt.doc = header;
    options_.push_back(opt);
  }

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
  // Conflicts checking.
  std::set<std::string> name_set_;
  // Compile result.
  std::vector<ArgpOption> options_;
  int next_to_compile_ = 0;
  bool frozen_ = false;
};

// Impl add_group() call.
class ArgumentGroup {
 public:
  explicit ArgumentGroup(ArgumentHolder* holder) : holder_(holder) {}

  template <typename... Args>
  ArgumentBuilder add_argument(Args&&... args) {
    return holder_->add_argument(std::forward<Args>(args)...);
  }

 private:
  ArgumentHolder* holder_;
};

ArgumentGroup ArgumentHolder::add_argument_group(const char* header) {
  // Generate a group entry in the options. group id is automatically generated.
  FlushCompile();
  CompileGroup(header);
  return ArgumentGroup(this);
}

// This handles the argp_parser_t function.
class ArgpParser {
 public:
  using Argp = ::argp;
  using ArgpParserCallback = ::argp_parser_t;
  using ArgpState = ::argp_state;
  using ArgpErrorType = ::error_t;
  using ArgpHelpFilterCallback = decltype(Argp::help_filter);

  explicit ArgpParser(ArgumentHolder* holder) : holder_(holder) {
    argp_.parser = &ArgpParser::Callback;
  }

  // These storage is managed by caller to save a lot of strings.
  // If the user makes no demand, then all of these field is null.
  // No std::string is stored.
  // Caller may store some of them as string, and requires others to be
  // immorable strings.
  void set_doc(const char* doc) { argp_.doc = doc; }
  void set_argp_domain(const char* domain) { argp_.argp_domain = domain; }
  void set_args_doc(const char* args_doc) { argp_.args_doc = args_doc; }
  void set_help_filter(ArgpHelpFilterCallback cb) { argp_.help_filter = cb; }

  void AddParserFlags(int flags) { parser_flags_ |= flags; }
  void RemoveParserFlags(int flags) { parser_flags_ &= ~flags; }

  void ParseArgs(int argc, char** argv) {
    int arg_index = -1;
    ArgumentHolder::FrozenScope scope(holder_);
    argp_.options = holder_->frozen_options();
    // TODO: error handling.
    auto err =
        ::argp_parse(&argp_, argc, argv, parser_flags_, &arg_index, this);
  }

 private:
  static void InvokeUserCallback(const Argument* arg,
                                 char* value,
                                 ArgpState* state) {
    UserCallback* cb = arg->user_callback();
    if (!cb)  // User doesn't provide any.
      return;
    auto status = cb->Run(Context{arg, std::string(value)});
    // If the user said no, just die with a msg.
    if (status)
      return;
    argp_error(state, "%s", status.message().c_str());
  }

  static constexpr unsigned kSpecialKeyMask = 0x1000000;

  ArgpErrorType ParseImpl(int key, char* arg, ArgpState* state) {
    const auto& positionals = holder_->positionals();

    // Positional argument.
    if (key == ARGP_KEY_ARG) {
      // Too many arguments.
      if (state->arg_num >= positionals.size())
        argp_error(state, "Too many positional arguments. Expected %d, got %d",
                   (int)positionals.size(), (int)state->arg_num);
      InvokeUserCallback(positionals[state->arg_num], arg, state);
      return 0;
    }

    // Next most frequent handling is options.
    const auto& key_index = holder_->key_index();

    if ((key & kSpecialKeyMask) == 0) {
      // This isn't a special key, but rather an option.
      auto iter = key_index.find(key);
      if (iter == key_index.end())
        return ARGP_ERR_UNKNOWN;
      InvokeUserCallback(iter->second, arg, state);
      return 0;
    }

    // No more commandline args, do some post-processing.
    if (key == ARGP_KEY_END) {
      // No enough args.
      if (state->arg_num < positionals.size())
        argp_error(state, "No enough positional arguments. Expected %d, got %d",
                   (int)positionals.size(), (int)state->arg_num);
    }

    // Remaining args (not parsed). Collect them or turn it into an error.
    if (key == ARGP_KEY_ARGS) {
      return 0;
    }

    return 0;
  }

  static ArgpErrorType Callback(int key, char* arg, ArgpState* state) {
    auto* self = reinterpret_cast<ArgpParser*>(state->input);
    return self->ParseImpl(key, arg, state);
  }

  // Holder tell us everythings about user's arguments.
  ArgumentHolder* holder_ = {};
  Argp argp_ = {};
  int parser_flags_ = 0;
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
class Options {
 public:
  // Only the most common options are listed in this list.
  Options(const char* version = nullptr, const char* description = nullptr)
      : program_version_(version), description_(description) {}
  // Options() = default;

  Options& version(const char* v) {
    program_version_ = v;
    return *this;
  }
  Options& description(const char* d) {
    description_ = d;
    return *this;
  }
  Options& after_doc(const char* a) {
    after_doc_ = a;
    return *this;
  }
  Options& domain(const char* d) {
    domain_ = d;
    return *this;
  }
  Options& bug_address(const char* b) {
    bug_address_ = b;
    return *this;
  }
  Options& flags(Flags f) {
    flags_ = f;
    return *this;
  }

 private:
  friend class ArgumentParser;
  const char* program_version_ = {};
  const char* description_ = {};
  const char* after_doc_ = {};
  const char* domain_ = {};
  const char* bug_address_ = {};
  Flags flags_ = kNoFlags;
};

class ArgumentParser : private ArgumentHolder {
 public:
  ArgumentParser() = default;

  explicit ArgumentParser(const Options& options) { set_options(options); }

  void set_options(const Options& options) {
    argp_program_version = options.program_version_;
    argp_program_bug_address = options.bug_address_;
    // TODO: may check domain?
    parser_.set_argp_domain(options.domain_);
    parser_.AddParserFlags(static_cast<int>(options.flags_));

    // Generate the program doc.
    if (options.description_) {
      program_doc_ = options.description_;
    }
    if (options.after_doc_) {
      program_doc_.append({'\v'});
      program_doc_.append(options.after_doc_);
    }
    if (!program_doc_.empty())
      parser_.set_doc(program_doc_.c_str());
    // args_doc is generated later.
  }

  using ArgumentHolder::add_argument;
  using ArgumentHolder::add_argument_group;

  void parse_args(int argc, const char** argv) {
    // parser_.Init(this);
    // argp wants a char**, but most user don't expect argv being changed. So
    // cheat them.
    parser_.ParseArgs(argc, const_cast<char**>(argv));
  }
  // Helper for demo and testing.
  void parse_args(std::initializer_list<const char*> args) {
    // init-er is not mutable.
    std::vector<const char*> args_copy(args.begin(), args.end());
    args_copy.push_back(nullptr);
    return parse_args(args.size(), args_copy.data());
  }
  // TODO: parse_known_args()

 private:
  ArgpParser parser_ = ArgpParser{this};
  std::string program_doc_;
};

}  // namespace argparse
