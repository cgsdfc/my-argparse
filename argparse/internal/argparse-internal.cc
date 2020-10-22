// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-internal.h"

#include <cxxabi.h>

#include <cstdarg>
#include <cstring>
#include <set>


namespace argparse {
namespace internal {

// Holds all meta-info about an argument.
class ArgumentImpl : public Argument {
 public:
  ArgumentImpl() = default;

  DestInfo* GetDest() override { return dest_info_.get(); }
  TypeInfo* GetType() override { return type_info_.get(); }
  ActionInfo* GetAction() override { return action_info_.get(); }
  NumArgsInfo* GetNumArgs() override { return num_args_.get(); }
  const Any* GetConstValue() override { return const_value_.get(); }
  const Any* GetDefaultValue() override { return default_value_.get(); }
  absl::string_view GetMetaVar() override { return meta_var_; }
  ArgumentGroup* GetGroup() override { return group_; }
  NamesInfo* GetNamesInfo() override { return names_info_.get(); }
  bool IsRequired() override { return is_required_; }
  absl::string_view GetHelpDoc() override { return help_doc_; }
  void SetNames(std::unique_ptr<NamesInfo> info) override {
    names_info_ = std::move(info);
  }
  void SetRequired(bool required) override { is_required_ = required; }
  void SetHelpDoc(std::string help_doc) override {
    help_doc_ = std::move(help_doc);
  }
  void SetMetaVar(std::string meta_var) override {
    meta_var_ = std::move(meta_var);
  }
  void SetDest(std::unique_ptr<DestInfo> info) override {
    if (info) dest_info_ = std::move(info);
  }
  void SetType(std::unique_ptr<TypeInfo> info) override {
    if (info) type_info_ = std::move(info);
  }
  void SetAction(std::unique_ptr<ActionInfo> info) override {
    if (info) action_info_ = std::move(info);
  }
  void SetConstValue(std::unique_ptr<Any> value) override {
    if (value) const_value_ = std::move(value);
  }
  void SetDefaultValue(std::unique_ptr<Any> value) override {
    if (value) default_value_ = std::move(value);
  }
  void SetGroup(ArgumentGroup* group) override {
    ARGPARSE_DCHECK(group);
    group_ = group;
  }
  void SetNumArgs(std::unique_ptr<NumArgsInfo> info) override {
    if (info) num_args_ = std::move(info);
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
  explicit ArgumentHolderImpl(SubCommand* cmd);

  ArgumentGroup* AddArgumentGroup(std::string header) override;

  SubCommand* GetSubCommand() override { return subcmd_; }
  void AddArgument(std::unique_ptr<Argument> arg) override {
    auto* group =
        arg->IsOption() ? GetDefaultOptionGroup() : GetDefaultPositionalGroup();
    return group->AddArgument(std::move(arg));
  }

  void ForEachArgument(std::function<void(Argument*)> callback) override {
    for (auto& arg : arguments_) callback(arg.get());
  }
  void ForEachGroup(std::function<void(ArgumentGroup*)> callback) override {
    for (auto& group : groups_) callback(group.get());
  }

  unsigned GetArgumentCount() override { return arguments_.size(); }

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

  void NotifyAddArgumentGroup(ArgumentGroup* group) {
    if (!listener_) return;
    listener_->OnAddArgumentGroup(group);
  }

  SubCommand* subcmd_ = nullptr;
  bool pending_default_groups_notification_ = true;
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
                   "Argument names conflict with existing names!");
  arg->SetGroup(group);
  if (listener_) listener_->OnAddArgument(arg.get());
  arguments_.push_back(std::move(arg));
}

ArgumentHolderImpl::ArgumentHolderImpl(SubCommand* cmd) : subcmd_(cmd) {
  AddArgumentGroup("optional arguments");
  AddArgumentGroup("positional arguments");
}

bool ArgumentHolderImpl::CheckNamesConflict(NamesInfo* names) {
  bool ok = true;
  names->ForEachName(NamesInfo::kAllNames, [this, &ok](const std::string& in) {
    if (!name_set_.insert(in).second) ok = false;
  });
  return ok;
}

class ArgumentHolderImpl::GroupImpl : public ArgumentGroup {
 public:
  GroupImpl(ArgumentHolderImpl* holder, std::string header)
      : holder_(holder), header_(std::move(header)) {
    if (header_.back() != ':') header_.push_back(':');
  }

  void AddArgument(std::unique_ptr<Argument> arg) override {
    ++members_;
    holder_->AddArgumentToGroup(std::move(arg), this);
  }
  absl::string_view GetHeader() override { return header_; }

  unsigned GetArgumentCount() override { return members_; }

  void ForEachArgument(std::function<void(Argument*)> callback) override {
    for (auto& arg : holder_->arguments_) {
      if (arg->GetGroup() == this) callback(arg.get());
    }
  }

  ArgumentHolder* GetHolder() override {
    ARGPARSE_DCHECK(holder_);
    return holder_;
  }

 private:
  ArgumentHolderImpl* holder_;
  std::string header_;  // the text provided by user plus a ':'.
  unsigned members_ = 0;
};

ArgumentGroup* ArgumentHolderImpl::AddArgumentGroup(std::string header) {
  auto* group = new GroupImpl(this, header);
  groups_.emplace_back(group);

  if (pending_default_groups_notification_) {
    pending_default_groups_notification_ = false;
    NotifyAddArgumentGroup(GetDefaultOptionGroup());
    NotifyAddArgumentGroup(GetDefaultPositionalGroup());
  }
  NotifyAddArgumentGroup(group);
  return group;
}

class SubCommandImpl : public SubCommand {
 public:
  explicit SubCommandImpl(std::string name)
      : name_(std::move(name)), holder_(ArgumentHolder::Create(this)) {}

  ArgumentHolder* GetHolder() override { return holder_.get(); }
  void SetGroup(SubCommandGroup* group) override { group_ = group; }
  SubCommandGroup* GetGroup() override { return group_; }
  absl::string_view GetName() override { return name_; }
  absl::string_view GetHelpDoc() override { return help_doc_; }
  void SetAliases(std::vector<std::string> val) override {
    aliases_ = std::move(val);
  }
  void SetHelpDoc(std::string val) override { help_doc_ = std::move(val); }

  void ForEachAlias(std::function<void(const std::string&)> callback) override {
    for (auto& al : aliases_) callback(al);
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
    for (auto& sub : subcmds_) callback(sub.get());
  }
  void ForEachSubCommandGroup(
      std::function<void(SubCommandGroup*)> callback) override {
    for (auto& group : groups_) callback(group.get());
  }

  SubCommandGroup* AddSubCommandGroup(
      std::unique_ptr<SubCommandGroup> group) override {
    auto* group_ptr = group.get();
    group_ptr->SetHolder(this);
    groups_.push_back(std::move(group));
    NotifyAddSubCommandGroup(group_ptr);
    return group_ptr;
  }

  void SetListener(std::unique_ptr<Listener> listener) override {
    listener_ = std::move(listener);
  }

  // static std::unique_ptr<SubCommandGroup> CreateGroup();

  SubCommand* AddSubCommandToGroup(SubCommandGroup* group,
                                   std::unique_ptr<SubCommand> cmd);

 private:
  class GroupImpl;
  class ListenerImpl;

  bool CheckNamesConflict(SubCommand* cmd) {
    if (!name_set_.insert(static_cast<std::string>(cmd->GetName())).second)
      return false;
    bool ok = true;
    cmd->ForEachAlias([this, &ok](const std::string& in) {
      ok = ok && name_set_.insert(in).second;
    });
    return ok;
  }

  void NotifyAddArgument(Argument* arg) {
    if (listener_) listener_->OnAddArgument(arg);
  }
  void NotifyAddArgumentGroup(ArgumentGroup* group) {
    if (listener_) listener_->OnAddArgumentGroup(group);
  }
  void NotifyAddSubCommand(SubCommand* cmd) {
    if (listener_) listener_->OnAddSubCommand(cmd);
  }
  void NotifyAddSubCommandGroup(SubCommandGroup* group) {
    if (listener_) listener_->OnAddSubCommandGroup(group);
  }

  std::unique_ptr<Listener> listener_;
  std::vector<std::unique_ptr<SubCommand>> subcmds_;
  std::vector<std::unique_ptr<SubCommandGroup>> groups_;
  std::set<std::string> name_set_;
};

class GroupImpl : public SubCommandGroup {
 public:
  GroupImpl() = default;

  SubCommand* AddSubCommand(std::unique_ptr<SubCommand> cmd) override {
    ARGPARSE_DCHECK(holder_);
    return holder_->AddSubCommandToGroup(this, std::move(cmd));
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
  void SetHolder(SubCommandHolder* holder) override {
    ARGPARSE_DCHECK(holder);
    // TODO:
    // holder_ = holder;
  }

  absl::string_view GetTitle() override { return title_; }
  absl::string_view GetDescription() override { return description_; }
  ActionInfo* GetAction() override { return action_info_.get(); }
  DestInfo* GetDest() override { return dest_info_.get(); }
  bool IsRequired() override { return required_; }
  absl::string_view GetHelpDoc() override { return help_doc_; }
  absl::string_view GetMetaVar() override { return meta_var_; }
  SubCommandHolder* GetHolder() override { return holder_; }

 private:
  SubCommandHolderImpl* holder_ = nullptr;
  bool required_ = false;
  std::string title_;
  std::string description_;
  std::string help_doc_;
  std::string meta_var_;
  std::unique_ptr<DestInfo> dest_info_;
  std::unique_ptr<ActionInfo> action_info_;
};

class SubCommandHolderImpl::ListenerImpl : public ArgumentHolder::Listener {
 public:
  explicit ListenerImpl(SubCommandHolderImpl* holder) : holder_(holder) {}

 private:
  void OnAddArgument(Argument* arg) override {
    holder_->NotifyAddArgument(arg);
  }
  void OnAddArgumentGroup(ArgumentGroup* group) override {
    holder_->NotifyAddArgumentGroup(group);
  }
  SubCommandHolderImpl* holder_;
};

SubCommand* SubCommandHolderImpl::AddSubCommandToGroup(
    SubCommandGroup* group, std::unique_ptr<SubCommand> cmd) {
  ARGPARSE_CHECK_F(CheckNamesConflict(cmd.get()),
                   "SubCommand name or aliases conflict with existing names!");
  auto* cmd_ptr = cmd.get();
  cmd_ptr->SetGroup(group);
  // Setup listener.
  ARGPARSE_DCHECK(cmd_ptr->GetHolder());
  cmd_ptr->GetHolder()->SetListener(absl::make_unique<ListenerImpl>(this));
  subcmds_.push_back(std::move(cmd));

  if (listener_) listener_->OnAddSubCommand(cmd_ptr);
  return cmd_ptr;
}

static ActionKind StringToActions(const std::string& str) {
  static const std::map<std::string, ActionKind> kStringToActions{
      {"store", ActionKind::kStore},
      {"store_const", ActionKind::kStoreConst},
      {"store_true", ActionKind::kStoreTrue},
      {"store_false", ActionKind::kStoreFalse},
      {"append", ActionKind::kAppend},
      {"append_const", ActionKind::kAppendConst},
      {"count", ActionKind::kCount},
      {"print_help", ActionKind::kPrintHelp},
      {"print_usage", ActionKind::kPrintUsage},
  };
  auto iter = kStringToActions.find(str);
  ARGPARSE_CHECK_F(iter != kStringToActions.end(),
                   "Unknown action string '%s' passed in", str.c_str());
  return iter->second;
}

class ArgumentBuilderImpl : public ArgumentBuilder {
 public:
  ArgumentBuilderImpl() : arg_(Argument::Create()) {}

  void SetNames(std::unique_ptr<NamesInfo> info) override {
    // ARGPARSE_DCHECK_F(!arg_, "SetNames should only be called once");
    arg_->SetNames(std::move(info));
  }

  void SetDest(std::unique_ptr<DestInfo> info) override {
    arg_->SetDest(std::move(info));
  }

  void SetActionString(const char* str) override {
    action_kind_ = StringToActions(str);
  }

  void SetActionCallback(ActionFunction cb) override {
    arg_->SetAction(ActionInfo::CreateFromCallback(std::move(cb)));
  }

  void SetTypeOperations(Operations* ops) override {
    arg_->SetType(TypeInfo::CreateDefault(ops));
  }

  void SetTypeCallback(TypeFunction cb) override {
    arg_->SetType(TypeInfo::CreateFromCallback(std::move(cb)));
  }

  void SetTypeInfo(std::unique_ptr<TypeInfo> info) override {
    if (info) arg_->SetType(std::move(info));
  }

  void SetActionInfo(std::unique_ptr<ActionInfo> info) override {
    if (info) arg_->SetAction(std::move(info));
  }

  void SetTypeFileType(OpenMode mode) override { open_mode_ = mode; }

  void SetNumArgs(std::unique_ptr<NumArgsInfo> info) override {
    if (info) arg_->SetNumArgs(std::move(info));
  }

  void SetConstValue(std::unique_ptr<Any> val) override {
    arg_->SetConstValue(std::move(val));
  }

  void SetDefaultValue(std::unique_ptr<Any> val) override {
    arg_->SetDefaultValue(std::move(val));
  }

  void SetMetaVar(std::string val) override {
    meta_var_ = absl::make_unique<std::string>(std::move(val));
  }

  void SetRequired(bool val) override {
    ARGPARSE_DCHECK(arg_);
    arg_->SetRequired(val);
  }

  void SetHelp(std::string val) override {
    ARGPARSE_DCHECK(arg_);
    arg_->SetHelpDoc(std::move(val));
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

static bool ActionNeedsBool(ActionKind in) {
  return in == ActionKind::kStoreFalse || in == ActionKind::kStoreTrue;
}

static bool ActionNeedsValueType(ActionKind in) {
  return in == ActionKind::kAppend || in == ActionKind::kAppendConst;
}

std::unique_ptr<Argument> ArgumentBuilderImpl::CreateArgument() {
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

  if (!arg_->GetAction()) {
    // We assume a default store action but only if has dest.
    if (action_kind_ == ActionKind::kNoAction && dest)
      action_kind_ = ActionKind::kStore;
    // Some action don't need an ops, like print_help, we perhaps need to
    // distinct that..
    auto* ops = dest ? dest->GetOperations() : nullptr;
    arg_->SetAction(ActionInfo::CreateDefault(action_kind_, ops));
  }

  if (!arg_->GetType()) {
    Operations* ops = nullptr;
    if (dest)
      ops = ActionNeedsValueType(action_kind_) ? dest->GetValueTypeOps()
                                               : dest->GetOperations();
    auto info = open_mode_ == kModeNoMode
                    ? TypeInfo::CreateDefault(ops)
                    : TypeInfo::CreateFileType(ops, open_mode_);
    arg_->SetType(std::move(info));
  }

  return std::move(arg_);
}

class ArgumentContainerImpl : public ArgumentContainer {
 public:
  ArgumentContainerImpl();

  ArgumentHolder* GetMainHolder() override { return main_holder_.get(); }
  SubCommandHolder* GetSubCommandHolder() override {
    return subcmd_holder_.get();
  }

  void AddListener(std::unique_ptr<Listener> listener) override {
    ARGPARSE_DCHECK(listener);
    listeners_.push_back(std::move(listener));
  }

 private:
  template <typename Method, typename... Args>
  void NotifyListeners(Method method, Args&&... args) {
    for (auto& listener : listeners_) {
      ((*listener).*method)(std::forward<Args>(args)...);
    }
  }

  class ListenerImpl;

  std::unique_ptr<ArgumentHolder> main_holder_;
  std::unique_ptr<SubCommandHolder> subcmd_holder_;
  std::vector<std::unique_ptr<Listener>> listeners_;
};

class ArgumentContainerImpl::ListenerImpl : ArgumentHolder::Listener,
                                            SubCommandHolder::Listener {
 public:
  explicit ListenerImpl(ArgumentContainerImpl* impl) : impl_(impl) {}

  void Listen(ArgumentHolder* holder) {
    holder->SetListener(std::unique_ptr<ArgumentHolder::Listener>(this));
  }
  void Listen(SubCommandHolder* holder) {
    holder->SetListener(std::unique_ptr<SubCommandHolder::Listener>(this));
  }

 private:
  void OnAddArgument(Argument* arg) override {
    impl_->NotifyListeners(&ArgumentContainer::Listener::OnAddArgument, arg);
  }
  void OnAddArgumentGroup(ArgumentGroup* group) override {
    impl_->NotifyListeners(&ArgumentContainer::Listener::OnAddArgumentGroup,
                           group);
  }
  void OnAddSubCommand(SubCommand* cmd) override {
    impl_->NotifyListeners(&ArgumentContainer::Listener::OnAddSubCommand, cmd);
  }
  void OnAddSubCommandGroup(SubCommandGroup* group) override {
    impl_->NotifyListeners(&ArgumentContainer::Listener::OnAddSubCommandGroup,
                           group);
  }

  ArgumentContainerImpl* impl_;
};

class ArgumentControllerImpl : public ArgumentController {
 public:
  ArgumentControllerImpl();

  void AddArgument(std::unique_ptr<Argument> arg) override {
    ARGPARSE_DCHECK(arg);
    container_->GetMainHolder()->AddArgument(std::move(arg));
  }

  ArgumentGroup* AddArgumentGroup(std::string title) override {
    return container_->GetMainHolder()->AddArgumentGroup(std::move(title));
  }

  SubCommandGroup* AddSubCommandGroup(
      std::unique_ptr<SubCommandGroup> group) override {
    return container_->GetSubCommandHolder()->AddSubCommandGroup(
        std::move(group));
  }

  // Methods forwarded from ArgumentParser.
  OptionsListener* GetOptionsListener() override {
    return options_listener_.get();
  }

  bool ParseKnownArgs(ArgArray args, std::vector<std::string>* out) override {
    return parser_->ParseKnownArgs(args, out);
  }

 private:
  class ForwardToParserListener;

  std::unique_ptr<ArgumentContainer> container_;
  std::unique_ptr<ArgumentParser> parser_;
  std::unique_ptr<OptionsListener> options_listener_;
};

class ArgumentControllerImpl::ForwardToParserListener
    : ArgumentContainer::Listener {
 public:
 private:
  void OnAddArgument(Argument* arg) { return parser_->AddArgument(arg); }
  void OnAddArgumentGroup(ArgumentGroup* group) {
    return parser_->AddArgumentGroup(group);
  }
  void OnAddSubCommand(SubCommand* cmd) { return parser_->AddSubCommand(cmd); }
  void OnAddSubCommandGroup(SubCommandGroup* group) {
    return parser_->AddSubCommandGroup(group);
  }

  ArgumentParser* parser_;
};

ArgumentControllerImpl::ArgumentControllerImpl()
    : container_(ArgumentContainer::Create()),
      parser_(ArgumentParser::CreateDefault()) {
  options_listener_ = parser_->CreateOptionsListener();
}

ArgumentContainerImpl::ArgumentContainerImpl()
    : main_holder_(ArgumentHolder::Create(nullptr)),
      subcmd_holder_(SubCommandHolder::Create()) {
  (new ListenerImpl(this))->Listen(main_holder_.get());
  (new ListenerImpl(this))->Listen(subcmd_holder_.get());
}

bool Argument::Less(Argument* a, Argument* b) {
  // options go before positionals.
  if (a->IsOption() != b->IsOption()) return a->IsOption();

  // positional compares on their names.
  if (!a->IsOption() && !b->IsOption()) {
    return a->GetName() < b->GetName();
  }

  // required option goes first.
  if (a->IsRequired() != b->IsRequired()) return a->IsRequired();

  // // short-only option (flag) goes before the rest.
  if (a->IsFlag() != b->IsFlag()) return a->IsFlag();

  // a and b are both long options or both flags.
  return a->GetName() < b->GetName();
}

std::unique_ptr<SubCommandGroup> SubCommandGroup::Create() {
  return absl::make_unique<GroupImpl>();
}

std::unique_ptr<ArgumentBuilder> ArgumentBuilder::Create() {
  return absl::make_unique<ArgumentBuilderImpl>();
}

std::unique_ptr<Argument> Argument::Create() {
  return absl::make_unique<ArgumentImpl>();
}

std::unique_ptr<ArgumentHolder> ArgumentHolder::Create(SubCommand* cmd) {
  return absl::make_unique<ArgumentHolderImpl>(cmd);
}

std::unique_ptr<SubCommand> SubCommand::Create(std::string name) {
  return absl::make_unique<SubCommandImpl>(std::move(name));
}

std::unique_ptr<SubCommandHolder> SubCommandHolder::Create() {
  return absl::make_unique<SubCommandHolderImpl>();
}

std::unique_ptr<ArgumentContainer> ArgumentContainer::Create() {
  return absl::make_unique<ArgumentContainerImpl>();
}

std::unique_ptr<ArgumentParser> ArgumentParser::CreateDefault() {
  return nullptr;
}

std::unique_ptr<ArgumentController> ArgumentController::Create() {
  return absl::make_unique<ArgumentControllerImpl>();
}

static std::string Demangle(const char* mangled_name) {
  std::size_t length;
  int status;
  const char* realname =
      abi::__cxa_demangle(mangled_name, nullptr, &length, &status);

  if (status) {
    static constexpr const char kDemangleFailedSub[] =
        "<error-type(demangle failed)>";
    return kDemangleFailedSub;
  }

  ARGPARSE_DCHECK(realname);
  std::string result(realname, length);
  std::free((void*)realname);
  return result;
}

const char* TypeNameImpl(const std::type_info& type) {
  static std::map<std::type_index, std::string> g_typenames;
  auto iter = g_typenames.find(type);
  if (iter == g_typenames.end()) {
    g_typenames.emplace(type, Demangle(type.name()));
  }
  return g_typenames[type].c_str();
}

void CheckFailed(SourceLocation loc, const char* fmt, ...) {
  std::fprintf(stderr, "Error at %s:%d:%s: ", loc.filename, loc.line,
               loc.function);

  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(stderr, fmt, ap);
  va_end(ap);

  std::fprintf(
      stderr,
      "\n\nPlease check your code and read the documents of argparse.\n\n");
  std::abort();
}

const char* OpsToString(OpsKind ops) {
  static const std::map<OpsKind, std::string> kOpsToStrings{
      {OpsKind::kStore, "Store"},   {OpsKind::kStoreConst, "StoreConst"},
      {OpsKind::kAppend, "Append"}, {OpsKind::kAppendConst, "AppendConst"},
      {OpsKind::kParse, "Parse"},   {OpsKind::kOpen, "Open"},
  };
  auto iter = kOpsToStrings.find(ops);
  ARGPARSE_DCHECK(iter != kOpsToStrings.end());
  return iter->second.c_str();
}

}  // namespace internal
}  // namespace argparse
