// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-argument-holder.h"

#include "argparse/internal/argparse-argument.h"

namespace argparse {
namespace internal {

Argument* ArgumentGroup::GetArgument(std::size_t i) {
  ARGPARSE_DCHECK(i < GetArgumentCount());
  return arguments_[i].get();
}

void ArgumentGroup::AddArgument(std::unique_ptr<Argument> arg) {
  ARGPARSE_DCHECK(arg);
  auto* arg_ptr = arg.get();
  arguments_.push_back(std::move(arg));
  delegate_->OnAddArgument(arg_ptr, this);
}

void ArgumentGroup::SetTitle(absl::string_view title) {
  ARGPARSE_DCHECK(!title.empty());
  title_ = std::string(title);
  if (title_.back() != ':') {
    title_.push_back(':');
  }
}

ArgumentHolder::ArgumentHolder(Delegate* delegate) : delegate_(delegate) {
  ARGPARSE_DCHECK(delegate_);
  constexpr absl::string_view kDefaultGroupTitles[] = {
      "optional arguments:",
      "positional arguments:",
  };
  for (auto title : kDefaultGroupTitles) {
    AddArgumentGroup(std::string(title));
  }
}

ArgumentGroup* ArgumentHolder::AddArgumentGroup(std::string title) {
  auto group = ArgumentGroup::Create(this);
  group->SetTitle(title);
  auto* group_ptr = group.get();
  groups_.push_back(std::move(group));
  delegate_->OnAddArgumentGroup(group_ptr, this);
  return group_ptr;
}

void ArgumentHolder::AddArgument(std::unique_ptr<Argument> arg) {
  ARGPARSE_DCHECK(arg);
  auto index = static_cast<ArgumentGroup::GroupIndex>(arg->IsOption());
  GetDefaultGroup(index)->AddArgument(std::move(arg));
}

void ArgumentHolder::OnAddArgument(Argument* arg, ArgumentGroup* group) {
  CheckNamesConflict(arg);
  delegate_->OnAddArgument(arg, group);
}

// All names should be checked, including positional names.
void ArgumentHolder::CheckNamesConflict(Argument* arg) {
  bool ok = true;
  auto* info = arg->GetNamesInfo();
  info->ForEachName(NamesInfo::kAllNames, [this, &ok](const std::string& in) {
    // If ok becomes false, don't bother inserting any more.
    ok = ok && name_set_.insert(in).second;
  });
  ARGPARSE_CHECK_F(ok, "Argument names conflict with existing names!");
}

}  // namespace internal
}  // namespace argparse
