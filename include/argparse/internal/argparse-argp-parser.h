#pragma once

#include <argp.h>

namespace argparse {

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

}  // namespace argparse