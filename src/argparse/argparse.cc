#include "argparse/argparse.h"

#include <cxxabi.h>  // demangle().
#include <cstdlib>   // malloc()
#include <cstring>   // strlen()

namespace argparse {

ArgpParserImpl::Context::Context(const Argument* argument,
                                 const char* value,
                                 ArgpState state)
    : has_value_(bool(value)), arg_(argument), state_(state) {
  if (has_value_)
    this->value_.assign(value);
}

NamesInfo::NamesInfo(std::string name) {
  is_option = false;
  meta_var = ToUpper(name);
  long_names.push_back(std::move(name));
}

NamesInfo::NamesInfo(std::vector<const char*> names) {
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

Names::Names(const char* name) {
  if (name[0] == '-') {
    info.reset(new NamesInfo(std::vector<const char*>{name}));
    return;
  }
  auto len = std::strlen(name);
  CHECK_USER(IsValidPositionalName(name, len),
             "Not a valid positional name: %s", name);
  info.reset(new NamesInfo(name));
}

Names::Names(std::initializer_list<const char*> names) {
  CHECK_USER(names.size(), "At least one name must be provided");
  info.reset(new NamesInfo(names));
}

// ArgumentImpl:
ArgumentImpl::ArgumentImpl(std::unique_ptr<NamesInfo> names, ArgumentGroup* group)
    : names_info_(std::move(names)), group_(group) {
  DCHECK(names_info_);
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
  // if (!a->name() && !b->name())
  //   return a->key() < b->key();

  // a and b are both long option.
  DCHECK(a->name() && b->name());
  return std::strcmp(a->name(), b->name()) < 0;
}

static OpsKind TypesToOpsKind(Types in) {
  switch (in) {
    case Types::kOpen:
      return OpsKind::kOpen;
    case Types::kParse:
      return OpsKind::kParse;
    default:
      DCHECK2(false, "No corresponding OpsKind");
  }
}

static OpsKind ActionsToOpsKind(Actions in) {
  switch (in) {
    case Actions::kAppend:
      return OpsKind::kAppend;
    case Actions::kAppendConst:
      return OpsKind::kAppendConst;
    case Actions::kStore:
      return OpsKind::kStore;
    case Actions::kStoreConst:
      return OpsKind::kStoreConst;
    default:
      DCHECK2(false, "No corresponding OpsKind");
  }
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
  return in == Actions::kAppend || in == Actions::kStore ||
         in == Actions::kCustom;
}

const char* TypesToString(Types in) {
  switch (in) {
    case Types::kOpen:
      return "Open";
    case Types::kParse:
      return "Parse";
    case Types::kCustom:
      return "Custom";
    case Types::kNothing:
      return "Nothing";
  }
}

const char* ActionsToString(Actions in) {
  switch (in) {
    case Actions::kAppend:
      return "Append";
    case Actions::kAppendConst:
      return "AppendConst";
    case Actions::kCustom:
      return "Custom";
    case Actions::kNoAction:
      return "NoAction";
    case Actions::kPrintHelp:
      return "PrintHelp";
    case Actions::kPrintUsage:
      return "PrintUsage";
    case Actions::kStore:
      return "Store";
    case Actions::kStoreConst:
      return "StoreConst";
    case Actions::kStoreFalse:
      return "StoreFalse";
    case Actions::kStoreTrue:
      return "StoreTrue";
  }
}

void ArgumentImpl::CallbackInfo::InitAction() {
  if (!action_info_) {
    action_info_ = std::make_unique<ActionInfo>();
  }

  if (action_info_->action_code == Actions::kCustom) {
    DCHECK(action_info_->callback);
    return;
  }

  if (action_info_->action_code == Actions::kNoAction) {
    // If there is a dest, but no explicit action given, default is store.
    action_info_->action_code = Actions::kStore;
  }

  auto action_code = action_info_->action_code;
  const bool need_dest = ActionNeedsDest(action_code);
  CHECK_USER(dest_info_ || !need_dest,
             "Action %s needs a dest, which is not provided",
             ActionsToString(action_code));

  if (!need_dest) {
    // This action don't need a dest (provided or not).
    dest_info_.reset();
    return;
  }

  // Ops of action is always created from dest's ops-factory.
  DCHECK(dest_info_ && dest_info_->ops_factory);
  DCHECK(!action_info_->ops);
  action_info_->ops = dest_info_->ops_factory->Create();

  // See if Ops supports this action.
  auto* action_ops = action_info_->ops.get();
  auto ops_kind = ActionsToOpsKind(action_code);
  CHECK_USER(action_ops->IsSupported(ops_kind),
             "Action %s is not supported by type %s",
             ActionsToString(action_code), action_ops->GetTypeName());

  // See if const value is provided as needed.
  if (ActionNeedsConstValue(action_code)) {
    CHECK_USER(const_value_.get(),
               "Action %s needs a const value, which is not provided",
               ActionsToString(action_code));
  }
}

void ArgumentImpl::CallbackInfo::InitType() {
  if (!type_info_) {
    type_info_ = std::make_unique<TypeInfo>();
  }

  if (type_info_->type_code == Types::kCustom) {
    DCHECK(type_info_->callback);
    type_info_->ops.reset();
    return;
  }

  if (!ActionNeedsTypeCallback(action_info_->action_code)) {
    // Some action, like print usage, store const.., don't need a type..
    type_info_->type_code = Types::kNothing;
    type_info_->ops.reset();
    return;
  }

  // Figure out type_code_.
  if (type_info_->type_code == Types::kNothing) {
    // The action needs a type, but is not explicitly set, so default is kParse.
    // So if your dest is a filetype, but type is not FileType, it will be an
    // error.
    type_info_->type_code = Types::kParse;
  }

  auto type_code = type_info_->type_code;
  // If type_code is open, mode shouldn't be no mode.
  DCHECK(type_code != Types::kOpen || type_info_->mode != kModeNoMode);

  // Create type_ops_.
  if (!type_info_->ops) {
    auto* ops_factory = dest_info_->ops_factory.get();
    type_info_->ops = action_info_->action_code == Actions::kAppend
                          ? ops_factory->CreateValueTypeOps()
                          : ops_factory->Create();
  }

  // See if type_code_ is supported.
  auto ops = TypesToOpsKind(type_code);
  auto* type_ops = type_info_->ops.get();
  CHECK_USER(type_ops->IsSupported(ops),
             "Type %s is not supported by type % s ", TypesToString(type_code),
             type_ops->GetTypeName());
}

void ArgumentImpl::CallbackInfo::InitDefaultValue() {
  switch (action_info_->action_code) {
    case Actions::kStoreFalse:
      default_value_ = MakeAny(true);
      const_value_ = MakeAny(false);
      break;
    case Actions::kStoreTrue:
      default_value_ = MakeAny(false);
      const_value_ = MakeAny(true);
      break;
    default:
      break;
  }
}

void ArgumentImpl::CallbackInfo::Initialize() {
  InitAction();
  InitType();
  InitDefaultValue();
  // TODO: Do some type checking..
  // if (dest_info_ && action_info_->ops) {
  //   CHECK_USER(
  //       dest_info_->ops->GetTypeInfo() == action_info_->ops->GetTypeInfo(),
  //       "Type of dest should match type of action");
  // }
}

void ArgumentImpl::Initialize() {
  DCHECK(callback_info_);
  callback_info_->Initialize();
  // ProcessHelpFormatPolicy(policy);
}

void ArgumentImpl::CallbackInfo::FormatTypeHint(std::ostream& os) const {
  if (type_info_->ops) {
    os << "(type: " << type_info_->ops->GetTypeHint() << ")";
  }
}

void ArgumentImpl::CallbackInfo::FormatDefaultValue(std::ostream& os) const {
  // The type of default value is always the type of dest.
  // the type of dest is reflected by action_ops.
  if (default_value_ && dest_info_) {
    os << "(default: " << dest_info_->ops->FormatValue(*default_value_) << ")";
  }
}

void ArgumentImpl::ProcessHelpFormatPolicy(HelpFormatPolicy policy) {
  if (policy == HelpFormatPolicy::kDefault)
    return;
  std::ostringstream os;
  os << "  ";
  if (policy == HelpFormatPolicy::kTypeHint) {
    callback_info_->FormatTypeHint(os);
  } else if (policy == HelpFormatPolicy::kDefaultValueHint) {
    callback_info_->FormatDefaultValue(os);
  }
  help_doc_.append(os.str());
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

void ArgpParserImpl::RunCallback(Argument* arg, char* value, ArgpState state) {
  arg->GetCallbackRunner()->RunCallback(
      std::make_unique<Context>(arg, value, state));
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

Argument* ArgumentHolderImpl::AddArgumentToGroup(
    std::unique_ptr<NamesInfo> names,
    ArgumentGroup* group) {
  // First check if this arg will conflict with existing ones.
  CHECK_USER(CheckNamesConflict(*names), "Names conflict with existing names!");
  return &arguments_.emplace_back(std::move(names), group);
}

ArgumentGroup* ArgumentHolderImpl::AddArgumentGroup(
    const char* header) {
  auto* group = new GroupImpl(this, header);
  groups_.emplace_back(group);
  return group;
}

ArgumentHolderImpl::ArgumentHolderImpl() {
  AddArgumentGroup("optional arguments");
  AddArgumentGroup("positional arguments");
}

// Argument* ArgumentHolderImpl::FindOptionalArgument(int key) {
//   auto iter = optional_arguments_.find(key);
//   return iter == optional_arguments_.end() ? nullptr : iter->second;
// }

// Argument* ArgumentHolderImpl::FindPositionalArgument(int index) {
//   return (0 <= index && index < positional_arguments_.size())
//              ? positional_arguments_[index]
//              : nullptr;
// }

bool ArgumentHolderImpl::CheckNamesConflict(const NamesInfo& names) {
  for (auto&& long_name : names.long_names)
    if (!name_set_.insert(long_name).second)
      return false;
  // May not use multiple short names.
  for (char short_name : names.short_names)
    if (!name_set_.insert(std::string(&short_name, 1)).second)
      return false;
  return true;
}

std::unique_ptr<ArgumentController> ArgumentController::Create() {
  return nullptr;
}

// void ArgumentParser::parse_args(int argc, const char** argv) {
//   controller_->ParseKnownArgs(ArgArray(argc, argv), nullptr);
//   // auto* parser = holder_->GetParser();
//   // // parser->Init(user_options_.options);
//   // return parser->ParseArgs(ArgArray(argc, argv));
// }

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

void ArgumentImpl::CallbackInfo::RunAction(std::unique_ptr<Any> data,
                                           CallbackRunner::Delegate* delegate) {
  auto* ops = action_info_->ops.get();
  switch (action_info_->action_code) {
    case Actions::kNoAction:
      break;
    case Actions::kStore:
      ops->Store(dest_ptr(), std::move(data));
      break;
    case Actions::kStoreConst:
    case Actions::kStoreTrue:
    case Actions::kStoreFalse:
      ops->StoreConst(dest_ptr(), const_value());
      break;
    case Actions::kAppend:
      ops->Append(dest_ptr(), std::move(data));
      break;
    case Actions::kAppendConst:
      ops->AppendConst(dest_ptr(), const_value());
      break;
    case Actions::kPrintHelp:
      delegate->OnPrintHelp();
      break;
    case Actions::kPrintUsage:
      delegate->OnPrintUsage();
      break;
    case Actions::kCustom:
      DCHECK(action_info_->callback);
      action_info_->callback->Run(dest_ptr(), std::move(data));
      break;
  }
}

CallbackRunner* ArgumentImpl::GetCallbackRunner() {
  DCHECK(callback_info_);
  return callback_info_.get();
}

void ArgumentImpl::CallbackInfo::RunType(const std::string& in,
                                         OpsResult* out) {
  auto* ops = type_info_->ops.get();
  switch (type_info_->type_code) {
    case Types::kNothing:
      break;
    case Types::kParse:
      ops->Parse(in, out);
      break;
    case Types::kOpen:
      DCHECK(type_info_->mode != kModeNoMode);
      ops->Open(in, type_info_->mode, out);
      break;
    case Types::kCustom:
      DCHECK(type_info_->callback);
      type_info_->callback->Run(in, out);
      break;
  }
}

void ArgumentImpl::CallbackInfo::RunCallback(
    std::unique_ptr<CallbackRunner::Delegate> delegate) {
  std::string in;
  OpsResult result;
  if (delegate->GetValue(&in))
    RunType(in, &result);
  if (result.has_error) {
    delegate->OnCallbackError(result.errmsg);
    return;
  }
  RunAction(std::move(result.value), delegate.get());
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

  DCHECK(realname);
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

void ArgpCompiler::CompileGroup(ArgumentGroup* group,
                                std::vector<argp_option>* out) {
  if (!group->GetArgumentCount())
    return;
  argp_option opt{};
  opt.group = group_to_id_[group];
  opt.doc = group->GetHeader();
  out->push_back(opt);
}

void ArgpCompiler::CompileArgument(Argument* arg,
                                   std::vector<argp_option>* out) {
  argp_option opt{};
  opt.doc = arg->GetDoc();
  opt.group = FindGroup(arg->GetGroup());
  // opt.name = name();

  if (!arg->IsOption()) {
    // positional means none-zero in only doc and name, and flag should be
    // OPTION_DOC.
    opt.flags = OPTION_DOC;
    return out->push_back(opt);
  }

  // opt.arg = arg();
  opt.key = FindArgument(arg);
  out->push_back(opt);

  // TODO: handle alias correctly. Add all aliases.
  auto* info = arg->GetNamesInfo();
  for (auto first = info->long_names.begin() + 1, last = info->long_names.end();
       first != last; ++first) {
    argp_option opt_alias;
    std::memcpy(&opt_alias, &opt, sizeof(argp_option));
    opt.name = first->c_str();
    opt.flags = OPTION_ALIAS;
    out->push_back(opt_alias);
  }
}

void ArgpCompiler::CompileOptions(std::vector<argp_option>* out) {
  holder_->ForEachGroup(
      [this, out](ArgumentGroup* g) { return CompileGroup(g, out); });
  holder_->ForEachArgument([this, out](Argument* arg) {
    return CompileArgument(arg, out);
  });
  out->push_back({});
}

void ArgpCompiler::InitArgument(Argument* arg) {
  const auto& short_names = arg->GetNamesInfo()->short_names;
  int key = short_names.empty() ? next_arg_key_++ : short_names[0];
  argument_to_id_[arg] = key;
}

void ArgpCompiler::InitGroup(ArgumentGroup* group) {
  // There is no need to allocate special ids for default groups.
  // 1. argp sorts groups in id order.
  // 2. groups are in their added order.
  // 3. Holder always adds default groups at first.
  // 4. So default group is always presented at first.
  group_to_id_[group] = next_group_id_++;
}

void ArgpCompiler::Initialize() {
  // Gen keys for args.
  holder_->ForEachArgument([this](Argument* arg) {
    return InitArgument(arg);
  });
  // Gen ids for groups.
  holder_->ForEachGroup([this](ArgumentGroup* group) {
    // TODO: pos/opt default group..
    return InitGroup(group);
  });
}

void ArgpCompiler::CompileUsageFor(Argument* arg, std::ostream& os) {
    if (!arg->IsOption()) {
      os << arg->GetNamesInfo()->meta_var;
      return;
    }
    os << '[';
    std::size_t i = 0;
    const auto& long_names = arg->GetNamesInfo()->long_names;
    const auto& short_names = arg->GetNamesInfo()->short_names;
    const auto size = long_names.size() + short_names.size();

    for (; i < size; ++i) {
      if (i < long_names.size()) {
        os << "--" << long_names[i];
      } else {
        os << '-' << short_names[i - long_names.size()];
      }
      if (i < size - 1)
        os << '|';
    }

    if (!arg->IsRequired())
      os << '[';
    os << '=' << arg->GetNamesInfo()->meta_var;
    if (!arg->IsRequired())
      os << ']';
    os << ']';
}

void ArgpCompiler::CompileUsage(std::string* out) {
  std::vector<Argument*> args;
  args.reserve(holder_->GetArgumentCount());
  holder_->ForEachArgument(
      [&args](Argument* arg) { return args.push_back(arg); });

  std::sort(args.begin(), args.end(),
            [](Argument* a, Argument* b) { return a->Before(b); });

  // join the dump of each arg with a space.
  std::ostringstream os;
  for (std::size_t i = 0, size = args.size(); i < size; ++i) {
    CompileUsageFor(args[i], os);
    if (i != size - 1)
      os << ' ';
  }

  *out = os.str();
}

void ArgpCompiler::CompileArgumentIndexes(ArgpIndexesInfo* out) {
  holder_->ForEachArgument([this, out](Argument* arg) {
    if (arg->IsOption()) {
      out->optionals.emplace(argument_to_id_[arg], arg);
    } else {
      out->positionals.push_back(arg);
    }
  });
}

}  // namespace argparse
