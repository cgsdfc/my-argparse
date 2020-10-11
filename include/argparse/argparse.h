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

  Argument& SetDest(Dest dest) {
    builder_->SetDest(internal::GetBuiltObject(&dest));
    return *this;
  }
  Argument& SetAction(const char* str) {
    builder_->SetActionString(str);
    return *this;
  }
  Argument& SetAction(ActionCallback cb) {
    builder_->SetActionCallback(internal::GetBuiltObject(&cb));
    return *this;
  }
  Argument& SetType(TypeCallback cb) {
    builder_->SetTypeCallback(internal::GetBuiltObject(&cb));
    return *this;
  }
  template <typename T>
  Argument& SetType() {
    builder_->SetTypeOperations(internal::CreateOperations<T>());
    return *this;
  }
  Argument& SetType(FileType file_type) {
    builder_->SetTypeFileType(internal::GetBuiltObject(&file_type));
    return *this;
  }
  Argument& SetConstValue(AnyValue val) {
    builder_->SetConstValue(internal::GetBuiltObject(&val));
    return *this;
  }
  Argument& SetDefaultValue(AnyValue val) {
    builder_->SetDefaultValue(internal::GetBuiltObject(&val));
    return *this;
  }
  Argument& SetHelp(std::string val) {
    builder_->SetHelp(std::move(val));
    return *this;
  }
  Argument& SetRequired(bool val) {
    builder_->SetRequired(val);
    return *this;
  }
  Argument& SetMetaVar(std::string val) {
    builder_->SetMetaVar(std::move(val));
    return *this;
  }
  Argument& SetNumArgs(NumArgs num_args) {
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
// For derived, void AddArgumentImpl(std::unique_ptr<internal::Argument>) should
// be implemented.
// Note: THIS IS NOT A PUBLIC API!
template <typename Derived>
class SupportAddArgument {
 public:
  template <typename T>
  Derived& AddArgument(T&& arg) {
    auto* self = static_cast<Derived*>(this);
    self->AddArgumentImpl(
        std::unique_ptr<internal::Argument>(internal::GetBuiltObject(&arg)));
    return *self;
  }
};

// ArgumentGroup: a group of arguments that share the same title.
class ArgumentGroup : public SupportAddArgument<ArgumentGroup> {
 public:
  ArgumentGroup(internal::ArgumentGroup* group) : group_(group) {}

 private:
  friend class SupportAddArgument<ArgumentGroup>;

  // SupportArgument implementation.
  void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) {
    group_->AddArgument(std::move(arg));
  }
  internal::ArgumentGroup* group_;
};

// If we can do add_argument_group(), add_argument() is always possible.
// For derived, void AddArgumentGroupImpl(std::string) should be implemented.
// Note: THIS IS NOT A PUBLIC API!
template <typename Derived>
class SupportAddArgumentGroup : public SupportAddArgument<Derived> {
 public:
  // Add an argument group to this object.
  ArgumentGroup AddArgumentGroup(std::string header) {
    auto* self = static_cast<Derived*>(this);
    self->AddArgumentGroupImpl(std::move(header));
    return *self;
  }
};

class SubCommandProxy
    : public SupportAddArgumentGroup<SubCommandProxy> {
 public:
  SubCommandProxy(internal::SubCommand* sub) : sub_(sub) {}

 private:
  friend class SupportAddArgumentGroup<SubCommandProxy>;
  void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) {
    return sub_->GetHolder()->AddArgument(std::move(arg));
  }
  internal::ArgumentGroup* AddArgumentGroupImpl(std::string header) {
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

  SubCommand& SetAliases(std::vector<std::string> als) {
    cmd_->SetAliases(std::move(als));
    return *this;
  }
  SubCommand& SetHelp(std::string val) {
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

  SubCommandGroup& SetTitle(std::string val) {
    group_->SetTitle(std::move(val));
    return *this;
  }

  SubCommandGroup& SetDescription(std::string val) {
    group_->SetDescription(std::move(val));
    return *this;
  }

  SubCommandGroup& SetMetaVar(std::string val) {
    group_->SetMetaVar(std::move(val));
    return *this;
  }

  SubCommandGroup& SetHelp(std::string val) {
    group_->SetHelpDoc(std::move(val));
    return *this;
  }

  SubCommandGroup& SetDest(Dest val) {
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

class ArgumentParser : public SupportAddArgumentGroup<ArgumentParser> {
 public:
  ArgumentParser() : controller_(internal::ArgumentController::Create()) {}

  ArgumentParser& SetDescription(std::string val) {
    GetOptions()->SetDescription(std::move(val));
    return *this;
  }
  ArgumentParser& SetProgramVersion(std::string val) {
    GetOptions()->SetProgramVersion(std::move(val));
    return *this;
  }
  ArgumentParser& SetEmail(std::string val) {
    GetOptions()->SetEmail(std::move(val));
    return *this;
  }
  ArgumentParser& SetProgramName(std::string& val) {
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
  friend class SupportAddArgumentGroup<ArgumentParser>;

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
