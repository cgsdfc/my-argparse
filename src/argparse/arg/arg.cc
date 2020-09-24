#include "argparse/arg/argument_impl.h"

namespace argparse {

bool Argument::Less(Argument* a, Argument* b) {
  // options go before positionals.
  if (a->IsOption() != b->IsOption())
    return a->IsOption();

  // positional compares on their names.
  if (!a->IsOption() && !b->IsOption()) {
    return a->GetName() < b->GetName();
  }

  // required option goes first.
  if (a->IsRequired() != b->IsRequired())
    return a->IsRequired();

  // // short-only option (flag) goes before the rest.
  if (a->IsFlag() != b->IsFlag())
    return a->IsFlag();

  // a and b are both long options or both flags.
  return a->GetName() < b->GetName();
}

void ArgumentHolderImpl::AddArgumentToGroup(std::unique_ptr<Argument> arg,
                                            ArgumentGroup* group) {
  // First check if this arg will conflict with existing ones.
  ARGPARSE_CHECK_F(CheckNamesConflict(arg->GetNamesInfo()),
                   "Names conflict with existing names!");
  arg->SetGroup(group);
  if (listener_)
    listener_->OnAddArgument(arg.get());
  arguments_.push_back(std::move(arg));
}

ArgumentHolderImpl::ArgumentHolderImpl() {
  AddArgumentGroup("optional arguments");
  AddArgumentGroup("positional arguments");
}

bool ArgumentHolderImpl::CheckNamesConflict(NamesInfo* names) {
  bool ok = true;
  names->ForEachName(NamesInfo::kAllNames, [this, &ok](const std::string& in) {
    if (!name_set_.insert(in).second)
      ok = false;
  });
  return ok;
}

}  // namespace argparse
