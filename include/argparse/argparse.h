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
  kNoFlags = 0,  // The default.
  // kNoHelp = ARGP_NO_HELP,  // Don't produce --help.
  // kLongOnly = ARGP_LONG_ONLY,
  // kNoExit = ARGP_NO_EXIT,
};

class Argument {
 public:
  explicit Argument(Names names, Dest dest = {}, const char* help = {})
      : builder_(internal::ArgumentBuilder::Create()) {
    builder_->SetNames(internal::GetBuiltObject(&names));
    builder_->SetDest(internal::GetBuiltObject(&dest));
    if (help) builder_->SetHelp(help);
  }

  Argument& Dest(Dest dest) {
    builder_->SetDest(internal::GetBuiltObject(&dest));
    return *this;
  }
  Argument& Action(const char* str) {
    builder_->SetActionString(str);
    return *this;
  }
  Argument& Action(ActionCallback cb) {
    builder_->SetActionCallback(internal::GetBuiltObject(&cb));
    return *this;
  }
  Argument& Type(TypeCallback cb) {
    builder_->SetTypeCallback(internal::GetBuiltObject(&cb));
    return *this;
  }
  template <typename T>
  Argument& Type() {
    builder_->SetTypeOperations(internal::CreateOperations<T>());
    return *this;
  }
  Argument& Type(FileType file_type) {
    builder_->SetTypeFileType(internal::GetBuiltObject(&file_type));
    return *this;
  }
  Argument& ConstValue(AnyValue val) {
    builder_->SetConstValue(internal::GetBuiltObject(&val));
    return *this;
  }
  Argument& DefaultValue(AnyValue val) {
    builder_->SetDefaultValue(internal::GetBuiltObject(&val));
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
  Argument& NumArgs(NumArgs num_args) {
    builder_->SetNumArgs(internal::GetBuiltObject(&num_args));
    return *this;
  }

 private:
  friend class internal::BuilderAccessor;
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
    AddArgumentImpl(internal::GetBuiltObject(&arg));
    return *this;
  }
  virtual ~SupportAddArgument() {}

 private:
  virtual void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) = 0;
};

class ArgumentGroup : public SupportAddArgument {
 public:
  ArgumentGroup(internal::ArgumentGroup* group) : group_(group) {}

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
    return AddArgumentGroupImpl(std::move(header));
  }

 private:
  virtual internal::ArgumentGroup* AddArgumentGroupImpl(std::string header) = 0;
};

class SubCommandProxy : public SupportAddArgumentGroup {
 public:
  SubCommandProxy(internal::SubCommand* sub) : sub_(sub) {}

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
    if (help) cmd_->SetHelpDoc(help);
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
  friend class internal::BuilderAccessor;
  std::unique_ptr<internal::SubCommand> Build() {
    ARGPARSE_DCHECK(cmd_);
    return std::move(cmd_);
  }

  std::unique_ptr<internal::SubCommand> cmd_;
};

class SubCommandGroupProxy {
 public:
  SubCommandGroupProxy(internal::SubCommandGroup* group) : group_(group) {}

  SubCommandProxy AddParser(SubCommand cmd) {
    auto real_cmd = internal::GetBuiltObject(&cmd);
    return group_->AddSubCommand(std::move(real_cmd));
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
    group_->SetDest(internal::GetBuiltObject(&val));
    return *this;
  }

 private:
  friend class internal::BuilderAccessor;
  std::unique_ptr<internal::SubCommandGroup> Build() {
    return std::move(group_);
  }
  std::unique_ptr<internal::SubCommandGroup> group_;
};

class ArgumentParser : public SupportAddArgumentGroup {
 public:
  ArgumentParser() : controller_(internal::ArgumentController::Create()) {}

  ArgumentParser& Description(std::string val) {
    GetOptions()->SetDescription(std::move(val));
    return *this;
  }
  ArgumentParser& ProgramVersion(std::string val) {
    GetOptions()->SetProgramVersion(std::move(val));
    return *this;
  }
  ArgumentParser& Email(std::string val) {
    GetOptions()->SetEmail(std::move(val));
    return *this;
  }
  ArgumentParser& ProgramName(std::string& val) {
    GetOptions()->SetProgramName(std::move(val));
    return *this;
  }

  void ParseArgs(int argc, const char** argv) {
    ParseArgsImpl({argc, argv}, nullptr);
  }
  void ParseArgs(std::vector<const char*> args) {
    ParseArgsImpl(args, nullptr);
  }
  bool ParseKnownArgs(int argc, const char** argv,
                      std::vector<std::string>* out) {
    return ParseArgsImpl({argc, argv}, out);
  }
  bool ParseKnownArgs(std::vector<const char*> args,
                      std::vector<std::string>* out) {
    return ParseArgsImpl(args, out);
  }
  SubCommandGroupProxy AddSubParsers(SubCommandGroup&& group) {
    return AddSubCommandGroupImpl(internal::GetBuiltObject(&group));
  }
  SubCommandGroupProxy AddSubParsers(SubCommandGroup& group) {
    return AddSubParsers(std::move(group));
  }

 private:
  bool ParseArgsImpl(internal::ArgArray args, std::vector<std::string>* out) {
    ARGPARSE_DCHECK(out);
    return controller_->ParseKnownArgs(args, out);
  }
  void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) {
    return controller_->AddArgument(std::move(arg));
  }
  internal::ArgumentGroup* AddArgumentGroupImpl(std::string header) {
    return controller_->AddArgumentGroup(std::move(header));
  }
  internal::SubCommandGroup* AddSubCommandGroupImpl(
      std::unique_ptr<internal::SubCommandGroup> group) {
    return controller_->AddSubCommandGroup(std::move(group));
  }
  internal::OptionsListener* GetOptions() {
    return controller_->GetOptionsListener();
  }

  std::unique_ptr<internal::ArgumentController> controller_;
};

}  // namespace argparse
