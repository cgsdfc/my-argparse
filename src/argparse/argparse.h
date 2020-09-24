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

namespace argparse {

class Argument;
class ArgumentGroup;

// Throw this exception will cause an error msg to be printed (via what()).
class ArgumentError final : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

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

// Control whether some extra info appear in the help doc.
enum class HelpFormatPolicy {
  kDefault,           // add nothing.
  kTypeHint,          // add (type: <type-hint>) to help doc.
  kDefaultValueHint,  // add (default: <default-value>) to help doc.
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
  virtual std::unique_ptr<Argument> CreateArgument() = 0;

  static std::unique_ptr<ArgumentFactory> Create();
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
  virtual unsigned GetArgumentCount() = 0;
  // method to add arg to default group.
  virtual void AddArgument(std::unique_ptr<Argument> arg) = 0;
  virtual ~ArgumentHolder() {}
  static std::unique_ptr<ArgumentHolder> Create();

  void CopyArguments(std::vector<Argument*>* out) {
    out->clear();
    out->reserve(GetArgumentCount());
    ForEachArgument([out](Argument* arg) { out->push_back(arg); });
  }

  // Get a sorted list of Argument.
  void SortArguments(
      std::vector<Argument*>* out,
      std::function<bool(Argument*, Argument*)> cmp = &Argument::Less) {
    CopyArguments(out);
    std::sort(out->begin(), out->end(), std::move(cmp));
  }
};

class SubCommandGroup;

class SubCommand {
 public:
  virtual ~SubCommand() {}
  virtual ArgumentHolder* GetHolder() = 0;
  virtual void SetAliases(std::vector<std::string> val) = 0;
  virtual void SetHelpDoc(std::string val) = 0;
  virtual StringView GetName() = 0;
  virtual StringView GetHelpDoc() = 0;
  virtual void ForEachAlias(
      std::function<void(const std::string&)> callback) = 0;
  virtual void SetGroup(SubCommandGroup* group) = 0;
  virtual SubCommandGroup* GetGroup() = 0;

  static std::unique_ptr<SubCommand> Create(std::string name);
};

// A group of SubCommands, which can have things like description...
class SubCommandGroup {
 public:
  virtual ~SubCommandGroup() {}
  virtual SubCommand* AddSubCommand(std::unique_ptr<SubCommand> cmd) = 0;

  virtual void SetTitle(std::string val) = 0;
  virtual void SetDescription(std::string val) = 0;
  virtual void SetAction(std::unique_ptr<ActionInfo> info) = 0;
  virtual void SetDest(std::unique_ptr<DestInfo> info) = 0;
  virtual void SetRequired(bool val) = 0;
  virtual void SetHelpDoc(std::string val) = 0;
  virtual void SetMetaVar(std::string val) = 0;

  virtual StringView GetTitle() = 0;
  virtual StringView GetDescription() = 0;
  virtual ActionInfo* GetAction() = 0;
  virtual DestInfo* GetDest() = 0;
  virtual bool IsRequired() = 0;
  virtual StringView GetHelpDoc() = 0;
  virtual StringView GetMetaVar() = 0;
  // TODO: change all const char* to StringView..

  static std::unique_ptr<SubCommandGroup> Create();
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
      std::unique_ptr<SubCommandGroup> group) = 0;
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

  std::unique_ptr<Argument> CreateArgument() override;

 private:
  // Some options are directly fed into arg.
  std::unique_ptr<Argument> arg_;
  // If not given, use default from NamesInfo.
  std::unique_ptr<std::string> meta_var_;
  ActionKind action_kind_ = ActionKind::kNoAction;
  OpenMode open_mode_ = kModeNoMode;
};

template <typename T>
std::unique_ptr<DestInfo> DestInfo::CreateFromPtr(T* ptr) {
  ARGPARSE_CHECK_F(ptr, "Pointer passed to dest() must not be null.");
  return std::make_unique<DestInfoImpl>(DestPtr(ptr), CreateOpsFactory<T>());
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

  unsigned GetArgumentCount() override { return arguments_.size(); }

  // TODO: merge listener into one class about the events during argument
  // adding.
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

  bool CheckNamesConflict(NamesInfo* names);

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
  explicit SubCommandImpl(std::string name) : name_(std::move(name)) {}

  ArgumentHolder* GetHolder() override { return holder_.get(); }
  void SetGroup(SubCommandGroup* group) override { group_ = group; }
  SubCommandGroup* GetGroup() override { return group_; }
  StringView GetName() override { return name_; }
  StringView GetHelpDoc() override { return help_doc_; }
  void SetAliases(std::vector<std::string> val) override {
    aliases_ = std::move(val);
  }
  void SetHelpDoc(std::string val) override { help_doc_ = std::move(val); }

  void ForEachAlias(std::function<void(const std::string&)> callback) override {
    for (auto& al : aliases_)
      callback(al);
  }

 private:
  SubCommandGroup* group_ = nullptr;
  std::string name_;
  std::string help_doc_;
  std::vector<std::string> aliases_;
  std::unique_ptr<ArgumentHolder> holder_;
};

inline std::unique_ptr<SubCommand> SubCommand::Create(std::string name) {
  return std::make_unique<SubCommandImpl>(std::move(name));
}

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
      std::unique_ptr<SubCommandGroup> group) override {
    auto* g = group.get();
    groups_.push_back(std::move(group));
    return g;
  }

  void SetListener(std::unique_ptr<Listener> listener) override {
    listener_ = std::move(listener);
  }

 private:
  SubCommand* AddSubCommandToGroup(SubCommandGroup* group,
                                   std::unique_ptr<SubCommand> cmd) {
    cmd->SetGroup(group);
    subcmds_.push_back(std::move(cmd));
    return subcmds_.back().get();
  }

  std::unique_ptr<Listener> listener_;
  std::vector<std::unique_ptr<SubCommand>> subcmds_;
  std::vector<std::unique_ptr<SubCommandGroup>> groups_;
};

class SubCommandGroupImpl : public SubCommandGroup {
 public:
  SubCommandGroupImpl() = default;

  SubCommand* AddSubCommand(std::unique_ptr<SubCommand> cmd) override {
    auto* cmd_ptr = cmd.get();
    commands_.push_back(std::move(cmd));
    cmd_ptr->SetGroup(this);
    return cmd_ptr;
  }

  void SetTitle(std::string val) override { title_ = std::move(val); }
  void SetDescription(std::string val) override {
    description_ = std::move(val);
  }
  void SetAction(std::unique_ptr<ActionInfo> info) override {
    action_info_ = std::move(info);
  }
  void SetDest(std::unique_ptr<DestInfo> info) override {
    dest_info_ = std::move(info);
  }
  void SetRequired(bool val) override { required_ = val; }
  void SetHelpDoc(std::string val) override { help_doc_ = std::move(val); }
  void SetMetaVar(std::string val) override { meta_var_ = std::move(val); }

  StringView GetTitle() override { return title_; }
  StringView GetDescription() override { return description_; }
  ActionInfo* GetAction() override { return action_info_.get(); }
  DestInfo* GetDest() override { return dest_info_.get(); }
  bool IsRequired() override { return required_; }
  StringView GetHelpDoc() override { return help_doc_; }
  StringView GetMetaVar() override { return meta_var_; }

 private:
  bool required_ = false;
  std::string title_;
  std::string description_;
  std::string help_doc_;
  std::string meta_var_;
  std::unique_ptr<DestInfo> dest_info_;
  std::unique_ptr<ActionInfo> action_info_;
  std::vector<std::unique_ptr<SubCommand>> commands_;
};

inline std::unique_ptr<SubCommandGroup> SubCommandGroup::Create() {
  return std::make_unique<SubCommandGroupImpl>();
}

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

bool IsValidPositionalName(const std::string& name);

// A valid option name is long or short option name and not '--', '-'.
// This is only checked once and true for good.
bool IsValidOptionName(const std::string& name);

// These two predicates must be called only when IsValidOptionName() holds.
inline bool IsLongOptionName(const std::string& name) {
  ARGPARSE_DCHECK(IsValidOptionName(name));
  return name.size() > 2;
}

inline bool IsShortOptionName(const std::string& name) {
  ARGPARSE_DCHECK(IsValidOptionName(name));
  return name.size() == 2;
}

inline std::string ToUpper(const std::string& in) {
  std::string out(in);
  std::transform(in.begin(), in.end(), out.begin(), ::toupper);
  return out;
}

class PositionalName : public NamesInfo {
 public:
  explicit PositionalName(std::string name) : name_(std::move(name)) {}

  bool IsOption() override { return false; }
  std::string GetDefaultMetaVar() override { return ToUpper(name_); }
  void ForEachName(NameKind name_kind,
                   std::function<void(const std::string&)> callback) override {
    if (name_kind == kPosName)
      callback(name_);
  }
  StringView GetName() override { return name_; }

 private:
  std::string name_;
};

class OptionalNames : public NamesInfo {
 public:
  explicit OptionalNames(const std::vector<std::string>& names) {
    for (auto& name : names) {
      ARGPARSE_CHECK_F(IsValidOptionName(name), "Not a valid option name: %s",
                       name.c_str());
      if (IsLongOptionName(name)) {
        long_names_.push_back(name);
      } else {
        ARGPARSE_DCHECK(IsShortOptionName(name));
        short_names_.push_back(name);
      }
    }
  }

  bool IsOption() override { return true; }
  unsigned GetLongNamesCount() override { return long_names_.size(); }
  unsigned GetShortNamesCount() override { return short_names_.size(); }

  std::string GetDefaultMetaVar() override {
    std::string in =
        long_names_.empty() ? short_names_.front() : long_names_.front();
    std::replace(in.begin(), in.end(), '-', '_');
    return ToUpper(in);
  }

  void ForEachName(NameKind name_kind,
                   std::function<void(const std::string&)> callback) override {
    switch (name_kind) {
      case kPosName:
        return;
      case kLongName: {
        for (auto& name : std::as_const(long_names_))
          callback(name);
        break;
      }
      case kShortName: {
        for (auto& name : std::as_const(short_names_))
          callback(name);
        break;
      }
      case kAllNames: {
        for (auto& name : std::as_const(long_names_))
          callback(name);
        for (auto& name : std::as_const(short_names_))
          callback(name);
        break;
      }
      default:
        break;
    }
  }

  StringView GetName() override {
    const auto& name =
        long_names_.empty() ? short_names_.front() : long_names_.front();
    return name;
  }

 private:
  std::vector<std::string> long_names_;
  std::vector<std::string> short_names_;
};

inline std::unique_ptr<NamesInfo> NamesInfo::CreatePositional(std::string in) {
  return std::make_unique<PositionalName>(std::move(in));
}

inline std::unique_ptr<NamesInfo> NamesInfo::CreateOptional(
    const std::vector<std::string>& in) {
  return std::make_unique<OptionalNames>(in);
}

struct Names {
  std::unique_ptr<NamesInfo> info;
  Names(const char* name) : Names(std::string(name)) { ARGPARSE_DCHECK(name); }
  Names(std::string name);
  Names(std::initializer_list<std::string> names);
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

// Creator of DestInfo. For those that need a DestInfo, just take Dest
// as an arg.
struct Dest {
  std::unique_ptr<DestInfo> info;
  template <typename T>
  Dest(T* ptr) : info(DestInfo::CreateFromPtr(ptr)) {}
};

class ArgumentBuilder {
 public:
  explicit ArgumentBuilder(Names names) : factory_(ArgumentFactory::Create()) {
    ARGPARSE_DCHECK(names.info);
    factory_->SetNames(std::move(names.info));
  }

  // TODO: Fix the typeinfo/actioninfo deduction.
  ArgumentBuilder& dest(Dest dest) {
    factory_->SetDest(std::move(dest.info));
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
    using Type = std::decay_t<T>;
    factory_->SetConstValue(MakeAny<Type>(std::forward<T>(val)));
    return *this;
  }
  template <typename T>
  ArgumentBuilder& default_value(T&& val) {
    using Type = std::decay_t<T>;
    factory_->SetDefaultValue(MakeAny<Type>(std::forward<T>(val)));
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

  std::unique_ptr<Argument> Build() { return factory_->CreateArgument(); }

 private:
  std::unique_ptr<ArgumentFactory> factory_;
};

// Helper alias.
using argument = ArgumentBuilder;

// This is a helper that provides add_argument().
class AddArgumentHelper {
 public:
  // add_argument(ArgumentBuilder(...).Build());
  void add_argument(std::unique_ptr<Argument> arg) {
    return AddArgumentImpl(std::move(arg));
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
class SubCommandBuilder {
 public:
  explicit SubCommandBuilder(std::string name)
      : cmd_(SubCommand::Create(std::move(name))) {}

  SubCommandBuilder& aliases(std::vector<std::string> als) {
    cmd_->SetAliases(std::move(als));
    return *this;
  }
  SubCommandBuilder& help(std::string val) {
    cmd_->SetHelpDoc(std::move(val));
    return *this;
  }

  std::unique_ptr<SubCommand> Build() {
    ARGPARSE_DCHECK(cmd_);
    return std::move(cmd_);
  }

 private:
  std::unique_ptr<SubCommand> cmd_;
};

class SubParserGroup {
 public:
  explicit SubParserGroup(SubCommandGroup* group) : group_(group) {}

  // Positional.
  SubParser add_parser(std::string name,
                       std::string help = {},
                       std::vector<std::string> aliases = {}) {
    SubCommandBuilder builder(std::move(name));
    builder.help(std::move(help)).aliases(std::move(aliases));
    return add_parser(builder.Build());
  }

  // Builder pattern.
  SubParser add_parser(std::unique_ptr<SubCommand> cmd) {
    auto* cmd_ptr = group_->AddSubCommand(std::move(cmd));
    return SubParser(cmd_ptr);
  }

 private:
  SubCommandGroup* group_;
};

class MainParserHelper;

// Support add(subparsers(...))
class SubParsersBuilder {
 public:
  explicit SubParsersBuilder() : group_(SubCommandGroup::Create()) {}

  SubParsersBuilder& title(std::string val) {
    group_->SetTitle(std::move(val));
    return *this;
  }

  SubParsersBuilder& description(std::string val) {
    group_->SetDescription(std::move(val));
    return *this;
  }

  SubParsersBuilder& meta_var(std::string val) {
    group_->SetMetaVar(std::move(val));
    return *this;
  }

  SubParsersBuilder& help(std::string val) {
    group_->SetHelpDoc(std::move(val));
    return *this;
  }

  SubParsersBuilder& dest(Dest val) {
    group_->SetDest(std::move(val.info));
    return *this;
  }

  std::unique_ptr<SubCommandGroup> Build() { return std::move(group_); }

 private:
  std::unique_ptr<SubCommandGroup> group_;
};

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

  using AddArgumentGroupHelper::add_argument_group;
  using AddArgumentHelper::add_argument;

  SubParserGroup add_subparsers(std::unique_ptr<SubCommandGroup> group) {
    return SubParserGroup(AddSubParsersImpl(std::move(group)));
  }
  // TODO: More precise signature.
  SubParserGroup add_subparsers(Dest dest, std::string help = {}) {
    SubParsersBuilder builder;
    builder.dest(std::move(dest)).help(std::move(help));
    return add_subparsers(builder.Build());
  }

 private:
  virtual bool ParseArgsImpl(ArgArray args, std::vector<std::string>* out) = 0;
  virtual SubCommandGroup* AddSubParsersImpl(
      std::unique_ptr<SubCommandGroup> group) = 0;
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
      std::unique_ptr<SubCommandGroup> group) override {
    return controller_->GetSubCommandHolder()->AddSubCommandGroup(
        std::move(group));
  }

  std::unique_ptr<ArgumentController> controller_;
};

}  // namespace argparse
