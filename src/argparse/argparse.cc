#include "argparse/argparse.h"

#include <cstdlib>  // malloc()
#include <cstring>  // strlen()
#include <cxxabi.h> // demangle().

namespace argparse {

Context::Context(const Argument* argument, const char* value, ArgpState state)
    : has_value(bool(value)), argument(argument), state(state) {
  if (has_value)
    this->value.assign(value);
}

Names::Names(const char* name) {
  if (name[0] == '-') {
    InitOptions({name});
    return;
  }
  auto len = std::strlen(name);
  CHECK_USER(IsValidPositionalName(name, len),
             "Not a valid positional name: %s", name);
  is_option = false;
  std::string positional(name, len);
  meta_var = ToUpper(positional);
  long_names.push_back(std::move(positional));
}

void Names::InitOptions(std::initializer_list<const char*> names) {
  CHECK_USER(names.size(), "At least one name must be provided");
  is_option = true;

  for (auto name : names) {
    std::size_t len = std::strlen(name);
    CHECK_USER(IsValidOptionName(name, len), "Not a valid option name: %s",
               name);
    if (IsLongOptionName(name, len)) {
      // Strip leading '-' at most twice.
      for (int i = 0; *name == '-' && i < 2; ++i) {
        ++name;
        --len;
      }
      long_names.emplace_back(name, len);
    } else {
      short_names.push_back(name[1]);
    }
  }
  if (long_names.size())
    meta_var = ToUpper(long_names[0]);
  else
    meta_var = ToUpper({&short_names[0], 1});
}

// ArgumentImpl:
ArgumentImpl::ArgumentImpl(Argument::Delegate* delegate,
                           const Names& names,
                           int group)
    : delegate_(delegate), group_(group) {
  InitNames(names);
  InitKey(names.is_option);
  delegate_->OnArgumentCreated(this);
}

void ArgumentImpl::InitNames(Names names) {
  long_names_ = std::move(names.long_names);
  short_names_ = std::move(names.short_names);
  meta_var_ = std::move(names.meta_var);
}

void ArgumentImpl::InitKey(bool is_option) {
  if (!is_option) {
    key_ = kKeyForPositional;
    return;
  }
  key_ = short_names_.empty() ? delegate_->NextOptionKey() : short_names_[0];
}

void ArgumentImpl::CompileToArgpOptions(
    std::vector<argp_option>* options) const {
  argp_option opt{};
  opt.doc = doc();
  opt.group = group();
  opt.name = name();
  if (!is_option()) {
    // positional means none-zero in only doc and name, and flag should be
    // OPTION_DOC.
    opt.flags = OPTION_DOC;
    return options->push_back(opt);
  }
  opt.arg = arg();
  opt.key = key();
  options->push_back(opt);
  // TODO: handle alias correctly. Add all aliases.
  for (auto first = long_names_.begin() + 1, last = long_names_.end();
       first != last; ++first) {
    argp_option opt_alias;
    std::memcpy(&opt_alias, &opt, sizeof(argp_option));
    opt.name = first->c_str();
    opt.flags = OPTION_ALIAS;
    options->push_back(opt_alias);
  }
}

bool ArgumentImpl::CompareArguments(const ArgumentImpl* a,
                                    const ArgumentImpl* b) {
  // options go before positionals.
  if (a->is_option() != b->is_option())
    return int(a->is_option()) > int(b->is_option());

  // positional compares on their names.
  if (!a->is_option() && !b->is_option()) {
    DCHECK(a->name() && b->name());
    return std::strcmp(a->name(), b->name()) < 0;
  }

  // required option goes first.
  if (a->is_required() != b->is_required())
    return int(a->is_required()) > int(b->is_required());

  // short-only option goes before the rest.
  if (bool(a->name()) != bool(b->name()))
    return bool(a->name()) < bool(b->name());

  // a and b are both short-only option.
  if (!a->name() && !b->name())
    return a->key() < b->key();

  // a and b are both long option.
  DCHECK(a->name() && b->name());
  return std::strcmp(a->name(), b->name()) < 0;
}

void ArgumentImpl::FormatArgsDoc(std::ostream& os) const {
  if (!is_option()) {
    os << meta_var();
    return;
  }
  os << '[';
  std::size_t i = 0;
  const auto size = long_names_.size() + short_names_.size();

  for (; i < size; ++i) {
    if (i < long_names_.size()) {
      os << "--" << long_names_[i];
    } else {
      os << '-' << short_names_[i - long_names_.size()];
    }
    if (i < size - 1)
      os << '|';
  }

  if (!is_required())
    os << '[';
  os << '=' << meta_var();
  if (!is_required())
    os << ']';
  os << ']';
}

static bool TypesToOpsKind(Types in, OpsKind* out) {
  static const std::map<Types, OpsKind> kTypesToOpsKind{
      {Types::kOpen, OpsKind::kOpen},
      {Types::kParse, OpsKind::kParse},
  };
  auto iter = kTypesToOpsKind.find(in);
  if (iter == kTypesToOpsKind.end())
    return false;
  *out = iter->second;
  return true;
}

static bool ActionsToOpsKind(Actions in, OpsKind* out) {
  static const std::map<Actions, OpsKind> kActionsToOpsKind{
      {Actions::kAppend, OpsKind::kAppend},
      {Actions::kAppendConst, OpsKind::kAppendConst},
      {Actions::kStore, OpsKind::kStore},
      {Actions::kStoreConst, OpsKind::kStoreConst},
  };
  auto iter = kActionsToOpsKind.find(in);
  if (iter == kActionsToOpsKind.end())
    return false;
  *out = iter->second;
  return true;
}

static bool ActionNeedsConstValue(Actions in) {
  return in == Actions::kStoreConst || in == Actions::kAppendConst;
}

static bool ActionNeedsDest(Actions in) {
  // These actions don't need a dest.
  return !(in == Actions::kPrintHelp || in == Actions::kPrintUsage ||
           in == Actions::kNoAction);
}

static bool ActionNeedsTypeCallback(Actions in) {
}

const char* TypesToString(Types in) {
  static const std::map<Types, std::string> kTypesToString{
      {Types::kOpen, "Open"},
      {Types::kParse, "Parse"},
      {Types::kCustom, "Custom"},
      {Types::kNothing, "Nothing"},
  };
  DCHECK(kTypesToString.find(in) != kTypesToString.end());
  return kTypesToString.find(in)->second.c_str();
}

const char* ActionsToString(Actions in) {}

void ArgumentImpl::InitActionCallback() {
  const bool need_dest = ActionNeedsDest(action_code_);
  CHECK_USER(dest_ptr_ || !need_dest,
             "Action %s needs a dest, which is not provided",
             ActionsToString(action_code_));

  if (!need_dest) {
    // This action don't need a dest (provided or not).
    return;
  }

  DCHECK(dest_ptr_ && ops_factory_);

  if (action_code_ == Actions::kCustom) {
    DCHECK(custom_action_);
    return;
  }

  if (action_code_ == Actions::kNoAction) {
    // If there is a dest, but no explicit action given, default is store.
    action_code_ = Actions::kStore;
  }

  // Ops of action is always created from dest's ops-factory.
  action_ops_ = ops_factory_->Create();

  // See if Ops supports this action.
  OpsKind ops_kind;
  // Custom action don't have an OpsKind so it can't be checked.
  bool ok = ActionsToOpsKind(action_code_, &ops_kind);
  DCHECK(ok);
  CHECK_USER(action_ops_->IsSupported(ops_kind),
             "Action %s is not supported by type %s",
             ActionsToString(action_code_), action_ops_->TypeName());

  // See if const value is provided as needed.
  if (ActionNeedsConstValue(action_code_)) {
    CHECK_USER(const_value_.get(),
               "Action %s needs a const value, which is not provided",
               ActionsToString(action_code_));
  }
}

void ArgumentImpl::InitTypeCallback() {
  if (!ActionNeedsTypeCallback(action_code_)) {
    // Some action, like print usage, store const.., don't need a type..
    type_code_ = Types::kNothing;
    type_ops_.reset();
    return;
  }

  if (type_code_ == Types::kCustom) {
    DCHECK(custom_type_);
    type_ops_.reset();
    return;
  }

  // Create type_ops_.
  if (!type_ops_) {
    type_ops_ = action_code_ == Actions::kAppend
                    ? ops_factory_->CreateValueTypeOps()
                    : ops_factory_->Create();
  }

  // Figure out type_code_.
  if (type_code_ == Types::kNothing) {
    // The action needs a type, but is not explicityl set, so default is parse.
    type_code_ = Types::kParse;
  }

  // See if type_code_ is supported.
  OpsKind ops;
  bool ok = TypesToOpsKind(type_code_, &ops);
  DCHECK(ok);
  CHECK_USER(type_ops_->IsSupported(ops), "Type %s is not supported by type %s",
             TypesToString(type_code_), type_ops_->TypeName());

  if (type_code_ == Types::kOpen) {
    DCHECK(mode_ != kModeNoMode);
  }
}

void ArgumentImpl::PerformTypeCheck() const {
  // if (!dest_ptr_) return;
  // if (action_code_ == Actions::kStore && !custom_type_) {
  //   CHECK_USER(action_ops_->Type() == type_ops_->Type());
  // }
}

void ArgumentImpl::Finalize() {
  InitActionCallback();
  InitTypeCallback();
  PerformTypeCheck();
}

void ArgumentHolderImpl::CompileToArgpOptions(
    std::vector<argp_option>* options) {
  if (arguments_.empty())
    return options->push_back({});

  const unsigned option_size =
      arguments_.size() + groups_.size() + kAverageAliasCount + 1;
  options->reserve(option_size);

  for (const Group& group : groups_) {
    group.CompileToArgpOption(options);
  }

  // TODO: if there is not pos/opt at all but there are two groups, will argp
  // still print these empty groups?
  for (Argument& arg : arguments_) {
    arg.Finalize();
    arg.CompileToArgpOptions(options);
  }
  // Only when at least one opt/pos presents should we generate their groups.
  options->push_back({});

  // PrintArgpOptionArray(*options);
}

void ArgumentHolderImpl::GenerateArgsDoc(std::string* args_doc) {
  std::vector<Argument*> args(arguments_.size());
  std::transform(arguments_.begin(), arguments_.end(), args.begin(),
                 [](ArgumentImpl& arg) { return &arg; });
  std::sort(args.begin(), args.end(),
            [](Argument* a, Argument* b) { return a->Before(b); });

  // join the dump of each arg with a space.
  std::ostringstream os;
  for (std::size_t i = 0, size = args.size(); i < size; ++i) {
    args[i]->FormatArgsDoc(os);
    if (i != size - 1)
      os << ' ';
  }
  args_doc->assign(os.str());
}

std::unique_ptr<ArgpParser> ArgpParser::Create(Delegate* delegate) {
  return std::make_unique<ArgpParserImpl>(delegate);
}

ArgpParserImpl::ArgpParserImpl(ArgpParser::Delegate* delegate)
    : delegate_(delegate) {
  argp_.parser = &ArgpParserImpl::ArgpParserCallbackImpl;
  positional_count_ = delegate_->PositionalArgumentCount();
  delegate_->CompileToArgpOptions(&argp_options_);
  argp_.options = argp_options_.data();
  delegate_->GenerateArgsDoc(&args_doc_);
  argp_.args_doc = args_doc_.c_str();
}

void ArgpParserImpl::Init(const Options& options) {
  if (options.program_version)
    argp_program_version = options.program_version;
  if (options.program_version_callback) {
    argp_program_version_hook = options.program_version_callback;
  }
  if (options.email)
    argp_program_bug_address = options.email;
  if (options.help_filter) {
    argp_.help_filter = &ArgpHelpFilterCallbackImpl;
    help_filter_ = options.help_filter;
  }

  // TODO: may check domain?
  set_argp_domain(options.domain);
  AddFlags(options.flags);

  // Generate the program doc.
  if (options.description) {
    program_doc_ = options.description;
  }
  if (options.after_doc) {
    program_doc_.append({'\v'});
    program_doc_.append(options.after_doc);
  }

  if (!program_doc_.empty())
    set_doc(program_doc_.c_str());
}

void ArgpParserImpl::ParseArgs(ArgArray args) {
  argp_parse(&argp_, args.argc(), args.argv(), parser_flags_, nullptr, this);
}

bool ArgpParserImpl::ParseKnownArgs(ArgArray args,
                                    std::vector<std::string>* rest) {
  int arg_index;
  error_t error = argp_parse(&argp_, args.argc(), args.argv(), parser_flags_,
                             &arg_index, this);
  if (!error)
    return true;
  for (int i = arg_index; i < args.argc(); ++i) {
    rest->emplace_back(args[i]);
  }
  return false;
}

error_t ArgpParserImpl::DoParse(int key, char* arg, ArgpState state) {
  // Positional argument.
  if (key == ARGP_KEY_ARG) {
    const int arg_num = state->arg_num;
    if (Argument* argument = delegate_->FindPositionalArgument(arg_num)) {
      RunCallback(argument, arg, state);
      return 0;
    }
    // Too many arguments.
    if (state->arg_num >= positional_count())
      state.ErrorF("Too many positional arguments. Expected %d, got %d",
                   (int)positional_count(), (int)state->arg_num);
    return ARGP_ERR_UNKNOWN;
  }

  // Next most frequent handling is options.
  if ((key & kSpecialKeyMask) == 0) {
    // This isn't a special key, but rather an option.
    Argument* argument = delegate_->FindOptionalArgument(key);
    if (!argument)
      return ARGP_ERR_UNKNOWN;
    RunCallback(argument, arg, state);
    return 0;
  }

  // No more commandline args, do some post-processing.
  if (key == ARGP_KEY_END) {
    // No enough args.
    if (state->arg_num < positional_count())
      state.ErrorF("Not enough positional arguments. Expected %d, got %d",
                   (int)positional_count(), (int)state->arg_num);
  }

  // Remaining args (not parsed). Collect them or turn it into an error.
  if (key == ARGP_KEY_ARGS) {
    return 0;
  }

  return 0;
}

char* ArgpParserImpl::ArgpHelpFilterCallbackImpl(int key,
                                                 const char* text,
                                                 void* input) {
  if (!input || !text)
    return (char*)text;
  auto* self = reinterpret_cast<ArgpParserImpl*>(input);
  DCHECK2(self->help_filter_,
          "should only be called if user install help filter!");
  auto* arg = self->delegate_->FindOptionalArgument(key);
  DCHECK2(arg, "argp calls us with unknown key!");

  std::string repl(text);
  HelpFilterResult result = std::invoke(self->help_filter_, *arg, &repl);
  switch (result) {
    case HelpFilterResult::kKeep:
      return const_cast<char*>(text);
    case HelpFilterResult::kDrop:
      return nullptr;
    case HelpFilterResult::kReplace: {
      char* s = (char*)std::malloc(1 + repl.size());
      return std::strcpy(s, repl.c_str());
    }
  }
}

Argument* ArgumentHolderImpl::AddArgumentToGroup(Names names, int group) {
  // First check if this arg will conflict with existing ones.
  CHECK_USER(CheckNamesConflict(names), "Names conflict with existing names!");
  DCHECK2(group <= groups_.size(), "No such group");
  GroupFromID(group)->IncRef();
  Argument& arg = arguments_.emplace_back(this, names, group);
  SetDirty(true);
  return &arg;
}

Argument* ArgumentHolderImpl::AddArgument(Names names) {
  int group = names.is_option ? kOptionGroup : kPositionalGroup;
  return AddArgumentToGroup(std::move(names), group);
}

ArgumentGroup ArgumentHolderImpl::AddArgumentGroup(const char* header) {
  int group = AddGroup(header);
  return ArgumentGroup(this, group);
}

void ArgumentHolderImpl::OnArgumentCreated(Argument* arg) {
  if (arg->IsOption()) {
    bool inserted = optional_arguments_.emplace(arg->GetKey(), arg).second;
    DCHECK(inserted);
  } else {
    positional_arguments_.push_back(arg);
  }
}

ArgumentHolderImpl::Group::Group(int group, const char* header)
    : group_(group), header_(header) {
  DCHECK(group_ > 0);
  DCHECK(header_.size());
  if (header_.back() != ':')
    header_.push_back(':');
}

void ArgumentHolderImpl::Group::CompileToArgpOption(
    std::vector<argp_option>* options) const {
  if (!members_)
    return;
  argp_option opt{};
  opt.group = group_;
  opt.doc = header_.c_str();
  options->push_back(opt);
}

ArgumentHolderImpl::ArgumentHolderImpl() {
  AddGroup("optional arguments");
  AddGroup("positional arguments");
}

Argument* ArgumentHolderImpl::FindOptionalArgument(int key) {
  auto iter = optional_arguments_.find(key);
  return iter == optional_arguments_.end() ? nullptr : iter->second;
}

Argument* ArgumentHolderImpl::FindPositionalArgument(int index) {
  return (0 <= index && index < positional_arguments_.size())
             ? positional_arguments_[index]
             : nullptr;
}

ArgpParser* ArgumentHolderImpl::GetParser() {
  if (dirty()) {
    parser_ = CreateParser();
    SetDirty(false);
  }
  DCHECK(parser_);
  return parser_.get();
}

int ArgumentHolderImpl::AddGroup(const char* header) {
  int group = groups_.size() + 1;
  groups_.emplace_back(group, header);
  SetDirty(true);
  return group;
}

bool ArgumentHolderImpl::CheckNamesConflict(const Names& names) {
  for (auto&& long_name : names.long_names)
    if (!name_set_.insert(long_name).second)
      return false;
  // May not use multiple short names.
  for (char short_name : names.short_names)
    if (!name_set_.insert(std::string(&short_name, 1)).second)
      return false;
  return true;
}

void ArgumentParser::parse_args(int argc, const char** argv) {
  auto* parser = holder_->GetParser();
  parser->Init(user_options_.options);
  return parser->ParseArgs(ArgArray(argc, argv));
}

bool IsValidPositionalName(const char* name, std::size_t len) {
  if (!name || !len || !std::isalpha(name[0]))
    return false;
  for (++name, --len; len > 0; ++name, --len) {
    if (std::isalnum(*name) || *name == '-' || *name == '_')
      continue;  // allowed.
    return false;
  }
  return true;
}

bool IsValidOptionName(const char* name, std::size_t len) {
  if (!name || len < 2 || name[0] != '-')
    return false;
  if (len == 2)  // This rules out -?, -* -@ -= --
    return std::isalnum(name[1]);
  // check for long-ness.
  CHECK_USER(
      name[1] == '-',
      "Single-dash long option (i.e., -jar) is not supported. Please use "
      "GNU-style long option (double-dash)");

  for (name += 2; *name; ++name) {
    if (*name == '-' || *name == '_' || std::isalnum(*name))
      continue;
    return false;
  }
  return true;
}

Actions StringToActions(const std::string& str) {
  static const std::map<std::string, Actions> kStringToActions{
      {"store", Actions::kStore},
      {"store_const", Actions::kStoreConst},
      {"store_true", Actions::kStoreTrue},
      {"store_false", Actions::kStoreFalse},
      {"append", Actions::kAppend},
      {"append_const", Actions::kAppendConst},
      {"print_help", Actions::kPrintHelp},
      {"print_usage", Actions::kPrintUsage},
  };
  auto iter = kStringToActions.find(str);
  CHECK_USER(iter != kStringToActions.end(),
             "Unknown action string '%s' passed in", str.c_str());
  return iter->second;
}

void ArgumentImpl::RunActionCallback(std::unique_ptr<Any> data, Context* ctx) {
  auto* ops = action_ops_.get();
  DCHECK(ops);

  switch (action_code_) {
    case Actions::kNoAction:
      break;
    case Actions::kStore:
      ops->Store(dest(), std::move(data));
      break;
    case Actions::kStoreConst:
      ops->StoreConst(dest(), const_value());
      break;
    case Actions::kStoreTrue:
      ops->StoreConst(dest(), MakeAnyOnStack(true));
      break;
    case Actions::kStoreFalse:
      ops->StoreConst(dest(), MakeAnyOnStack(false));
      break;
    case Actions::kAppend:
      ops->Append(dest(), std::move(data));
      break;
    case Actions::kAppendConst:
      ops->AppendConst(dest(), const_value());
      break;
    case Actions::kPrintHelp:
      runner_delegate_->HandlePrintHelp(ctx);
      break;
    case Actions::kPrintUsage:
      runner_delegate_->HandlePrintUsage(ctx);
      break;
    case Actions::kCustom:
      DCHECK(custom_action_);
      custom_action_->Run(dest(), std::move(data));
      break;
  }
}

void ArgumentImpl::RunTypeCallback(const std::string& in, OpsResult* out) {
  auto* ops = type_ops_.get();
  DCHECK(ops);
  switch (type_code_) {
    case Types::kParse:
      ops->Parse(in, out);
      break;
    case Types::kOpen:
      ops->Open(in, mode_, out);
      break;
    case Types::kNothing:
      break;
    case Types::kCustom:
      DCHECK(custom_type_);
      custom_type_->Run(in, out);
      break;
  }
}

void ArgumentImpl::RunCallback(Context* ctx,
                               CallbackRunner::Delegate* delegate) {
  runner_delegate_ = delegate;
  OpsResult result;
  if (ctx->has_value)
    RunTypeCallback(ctx->value, &result);
  if (result.has_error) {
    ctx->errmsg = std::move(result.errmsg);
    runner_delegate_->HandleCallbackError(ctx);
    return;
  }
  RunActionCallback(std::move(result.value), ctx);
}

std::string ModeToChars(Mode mode) {
  std::string m;
  if (mode & kModeRead)
    m.append("r");
  if (mode & kModeWrite)
    m.append("w");
  if (mode & kModeAppend)
    m.append("a");
  if (mode & kModeBinary)
    m.append("b");
  return m;
}

void CFileOpenTraits::Run(const std::string& in,
                          Mode mode,
                          Result<FILE*>* out) {
  auto mode_str = ModeToChars(mode);
  auto* file = std::fopen(in.c_str(), mode_str.c_str());
  if (file)
    return out->set_value(file);
  if (int e = errno) {
    errno = 0;
    return out->set_error(std::strerror(e));
  }
  out->set_error(kDefaultOpenFailureMsg);
}

std::ios_base::openmode ModeToStreamMode(Mode m) {
  std::ios_base::openmode out;
  if (m & kModeRead)
    out |= std::ios_base::in;
  if (m & kModeWrite)
    out |= std::ios_base::out;
  if (m & kModeAppend)
    out |= std::ios_base::app;
  if (m & kModeTruncate)
    out |= std::ios_base::trunc;
  if (m & kModeBinary)
    out |= std::ios_base::binary;
  return out;
}

Mode StreamModeToMode(std::ios_base::openmode stream_mode) {
  int m = kModeNoMode;
  if (stream_mode & std::ios_base::in)
    m |= kModeRead;
  if (stream_mode & std::ios_base::out)
    m |= kModeWrite;
  if (stream_mode & std::ios_base::app)
    m |= kModeAppend;
  if (stream_mode & std::ios_base::trunc)
    m |= kModeTruncate;
  if (stream_mode & std::ios_base::binary)
    m |= kModeBinary;
  return static_cast<Mode>(m);
}

Mode CharsToMode(const char* str) {
  DCHECK(str);
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
  return static_cast<Mode>(m);
}

const char* OpsToString(OpsKind ops) {
  static const std::map<OpsKind, std::string> kOpsToStrings{
      {OpsKind::kStore, "Store"},   {OpsKind::kStoreConst, "StoreConst"},
      {OpsKind::kAppend, "Append"}, {OpsKind::kAppendConst, "AppendConst"},
      {OpsKind::kParse, "Parse"},   {OpsKind::kOpen, "Open"},
  };
  auto iter = kOpsToStrings.find(ops);
  DCHECK(iter != kOpsToStrings.end());
  return iter->second.c_str();
}

std::string Demangle(const char* mangled_name) {
  std::size_t length;
  int status;
  const char* realname =
      abi::__cxa_demangle(mangled_name, nullptr, &length, &status);

  if (status) {
    static constexpr const char kDemangleFailedSub[] =
        "<error-type(demangle failed)>";
    return kDemangleFailedSub;
  }

  DCHECK(realname);
  std::string result(realname, length);
  std::free((void*)realname);
  return result;
}

const char* TypeNameImpl(const std::type_info& type,
                         std::string (*callback)()) {
  // Some typenames like std::string, is very ugly since it is a typedef...
  // Provide a chance to let user change their typenames..
  static std::map<std::type_index, std::string> g_typenames;

  auto iter = g_typenames.find(type);
  if (iter == g_typenames.end()) {
    g_typenames.emplace(type, callback());
  }
  return g_typenames[type].c_str();
}

void CheckUserError(bool cond, SourceLocation loc, const char* fmt, ...) {
  if (cond)
    return;
  std::fprintf(stderr, "Error at %s:%d: ", loc.filename, loc.line);

  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(stderr, fmt, ap);
  va_end(ap);

  std::fprintf(
      stderr,
      "\n\nPlease check your code and read the documents of argparse.\n\n");
  std::abort();
}

}  // namespace argparse
