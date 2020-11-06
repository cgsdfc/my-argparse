// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "absl/container/flat_hash_set.h"
#include "argparse/internal/argparse-argument.h"

namespace argparse {
namespace internal {

class Argument;
class ArgumentHolder;

// ArgumentGroup
// It owns a list of `Argument` object with pointer stability and a title that
// decribes what this group is for.
class ArgumentGroup : public SupportUserData {
 public:
  // This is needed to send out notification.
  class Delegate {
   public:
    virtual void OnAddArgument(Argument* arg, ArgumentGroup* group) {}
    virtual ~Delegate() {}
  };

  // Two special groups will be created by default, which can always be accessed
  // under these constants.
  enum GroupIndex {
    kPositionalGroupIndex = 0,
    kOptionalGroupIndex = 1,
    kOtherGroupIndex = 2,
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

  // ArgumentGroup is allocated on the heap for pointer stability.
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
  // Allocated directly.
  // Two default groups will be created and delegate will be notified.
  ArgumentHolder();

  // Allow fast iteration over all ArgumentGroups.
  std::size_t GetArgumentGroupCount() const { return groups_.size(); }

  // 0 is for default option group. 1 is for default positional group.
  ArgumentGroup* GetArgumentGroup(std::size_t i) const {
    ARGPARSE_DCHECK(i < GetArgumentGroupCount());
    return groups_[i].get();
  }

  // Helper to access the default groups.
  ArgumentGroup* GetDefaultGroup(ArgumentGroup::GroupIndex index) const {
    return GetArgumentGroup(index);
  }
  ArgumentGroup* AddArgumentGroup(std::string title);

  // method to add arg to default group (inferred from arg).
  void AddArgument(std::unique_ptr<Argument> arg);

  // Return the total number of arguments in all groups.
  std::size_t GetTotalArgumentCount() const;

 private:
  // All the names of the arguments from all groups, including optional and
  // positional ones should not be duplicated. The namespace is not per
  // ArgumentGroup, but per ArgumentHolder. Namely, arguments in different
  // groups but within the same holder will share a single namespace.
  void CheckNamesConflict(Argument* arg);
  // ArgumentGroup::Delegate:
  void OnAddArgument(Argument* arg, ArgumentGroup* group) override;

  // In many cases, there are just default groups, so make the capacity 2.
  absl::InlinedVector<std::unique_ptr<ArgumentGroup>, 2> groups_;
  // The strings are kept alive by NamesInfo.
  absl::flat_hash_set<absl::string_view> name_set_;
};

}  // namespace internal
}  // namespace argparse
