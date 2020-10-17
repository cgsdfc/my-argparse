// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <functional>  // function
#include <memory>      // unique_ptr
#include <vector>      // vector

#include "argparse/internal/argparse-arg-array.h"
#include "argparse/internal/argparse-info.h"
#include "argparse/internal/argparse-port.h"

// For now, this file should only hold interfaces of core classes.
namespace argparse {
namespace internal {

class Argument;
class ArgumentGroup;
class ArgumentHolder;

class SubCommand;
class SubCommandGroup;
class SubCommandHolder;

class ArgumentContainer;
class ArgumentParser;
class ArgumentController;

// Control whether some extra info appear in the help doc.
enum class HelpFormatPolicy {
  kDefault,           // add nothing.
  kTypeHint,          // add (type: <type-hint>) to help doc.
  kDefaultValueHint,  // add (default: <default-value>) to help doc.
};

class ArgumentGroup {
 public:
  virtual ~ArgumentGroup() {}
  virtual absl::string_view GetHeader() = 0;
  // Visit each arg.
  virtual void ForEachArgument(std::function<void(Argument*)> callback) = 0;
  // Add an arg to this group.
  virtual void AddArgument(std::unique_ptr<Argument> arg) = 0;
  virtual unsigned GetArgumentCount() = 0;
  virtual ArgumentHolder* GetHolder() = 0;
};

class Argument {
 public:
  virtual bool IsRequired() = 0;
  virtual absl::string_view GetHelpDoc() = 0;
  virtual absl::string_view GetMetaVar() = 0;
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
  absl::string_view GetName() {
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

  virtual SubCommand* GetSubCommand() = 0;
  virtual void SetListener(std::unique_ptr<Listener> listener) {}
  virtual ArgumentGroup* AddArgumentGroup(std::string header) = 0;
  virtual void ForEachArgument(std::function<void(Argument*)> callback) = 0;
  virtual void ForEachGroup(std::function<void(ArgumentGroup*)> callback) = 0;
  virtual unsigned GetArgumentCount() = 0;
  // method to add arg to default group.
  virtual void AddArgument(std::unique_ptr<Argument> arg) = 0;
  virtual ~ArgumentHolder() {}
  static std::unique_ptr<ArgumentHolder> Create(SubCommand* cmd);

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
  virtual absl::string_view GetName() = 0;
  virtual absl::string_view GetHelpDoc() = 0;
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
  virtual void SetHolder(SubCommandHolder* holder) = 0;

  virtual absl::string_view GetTitle() = 0;
  virtual absl::string_view GetDescription() = 0;
  virtual ActionInfo* GetAction() = 0;
  virtual DestInfo* GetDest() = 0;
  virtual bool IsRequired() = 0;
  virtual absl::string_view GetHelpDoc() = 0;
  virtual absl::string_view GetMetaVar() = 0;
  virtual SubCommandHolder* GetHolder() = 0;

  static std::unique_ptr<SubCommandGroup> Create();
};

// Like ArgumentHolder, but holds subcommands.
class SubCommandHolder {
 public:
  class Listener {
   public:
    virtual ~Listener() {}
    virtual void OnAddArgument(Argument* arg) {}
    virtual void OnAddArgumentGroup(ArgumentGroup* group) {}
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
class ArgumentBuilder {
 public:
  virtual ~ArgumentBuilder() {}

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
  virtual void SetNumArgs(std::unique_ptr<NumArgsInfo> info) = 0;

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

  static std::unique_ptr<ArgumentBuilder> Create();
};

// ArgumentParser has some standard options to tune its behaviours.
class OptionsListener {
 public:
  virtual ~OptionsListener() {}
  virtual void SetProgramVersion(std::string val) = 0;
  virtual void SetDescription(std::string val) = 0;
  virtual void SetBugReportEmail(std::string val) = 0;
  virtual void SetProgramName(std::string val) = 0;
  virtual void SetProgramUsage(std::string usage) = 0;
};

// internal::ArgumentParser is the analogy of argparse::ArgumentParser,
// except that its methods take internal objects as inputs.
// ArgumentController exposes ArgumentContainer to receive user's input,
// and feeds the notification of ArgumentContainer to ArgumentParser through
// adapter so that the latter can build its data-structure that is optimized for
// parsing arguments.
class ArgumentParser {
 public:
  virtual ~ArgumentParser() {}
  virtual std::unique_ptr<OptionsListener> CreateOptionsListener() = 0;
  virtual void AddArgument(Argument* arg) = 0;
  virtual void AddArgumentGroup(ArgumentGroup* group) = 0;
  virtual void AddSubCommand(SubCommand* cmd) = 0;
  virtual void AddSubCommandGroup(SubCommandGroup* group) = 0;
  // Parse args, if rest is null, exit on error. Otherwise put unknown ones into
  // rest and return status code.
  virtual bool ParseKnownArgs(ArgArray args, std::vector<std::string>* out) = 0;
  static std::unique_ptr<ArgumentParser> CreateDefault();

  class Factory {
   public:
    virtual ~Factory() {}
    virtual std::unique_ptr<ArgumentParser> CreateParser() = 0;
  };
};

// ArgumentContainer contains everything user plugs into us, namely,
// Arguments, ArgumentGroups, SubCommands, SubCommandGroups, etc.
// It keeps all these objects alive as long as it is alive.
// It also sends out notifications of events of the insertion of these objects.
// It's main role is to receive and hold things, providing iteration methods,
// etc.
class ArgumentContainer {
 public:
  class Listener {
   public:
    virtual ~Listener() {}
    // TODO: make
    // argument->argument_group->argument_holder->subcommand->subcommand_group
    // chain.
    virtual void OnAddArgument(Argument* arg) {}
    virtual void OnAddArgumentGroup(ArgumentGroup* group) {}
    virtual void OnAddSubCommand(SubCommand* cmd) {}
    virtual void OnAddSubCommandGroup(SubCommandGroup* group) {}
  };
  virtual ~ArgumentContainer() {}
  virtual void AddListener(std::unique_ptr<Listener> listener) = 0;
  virtual ArgumentHolder* GetMainHolder() = 0;
  virtual SubCommandHolder* GetSubCommandHolder() = 0;
  static std::unique_ptr<ArgumentContainer> Create();
};

// This combines the functionality of ArgumentContainer and ArgumentParser and
// connects them. It exposes an interface that is directly usable by the wrapper
// layers.
class ArgumentController {
 public:
  virtual ~ArgumentController() {}

  // Methods forwarded from ArgumentContainer.
  virtual void AddArgument(std::unique_ptr<Argument> arg) = 0;
  virtual ArgumentGroup* AddArgumentGroup(std::string title) = 0;
  virtual SubCommandGroup* AddSubCommandGroup(
      std::unique_ptr<SubCommandGroup> group) = 0;

  // Methods forwarded from ArgumentParser.
  virtual OptionsListener* GetOptionsListener() = 0;
  virtual bool ParseKnownArgs(ArgArray args, std::vector<std::string>* out) = 0;

  static std::unique_ptr<ArgumentController> Create();
};

}  // namespace internal
}  // namespace argparse
