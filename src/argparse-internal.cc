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
  explicit ArgumentImpl(std::unique_ptr<NamesInfo> names)
      : names_info_(std::move(names)) {}

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

class NumberNumArgsInfo : public NumArgsInfo {
 public:
  explicit NumberNumArgsInfo(unsigned num) : num_(num) {}
  bool Run(unsigned in, std::string* errmsg) override {
    if (in == num_) return true;
    std::ostringstream os;
    os << "expected " << num_ << " values, got " << in;
    *errmsg = os.str();
    return false;
  }

 private:
  const unsigned num_;
};

class FlagNumArgsInfo : public NumArgsInfo {
 public:
  explicit FlagNumArgsInfo(char flag);
  bool Run(unsigned in, std::string* errmsg) override;

 private:
  const char flag_;
};

// ActionInfo for builtin actions like store and append.
class DefaultActionInfo : public ActionInfo {
 public:
  DefaultActionInfo(ActionKind action_kind, std::unique_ptr<Operations> ops)
      : action_kind_(action_kind), ops_(std::move(ops)) {}

  void Run(CallbackClient* client) override;

 private:
  // Since kind of action is too much, we use a switch instead of subclasses.
  ActionKind action_kind_;
  std::unique_ptr<Operations> ops_;
};

// class TypeLessActionInfo : public

// Adapt an ActionCallback to ActionInfo.
class ActionCallbackInfo : public ActionInfo {
 public:
  explicit ActionCallbackInfo(std::unique_ptr<ActionCallback> cb)
      : action_callback_(std::move(cb)) {}

  void Run(CallbackClient* client) override {
    return action_callback_->Run(client->GetDestPtr(), client->GetData());
  }

 private:
  std::unique_ptr<ActionCallback> action_callback_;
};

// The default of TypeInfo: parse a single string into a value
// using ParseTraits.
class DefaultTypeInfo : public TypeInfo {
 public:
  explicit DefaultTypeInfo(std::unique_ptr<Operations> ops)
      : ops_(std::move(ops)) {}

  void Run(const std::string& in, OpsResult* out) override {
    return ops_->Parse(in, out);
  }

  std::string GetTypeHint() override { return ops_->GetTypeHint(); }

 private:
  std::unique_ptr<Operations> ops_;
};

// TypeInfo that opens a file according to some mode.
class FileTypeInfo : public TypeInfo {
 public:
  // TODO: set up cache of Operations objs..
  FileTypeInfo(std::unique_ptr<Operations> ops, OpenMode mode)
      : ops_(std::move(ops)), mode_(mode) {
    ARGPARSE_DCHECK(mode != kModeNoMode);
  }

  void Run(const std::string& in, OpsResult* out) override {
    return ops_->Open(in, mode_, out);
  }

  std::string GetTypeHint() override { return ops_->GetTypeHint(); }

 private:
  std::unique_ptr<Operations> ops_;
  OpenMode mode_;
};

// TypeInfo that runs user's callback.
class TypeCallbackInfo : public TypeInfo {
 public:
  explicit TypeCallbackInfo(std::unique_ptr<TypeCallback> cb)
      : type_callback_(std::move(cb)) {}

  void Run(const std::string& in, OpsResult* out) override {
    return type_callback_->Run(in, out);
  }

  std::string GetTypeHint() override { return type_callback_->GetTypeHint(); }

 private:
  std::unique_ptr<TypeCallback> type_callback_;
};

static bool IsValidNumArgsFlag(char in) {
  return in == '+' || in == '*' || in == '+';
}

static const char* FlagToString(char flag) {
  switch (flag) {
    case '+':
      return "one or more";
    case '?':
      return "zero or one";
    case '*':
      return "zero or more";
    default:
      ARGPARSE_DCHECK(false);
  }
}

bool FlagNumArgsInfo::Run(unsigned in, std::string* errmsg) {
  bool ok = false;
  switch (flag_) {
    case '+':
      ok = in >= 1;
      break;
    case '?':
      ok = in == 0 || in == 1;
      break;
    case '*':
      ok = true;
    default:
      ARGPARSE_DCHECK(false);
  }
  if (ok) return true;
  std::ostringstream os;
  os << "expected " << FlagToString(flag_) << " values, got " << in;
  *errmsg = os.str();
  return false;
}

FlagNumArgsInfo::FlagNumArgsInfo(char flag) : flag_(flag) {
  ARGPARSE_CHECK_F(IsValidNumArgsFlag(flag), "Not a valid flag to nargs: %c",
                   flag);
}

void DefaultActionInfo::Run(CallbackClient* client) {
  auto dest_ptr = client->GetDestPtr();
  auto data = client->GetData();

  switch (action_kind_) {
    case ActionKind::kNoAction:
      break;
    case ActionKind::kStore:
      ops_->Store(dest_ptr, std::move(data));
      break;
    case ActionKind::kStoreTrue:
    case ActionKind::kStoreFalse:
    case ActionKind::kStoreConst:
      ops_->StoreConst(dest_ptr, *client->GetConstValue());
      break;
    case ActionKind::kAppend:
      ops_->Append(dest_ptr, std::move(data));
      break;
    case ActionKind::kAppendConst:
      ops_->AppendConst(dest_ptr, *client->GetConstValue());
      break;
    case ActionKind::kPrintHelp:
      client->PrintHelp();
      break;
    case ActionKind::kPrintUsage:
      client->PrintUsage();
      break;
    case ActionKind::kCustom:
      break;
    case ActionKind::kCount:
      ops_->Count(dest_ptr);
      break;
  }
}

class PositionalName : public NamesInfo {
 public:
  explicit PositionalName(std::string name) : name_(std::move(name)) {}

  bool IsOption() override { return false; }
  std::string GetDefaultMetaVar() override { return ToUpper(name_); }
  void ForEachName(NameKind name_kind,
                   std::function<void(const std::string&)> callback) override {
    if (name_kind == kPosName) callback(name_);
  }
  absl::string_view GetName() override { return name_; }

 private:
  std::string name_;
};

class OptionalNames : public NamesInfo {
 public:
  explicit OptionalNames(const std::vector<std::string>& names) {
    for (auto& name : names) {
      ARGPARSE_CHECK_F(IsValidOptionName(name), "Not a valid option name: %s",
                       name.c_str());
      if (IsLongOptionName(name)) {
        long_names_.push_back(name);
      } else {
        ARGPARSE_DCHECK(IsShortOptionName(name));
        short_names_.push_back(name);
      }
    }
  }

  bool IsOption() override { return true; }
  unsigned GetLongNamesCount() override { return long_names_.size(); }
  unsigned GetShortNamesCount() override { return short_names_.size(); }

  std::string GetDefaultMetaVar() override {
    std::string in =
        long_names_.empty() ? short_names_.front() : long_names_.front();
    std::replace(in.begin(), in.end(), '-', '_');
    return ToUpper(in);
  }

  void ForEachName(NameKind name_kind,
                   std::function<void(const std::string&)> callback) override {
    switch (name_kind) {
      case kPosName:
        return;
      case kLongName: {
        for (auto& name : long_names_) callback(name);
        break;
      }
      case kShortName: {
        for (auto& name : short_names_) callback(name);
        break;
      }
      case kAllNames: {
        for (auto& name : long_names_) callback(name);
        for (auto& name : short_names_) callback(name);
        break;
      }
      default:
        break;
    }
  }

  absl::string_view GetName() override {
    const auto& name =
        long_names_.empty() ? short_names_.front() : long_names_.front();
    return name;
  }

 private:
  std::vector<std::string> long_names_;
  std::vector<std::string> short_names_;
};

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

std::unique_ptr<Argument> Argument::Create(std::unique_ptr<NamesInfo> info) {
  ARGPARSE_DCHECK(info);
  return absl::make_unique<ArgumentImpl>(std::move(info));
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

std::unique_ptr<TypeInfo> TypeInfo::CreateDefault(
    std::unique_ptr<Operations> ops) {
  return absl::make_unique<DefaultTypeInfo>(std::move(ops));
}

std::unique_ptr<TypeInfo> TypeInfo::CreateFileType(
    std::unique_ptr<Operations> ops, OpenMode mode) {
  return absl::make_unique<FileTypeInfo>(std::move(ops), mode);
}

// Invoke user's callback.
std::unique_ptr<TypeInfo> TypeInfo::CreateFromCallback(
    std::unique_ptr<TypeCallback> cb) {
  return absl::make_unique<TypeCallbackInfo>(std::move(cb));
}

std::unique_ptr<NumArgsInfo> NumArgsInfo::CreateFromFlag(char flag) {
  return absl::make_unique<FlagNumArgsInfo>(flag);
}

std::unique_ptr<NumArgsInfo> NumArgsInfo::CreateFromNum(int num) {
  ARGPARSE_CHECK_F(num >= 0, "nargs number must be >= 0");
  return absl::make_unique<NumberNumArgsInfo>(num);
}

std::unique_ptr<ActionInfo> ActionInfo::CreateDefault(
    ActionKind action_kind, std::unique_ptr<Operations> ops) {
  return absl::make_unique<DefaultActionInfo>(action_kind, std::move(ops));
}

std::unique_ptr<ActionInfo> ActionInfo::CreateFromCallback(
    std::unique_ptr<ActionCallback> cb) {
  return absl::make_unique<ActionCallbackInfo>(std::move(cb));
}

std::unique_ptr<NamesInfo> NamesInfo::CreatePositional(std::string in) {
  return absl::make_unique<PositionalName>(std::move(in));
}

std::unique_ptr<NamesInfo> NamesInfo::CreateOptional(
    const std::vector<std::string>& in) {
  return absl::make_unique<OptionalNames>(in);
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

std::string ModeToChars(OpenMode mode) {
  std::string m;
  if (mode & kModeRead) m.append("r");
  if (mode & kModeWrite) m.append("w");
  if (mode & kModeAppend) m.append("a");
  if (mode & kModeBinary) m.append("b");
  return m;
}

void CFileOpenTraits::Run(const std::string& in, OpenMode mode,
                          Result<FILE*>* out) {
  auto mode_str = ModeToChars(mode);
  auto* file = std::fopen(in.c_str(), mode_str.c_str());
  if (file) return out->SetValue(file);
  if (int e = errno) {
    errno = 0;
    return out->SetError(std::strerror(e));
  }
  out->SetError(kDefaultOpenFailureMsg);
}

std::ios_base::openmode ModeToStreamMode(OpenMode m) {
  std::ios_base::openmode out;
  if (m & kModeRead) out |= std::ios_base::in;
  if (m & kModeWrite) out |= std::ios_base::out;
  if (m & kModeAppend) out |= std::ios_base::app;
  if (m & kModeTruncate) out |= std::ios_base::trunc;
  if (m & kModeBinary) out |= std::ios_base::binary;
  return out;
}

OpenMode StreamModeToMode(std::ios_base::openmode stream_mode) {
  int m = kModeNoMode;
  if (stream_mode & std::ios_base::in) m |= kModeRead;
  if (stream_mode & std::ios_base::out) m |= kModeWrite;
  if (stream_mode & std::ios_base::app) m |= kModeAppend;
  if (stream_mode & std::ios_base::trunc) m |= kModeTruncate;
  if (stream_mode & std::ios_base::binary) m |= kModeBinary;
  return static_cast<OpenMode>(m);
}

OpenMode CharsToMode(const char* str) {
  ARGPARSE_DCHECK(str);
  int m;
  for (; *str; ++str) {
    switch (*str) {
      case 'r':
        m |= kModeRead;
        break;
      case 'w':
        m |= kModeWrite;
        break;
      case 'a':
        m |= kModeAppend;
        break;
      case 'b':
        m |= kModeBinary;
        break;
      case '+':
        // Valid combs are a+, w+, r+.
        if (m & kModeAppend)
          m |= kModeRead;
        else if (m & kModeWrite)
          m |= kModeRead;
        else if (m & kModeRead)
          m |= kModeWrite;
        break;
    }
  }
  return static_cast<OpenMode>(m);
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

bool IsValidPositionalName(const std::string& name) {
  auto len = name.size();
  if (!len || !std::isalpha(name[0])) return false;

  return std::all_of(name.begin() + 1, name.end(), [](char c) {
    return std::isalnum(c) || c == '-' || c == '_';
  });
}

bool IsValidOptionName(const std::string& name) {
  auto len = name.size();
  if (len < 2 || name[0] != '-') return false;
  if (len == 2)  // This rules out -?, -* -@ -= --
    return std::isalnum(name[1]);
  // check for long-ness.
  // TODO: fixthis.
  ARGPARSE_CHECK_F(
      name[1] == '-',
      "Single-dash long option (i.e., -jar) is not supported. Please use "
      "GNU-style long option (double-dash)");

  return std::all_of(name.begin() + 2, name.end(), [](char c) {
    return c == '-' || c == '_' || std::isalnum(c);
  });
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
