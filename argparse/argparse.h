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

namespace internal {
namespace argument_internal {

template <typename Derived>
class BasicMethods {
 public:
  Derived& SetHelp(std::string val) {
    builder()->SetHelp(std::move(val));
    return derived_this();
  }
  Derived& SetRequired(bool val) {
    builder()->SetRequired(val);
    return derived_this();
  }
  Derived& SetMetaVar(std::string val) {
    builder()->SetMetaVar(std::move(val));
    return derived_this();
  }
  Derived& SetNumArgs(NumArgs num_args) {
    builder()->SetNumArgs(internal::GetBuiltObject(&num_args));
    return derived_this();
  }

 private:
  internal::ArgumentBuilder* builder() { return derived_this().GetBuilder(); }
  Derived& derived_this() { return static_cast<Derived&>(*this); }
};

template <typename T, typename Derived>
class DestTypeMethods {
 public:
  Derived& SetConstValue(T&& value) {
    builder()->SetConstValue(MakeAny<T>(std::forward<T>(value)));
    return derived_this();
  }
  Derived& SetDefaultValue(T&& value) {
    builder()->SetDefaultValue(MakeAny<T>(std::move(value)));
    return derived_this();
  }
  Derived& SetAction(ActionCallback<T>&& func) {
    builder()->SetActionInfo(ActionInfo::CreateCallbackAction(std::move(func)));
    return derived_this();
  }
  Derived& SetAction(const char* str) {
    builder()->SetActionString(str);
    return derived_this();
  }
  Derived& SetType(TypeCallback<T>&& func) {
    builder()->SetTypeInfo(TypeInfo::CreateCallbackType(std::move(func)));
    return derived_this();
  }

 private:
  internal::ArgumentBuilder* builder() { return derived_this().GetBuilder(); }
  Derived& derived_this() { return static_cast<Derived&>(*this); }
};

template <typename T, typename Derived, typename ValueType = ValueTypeOf<T>>
class ValueTypeMethods {
 public:
  Derived& SetValueTypeConst(ValueType&& value) {
    builder()->SetConstValue(MakeAny<ValueType>(std::move(value)));
    return derived_this();
  }
  Derived& SetValueType(TypeCallback<ValueType>&& func) {
    builder()->SetTypeInfo(TypeInfo::CreateCallbackType(std::move(func)));
    return derived_this();
  }

 private:
  internal::ArgumentBuilder* builder() { return derived_this().GetBuilder(); }
  Derived& derived_this() { return static_cast<Derived&>(*this); }
};

template <typename T, typename Derived>
class ValueTypeMethods<T, Derived, void> {};

template <typename T, typename Derived, bool = IsOpenSupported<T>{}>
class FileTypeMethods {
 public:
  Derived& SetType(FileType file_type) {
    builder()->SetTypeFileType(internal::GetBuiltObject(&file_type));
    return derived_this();
  }

 private:
  internal::ArgumentBuilder* builder() { return derived_this().GetBuilder(); }
  Derived& derived_this() { return static_cast<Derived&>(*this); }
};

template <typename T, typename Derived>
class FileTypeMethods<T, Derived, false> {};

template <typename T>
class Argument : public BasicMethods<Argument<T>>,
                 public DestTypeMethods<T, Argument<T>>,
                 public ValueTypeMethods<T, Argument<T>>,
                 public FileTypeMethods<T, Argument<T>> {
 public:
  explicit Argument(T* ptr) : builder_(internal::ArgumentBuilder::Create()) {
    builder_->SetDest(DestInfo::CreateFromPtr(ptr));
  }

 private:
  friend class BuilderAccessor;
  friend class BasicMethods<Argument<T>>;
  friend class DestTypeMethods<T, Argument<T>>;
  friend class ValueTypeMethods<T, Argument<T>>;
  friend class FileTypeMethods<T, Argument<T>>;

  internal::ArgumentBuilder* GetBuilder() { return builder_.get(); }
  std::unique_ptr<internal::ArgumentBuilder> builder_;
};

}  // namespace argument_internal
}  // namespace internal

// TODO: make this a typesafe class using template.

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
  Argument& SetAction(ActionFunction cb) {
    builder_->SetActionCallback(std::move(cb));
    return *this;
  }
  Argument& SetType(TypeFunction cb) {
    builder_->SetTypeCallback(std::move(cb));
    return *this;
  }
  template <typename T>
  Argument& SetType() {
    builder_->SetTypeOperations(internal::Operations::GetInstance<T>());
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

class SubCommandProxy : public SupportAddArgumentGroup<SubCommandProxy> {
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
  ArgumentParser& SetBugReportEmail(std::string val) {
    GetOptions()->SetBugReportEmail(std::move(val));
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
  friend class SupportAddArgument<ArgumentParser>;
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
