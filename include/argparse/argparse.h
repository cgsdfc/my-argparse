// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/argparse-traits.h"
#include "argparse/internal/argparse-internal.h"

// Holds public things
namespace argparse {

class FileType {
 public:
  explicit FileType(const char* mode) : mode_(internal::CharsToMode(mode)) {}
  explicit FileType(std::ios_base::openmode mode)
      : mode_(internal::StreamModeToMode(mode)) {}
  OpenMode mode() const { return mode_; }

 private:
  OpenMode mode_;
};

// Public flags user can use. These are corresponding to the ARGP_XXX flags
// passed to argp_parse().
enum Flags {
  kNoFlags = 0,            // The default.
  // kNoHelp = ARGP_NO_HELP,  // Don't produce --help.
  // kLongOnly = ARGP_LONG_ONLY,
  // kNoExit = ARGP_NO_EXIT,
};

// Options to ArgumentParser constructor.
// TODO: rename to OptionsBuilder and typedef.
struct Options {
  // Only the most common options are listed in this list.
  Options() : info(new internal::OptionsInfo) {}
  Options& version(const char* v) {
    info->program_version = v;
    return *this;
  }
  Options& description(const char* d) {
    info->description = d;
    return *this;
  }
  Options& after_doc(const char* a) {
    info->after_doc = a;
    return *this;
  }
  Options& domain(const char* d) {
    info->domain = d;
    return *this;
  }
  Options& email(const char* b) {
    info->email = b;
    return *this;
  }
  Options& flags(Flags f) {
    info->flags |= f;
    return *this;
  }

  std::unique_ptr<internal::OptionsInfo> info;
};

class Argument {
 public:
  explicit Argument(Names names, Dest dest = {}, const char* help = {})
      : builder_(internal::ArgumentBuilder::Create()) {
    ARGPARSE_DCHECK(names.info);
    builder_->SetNames(std::move(names.info));
    if (dest.info)
      builder_->SetDest(std::move(dest.info));
    if (help)
      builder_->SetHelp(help);
  }

  Argument& Dest(Dest dest) {
    builder_->SetDest(std::move(dest.info));
    return *this;
  }
  Argument& Action(const char* str) {
    builder_->SetActionString(str);
    return *this;
  }
  Argument& Action(ActionCallback cb) {
    builder_->SetActionCallback(std::move(cb.cb));
    return *this;
  }
  Argument& Type(TypeCallback cb) {
    builder_->SetTypeCallback(std::move(cb.cb));
    return *this;
  }
  template <typename T>
  Argument& Type() {
    builder_->SetTypeOperations(internal::CreateOperations<T>());
    return *this;
  }
  Argument& Type(FileType file_type) {
    builder_->SetTypeFileType(file_type.mode());
    return *this;
  }
  Argument& ConstValue(AnyValue val) {
    builder_->SetConstValue(val.Release());
    return *this;
  }
  Argument& DefaultValue(AnyValue val) {
    builder_->SetDefaultValue(val.Release());
    return *this;
  }
  Argument& Help(std::string val) {
    builder_->SetHelp(std::move(val));
    return *this;
  }
  Argument& Required(bool val) {
    builder_->SetRequired(val);
    return *this;
  }
  Argument& MetaVar(std::string val) {
    builder_->SetMetaVar(std::move(val));
    return *this;
  }
  Argument& NumArgs(int num) {
    builder_->SetNumArgsNumber(num);
    return *this;
  }
  Argument& NumArgs(char flag) {
    builder_->SetNumArgsFlag(flag);
    return *this;
  }

  std::unique_ptr<internal::Argument> Build() {
    return builder_->CreateArgument();
  }

 private:
  std::unique_ptr<internal::ArgumentBuilder> builder_;
};

// This is a helper that provides add_argument().
class AddArgumentHelper {
 public:
  void Add(Argument& arg) {
    return Add(std::move(arg));
  }
  void Add(Argument&& arg) {
    return AddArgumentImpl(arg.Build());
  }
  virtual ~AddArgumentHelper() {}

 private:
  virtual void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) = 0;
};

class ArgumentGroup : public AddArgumentHelper {
 public:
  explicit ArgumentGroup(internal::ArgumentGroup* group) : group_(group) {}

 private:
  void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) override {}
  internal::ArgumentGroup* group_;
};

// If we can do add_argument_group(), add_argument() is always possible.
class AddArgumentGroupHelper : public AddArgumentHelper {
 public:
  ArgumentGroup Add(const char* header) {
    ARGPARSE_DCHECK(header);
    return ArgumentGroup(AddArgumentGroupImpl(header));
  }

 private:
  virtual internal::ArgumentGroup* AddArgumentGroupImpl(const char* header) = 0;
};

class SubParser : public AddArgumentGroupHelper {
 public:
  explicit SubParser(internal::SubCommand* sub) : sub_(sub) {}

 private:
  void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) override {
    return sub_->GetHolder()->AddArgument(std::move(arg));
  }
  internal::ArgumentGroup* AddArgumentGroupImpl(const char* header) override {
    return sub_->GetHolder()->AddArgumentGroup(header);
  }
  internal::SubCommand* sub_;
};

class SubCommand {
 public:
  explicit SubCommand(std::string name, const char* help = {})
      : cmd_(internal::SubCommand::Create(std::move(name))) {
    if (help)
      cmd_->SetHelpDoc(help);
  }

  SubCommand& Aliases(std::vector<std::string> als) {
    cmd_->SetAliases(std::move(als));
    return *this;
  }
  SubCommand& Help(std::string val) {
    cmd_->SetHelpDoc(std::move(val));
    return *this;
  }

  std::unique_ptr<internal::SubCommand> Build() {
    ARGPARSE_DCHECK(cmd_);
    return std::move(cmd_);
  }

 private:
  std::unique_ptr<internal::SubCommand> cmd_;
};

class SubParserGroup {
 public:
  explicit SubParserGroup(internal::SubCommandGroup* group) : group_(group) {}

  // Positional.
  // SubParser add_parser(std::string name,
  //                      std::string help = {},
  //                      std::vector<std::string> aliases = {}) {
  //   SubCommand builder(std::move(name));
  //   builder.help(std::move(help)).aliases(std::move(aliases));
  //   return add_parser(builder.Build());
  // }

  // Builder pattern.
  SubParser Add(SubCommand cmd) {
    // auto* cmd_ptr = group_->AddSubCommand(std::move(cmd));
    // return SubParser(cmd_ptr);
  }

 private:
  internal::SubCommandGroup* group_;
};

// Support add(subparsers(...))
class SubParsersBuilder {
 public:
  explicit SubParsersBuilder() : group_(internal::SubCommandGroup::Create()) {}

  SubParsersBuilder& Title(std::string val) {
    group_->SetTitle(std::move(val));
    return *this;
  }

  SubParsersBuilder& Description(std::string val) {
    group_->SetDescription(std::move(val));
    return *this;
  }

  SubParsersBuilder& meta_var(std::string val) {
    group_->SetMetaVar(std::move(val));
    return *this;
  }

  SubParsersBuilder& Help(std::string val) {
    group_->SetHelpDoc(std::move(val));
    return *this;
  }

  SubParsersBuilder& dest(Dest val) {
    group_->SetDest(std::move(val.info));
    return *this;
  }

  std::unique_ptr<internal:: SubCommandGroup> Build() { return std::move(group_); }

 private:
  std::unique_ptr<internal::SubCommandGroup> group_;
};

// Interface of ArgumentParser.
class MainParserHelper : public AddArgumentGroupHelper {
 public:
  void parse_args(int argc, const char** argv) {
    ParseArgsImpl(ArgArray(argc, argv), nullptr);
  }
  void parse_args(std::vector<const char*> args) {
    ParseArgsImpl(ArgArray(args), nullptr);
  }
  bool parse_known_args(int argc,
                        const char** argv,
                        std::vector<std::string>* out) {
    return ParseArgsImpl(ArgArray(argc, argv), out);
  }
  bool parse_known_args(std::vector<const char*> args,
                        std::vector<std::string>* out) {
    return ParseArgsImpl(args, out);
  }

  using AddArgumentGroupHelper::add_argument_group;
  using AddArgumentHelper::add_argument;

  SubParserGroup add_subparsers(std::unique_ptr<internal::SubCommandGroup> group) {
    return SubParserGroup(AddSubParsersImpl(std::move(group)));
  }
  SubParserGroup add_subparsers(Dest dest, std::string help = {}) {
    SubParsersBuilder builder;
    builder.dest(std::move(dest)).help(std::move(help));
    return add_subparsers(builder.Build());
  }

 private:
  virtual bool ParseArgsImpl(ArgArray args, std::vector<std::string>* out) = 0;
  virtual internal::SubCommandGroup* AddSubParsersImpl(
      std::unique_ptr<internal::SubCommandGroup> group) = 0;
};

class ArgumentParser : public MainParserHelper {
 public:
  ArgumentParser() : controller_(internal::ArgumentController::Create()) {}

  explicit ArgumentParser(Options options) : ArgumentParser() {
    if (options.info)
      controller_->SetOptions(std::move(options.info));
  }

 private:
  bool ParseArgsImpl(ArgArray args, std::vector<std::string>* out) override {
    ARGPARSE_DCHECK(out);
    return controller_->GetParser()->ParseKnownArgs(args, out);
  }
  void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) override {
    return controller_->GetMainHolder()->AddArgument(std::move(arg));
  }
  internal::ArgumentGroup* AddArgumentGroupImpl(const char* header) override {
    return controller_->GetMainHolder()->AddArgumentGroup(header);
  }
internal::  SubCommandGroup* AddSubParsersImpl(
      std::unique_ptr<internal::SubCommandGroup> group) override {
    return controller_->GetSubCommandHolder()->AddSubCommandGroup(
        std::move(group));
  }

  std::unique_ptr<internal::ArgumentController> controller_;
};

}  // namespace argparse
