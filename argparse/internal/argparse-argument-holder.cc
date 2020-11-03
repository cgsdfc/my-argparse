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

ArgumentHolder::ArgumentHolder() {
  constexpr absl::string_view kDefaultGroupTitles[] = {
      "positional arguments:",
      "optional arguments:",
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
  return group_ptr;
}

void ArgumentHolder::AddArgument(std::unique_ptr<Argument> arg) {
  ARGPARSE_DCHECK(arg);
  // True == isOption() == OptionalGroupIndex == 1
  auto index = static_cast<ArgumentGroup::GroupIndex>(arg->IsOptional());
  GetDefaultGroup(index)->AddArgument(std::move(arg));
}

void ArgumentHolder::OnAddArgument(Argument* arg, ArgumentGroup* group) {
  CheckNamesConflict(arg);
}

// All names should be checked, including positional names.
void ArgumentHolder::CheckNamesConflict(Argument* arg) {
  auto* info = arg->GetNames();
  for (size_t i = 0; i < info->GetNameCount(); ++i) {
    auto name = info->GetName(i);
    bool ok = name_set_.insert(name).second;
    ARGPARSE_CHECK_F(ok, "Argument name %s conflict with existing names!",
                     name.data());
  }
}

std::size_t ArgumentHolder::GetTotalArgumentCount() const {
  size_t count = 0;
  for (auto&& group : groups_) count += group->GetArgumentCount();
  return count;
}

}  // namespace internal
}  // namespace argparse
