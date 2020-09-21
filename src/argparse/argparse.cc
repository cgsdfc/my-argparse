#include "argparse/argparse.h"

#include <cxxabi.h>  // demangle().
#include <cstdlib>   // malloc()
#include <cstring>   // strlen()

namespace argparse {

GNUArgpParser::Context::Context(const Argument* argument,
                                const char* value,
                                argp_state* state)
    : has_value_(bool(value)), arg_(argument), state_(state) {
  if (has_value_)
    this->value_.assign(value);
}

Names::Names(std::string name) {
  if (name[0] == '-') {
    // This is in fact an option.
    std::vector<std::string> names{std::move(name)};
    info = NamesInfo::CreateOptional(std::move(names));
    return;
  }
  ARGPARSE_CHECK_F(IsValidPositionalName(name),
                   "Not a valid positional name: %s", name.c_str());
  info = NamesInfo::CreatePositional(std::move(name));
}

Names::Names(std::initializer_list<std::string> names)
    : info(NamesInfo::CreateOptional(names)) {
  ARGPARSE_CHECK_F(names.size(), "At least one name must be provided");
}

// ArgumentImpl:
bool Argument::Less(Argument* a, Argument* b) {
  // options go before positionals.
  if (a->IsOption() != b->IsOption())
    return a->IsOption();

  // positional compares on their names.
  if (!a->IsOption() && !b->IsOption()) {
    ARGPARSE_DCHECK(a->GetName());
    ARGPARSE_DCHECK(b->GetName());
    return std::strcmp(a->GetName(), b->GetName()) < 0;
  }

  // required option goes first.
  if (a->IsRequired() != b->IsRequired())
    return a->IsRequired();

  // // short-only option (flag) goes before the rest.
  if (a->IsFlag() != b->IsFlag())
    return a->IsFlag();

  // a and b are both long options or both flags.
  ARGPARSE_DCHECK(a->GetName() && b->GetName());
  return std::strcmp(a->GetName(), b->GetName()) < 0;
}

static OpsKind TypesToOpsKind(TypeKind in) {
  switch (in) {
    case TypeKind::kOpen:
      return OpsKind::kOpen;
    case TypeKind::kParse:
      return OpsKind::kParse;
    default:
      ARGPARSE_DCHECK_F(false, "No corresponding OpsKind");
  }
}

static OpsKind ActionsToOpsKind(ActionKind in) {
  switch (in) {
    case ActionKind::kAppend:
      return OpsKind::kAppend;
    case ActionKind::kAppendConst:
      return OpsKind::kAppendConst;
    case ActionKind::kStore:
      return OpsKind::kStore;
    case ActionKind::kStoreConst:
      return OpsKind::kStoreConst;
    default:
      ARGPARSE_DCHECK_F(false, "No corresponding OpsKind");
  }
}


static bool ActionNeedsConstValue(ActionKind in) {
  return in == ActionKind::kStoreConst || in == ActionKind::kAppendConst;
}

static bool ActionNeedsDest(ActionKind in) {
  // These actions don't need a dest.
  return !(in == ActionKind::kPrintHelp || in == ActionKind::kPrintUsage ||
           in == ActionKind::kNoAction);
}

static bool ActionNeedsTypeCallback(ActionKind in) {
  return in == ActionKind::kAppend || in == ActionKind::kStore ||
         in == ActionKind::kCustom;
}

static const char* TypesToString(TypeKind in) {
  switch (in) {
    case TypeKind::kOpen:
      return "Open";
    case TypeKind::kParse:
      return "Parse";
    case TypeKind::kCustom:
      return "Custom";
    case TypeKind::kNothing:
      return "Nothing";
  }
}

static const char* ActionsToString(ActionKind in) {
  switch (in) {
    case ActionKind::kAppend:
      return "Append";
    case ActionKind::kAppendConst:
      return "AppendConst";
    case ActionKind::kCustom:
      return "Custom";
    case ActionKind::kNoAction:
      return "NoAction";
    case ActionKind::kPrintHelp:
      return "PrintHelp";
    case ActionKind::kPrintUsage:
      return "PrintUsage";
    case ActionKind::kStore:
      return "Store";
    case ActionKind::kStoreConst:
      return "StoreConst";
    case ActionKind::kStoreFalse:
      return "StoreFalse";
    case ActionKind::kStoreTrue:
      return "StoreTrue";
    case ActionKind::kCount:
      return "Count";
  }
}

// void ArgumentImpl::InitAction() {
  // if (!action_info_) {
  //   // action_info_ = std::make_unique<ActionInfoImpl>();
  // }

  // if (action_info_->action_code == ActionKind::kCustom) {
  //   // ARGPARSE_DCHECK(action_info_->callback);
  //   return;
  // }

  // if (action_info_->action_code == ActionKind::kNoAction) {
  //   // If there is a dest, but no explicit action given, default is store.
  //   action_info_->action_code = ActionKind::kStore;
  // }

  // auto action_code = action_info_->action_code;
  // const bool need_dest = ActionNeedsDest(action_code);
  // ARGPARSE_CHECK_F(dest_info_ || !need_dest,
  //                  "Action %s needs a dest, which is not provided",
  //                  ActionsToString(action_code));

  // if (!need_dest) {
  //   // This action don't need a dest (provided or not).
  //   dest_info_.reset();
  //   return;
  // }

  // // Ops of action is always created from dest's ops-factory.
  // // ARGPARSE_DCHECK(dest_info_ && dest_info_->ops_factory);
  // // ARGPARSE_DCHECK(!action_info_->ops);
  // // action_info_->ops = dest_info_->ops_factory->Create();

  // // See if Ops supports this action.
  // auto* action_ops = action_info_->ops.get();
  // auto ops_kind = ActionsToOpsKind(action_code);
  // ARGPARSE_CHECK_F(action_ops->IsSupported(ops_kind),
  //                  "Action %s is not supported by type %s",
  //                  ActionsToString(action_code), action_ops->GetTypeName());

  // // See if const value is provided as needed.
  // if (ActionNeedsConstValue(action_code)) {
  //   ARGPARSE_CHECK_F(const_value_.get(),
  //                    "Action %s needs a const value, which is not provided",
  //                    ActionsToString(action_code));
  // }
// }

// void ArgumentImpl::InitType() {
  // if (!type_info_) {
  //   type_info_ = std::make_unique<TypeInfoImpl>();
  // }

  // if (type_info_->type_code == TypeKind::kCustom) {
  //   ARGPARSE_DCHECK(type_info_->callback);
  //   type_info_->ops.reset();
  //   return;
  // }

  // if (!ActionNeedsTypeCallback(action_info_->action_code)) {
  //   // Some action, like print usage, store const.., don't need a type..
  //   type_info_->type_code = TypeKind::kNothing;
  //   type_info_->ops.reset();
  //   return;
  // }

  // // Figure out type_code_.
  // if (type_info_->type_code == TypeKind::kNothing) {
  //   // The action needs a type, but is not explicitly set, so default is kParse.
  //   // So if your dest is a filetype, but type is not FileType, it will be an
  //   // error.
  //   type_info_->type_code = TypeKind::kParse;
  // }

  // auto type_code = type_info_->type_code;
  // // If type_code is open, mode shouldn't be no mode.
  // ARGPARSE_DCHECK(type_code != TypeKind::kOpen ||
  //                 type_info_->mode != kModeNoMode);

  // // Create type_ops_.
  // if (!type_info_->ops) {
  //   // auto* ops_factory = dest_info_->ops_factory.get();
  //   // type_info_->ops = action_info_->action_code == ActionKind::kAppend
  //   //                       ? ops_factory->CreateValueTypeOps()
  //   //                       : ops_factory->Create();
  // }

  // // See if type_code_ is supported.
  // auto ops = TypesToOpsKind(type_code);
  // auto* type_ops = type_info_->ops.get();
  // ARGPARSE_CHECK_F(type_ops->IsSupported(ops),
  //                  "Type %s is not supported by type % s ",
  //                  TypesToString(type_code), type_ops->GetTypeName());
// }

// void ArgumentImpl::InitDefaultValue() {
  // switch (action_info_->action_code) {
  //   case ActionKind::kStoreFalse:
  //     default_value_ = MakeAny(true);
  //     const_value_ = MakeAny(false);
  //     break;
  //   case ActionKind::kStoreTrue:
  //     default_value_ = MakeAny(false);
  //     const_value_ = MakeAny(true);
  //     break;
  //   default:
  //     break;
  // }
// }

// void ArgumentImpl::Initialize() {
//   InitAction();
//   InitType();
//   InitDefaultValue();
// }

// bool ArgumentImpl::GetTypeHint(std::string* out) {
//   // The type of default value is always the type of dest.
//   if (default_value_ && dest_info_) {
//     // *out = dest_info_->ops->FormatValue(*default_value_);
//     return true;
//   }
//   return false;
// }

// bool ArgumentImpl::FormatDefaultValue(std::string* out) {
//   // if (type_info_->ops) {
//   //   *out = type_info_->ops->GetTypeHint();
//   //   return true;
//   // }
//   return false;
// }

// void ArgumentImpl::ProcessHelpFormatPolicy(HelpFormatPolicy policy) {
//   if (policy == HelpFormatPolicy::kDefault)
//     return;
//   std::ostringstream os;
//   os << "  ";
//   if (policy == HelpFormatPolicy::kTypeHint) {
//     callback_info_->FormatTypeHint(os);
//   } else if (policy == HelpFormatPolicy::kDefaultValueHint) {
//     callback_info_->FormatDefaultValue(os);
//   }
//   help_doc_.append(os.str());
// }

// std::unique_ptr<ArgpParser> ArgpParser::Create(Delegate* delegate) {
//   return std::make_unique<ArgpParserImpl>(delegate);
// }

// ArgpParserImpl::ArgpParserImpl(ArgpParser::Delegate* delegate)
//     : delegate_(delegate) {
//   argp_.parser = &ArgpParserImpl::ParserCallbackImpl;
//   positional_count_ = delegate_->PositionalArgumentCount();
//   delegate_->CompileToArgpOptions(&argp_options_);
//   argp_.options = argp_options_.data();
//   delegate_->GenerateArgsDoc(&args_doc_);
//   argp_.args_doc = args_doc_.c_str();
// }

// void ArgpParserImpl::RunCallback(Argument* arg, char* value, argp_state*
// state) {
//   arg->GetCallbackRunner()->RunCallback(
//       std::make_unique<Context>(arg, value, state));
// }

// void ArgpParserImpl::Init(const Options& options) {
//   if (options.program_version)
//     argp_program_version = options.program_version;
//   if (options.program_version_callback) {
//     argp_program_version_hook = options.program_version_callback;
//   }
//   if (options.email)
//     argp_program_bug_address = options.email;
//   if (options.help_filter) {
//     argp_.help_filter = &HelpFilterCallbackImpl;
//     help_filter_ = options.help_filter;
//   }

//   // TODO: may check domain?
//   set_argp_domain(options.domain);
//   AddFlags(options.flags);

//   // Generate the program doc.
//   if (options.description) {
//     program_doc_ = options.description;
//   }
//   if (options.after_doc) {
//     program_doc_.append({'\v'});
//     program_doc_.append(options.after_doc);
//   }

//   if (!program_doc_.empty())
//     set_doc(program_doc_.c_str());
// }

// void ArgpParserImpl::ParseArgs(ArgArray args) {
//   argp_parse(&argp_, args.argc(), args.argv(), parser_flags_, nullptr, this);
// }

// bool ArgpParserImpl::ParseKnownArgs(ArgArray args,
//                                     std::vector<std::string>* rest) {
//   int arg_index;
//   error_t error = argp_parse(&argp_, args.argc(), args.argv(), parser_flags_,
//                              &arg_index, this);
//   if (!error)
//     return true;
//   for (int i = arg_index; i < args.argc(); ++i) {
//     rest->emplace_back(args[i]);
//   }
//   return false;
// }

error_t GNUArgpParser::DoParse(int key, char* arg, argp_state* state) {
  // Positional argument.
  // if (key == ARGP_KEY_ARG) {
  //   const int arg_num = state->arg_num;
  //   if (Argument* argument = context_->FindPositionalArgument(arg_num)) {
  //     RunCallback(argument, arg, state);
  //     return 0;
  //   }
  //   // Too many arguments.
  //   if (state->arg_num >= positional_count())
  //     state.ErrorF("Too many positional arguments. Expected %d, got %d",
  //                  (int)positional_count(), (int)state->arg_num);
  //   return ARGP_ERR_UNKNOWN;
  // }

  // // Next most frequent handling is options.
  // if ((key & kSpecialKeyMask) == 0) {
  //   // This isn't a special key, but rather an option.
  //   Argument* argument = delegate_->FindOptionalArgument(key);
  //   if (!argument)
  //     return ARGP_ERR_UNKNOWN;
  //   RunCallback(argument, arg, state);
  //   return 0;
  // }

  // // No more commandline args, do some post-processing.
  // if (key == ARGP_KEY_END) {
  //   // No enough args.
  //   if (state->arg_num < positional_count())
  //     state.ErrorF("Not enough positional arguments. Expected %d, got %d",
  //                  (int)positional_count(), (int)state->arg_num);
  // }

  // // Remaining args (not parsed). Collect them or turn it into an error.
  // if (key == ARGP_KEY_ARGS) {
  //   return 0;
  // }

  // return 0;
}

// char* ArgpParserImpl::HelpFilterCallbackImpl(int key,
//                                                  const char* text,
//                                                  void* input) {
//   if (!input || !text)
//     return (char*)text;
//   auto* self = reinterpret_cast<ArgpParserImpl*>(input);
//   ARGPARSE_DCHECK_F(self->help_filter_,
//           "should only be called if user install help filter!");
//   auto* arg = self->delegate_->FindOptionalArgument(key);
//   ARGPARSE_DCHECK_F(arg, "argp calls us with unknown key!");

//   std::string repl(text);
//   HelpFilterResult result = std::invoke(self->help_filter_, *arg, &repl);
//   switch (result) {
//     case HelpFilterResult::kKeep:
//       return const_cast<char*>(text);
//     case HelpFilterResult::kDrop:
//       return nullptr;
//     case HelpFilterResult::kReplace: {
//       char* s = (char*)std::malloc(1 + repl.size());
//       return std::strcpy(s, repl.c_str());
//     }
//   }
// }

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

bool IsValidPositionalName(const std::string& name) {
  auto len = name.size();
  if (!len || !std::isalpha(name[0]))
    return false;

  return std::all_of(name.begin() + 1, name.end(), [](char c) {
    return std::isalnum(c) || c == '-' || c == '_';
  });
}

bool IsValidOptionName(const std::string& name) {
  auto len = name.size();
  if (len < 2 || name[0] != '-')
    return false;
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

ActionKind StringToActions(const std::string& str) {
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

// TODO: extract action-related logic into one class, say ActionHelper.
// void ArgumentImpl::RunAction(std::unique_ptr<Any> data,
//                              CallbackRunner::Delegate* delegate) {
// }

// CallbackRunner* ArgumentImpl::GetCallbackRunner() {
//   return this;
//   // ARGPARSE_DCHECK(callback_info_);
//   // return callback_info_.get();
// }

// void ArgumentImpl::RunType(const std::string& in, OpsResult* out) {
// }

// void ArgumentImpl::RunCallback(
//     std::unique_ptr<CallbackRunner::Delegate> delegate) {
//   std::string in;
//   OpsResult result;
//   if (delegate->GetValue(&in))
//     RunType(in, &result);
//   if (result.has_error) {
//     delegate->OnCallbackError(result.errmsg);
//     return;
//   }
//   RunAction(std::move(result.value), delegate.get());
// }

std::string ModeToChars(OpenMode mode) {
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
                          OpenMode mode,
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

std::ios_base::openmode ModeToStreamMode(OpenMode m) {
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

OpenMode StreamModeToMode(std::ios_base::openmode stream_mode) {
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
  // argp_option opt{};
  // opt.doc = arg->GetHelpDoc();
  // opt.group = FindGroup(arg->GetGroup());
  // // opt.name = name();

  // if (!arg->IsOption()) {
  //   // positional means none-zero in only doc and name, and flag should be
  //   // OPTION_DOC.
  //   opt.flags = OPTION_DOC;
  //   return out->push_back(opt);
  // }

  // // opt.arg = arg();
  // opt.key = FindArgument(arg);
  // out->push_back(opt);

  // // TODO: handle alias correctly. Add all aliases.
  // auto* info = arg->GetNamesInfo();
  // for (auto first = info->long_names.begin() + 1, last = info->long_names.end();
  //      first != last; ++first) {
  //   argp_option opt_alias;
  //   std::memcpy(&opt_alias, &opt, sizeof(argp_option));
  //   opt.name = first->c_str();
  //   opt.flags = OPTION_ALIAS;
  //   out->push_back(opt_alias);
  // }
}

void ArgpCompiler::CompileOptions(std::vector<argp_option>* out) {
  holder_->ForEachGroup(
      [this, out](ArgumentGroup* g) { return CompileGroup(g, out); });
  holder_->ForEachArgument(
      [this, out](Argument* arg) { return CompileArgument(arg, out); });
  out->push_back({});
}

void ArgpCompiler::InitArgument(Argument* arg) {
  // const auto& short_names = arg->GetNamesInfo()->short_names;
  // int key = short_names.empty() ? next_arg_key_++ : short_names[0];
  // argument_to_id_[arg] = key;
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
  holder_->ForEachArgument([this](Argument* arg) { return InitArgument(arg); });
  // Gen ids for groups.
  holder_->ForEachGroup([this](ArgumentGroup* group) {
    // TODO: pos/opt default group..
    return InitGroup(group);
  });
}

void ArgpCompiler::CompileUsageFor(Argument* arg, std::ostream& os) {
  // if (!arg->IsOption()) {
  //   os << arg->GetNamesInfo()->meta_var;
  //   return;
  // }
  // os << '[';
  // std::size_t i = 0;
  // const auto& long_names = arg->GetNamesInfo()->long_names;
  // const auto& short_names = arg->GetNamesInfo()->short_names;
  // const auto size = long_names.size() + short_names.size();

  // for (; i < size; ++i) {
  //   if (i < long_names.size()) {
  //     os << "--" << long_names[i];
  //   } else {
  //     os << '-' << short_names[i - long_names.size()];
  //   }
  //   if (i < size - 1)
  //     os << '|';
  // }

  // if (!arg->IsRequired())
  //   os << '[';
  // os << '=' << arg->GetNamesInfo()->meta_var;
  // if (!arg->IsRequired())
  //   os << ']';
  // os << ']';
}

void ArgpCompiler::CompileUsage(std::string* out) {
  std::vector<Argument*> args;
  holder_->SortArguments(&args);

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
  const char* GetHeader() override { return header_.c_str(); }

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

static ParserFactory::Callback g_parser_factory_callback;

void ParserFactory::RegisterCallback(Callback callback) {
  if (!g_parser_factory_callback) {
    g_parser_factory_callback = callback;
  }
}

std::unique_ptr<ArgumentHolder> ArgumentHolder::Create() {
  return std::make_unique<ArgumentHolderImpl>();
}
std::unique_ptr<SubCommandHolder> SubCommandHolder::Create() {
  return std::make_unique<SubCommandHolderImpl>();
}
std::unique_ptr<ArgumentController> ArgumentController::Create() {
  ARGPARSE_DCHECK(g_parser_factory_callback);
  return std::make_unique<ArgumentControllerImpl>(g_parser_factory_callback());
}

ArgumentControllerImpl::ArgumentControllerImpl(
    std::unique_ptr<ParserFactory> parser_factory)
    : parser_factory_(std::move(parser_factory)),
      main_holder_(ArgumentHolder::Create()),
      subcmd_holder_(SubCommandHolder::Create()) {
  main_holder_->SetListener(std::make_unique<ListenerImpl>(this));
  subcmd_holder_->SetListener(std::make_unique<ListenerImpl>(this));
}

static bool ActionNeedsBool(ActionKind in) {
  return in == ActionKind::kStoreFalse || in == ActionKind::kStoreTrue;
}

static bool ActionNeedsValueType(ActionKind in) {
  return in == ActionKind::kAppend || in == ActionKind::kAppendConst;
}

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

std::unique_ptr<ArgumentFactory> ArgumentFactory::Create() {
  return std::make_unique<ArgumentFactoryImpl>();
}

// SubCommandImpl::SubCommandImpl()
}  // namespace argparse
