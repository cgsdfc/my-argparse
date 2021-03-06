// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-argument-holder.h"

#include "argparse/internal/argparse-argument.h"

namespace argparse {
namespace internal {

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

ArgumentGroup* ArgumentHolder::AddArgumentGroup(absl::string_view title) {
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
  ++total_argument_count_;
}

// All names should be checked, including positional names.
void ArgumentHolder::CheckNamesConflict(Argument* arg) {
  auto* info = arg->GetNames();
  for (size_t i = 0; i < info->GetNameCount(); ++i) {
    auto name = info->GetName(i);
    bool ok = name_set_.insert(name).second;
    if (!ok)
      ARGPARSE_INTERNAL_LOG(FATAL,
                            "Argument name '%s' conflicts with existing names.",
                            name.data());
  }
}

}  // namespace internal
}  // namespace argparse
