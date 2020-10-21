#pragma once

// Holds commented-out code for the time being.

// class Argument;
// class ArgumentGroup;
// class AddArgumentHelper;

// class CallbackRunner {
//  public:
//   // Communicate with outside when callback is fired.
//   class Delegate {
//    public:
//     virtual ~Delegate() {}
//     virtual bool GetValue(std::string* out) = 0;
//     virtual void OnCallbackError(const std::string& errmsg) = 0;
//     virtual void OnPrintUsage() = 0;
//     virtual void OnPrintHelp() = 0;
//   };
//   // Before the callback is run, allow default value to be set.
//   virtual void InitCallback() {}
//   virtual void RunCallback(std::unique_ptr<Delegate> delegate) = 0;
//   virtual ~CallbackRunner() {}
// };

// // Return value of help filter function.
// enum class HelpFilterResult {
//   kKeep,
//   kDrop,
//   kReplace,
// };

// using HelpFilterCallback =
//     std::function<HelpFilterResult(const Argument&, std::string* text)>;

// // XXX: this depends on argp and is not general.
// // In fact, people only need to pass in a std::string.
// using ProgramVersionCallback = void (*)(std::FILE*, argp_state*);

// TEST(TypeName, WorksForTypicalTypes) {
//   EXPECT_TRUE(TypeName<int>() == "int");
//   EXPECT_TRUE(TypeName<double>() == "double");
//   EXPECT_TRUE(TypeName<char>() == "char");
// }


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
//       absl::make_unique<Context>(arg, value, state));
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

static ParserFactory::Callback g_parser_factory_callback;

void ParserFactory::RegisterCallback(Callback callback) {
  if (!g_parser_factory_callback) {
    g_parser_factory_callback = callback;
  }
}

// TODO: remove this..
using ::argp;
using ::argp_error;
using ::argp_failure;
using ::argp_help;
using ::argp_parse;
using ::argp_parser_t;
using ::argp_program_bug_address;
using ::argp_program_version;
using ::argp_program_version_hook;
using ::argp_state;
using ::argp_state_help;
using ::argp_usage;
using ::error_t;
using ::program_invocation_name;
using ::program_invocation_short_name;

// All the data elements needs by argp.
// Created by Compiler and kept alive by Parser.
class GNUArgpContext {
 public:
  using ParserCallback = argp_parser_t;
  virtual ~GNUArgpContext() {}
  virtual void SetParserCallback(ParserCallback cb) = 0;
  virtual int GetParseFlags() = 0;
  virtual Argument* FindOption(int key) = 0;
  virtual Argument* FindPositional(int pos) = 0;
  virtual argp* GetArgpStruct() = 0;
};

// Fast lookup of arguments based on their key or position.
// This is generated by Compiler mostly by looking at the Arguments and Options.
class ArgpIndexesInfo {
 public:
  Argument* FindOption(int key) const;
  Argument* FindPositional(int pos) const;

  std::map<int, Argument*> optionals;
  std::vector<Argument*> positionals;
};

class GNUArgpCompiler {
 public:
  virtual ~GNUArgpCompiler() {}
  virtual std::unique_ptr<GNUArgpContext> Compile(
      std::unique_ptr<ParserFactory::Delegate> delegate) = 0;
};

// Compile Arguments to argp data and various things needed by the parser.
class ArgpCompiler {
 public:
  ArgpCompiler(ArgumentHolder* holder) : holder_(holder) { Initialize(); }

  void CompileOptions(std::vector<argp_option>* out);
  void CompileUsage(std::string* out);
  void CompileArgumentIndexes(ArgpIndexesInfo* out);

 private:
  void Initialize();
  void CompileGroup(ArgumentGroup* group, std::vector<argp_option>* out);
  void CompileArgument(Argument* arg, std::vector<argp_option>* out);
  void CompileUsageFor(Argument* arg, std::ostream& os);
  int FindGroup(ArgumentGroup* g) { return group_to_id_[g]; }
  int FindArgument(Argument* a) { return argument_to_id_[a]; }
  void InitGroup(ArgumentGroup* group);
  void InitArgument(Argument* arg);

  static constexpr unsigned kFirstArgumentKey = 128;

  ArgumentHolder* holder_;
  int next_arg_key_ = kFirstArgumentKey;
  int next_group_id_ = 1;
  HelpFormatPolicy policy_;
  std::map<Argument*, int> argument_to_id_;
  std::map<ArgumentGroup*, int> group_to_id_;
};

// This class focuses on parsing (efficiently).. Any other things are handled by
// Compiler..
class GNUArgpParser : public Parser {
 public:
  explicit GNUArgpParser(std::unique_ptr<GNUArgpContext> context)
      : context_(std::move(context)) {
    context_->SetParserCallback(&ParserCallbackImpl);
  }

  bool ParseKnownArgs(ArgArray args, std::vector<std::string>* rest) override {
    int flags = context_->GetParseFlags();
    if (rest)
      flags |= ARGP_NO_EXIT;
    int arg_index = 0;
    auto err = argp_parse(context_->GetArgpStruct(), args.argc(), args.argv(),
                          flags, &arg_index, this);
    if (rest) {
      for (int i = arg_index; i < args.argc(); ++i)
        rest->push_back(args[i]);
      return !err;
    }
    // Should not get here...
    return true;
  }

 private:
  // This is an internal class to communicate data/state between user's
  // callback.
  class Context;
  void RunCallback(Argument* arg, char* value, argp_state* state);

  error_t DoParse(int key, char* arg, argp_state* state);

  static error_t ParserCallbackImpl(int key, char* arg, argp_state* state) {
    auto* self = reinterpret_cast<GNUArgpParser*>(state->input);
    return self->DoParse(key, arg, state);
  }

  static char* HelpFilterCallbackImpl(int key, const char* text, void* input);

  // unsigned positional_count() const { return positional_count_; }

  static constexpr unsigned kSpecialKeyMask = 0x1000000;

  // std::unique_ptr<ArgpIndexesInfo> index_info_;
  std::unique_ptr<GNUArgpContext> context_;
  // int parser_flags_ = 0;
  // unsigned positional_count_ = 0;
  // argp argp_ = {};
  // std::string program_doc_;
  // std::string args_doc_;
  // std::vector<argp_option> argp_options_;
  // HelpFilterCallback help_filter_;
};

class GNUArgpParser::Context : public CallbackRunner::Delegate {
 public:
  Context(const Argument* argument, const char* value, argp_state* state);

  void OnCallbackError(const std::string& errmsg) override {
    return argp_error(state_, "error parsing argument: %s", errmsg.c_str());
  }

  void OnPrintUsage() override { return argp_usage(state_); }
  void OnPrintHelp() override {
    return argp_state_help(state_, stderr, help_flags_);
  }

  bool GetValue(std::string* out) override {
    if (has_value_) {
      *out = value_;
      return true;
    }
    return false;
  }

 private:
  const bool has_value_;
  const Argument* arg_;
  argp_state* state_;
  std::string value_;
  int help_flags_ = 0;
};

// struct ArgpData {
//   int parser_flags = 0;
//   argp argp_info = {};
//   std::string program_doc;
//   std::string args_doc;
//   std::vector<argp_option> argp_options;
// };



namespace argparse {

GNUArgpParser::Context::Context(const Argument* argument,
                                const char* value,
                                argp_state* state)
    : has_value_(bool(value)), arg_(argument), state_(state) {
  if (has_value_)
    this->value_.assign(value);
}

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

void ArgpCompiler::CompileGroup(ArgumentGroup* group,
                                std::vector<argp_option>* out) {
  if (!group->GetArgumentCount())
    return;
  argp_option opt{};
  opt.group = group_to_id_[group];
  opt.doc = group->GetHeader().data();
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
  // for (auto first = info->long_names.begin() + 1, last =
  // info->long_names.end();
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

}  // namespace argparse