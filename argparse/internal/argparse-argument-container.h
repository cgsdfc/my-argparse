// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once
#include "argparse/internal/argparse-argument-holder.h"
#include "argparse/internal/argparse-argument-parser.h"

namespace argparse {
namespace internal {

class SubCommand;
class SubCommandGroup;

// ArgumentContainer contains everything user plugs into us, namely,
// Arguments, ArgumentGroups, SubCommands, SubCommandGroups, etc.
// It keeps all these objects alive as long as it is alive.
// It also sends out notifications of events of the insertion of these objects.
// It's main role is to receive and hold things, providing iteration methods,
// etc.
// It should be directly allocated.
class ArgumentContainer final : private ArgumentHolder::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnAddArgument(Argument* arg, ArgumentGroup* group,
                               SubCommand* cmd) {}
    virtual void OnAddArgumentGroup(ArgumentGroup* group, SubCommand* cmd) {}
    virtual void OnAddSubCommand(SubCommand* cmd, SubCommandGroup* group) {}
    virtual void OnAddSubCommandGroup(SubCommandGroup* group) {}
  };

  explicit ArgumentContainer(Delegate* delegate);
  ArgumentHolder* GetMainHolder() { return &main_holder_; }

 private:
  // Notifications from the main_holder:
  void OnAddArgument(Argument* arg, ArgumentGroup* group) override {
    delegate_->OnAddArgument(arg, group, nullptr);
  }
  void OnAddArgumentGroup(ArgumentGroup* group,
                          ArgumentHolder* holder) override {
    delegate_->OnAddArgumentGroup(group, nullptr);
  }

  Delegate* delegate_;
  ArgumentHolder main_holder_;
};

// This combines the functionality of ArgumentContainer and ArgumentParser and
// connects them. It exposes an interface that is directly usable by the wrapper
// layers.
class ArgumentController : private ArgumentContainer::Delegate {
 public:
  ArgumentController();

  // Methods forwarded from ArgumentContainer.
  void AddArgument(std::unique_ptr<Argument> arg);
  ArgumentGroup* AddArgumentGroup(std::string title);
  SubCommandGroup* AddSubCommandGroup(std::unique_ptr<SubCommandGroup> group);

  // Methods forwarded from ArgumentParser.
  OptionsListener* GetOptionsListener();
  bool ParseKnownArgs(ArgArray args, std::vector<std::string>* out);

 private:
  std::unique_ptr<ArgumentContainer> container_;
  std::unique_ptr<ArgumentParser> parser_;
};

}  // namespace internal
}  // namespace argparse
