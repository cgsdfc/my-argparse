// Impl argparse-holder.h

namespace argparse {
namespace internal {

// Holds all meta-info about an argument.
class ArgumentImpl : public Argument {
 public:
  explicit ArgumentImpl(std::unique_ptr<NamesInfo> names)
      : names_info_(std::move(names)) {}

  DestInfo* GetDest() override { return dest_info_.get(); }
  TypeInfo* GetType() override { return type_info_.get(); }
  ActionInfo* GetAction() override { return action_info_.get(); }
  NumArgsInfo* GetNumArgs() override { return num_args_.get(); }
  const Any* GetConstValue() override { return const_value_.get(); }
  const Any* GetDefaultValue() override { return default_value_.get(); }
  StringView GetMetaVar() override { return meta_var_; }
  ArgumentGroup* GetGroup() override { return group_; }
  NamesInfo* GetNamesInfo() override { return names_info_.get(); }
  bool IsRequired() override { return is_required_; }
  StringView GetHelpDoc() override { return help_doc_; }
  void SetRequired(bool required) override { is_required_ = required; }
  void SetHelpDoc(std::string help_doc) override {
    help_doc_ = std::move(help_doc);
  }
  void SetMetaVar(std::string meta_var) override {
    meta_var_ = std::move(meta_var);
  }
  void SetDest(std::unique_ptr<DestInfo> info) override {
    ARGPARSE_DCHECK(info);
    dest_info_ = std::move(info);
  }
  void SetType(std::unique_ptr<TypeInfo> info) override {
    ARGPARSE_DCHECK(info);
    type_info_ = std::move(info);
  }
  void SetAction(std::unique_ptr<ActionInfo> info) override {
    ARGPARSE_DCHECK(info);
    action_info_ = std::move(info);
  }
  void SetConstValue(std::unique_ptr<Any> value) override {
    ARGPARSE_DCHECK(value);
    const_value_ = std::move(value);
  }
  void SetDefaultValue(std::unique_ptr<Any> value) override {
    ARGPARSE_DCHECK(value);
    default_value_ = std::move(value);
  }
  void SetGroup(ArgumentGroup* group) override {
    ARGPARSE_DCHECK(group);
    group_ = group;
  }
  void SetNumArgs(std::unique_ptr<NumArgsInfo> info) override {
    ARGPARSE_DCHECK(info);
    num_args_ = std::move(info);
  }

 private:
  ArgumentGroup* group_ = nullptr;
  std::string help_doc_;
  std::string meta_var_;
  bool is_required_ = false;

  std::unique_ptr<NamesInfo> names_info_;
  std::unique_ptr<DestInfo> dest_info_;
  std::unique_ptr<ActionInfo> action_info_;
  std::unique_ptr<TypeInfo> type_info_;
  std::unique_ptr<NumArgsInfo> num_args_;
  std::unique_ptr<Any> const_value_;
  std::unique_ptr<Any> default_value_;
};

class ArgumentHolderImpl : public ArgumentHolder {
 public:
  ArgumentHolderImpl();

  ArgumentGroup* AddArgumentGroup(const char* header) override;

  void AddArgument(std::unique_ptr<Argument> arg) override {
    auto* group =
        arg->IsOption() ? GetDefaultOptionGroup() : GetDefaultPositionalGroup();
    return group->AddArgument(std::move(arg));
  }

  void ForEachArgument(std::function<void(Argument*)> callback) override {
    for (auto& arg : arguments_)
      callback(arg.get());
  }
  void ForEachGroup(std::function<void(ArgumentGroup*)> callback) override {
    for (auto& group : groups_)
      callback(group.get());
  }

  unsigned GetArgumentCount() override { return arguments_.size(); }

  // TODO: merge listener into one class about the events during argument
  // adding.
  void SetListener(std::unique_ptr<Listener> listener) override {
    listener_ = std::move(listener);
  }

 private:
  enum GroupID {
    kOptionGroup = 0,
    kPositionalGroup = 1,
  };

  class GroupImpl;

  // Add an arg to a specific group.
  void AddArgumentToGroup(std::unique_ptr<Argument> arg, ArgumentGroup* group);
  ArgumentGroup* GetDefaultOptionGroup() const {
    return groups_[kOptionGroup].get();
  }
  ArgumentGroup* GetDefaultPositionalGroup() const {
    return groups_[kPositionalGroup].get();
  }

  bool CheckNamesConflict(NamesInfo* names);

  std::unique_ptr<Listener> listener_;
  // Hold the storage of all args.
  std::vector<std::unique_ptr<Argument>> arguments_;
  std::vector<std::unique_ptr<ArgumentGroup>> groups_;
  // Conflicts checking.
  std::set<std::string> name_set_;
};

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

class ArgumentHolderImpl::GroupImpl : public ArgumentGroup {
 public:
  GroupImpl(ArgumentHolderImpl* holder, const char* header)
      : holder_(holder), header_(header) {
    ARGPARSE_DCHECK(header_.size());
    if (header_.back() != ':')
      header_.push_back(':');
  }

  void AddArgument(std::unique_ptr<Argument> arg) override {
    ++members_;
    holder_->AddArgumentToGroup(std::move(arg), this);
  }
  StringView GetHeader() override { return header_; }

  unsigned GetArgumentCount() override { return members_; }

  void ForEachArgument(std::function<void(Argument*)> callback) override {
    for (auto& arg : holder_->arguments_) {
      if (arg->GetGroup() == this)
        callback(arg.get());
    }
  }

 private:
  ArgumentHolderImpl* holder_;
  std::string header_;  // the text provided by user plus a ':'.
  unsigned members_ = 0;
};

ArgumentGroup* ArgumentHolderImpl::AddArgumentGroup(const char* header) {
  auto* group = new GroupImpl(this, header);
  groups_.emplace_back(group);
  if (listener_)
    listener_->OnAddArgumentGroup(group);
  return group;
}

class SubCommandImpl : public SubCommand {
 public:
  explicit SubCommandImpl(std::string name) : name_(std::move(name)) {}

  ArgumentHolder* GetHolder() override { return holder_.get(); }
  void SetGroup(SubCommandGroup* group) override { group_ = group; }
  SubCommandGroup* GetGroup() override { return group_; }
  StringView GetName() override { return name_; }
  StringView GetHelpDoc() override { return help_doc_; }
  void SetAliases(std::vector<std::string> val) override {
    aliases_ = std::move(val);
  }
  void SetHelpDoc(std::string val) override { help_doc_ = std::move(val); }

  void ForEachAlias(std::function<void(const std::string&)> callback) override {
    for (auto& al : aliases_)
      callback(al);
  }

 private:
  SubCommandGroup* group_ = nullptr;
  std::string name_;
  std::string help_doc_;
  std::vector<std::string> aliases_;
  std::unique_ptr<ArgumentHolder> holder_;
};

class SubCommandHolderImpl : public SubCommandHolder {
 public:
  void ForEachSubCommand(std::function<void(SubCommand*)> callback) override {
    for (auto& sub : subcmds_)
      callback(sub.get());
  }
  void ForEachSubCommandGroup(
      std::function<void(SubCommandGroup*)> callback) override {
    for (auto& group : groups_)
      callback(group.get());
  }

  SubCommandGroup* AddSubCommandGroup(
      std::unique_ptr<SubCommandGroup> group) override {
    auto* g = group.get();
    groups_.push_back(std::move(group));
    return g;
  }

  void SetListener(std::unique_ptr<Listener> listener) override {
    listener_ = std::move(listener);
  }

 private:
  SubCommand* AddSubCommandToGroup(SubCommandGroup* group,
                                   std::unique_ptr<SubCommand> cmd) {
    cmd->SetGroup(group);
    subcmds_.push_back(std::move(cmd));
    return subcmds_.back().get();
  }

  std::unique_ptr<Listener> listener_;
  std::vector<std::unique_ptr<SubCommand>> subcmds_;
  std::vector<std::unique_ptr<SubCommandGroup>> groups_;
};

class SubCommandGroupImpl : public SubCommandGroup {
 public:
  SubCommandGroupImpl() = default;

  SubCommand* AddSubCommand(std::unique_ptr<SubCommand> cmd) override {
    auto* cmd_ptr = cmd.get();
    commands_.push_back(std::move(cmd));
    cmd_ptr->SetGroup(this);
    return cmd_ptr;
  }

  void SetTitle(std::string val) override { title_ = std::move(val); }
  void SetDescription(std::string val) override {
    description_ = std::move(val);
  }
  void SetAction(std::unique_ptr<ActionInfo> info) override {
    action_info_ = std::move(info);
  }
  void SetDest(std::unique_ptr<DestInfo> info) override {
    dest_info_ = std::move(info);
  }
  void SetRequired(bool val) override { required_ = val; }
  void SetHelpDoc(std::string val) override { help_doc_ = std::move(val); }
  void SetMetaVar(std::string val) override { meta_var_ = std::move(val); }

  StringView GetTitle() override { return title_; }
  StringView GetDescription() override { return description_; }
  ActionInfo* GetAction() override { return action_info_.get(); }
  DestInfo* GetDest() override { return dest_info_.get(); }
  bool IsRequired() override { return required_; }
  StringView GetHelpDoc() override { return help_doc_; }
  StringView GetMetaVar() override { return meta_var_; }

 private:
  bool required_ = false;
  std::string title_;
  std::string description_;
  std::string help_doc_;
  std::string meta_var_;
  std::unique_ptr<DestInfo> dest_info_;
  std::unique_ptr<ActionInfo> action_info_;
  std::vector<std::unique_ptr<SubCommand>> commands_;
};

class ArgumentFactoryImpl : public ArgumentFactory {
 public:
  void SetNames(std::unique_ptr<NamesInfo> info) override {
    ARGPARSE_DCHECK_F(!arg_, "SetNames should only be called once");
    arg_ = Argument::Create(std::move(info));
  }

  void SetDest(std::unique_ptr<DestInfo> info) override {
    arg_->SetDest(std::move(info));
  }

  void SetActionString(const char* str) override {
    action_kind_ = StringToActions(str);
  }

  void SetTypeOperations(std::unique_ptr<Operations> ops) override {
    arg_->SetType(TypeInfo::CreateDefault(std::move(ops)));
  }

  void SetTypeCallback(std::unique_ptr<TypeCallback> cb) override {
    arg_->SetType(TypeInfo::CreateFromCallback(std::move(cb)));
  }

  void SetTypeFileType(OpenMode mode) override { open_mode_ = mode; }

  void SetNumArgsFlag(char flag) override {
    arg_->SetNumArgs(NumArgsInfo::CreateFromFlag(flag));
  }

  void SetNumArgsNumber(int num) override {
    arg_->SetNumArgs(NumArgsInfo::CreateFromNum(num));
  }

  void SetConstValue(std::unique_ptr<Any> val) override {
    arg_->SetConstValue(std::move(val));
  }

  void SetDefaultValue(std::unique_ptr<Any> val) override {
    arg_->SetDefaultValue(std::move(val));
  }

  void SetMetaVar(std::string val) override {
    meta_var_ = std::make_unique<std::string>(std::move(val));
  }

  void SetRequired(bool val) override {
    ARGPARSE_DCHECK(arg_);
    arg_->SetRequired(val);
  }

  void SetHelp(std::string val) override {
    ARGPARSE_DCHECK(arg_);
    arg_->SetHelpDoc(std::move(val));
  }

  void SetActionCallback(std::unique_ptr<ActionCallback> cb) override {
    arg_->SetAction(ActionInfo::CreateFromCallback(std::move(cb)));
  }

  std::unique_ptr<Argument> CreateArgument() override;

 private:
  // Some options are directly fed into arg.
  std::unique_ptr<Argument> arg_;
  // If not given, use default from NamesInfo.
  std::unique_ptr<std::string> meta_var_;
  ActionKind action_kind_ = ActionKind::kNoAction;
  OpenMode open_mode_ = kModeNoMode;
};

std::unique_ptr<Argument> ArgumentFactoryImpl::CreateArgument() {
  ARGPARSE_DCHECK(arg_);
  arg_->SetMetaVar(meta_var_ ? std::move(*meta_var_)
                             : arg_->GetNamesInfo()->GetDefaultMetaVar());

  // Put a bool if needed.
  if (ActionNeedsBool(action_kind_)) {
    const bool kStoreTrue = action_kind_ == ActionKind::kStoreTrue;
    arg_->SetDefaultValue(MakeAny<bool>(!kStoreTrue));
    arg_->SetConstValue(MakeAny<bool>(kStoreTrue));
  }

  // Important phrase..
  auto* dest = arg_->GetDest();
  OpsFactory* factory = dest ? dest->GetOpsFactory() : nullptr;

  if (!arg_->GetAction()) {
    // We assume a default store action but only if has dest.
    if (action_kind_ == ActionKind::kNoAction && dest)
      action_kind_ = ActionKind::kStore;
    // Some action don't need an ops, like print_help, we perhaps need to
    // distinct that..
    auto ops = factory ? factory->CreateOps() : nullptr;
    arg_->SetAction(ActionInfo::CreateDefault(action_kind_, std::move(ops)));
  }

  if (!arg_->GetType()) {
    std::unique_ptr<Operations> ops = nullptr;
    if (factory)
      ops = ActionNeedsValueType(action_kind_) ? factory->CreateValueTypeOps()
                                               : factory->CreateOps();
    auto info = open_mode_ == kModeNoMode
                    ? TypeInfo::CreateDefault(std::move(ops))
                    : TypeInfo::CreateFileType(std::move(ops), open_mode_);
    arg_->SetType(std::move(info));
  }

  return std::move(arg_);
}

}  // namespace internal

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

std::unique_ptr<SubCommandGroup> SubCommandGroup::Create() {
  return std::make_unique<SubCommandGroupImpl>();
}

std::unique_ptr<ArgumentFactory> ArgumentFactory::Create() {
  return std::make_unique<ArgumentFactoryImpl>();
}

std::unique_ptr<Argument> Argument::Create(std::unique_ptr<NamesInfo> info) {
  ARGPARSE_DCHECK(info);
  return std::make_unique<ArgumentImpl>(std::move(info));
}

std::unique_ptr<ArgumentHolder> ArgumentHolder::Create() {
  return std::make_unique<ArgumentHolderImpl>();
}

std::unique_ptr<SubCommand> SubCommand::Create(std::string name) {
  return std::make_unique<SubCommandImpl>(std::move(name));
}

std::unique_ptr<SubCommandHolder> SubCommandHolder::Create() {
  return std::make_unique<SubCommandHolderImpl>();
}

}  // namespace argparse
