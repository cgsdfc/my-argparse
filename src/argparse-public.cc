#include "argparse/argparse.h"

#include <cxxabi.h>  // demangle().
#include <cstdlib>   // malloc()
#include <cstring>   // strlen()

namespace argparse {

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

// static bool ActionNeedsConstValue(ActionKind in) {
//   return in == ActionKind::kStoreConst || in == ActionKind::kAppendConst;
// }

// static bool ActionNeedsDest(ActionKind in) {
//   // These actions don't need a dest.
//   return !(in == ActionKind::kPrintHelp || in == ActionKind::kPrintUsage ||
//            in == ActionKind::kNoAction);
// }

// static bool ActionNeedsTypeCallback(ActionKind in) {
//   return in == ActionKind::kAppend || in == ActionKind::kStore ||
//          in == ActionKind::kCustom;
// }

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

static ParserFactory::Callback g_parser_factory_callback;

void ParserFactory::RegisterCallback(Callback callback) {
  if (!g_parser_factory_callback) {
    g_parser_factory_callback = callback;
  }
}

}  // namespace argparse
