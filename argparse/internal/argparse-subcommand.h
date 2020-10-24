// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once
#include "argparse/internal/argparse-argument-holder.h"
#include "argparse/internal/argparse-argument.h"

namespace argparse {
namespace internal {

// A SubCommand is like a positional argument, but it holds a group of
// Arguments.
class SubCommand final : private ArgumentHolder::Delegate {
 public:
  class Delegate {
   public:
    virtual void OnAddArgument(Argument* arg, ArgumentGroup* group,
                               SubCommand* cmd) {}
    virtual void OnAddArgumentGroup(ArgumentGroup* group, SubCommand* cmd) {}
  };

  void SetAliases(std::vector<std::string> val);
  void SetHelp(std::string val);
  void SetName(std::string val);
  absl::string_view GetName();
  absl::string_view GetHelp();
  ArgumentHolder* GetHolder();

  static std::unique_ptr<SubCommand> Create();

 private:
  void OnAddArgument(Argument* arg, ArgumentGroup* group) override {
    

  }

  void OnAddArgumentGroup(ArgumentGroup* group, ArgumentHolder* holder) override {

  }
  
  Delegate* delegate_;
  // Name as well as aliases.
  std::vector<std::string> names_;
  std::string help_;
  ArgumentHolder holder_;
};

// A group of SubCommands, which can have things like description...
class SubCommandGroup {
 public:
  SubCommand* AddSubCommand(std::unique_ptr<SubCommand> cmd);

  void SetTitle(std::string val);
  void SetDescription(std::string val);
  void SetAction(std::unique_ptr<ActionInfo> info);
  void SetDest(std::unique_ptr<DestInfo> info);
  void SetRequired(bool val);
  void SetHelpDoc(std::string val);
  void SetMetaVar(std::string val);
  void SetHolder(SubCommandHolder* holder);

  absl::string_view GetTitle();
  absl::string_view GetDescription();
  ActionInfo* GetAction();
  DestInfo* GetDest();
  bool IsRequired();
  absl::string_view GetHelpDoc();
  absl::string_view GetMetaVar();
  SubCommandHolder* GetHolder();

  static std::unique_ptr<SubCommandGroup> Create();

 private:
};

// Like ArgumentHolder, but holds subcommands.
class SubCommandHolder {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnAddArgument(Argument* arg) {}
    virtual void OnAddArgumentGroup(ArgumentGroup* group) {}
    virtual void OnAddSubCommandGroup(SubCommandGroup* group) {}
    virtual void OnAddSubCommand(SubCommand* sub) {}
  };

  SubCommandGroup* AddSubCommandGroup(std::unique_ptr<SubCommandGroup> group);

 private:
};

}  // namespace internal
}  // namespace argparse
