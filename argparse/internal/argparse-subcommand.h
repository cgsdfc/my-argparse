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
class SubCommand final {
 public:
  // Used to iterate over all aliases:
  // Example:
  // for (auto i = SubCommand::kAliasIndex; i < cmd->GetNameOrAliasCount(); ++i)
  //    cmd->GetNameOrAlias(i);
  enum { kNameIndex = 0, kAliasIndex = 1 };

  void SetAliases(std::vector<std::string> val) {
    ARGPARSE_DCHECK(!names_.empty());
    // Append the aliases to names.
    std::move(val.begin(), val.end(), std::back_inserter(names_));
  }
  void SetHelp(absl::string_view val) {
    help_ = static_cast<std::string>(val);  //
  }

  void SetName(absl::string_view val) {
    names_.front() = static_cast<std::string>(val);  //
  }

  std::size_t GetNameOrAliasCount() const { return names_.size(); }
  absl::string_view GetNameOrAlias(std::size_t i) const {
    ARGPARSE_DCHECK(i < GetNameOrAliasCount());
    return names_[i];
    }
    absl::string_view GetName() const { return GetNameOrAlias(kNameIndex); }
    absl::string_view GetHelp() const { return help_; }
    ArgumentHolder* GetHolder() { return &holder_; }

    static std::unique_ptr<SubCommand> Create(std::string name) {
      auto cmd = Create();
      cmd->SetName(std::move(name));
      return cmd;
    }

    static std::unique_ptr<SubCommand> Create() {
      return absl::WrapUnique(new SubCommand);
    }

   private:
    SubCommand();

    ArgumentHolder holder_;
    // Name as well as aliases.
    absl::InlinedVector<std::string, 1> names_;
    std::string help_;
  };

// A group of SubCommands, which can have things like description...
class SubCommandGroup {
 public:
  SubCommand* AddSubCommand(std::unique_ptr<SubCommand> cmd);

  void SetTitle(absl::string_view val);
  void SetDescription(absl::string_view val);
  void SetAction(std::unique_ptr<ActionInfo> info);
  void SetDest(std::unique_ptr<DestInfo> info);
  void SetRequired(bool val);
  void SetHelpDoc(absl::string_view val);
  void SetMetaVar(absl::string_view val);

  absl::string_view GetTitle();
  absl::string_view GetDescription();
  ActionInfo* GetAction();
  DestInfo* GetDest();
  bool IsRequired();
  absl::string_view GetHelpDoc();
  absl::string_view GetMetaVar();

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
