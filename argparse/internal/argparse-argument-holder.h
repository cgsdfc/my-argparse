// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-port.h"

namespace argparse {
namespace internal {
class Argument;
class ArgumentHolder;

class ArgumentGroup {
 public:
  // This is needed to send out notification.
  class Delegate {
   public:
    virtual void OnAddArgument(Argument* arg, ArgumentGroup* group) {}
    virtual ~Delegate() {}
  };

  enum GroupIndex {
    kOptionalGroupIndex = 0,
    kPositionalGroupIndex = 1,
  };

  absl::string_view GetTitle() const { return title_; }
  void SetTitle(absl::string_view title);

  // Add an arg to this group.
  void AddArgument(std::unique_ptr<Argument> arg);
  // Allow fast iteration over all arguments:
  // for (auto i = 0; i < g->GetArgumentCount(); ++i)
  //    g->GetArgument(i);
  std::size_t GetArgumentCount() const { return arguments_.size(); }
  Argument* GetArgument(std::size_t i);

  // ArgumentGroup is allocated on the heap.
  static std::unique_ptr<ArgumentGroup> Create(Delegate* delegate) {
    return absl::WrapUnique(new ArgumentGroup(delegate));
  }

 private:
  explicit ArgumentGroup(Delegate* delegate) : delegate_(delegate) {}
  Delegate* delegate_;
  std::string title_;
  absl::InlinedVector<std::unique_ptr<Argument>, 4> arguments_;
};

class ArgumentHolder final : private ArgumentGroup::Delegate {
 public:
  // Notify outside some event.
  class Delegate {
   public:
    virtual void OnAddArgument(Argument* arg, ArgumentGroup* group) {}
    virtual void OnAddArgumentGroup(ArgumentGroup* group,
                                    ArgumentHolder* holder) {}
    virtual ~Delegate() {}
  };

  // Allocated directly.
  // Two default groups will be created and delegate will be notified.
  explicit ArgumentHolder(Delegate* delegate);

  // Allow fast iteration over all ArgumentGroups.
  std::size_t GetArgumentGroupCount() const { return groups_.size(); }
  // Helper to access the default groups.
  ArgumentGroup* GetDefaultGroup(ArgumentGroup::GroupIndex index) const {
    return GetArgumentGroup(index);
  }
  // 0 is for default option group. 1 is for default positional group.
  ArgumentGroup* GetArgumentGroup(std::size_t i) const {
    ARGPARSE_DCHECK(i < GetArgumentGroupCount());
    return groups_[i].get();
  }
  ArgumentGroup* AddArgumentGroup(std::string title);

  // method to add arg to default group (inferred from arg).
  void AddArgument(std::unique_ptr<Argument> arg);

 private:
  // ArgumentGroup::Delegate:
  void OnAddArgument(Argument* arg, ArgumentGroup* group) override {
    delegate_->OnAddArgument(arg, group);
  }

  Delegate* delegate_;
  // In many cases, there are just default groups, so make the capacity 2.
  absl::InlinedVector<std::unique_ptr<ArgumentGroup>, 2> groups_;
  std::set<std::string> name_set_;
};

}  // namespace internal
}  // namespace argparse
