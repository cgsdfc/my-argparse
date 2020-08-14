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
// class ArgpParserImpl;
class Options;

using ArgpOption = ::argp_option;
// using ArgpState = ::argp_state;
using ArgpProgramVersionCallback = decltype(::argp_program_version_hook);

// Wrapper of argp_state.
class ArgpState {
 public:
  ArgpState(::argp_state* state) : state_(state) {}
  void Help(FILE* file, unsigned flags) {
    ::argp_state_help(state_, file, flags);
  }
  void Usage() { ::argp_usage(state_); }

  void Error(const std::string& msg) {
    ::argp_error(state_, "%s", msg.c_str());
  }
  void Failure(int status, int errnum, const std::string& msg) {
    ::argp_failure(state_, status, errnum, "%s", msg.c_str());
  }
  ::argp_state* operator->() { return state_; }

 private:
  ::argp_state* state_;
};

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
  // TODO: maynot let user see this.
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

// When the user merely provides a dest, we will infer from the type of the
// pointer and provide this callback, which parses the string into the value of
// the type and store into the user's pointer.
template <typename T>
class DefaultUserCallback : public UserCallback {
 public:
  DefaultUserCallback() : UserCallback(typeid(T)) {}

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
      : UserCallback(typeid(T)), callback_(std::move(cb)) {}

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

  void SetHelpDoc(const char* help_doc) {
    if (help_doc)
      help_doc_ = help_doc;
  }

  void SetRequired(bool required) { is_required_ = required; }
  void SetMetaVar(const char* meta_var) { meta_var_ = meta_var; }

  bool initialized() const { return key_ != 0; }
  int key() const { return key_; }
  int group() const { return group_; }
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

  // TODO: split the long_names, short_names from Names into name, key and
  // alias.
  const std::vector<std::string>& long_names() const { return long_names_; }
  const std::vector<char>& short_names() const { return short_names_; }

  const char* name() const {
    return long_names_.empty() ? nullptr : long_names_[0].c_str();
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
  friend class ArgumentHolder;
  friend class ArgpParserImpl;

  enum Keys {
    kKeyForNothing = 0,
    kKeyForPositional = -1,
  };

  // Fill in members to do with names.
  void InitNames(Names names) {
    is_option_ = names.is_option;
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

  // Put here to record some metics.
  Status RunUserCallback(char* value) {
    if (!user_callback_)  // User doesn't provide any.
      return true;
    return user_callback_->Run(Context{this, std::string(value)});
  }

  // For extension.
  ArgumentDelegate* delegate_;
  // For positional, this is -1, for group-header, this is -2.
  int key_ = kKeyForNothing;
  int group_ = 0;
  std::optional<Dest> dest_;                     // Maybe null.
  std::unique_ptr<UserCallback> user_callback_;  // Maybe null.
  std::string help_doc_;
  std::vector<std::string> long_names_;
  std::vector<char> short_names_;
  std::string meta_var_;
  // TODO: This is encoded in key_, can be rm'ed.
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

  // for test:
  Argument* arg() const { return arg_; }

 private:
  Argument* arg_;
};

class ArgpParser {
 public:
  using Argp = ::argp;
  using ArgpParserCallback = ::argp_parser_t;
  using ArgpErrorType = ::error_t;
  // XXX: what is the use of help_filter?
  using ArgpHelpFilterCallback = decltype(Argp::help_filter);

  class Delegate {
   public:
    virtual Argument* FindOptionalArgument(int key) = 0;
    virtual Argument* FindPositionalArgument(int index) = 0;
    virtual std::size_t PositionalArgumentCount() = 0;
    virtual void CompileToArgpOptions(std::vector<ArgpOption>* options) = 0;
    virtual ~Delegate() {}
  };

  struct Options {
    const char* program_version = {};
    const char* description = {};
    const char* after_doc = {};
    const char* domain = {};
    const char* bug_address = {};
    ArgpProgramVersionCallback program_version_callback = {};
    int flags = 0;
  };

  // Initialize from a few options (user's options).
  virtual void Init(const Options& options) = 0;
  // Parse args, exit on errors.
  virtual void ParseArgs(int argc, char** argv) = 0;
  // Parse args, collect unknown args into rest, don't exit, report error via
  // Status.
  virtual Status ParseKnownArgs(int argc,
                                char** argv,
                                std::vector<std::string>* rest) {
    ParseArgs(argc, argv);
    return true;
  }
  virtual ~ArgpParser() {}
  static std::unique_ptr<ArgpParser> Create(Delegate* delegate);
};

// This is an interface that provides add_argument() and other common things.
class ArgumentContainer {
 public:
  ArgumentBuilder add_argument(Names names,
                               Destination dest = {},
                               const char* help = {},
                               Type type = {}) {
    Argument* arg = AddArgument(std::move(names));
    arg->SetDest(std::move(dest));
    arg->SetHelpDoc(help);
    arg->SetType(std::move(type));
    return ArgumentBuilder(arg);
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

  // ArgpParserDelegate:
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
    InitArgpOptions();
  }

  void Init(const Options& options) override {
    if (options.program_version)
      ::argp_program_version = options.program_version;
    if (options.program_version_callback)
      ::argp_program_version_hook = options.program_version_callback;
    if (options.bug_address)
      ::argp_program_bug_address = options.bug_address;
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

  void ParseArgs(int argc, char** argv) override {
    int arg_index = -1;
    auto err =
        ::argp_parse(&argp_, argc, argv, parser_flags_, &arg_index, this);
  }

 private:
  void InitArgpOptions() {
    DCHECK(argp_options_.empty());
    delegate_->CompileToArgpOptions(&argp_options_);
    argp_.options = argp_options_.data();
    positional_count_ = delegate_->PositionalArgumentCount();
  }

  void set_doc(const char* doc) { argp_.doc = doc; }
  void set_argp_domain(const char* domain) { argp_.argp_domain = domain; }
  void set_args_doc(const char* args_doc) { argp_.args_doc = args_doc; }
  void set_help_filter(ArgpHelpFilterCallback cb) { argp_.help_filter = cb; }
  void AddFlags(int flags) { parser_flags_ |= flags; }

  static void InvokeUserCallback(Argument* arg, char* value, ArgpState state) {
    auto status = arg->RunUserCallback(value);
    // If the user said no, just die with a msg.
    if (status)
      return;
    // argp_error(state, "%s", status.message().c_str());
    state.Error(status.message());
  }

  static constexpr unsigned kSpecialKeyMask = 0x1000000;

  ArgpErrorType ParseImpl(int key, char* arg, ArgpState state) {
    // const auto& positionals = holder_->positional_arguments();

    // Positional argument.
    if (key == ARGP_KEY_ARG) {
      const int arg_num = state->arg_num;
      if (Argument* argument = delegate_->FindPositionalArgument(arg_num)) {
        InvokeUserCallback(argument, arg, state);
        return 0;
      }
      // Too many arguments.
      // if (state->arg_num >= positional_count())
      // argp_error(state, "Too many positional arguments. Expected %d, got %d",
      //            (int)positional_count(), (int)state->arg_num);
      return ARGP_ERR_UNKNOWN;
    }

    // Next most frequent handling is options.
    // const auto& key_index = holder_->optional_arguments();

    if ((key & kSpecialKeyMask) == 0) {
      // This isn't a special key, but rather an option.
      Argument* argument = delegate_->FindOptionalArgument(key);
      // auto iter = key_index.find(key);
      if (!argument)
        return ARGP_ERR_UNKNOWN;
      InvokeUserCallback(argument, arg, state);
      return 0;
    }

    // No more commandline args, do some post-processing.
    if (key == ARGP_KEY_END) {
      // No enough args.
      if (state->arg_num < positional_count())
        state.Error("No enough positional arguments. Expected %d, got %d");
                  //  (int)positional_count(), (int)state->arg_num);
    }

    // Remaining args (not parsed). Collect them or turn it into an error.
    if (key == ARGP_KEY_ARGS) {
      return 0;
    }

    return 0;
  }

  static ArgpErrorType Callback(int key, char* arg, ::argp_state* state) {
    auto* self = reinterpret_cast<ArgpParserImpl*>(state->input);
    return self->ParseImpl(key, arg, state);
  }

  unsigned positional_count() const { return positional_count_; }

  Delegate* delegate_;
  std::string program_doc_;
  std::string args_doc_;
  Argp argp_ = {};
  std::vector<ArgpOption> argp_options_;
  int parser_flags_ = 0;
  unsigned positional_count_ = 0;
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
    return parser->ParseArgs(argc, const_cast<char**>(argv));
  }

  // Helper for demo and testing.
  void parse_args(std::initializer_list<const char*> args) {
    // init-er is not mutable.
    std::vector<const char*> args_copy(args.begin(), args.end());
    args_copy.push_back(nullptr);
    return parse_args(args.size(), args_copy.data());
  }
  // TODO: parse_known_args()

  const char* program_name() const { return ::program_invocation_name; }
  const char* program_short_name() const {
    return ::program_invocation_short_name;
  }
  const char* program_version() const { return ::argp_program_version; }
  const char* program_bug_address() const { return ::argp_program_bug_address; }

 private:
  Options user_options_;
};

}  // namespace argparse
