// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/argparse-builder.h"
#include "argparse/argparse-traits.h"

// Holds public things
namespace argparse {

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
    builder_->SetNames(GetBuiltObject(&names));
    builder_->SetDest(GetBuiltObject(&dest));
    if (help)
      builder_->SetHelp(help);
  }

  Argument& Dest(Dest dest) {
    builder_->SetDest(GetBuiltObject(&dest));
    return *this;
  }
  Argument& Action(const char* str) {
    builder_->SetActionString(str);
    return *this;
  }
  Argument& Action(ActionCallback cb) {
    builder_->SetActionCallback(GetBuiltObject(&cb));
    return *this;
  }
  Argument& Type(TypeCallback cb) {
    builder_->SetTypeCallback(GetBuiltObject(&cb));
    return *this;
  }
  template <typename T>
  Argument& Type() {
    builder_->SetTypeOperations(internal::CreateOperations<T>());
    return *this;
  }
  Argument& Type(FileType file_type) {
    builder_->SetTypeFileType(GetBuiltObject(&file_type));
    return *this;
  }
  Argument& ConstValue(AnyValue val) {
    builder_->SetConstValue(GetBuiltObject(&val));
    return *this;
  }
  Argument& DefaultValue(AnyValue val) {
    builder_->SetDefaultValue(GetBuiltObject(&val));
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

 private:
  friend class BuilderAccessor;
  std::unique_ptr<internal::Argument> Build() {
    return builder_->CreateArgument();
  }

  std::unique_ptr<internal::ArgumentBuilder> builder_;
};

// This is a helper that provides add_argument().
class SupportAddArgument {
 public:
  SupportAddArgument& AddArgument(Argument& arg) {
    return AddArgument(std::move(arg));
  }
  SupportAddArgument& AddArgument(Argument&& arg) {
    AddArgumentImpl(GetBuiltObject(&arg));
    return *this;
  }
  virtual ~SupportAddArgument() {}

 private:
  virtual void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) = 0;
};

class ArgumentGroup : public SupportAddArgument {
 public:
  explicit ArgumentGroup(internal::ArgumentGroup* group) : group_(group) {}

 private:
  void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) override {
    group_->AddArgument(std::move(arg));
  }
  internal::ArgumentGroup* group_;
};

// If we can do add_argument_group(), add_argument() is always possible.
class SupportAddArgumentGroup : public SupportAddArgument {
 public:
  ArgumentGroup AddArgumentGroup(std::string header) {
    return ArgumentGroup(AddArgumentGroupImpl(std::move(header)));
  }

 private:
  virtual internal::ArgumentGroup* AddArgumentGroupImpl(std::string header) = 0;
};

class SubCommandProxy : public SupportAddArgumentGroup {
 public:
  explicit SubCommandProxy(internal::SubCommand* sub) : sub_(sub) {}

 private:
  void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) override {
    return sub_->GetHolder()->AddArgument(std::move(arg));
  }
  internal::ArgumentGroup* AddArgumentGroupImpl(std::string header) override {
    return sub_->GetHolder()->AddArgumentGroup(std::move(header));
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

 private:
  friend class BuilderAccessor;
  std::unique_ptr<internal::SubCommand> Build() {
    ARGPARSE_DCHECK(cmd_);
    return std::move(cmd_);
  }

  std::unique_ptr<internal::SubCommand> cmd_;
};

class SubCommandGroupProxy {
 public:
  explicit SubCommandGroupProxy(internal::SubCommandGroup* group)
      : group_(group) {}

  SubCommandProxy AddParser(SubCommand cmd) {
    auto real_cmd = GetBuiltObject(&cmd);
    return SubCommandProxy(group_->AddSubCommand(std::move(real_cmd)));
  }

 private:
  internal::SubCommandGroup* group_;
};

class SubCommandGroup {
 public:
  explicit SubCommandGroup() : group_(internal::SubCommandGroup::Create()) {}

  SubCommandGroup& Title(std::string val) {
    group_->SetTitle(std::move(val));
    return *this;
  }

  SubCommandGroup& Description(std::string val) {
    group_->SetDescription(std::move(val));
    return *this;
  }

  SubCommandGroup& MetaVar(std::string val) {
    group_->SetMetaVar(std::move(val));
    return *this;
  }

  SubCommandGroup& Help(std::string val) {
    group_->SetHelpDoc(std::move(val));
    return *this;
  }

  SubCommandGroup& Dest(Dest val) {
    group_->SetDest(GetBuiltObject(&val));
    return *this;
  }

 private:
  friend class BuilderAccessor;
  std::unique_ptr<internal::SubCommandGroup> Build() {
    return std::move(group_);
  }
  std::unique_ptr<internal::SubCommandGroup> group_;
};

// Interface of ArgumentParser.
class ArgumentParserInterface : public SupportAddArgumentGroup {
 public:
  virtual ~ArgumentParserInterface() {}

  using SupportAddArgumentGroup::AddArgument;
  using SupportAddArgumentGroup::AddArgumentGroup;

  void ParseArgs(int argc, const char** argv) {
    ParseArgsImpl(ArgArray(argc, argv), nullptr);
  }
  void ParseArgs(std::vector<const char*> args) {
    ParseArgsImpl(ArgArray(args), nullptr);
  }
  bool ParseKnownArgs(int argc,
                      const char** argv,
                      std::vector<std::string>* out) {
    return ParseArgsImpl(ArgArray(argc, argv), out);
  }
  bool ParseKnownArgs(std::vector<const char*> args,
                      std::vector<std::string>* out) {
    return ParseArgsImpl(args, out);
  }

  SubCommandGroupProxy AddSubParsers(SubCommandGroup&& group) {
    return SubCommandGroupProxy(AddSubCommandGroupImpl(GetBuiltObject(&group)));
  }

  SubCommandGroupProxy AddSubParsers(SubCommandGroup& group) {
    return AddSubParsers(std::move(group));
  }

 private:
  virtual bool ParseArgsImpl(ArgArray args, std::vector<std::string>* out) = 0;
  virtual internal::SubCommandGroup* AddSubCommandGroupImpl(
      std::unique_ptr<internal::SubCommandGroup> group) = 0;
};

class ArgumentParser : public ArgumentParserInterface {
 public:
  ArgumentParser() : controller_(internal::ArgumentContainer::Create()) {}

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
  internal::ArgumentGroup* AddArgumentGroupImpl(std::string header) override {
    return controller_->GetMainHolder()->AddArgumentGroup(std::move(header));
  }
  internal::SubCommandGroup* AddSubCommandGroupImpl(
      std::unique_ptr<internal::SubCommandGroup> group) override {
    return controller_->GetSubCommandHolder()->AddSubCommandGroup(
        std::move(group));
  }

  std::unique_ptr<internal::ArgumentContainer> controller_;
};

}  // namespace argparse
