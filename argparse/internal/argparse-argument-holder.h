#pragma once

#include <array>

#include "argparse/internal/argparse-port.h"

namespace argparse {
namespace internal {
class Argument;
class ArgumentHolder;

class ArgumentGroup {
 public:
  enum GroupIndex {
    kOptionalGroupIndex = 0,
    kPositionalGroupIndex = 1,
  };

  ~ArgumentGroup() {}
  absl::string_view GetTitle();
//   GroupKind GetGroupKind() const;
  // Add an arg to this group.
  void AddArgument(std::unique_ptr<Argument> arg);
  // Allow fast iteration over all arguments:
  // for (auto i = 0; i < g->GetArgumentCount(); ++i)
  //    g->GetArgument(i);
  std::size_t GetArgumentCount();
  Argument* GetArgument(std::size_t i);

  // ArgumentGroup is allocated on the heap.
  static std::unique_ptr<ArgumentGroup> Create(absl::string_view title);

 private:
  std::string title_;
  std::vector<std::unique_ptr<Argument>> arguments_;
};

class ArgumentHolder {
 public:
  // Notify outside some event.
  class Delegate {
   public:
   virtual void OnAddArgument(Argument* arg) {}
   virtual void OnAddArgumentGroup(ArgumentGroup* group) {}
   virtual ~Delegate() {}
  };

  // Allocated directly.
  // Two default groups will be created and delegate will be notified.
  explicit ArgumentHolder(Delegate* delegate);

  // Allow fast iteration over all ArgumentGroups.
  std::size_t GetArgumentGroupCount();
  // Helper to access the default groups.
  ArgumentGroup* GetDefaultGroup(ArgumentGroup::GroupIndex index);
  // 0 is for default option group. 1 is for default positional group.
  ArgumentGroup* GetArgumentGroup(std::size_t i);
  ArgumentGroup* AddArgumentGroup(std::string title);

  // method to add arg to default group (inferred from arg).
  void AddArgument(std::unique_ptr<Argument> arg);
  ~ArgumentHolder() {}

 private:
  Delegate* delegate_;
  // In many cases, there are just default groups, so make the capacity 2.
  absl::InlinedVector<std::unique_ptr<ArgumentGroup>, 2> groups_;
  std::set<std::string> name_set_;
};

}  // namespace internal
}  // namespace argparse
