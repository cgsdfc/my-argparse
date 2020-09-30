#pragma once

#include <functional>  // function
#include <memory>      // unique_ptr
#include <vector>      // vector

#include "argparse/internal/argparse-operations.h"
#include "argparse/internal/argparse-port.h"

// For now, this file should only hold interfaces of core classes.
namespace argparse {
namespace internal {

// argparse-holder.h
class Argument;
class ArgumentGroup;
class SubCommandGroup;

// argparse-parser.h
class Parser;
class OptionsInfo;

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

class NamesInfo {
 public:
  virtual ~NamesInfo() {}
  virtual bool IsOption() = 0;
  virtual unsigned GetLongNamesCount() { return 0; }
  virtual unsigned GetShortNamesCount() { return 0; }
  bool HasLongNames() { return GetLongNamesCount(); }
  bool HasShortNames() { return GetShortNamesCount(); }

  virtual std::string GetDefaultMetaVar() = 0;

  enum NameKind {
    kLongName,
    kShortName,
    kPosName,
    kAllNames,
  };

  // Visit each name of the optional argument.
  virtual void ForEachName(NameKind name_kind,
                           std::function<void(const std::string&)> callback) {}

  virtual StringView GetName() = 0;

  static std::unique_ptr<NamesInfo> CreatePositional(std::string in);
  static std::unique_ptr<NamesInfo> CreateOptional(
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

class ArgumentGroup {
 public:
  virtual ~ArgumentGroup() {}
  virtual StringView GetHeader() = 0;
  // Visit each arg.
  virtual void ForEachArgument(std::function<void(Argument*)> callback) = 0;
  // Add an arg to this group.
  virtual void AddArgument(std::unique_ptr<Argument> arg) = 0;
  virtual unsigned GetArgumentCount() = 0;
};

class Argument {
 public:
  virtual bool IsRequired() = 0;
  virtual StringView GetHelpDoc() = 0;
  virtual StringView GetMetaVar() = 0;
  virtual ArgumentGroup* GetGroup() = 0;
  virtual NamesInfo* GetNamesInfo() = 0;
  virtual DestInfo* GetDest() = 0;
  virtual TypeInfo* GetType() = 0;
  virtual ActionInfo* GetAction() = 0;
  virtual NumArgsInfo* GetNumArgs() = 0;
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
  // Flag is an option that only has short names.
  bool IsFlag() {
    auto* names = GetNamesInfo();
    return names->IsOption() && 0 == names->GetLongNamesCount();
  }

  // For positional, this will be PosName. For Option, this will be
  // the first long name or first short name (if no long name).
  StringView GetName() {
    ARGPARSE_DCHECK(GetNamesInfo());
    return GetNamesInfo()->GetName();
  }

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

  // Default comparison of Argument.
  static bool Less(Argument* lhs, Argument* rhs);

  virtual ~Argument() {}
  static std::unique_ptr<Argument> Create(std::unique_ptr<NamesInfo> info);
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

// Main options passed to the parser.
// TODO: rename to ParserOptions.
struct OptionsInfo {
  int flags = 0;
  // TODO: may change some of these to std::string to allow dynamic generated
  // content.
  const char* program_version = {};
  const char* description = {};
  const char* after_doc = {};
  const char* domain = {};
  const char* email = {};
  // ProgramVersionCallback program_version_callback;
  // HelpFilterCallback help_filter;
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

// TODO: fixthis;
// template <typename T>
// std::unique_ptr<DestInfo> DestInfo::CreateFromPtr(T* ptr) {
//   ARGPARSE_CHECK_F(ptr, "Pointer passed to dest() must not be null.");
//   return std::make_unique<DestInfoImpl>(DestPtr(ptr), CreateOpsFactory<T>());
// }

}  // namespace internal
}  // namespace argparse
