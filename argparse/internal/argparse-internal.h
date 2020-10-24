// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <functional>  // function
#include <memory>      // unique_ptr
#include <vector>      // vector

#include "argparse/internal/argparse-arg-array.h"
#include "argparse/internal/argparse-argument-builder.h"
#include "argparse/internal/argparse-argument-container.h"
#include "argparse/internal/argparse-argument-holder.h"
#include "argparse/internal/argparse-argument.h"
#include "argparse/internal/argparse-info.h"
#include "argparse/internal/argparse-port.h"
#include "argparse/internal/argparse-subcommand.h"

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

// class ArgumentGroup {
//  public:
//   virtual ~ArgumentGroup() {}
//   virtual absl::string_view GetTitle() = 0;
//   // Visit each arg.
//   virtual void ForEachArgument(std::function<void(Argument*)> callback) = 0;
//   // Add an arg to this group.
//   virtual void AddArgument(std::unique_ptr<Argument> arg) = 0;
//   virtual unsigned GetArgumentCount() = 0;
//   virtual ArgumentHolder* GetHolder() = 0;
// };

// class ArgumentHolder {
//  public:
//   // Notify outside some event.
//   class Delegate {
//    public:
//     virtual void OnAddArgument(Argument* arg) {}
//     virtual void OnAddArgumentGroup(ArgumentGroup* group) {}
//     virtual ~Delegate() {}
//   };

//   virtual SubCommand* GetSubCommand() = 0;
//   virtual void SetListener(std::unique_ptr<Delegate> listener) {}
//   virtual ArgumentGroup* AddArgumentGroup(std::string title) = 0;
//   virtual void ForEachArgument(std::function<void(Argument*)> callback) = 0;
//   virtual void ForEachGroup(std::function<void(ArgumentGroup*)> callback) = 0;
//   virtual unsigned GetArgumentCount() = 0;
//   // method to add arg to default group.
//   virtual void AddArgument(std::unique_ptr<Argument> arg) = 0;
//   virtual ~ArgumentHolder() {}
//   static std::unique_ptr<ArgumentHolder> Create(SubCommand* cmd);

//   void CopyArguments(std::vector<Argument*>* out) {
//     out->clear();
//     out->reserve(GetArgumentCount());
//     ForEachArgument([out](Argument* arg) { out->push_back(arg); });
//   }

//   // Get a sorted list of Argument.
//   void SortArguments(
//       std::vector<Argument*>* out,
//       std::function<bool(Argument*, Argument*)> cmp = &Argument::BeforeInUsage) {
//     CopyArguments(out);
//     std::sort(out->begin(), out->end(), std::move(cmp));
//   }
// };

// class SubCommand {
//  public:
//   virtual ~SubCommand() {}
//   virtual ArgumentHolder* GetHolder() = 0;
//   virtual void SetAliases(std::vector<std::string> val) = 0;
//   virtual void SetHelpDoc(std::string val) = 0;
//   virtual absl::string_view GetName() = 0;
//   virtual absl::string_view GetHelpDoc() = 0;
//   virtual void ForEachAlias(
//       std::function<void(const std::string&)> callback) = 0;
//   virtual void SetGroup(SubCommandGroup* group) = 0;
//   virtual SubCommandGroup* GetGroup() = 0;

//   static std::unique_ptr<SubCommand> Create(std::string name);
// };

// // A group of SubCommands, which can have things like description...
// class SubCommandGroup {
//  public:
//   virtual ~SubCommandGroup() {}
//   virtual SubCommand* AddSubCommand(std::unique_ptr<SubCommand> cmd) = 0;

//   virtual void SetTitle(std::string val) = 0;
//   virtual void SetDescription(std::string val) = 0;
//   virtual void SetAction(std::unique_ptr<ActionInfo> info) = 0;
//   virtual void SetDest(std::unique_ptr<DestInfo> info) = 0;
//   virtual void SetRequired(bool val) = 0;
//   virtual void SetHelpDoc(std::string val) = 0;
//   virtual void SetMetaVar(std::string val) = 0;
//   virtual void SetHolder(SubCommandHolder* holder) = 0;

//   virtual absl::string_view GetTitle() = 0;
//   virtual absl::string_view GetDescription() = 0;
//   virtual ActionInfo* GetAction() = 0;
//   virtual DestInfo* GetDest() = 0;
//   virtual bool IsRequired() = 0;
//   virtual absl::string_view GetHelpDoc() = 0;
//   virtual absl::string_view GetMetaVar() = 0;
//   virtual SubCommandHolder* GetHolder() = 0;

//   static std::unique_ptr<SubCommandGroup> Create();
// };

// // Like ArgumentHolder, but holds subcommands.
// class SubCommandHolder {
//  public:
//   class Listener {
//    public:
//     virtual ~Listener() {}
//     virtual void OnAddArgument(Argument* arg) {}
//     virtual void OnAddArgumentGroup(ArgumentGroup* group) {}
//     virtual void OnAddSubCommandGroup(SubCommandGroup* group) {}
//     virtual void OnAddSubCommand(SubCommand* sub) {}
//   };

//   virtual ~SubCommandHolder() {}
//   virtual SubCommandGroup* AddSubCommandGroup(
//       std::unique_ptr<SubCommandGroup> group) = 0;
//   virtual void ForEachSubCommand(std::function<void(SubCommand*)> callback) = 0;
//   virtual void ForEachSubCommandGroup(
//       std::function<void(SubCommandGroup*)> callback) = 0;
//   virtual void SetListener(std::unique_ptr<Listener> listener) = 0;
//   static std::unique_ptr<SubCommandHolder> Create();
// };

// // ArgumentContainer contains everything user plugs into us, namely,
// // Arguments, ArgumentGroups, SubCommands, SubCommandGroups, etc.
// // It keeps all these objects alive as long as it is alive.
// // It also sends out notifications of events of the insertion of these objects.
// // It's main role is to receive and hold things, providing iteration methods,
// // etc.
// class ArgumentContainer {
//  public:
//   class Listener {
//    public:
//     virtual ~Listener() {}
//     // TODO: make
//     // argument->argument_group->argument_holder->subcommand->subcommand_group
//     // chain.
//     virtual void OnAddArgument(Argument* arg) {}
//     virtual void OnAddArgumentGroup(ArgumentGroup* group) {}
//     virtual void OnAddSubCommand(SubCommand* cmd) {}
//     virtual void OnAddSubCommandGroup(SubCommandGroup* group) {}
//   };
//   virtual ~ArgumentContainer() {}
//   virtual void AddListener(std::unique_ptr<Listener> listener) = 0;
//   virtual ArgumentHolder* GetMainHolder() = 0;
//   virtual SubCommandHolder* GetSubCommandHolder() = 0;
//   static std::unique_ptr<ArgumentContainer> Create();
// };

}  // namespace internal
}  // namespace argparse
