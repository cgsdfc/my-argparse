// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/argparse-open-mode.h"
#include "argparse/internal/argparse-internal.h"

namespace argparse {

// Implementation details of builder.
namespace builder_internal {
// To be a Builder, befriend this class.
class BuilderAccessor;

// Simple implementation of a builder that the building process only
// happens in its constructor.
template <typename T>
class SimpleBuilder {
 protected:
  // Derived can override this.
  using ObjectType = T;
  SimpleBuilder() = default;
  explicit SimpleBuilder(std::unique_ptr<T> obj) : object_(std::move(obj)) {}
  void SetObject(std::unique_ptr<T> obj) { object_ = std::move(obj); }
  T* GetObject() { return object_.get(); }

  // Default impl of Build(). Derived can override this.
  std::unique_ptr<ObjectType> Build() { return std::move(object_); }
  bool HasObject() const { return object_ != nullptr; }

 private:
  friend class BuilderAccessor;
  std::unique_ptr<T> object_;
};

// Friends of every Builder class to access their Build() method.
struct BuilderAccessor {
  template <typename Builder>
  static auto Build(Builder* builder) -> decltype(builder->Build()) {
    return builder->Build();
  }
};

// Call this function on a builder object to obtain the built object.
template <typename Builder>
auto GetBuiltObject(Builder* b) -> decltype(BuilderAccessor::Build(b)) {
  return BuilderAccessor::Build(b);
}

// Creator of DestInfo. For those that need a DestInfo, just take Dest
// as an arg.
class Dest : private SimpleBuilder<internal::DestInfo> {
 public:
  template <typename T>
  Dest(T* ptr) {
    this->SetObject(internal::DestInfo::CreateFromPtr(ptr));
  }
  // Dest() = default;

 private:
  friend class BuilderAccessor;
};

class Names : private SimpleBuilder<internal::NamesInfo> {
 public:
  Names(const char* name) : Names(std::string(name)) {
    ARGPARSE_CHECK_F(name, "name should not be null");
  }
  Names(std::string name);
  Names(std::initializer_list<std::string> names);

 private:
  friend class BuilderAccessor;
};

class NumArgs : private SimpleBuilder<internal::NumArgsInfo> {
 public:
  NumArgs(int number) {
    this->SetObject(internal::NumArgsInfo::CreateFromNum(number));
  }
  NumArgs(char flag) {
    this->SetObject(internal::NumArgsInfo::CreateFromFlag(flag));
  }

 private:
  friend class BuilderAccessor;
};

}  // namespace builder_internal

class AnyValue : private builder_internal::SimpleBuilder<internal::Any> {
 public:
  template <typename T,
            absl::enable_if_t<!std::is_convertible<T, AnyValue>{}>* = nullptr>
  AnyValue(T&& val) {
    auto obj = internal::MakeAny<absl::decay_t<T>>(std::forward<T>(val));
    this->SetObject(std::move(obj));
  }

 private:
  friend class builder_internal::BuilderAccessor;
};

class FileType {
 public:
  explicit FileType(const char* mode) : mode_(CharsToMode(mode)) {}
  explicit FileType(std::ios_base::openmode mode)
      : mode_(StreamModeToMode(mode)) {}

 private:
  friend class builder_internal::BuilderAccessor;
  OpenMode Build() const { return mode_; }
  OpenMode mode_;
};

namespace builder_internal {
// Component of a type-saft Argument's methods.
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
    builder()->SetNumArgs(GetBuiltObject(&num_args));
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
    builder()->SetConstValue(internal::MakeAny<T>(std::move(value)));
    return derived_this();
  }
  Derived& SetDefaultValue(T&& value) {
    builder()->SetDefaultValue(internal::MakeAny<T>(std::move(value)));
    return derived_this();
  }
  Derived& SetAction(ActionCallback<T>&& func) {
    builder()->SetActionInfo(
        internal::ActionInfo::CreateCallbackAction(std::move(func)));
    return derived_this();
  }
  Derived& SetAction(const char* str) {
    builder()->SetActionString(str);
    return derived_this();
  }
  Derived& SetType(TypeCallback<T>&& func) {
    builder()->SetTypeInfo(
        internal::TypeInfo::CreateCallbackType(std::move(func)));
    return derived_this();
  }

 private:
  internal::ArgumentBuilder* builder() { return derived_this().GetBuilder(); }
  Derived& derived_this() { return static_cast<Derived&>(*this); }
};

template <typename T, typename Derived,
          typename ValueType = internal::ValueTypeOf<T>>
class ValueTypeMethods {
 public:
  Derived& SetValueTypeConst(ValueType&& value) {
    builder()->SetConstValue(internal::MakeAny<ValueType>(std::move(value)));
    return derived_this();
  }
  Derived& SetValueType(TypeCallback<ValueType>&& func) {
    builder()->SetTypeInfo(
        internal::TypeInfo::CreateCallbackType(std::move(func)));
    return derived_this();
  }

 private:
  internal::ArgumentBuilder* builder() { return derived_this().GetBuilder(); }
  Derived& derived_this() { return static_cast<Derived&>(*this); }
};

template <typename T, typename Derived>
class ValueTypeMethods<T, Derived, void> {};

template <typename T, typename Derived, bool = internal::IsOpenSupported<T>{}>
class FileTypeMethods {
 public:
  Derived& SetFileType(FileType file_type) {
    builder()->SetTypeFileType(GetBuiltObject(&file_type));
    return derived_this();
  }

 private:
  internal::ArgumentBuilder* builder() { return derived_this().GetBuilder(); }
  Derived& derived_this() { return static_cast<Derived&>(*this); }
};

template <typename T, typename Derived>
class FileTypeMethods<T, Derived, false> {};

// This is a wrapper of internal::ArgumentBuilder for type-safety.
template <typename T>
class ArgumentBuilder : public BasicMethods<ArgumentBuilder<T>>,
                        public DestTypeMethods<T, ArgumentBuilder<T>>,
                        public ValueTypeMethods<T, ArgumentBuilder<T>>,
                        public FileTypeMethods<T, ArgumentBuilder<T>> {
 public:
  ArgumentBuilder(Names names, T* ptr, absl::string_view help)
      : builder_(internal::ArgumentBuilder::Create()) {
    GetBuilder()->SetDest(internal::DestInfo::CreateFromPtr(ptr));
    GetBuilder()->SetNames(GetBuiltObject(&names));
    GetBuilder()->SetHelp(std::string(help));
  }

 private:
  friend class BasicMethods<ArgumentBuilder<T>>;
  friend class DestTypeMethods<T, ArgumentBuilder<T>>;
  friend class ValueTypeMethods<T, ArgumentBuilder<T>>;
  friend class FileTypeMethods<T, ArgumentBuilder<T>>;
  friend class BuilderAccessor;

  // For being a Builder.
  std::unique_ptr<internal::Argument> Build() {
    return GetBuilder()->CreateArgument();
  }
  // For CRTP base classes.
  internal::ArgumentBuilder* GetBuilder() { return builder_.get(); }
  std::unique_ptr<internal::ArgumentBuilder> builder_;
};

// This is a helper that provides add_argument().
// For derived, void AddArgumentImpl(std::unique_ptr<internal::Argument>) should
// be implemented.
template <typename Derived>
class SupportAddArgument {
 public:
  // A short positional form of AddArgument().
  template <typename T>
  Derived& AddArgument(Names names, T* dest, absl::string_view help = {}) {
    ArgumentBuilder<T> builder(std::move(names), dest, help);
    return AddArgument(builder);
  }
  // Use it with Argument().
  template <typename T>
  Derived& AddArgument(T&& arg) {
    auto* self = static_cast<Derived*>(this);
    self->AddArgumentImpl(GetBuiltObject(&arg));
    return *self;
  }
};

// ArgumentGroup: a group of arguments that share the same title.
class ArgumentGroup : public SupportAddArgument<ArgumentGroup> {
 public:
  ArgumentGroup(internal::ArgumentGroup* group) : group_(group) {}

 private:
  // SupportAddArgument implementation.
  void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) {
    group_->AddArgument(std::move(arg));
  }

  friend class SupportAddArgument<ArgumentGroup>;
  internal::ArgumentGroup* group_;
};

// If we can do add_argument_group(), add_argument() is always possible.
// For derived, void AddArgumentGroupImpl(std::string) should be implemented.
template <typename Derived>
class SupportAddArgumentGroup : public SupportAddArgument<Derived> {
 public:
  // Add an argument group to this object.
  ArgumentGroup AddArgumentGroup(std::string title) {
    auto* self = static_cast<Derived*>(this);
    return self->AddArgumentGroupImpl(std::move(title));
  }
};

}  // namespace builder_internal

class SubCommandProxy
    : public builder_internal::SupportAddArgumentGroup<SubCommandProxy> {
 public:
  SubCommandProxy(internal::SubCommand* sub) : sub_(sub) {}

 private:
  // SupportAddArgument:
  void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) {
    return sub_->GetHolder()->AddArgument(std::move(arg));
  }
  // SupportAddArgumentGroup:
  internal::ArgumentGroup* AddArgumentGroupImpl(std::string title) {
    return sub_->GetHolder()->AddArgumentGroup(std::move(title));
  }
  friend class SupportAddArgumentGroup<SubCommandProxy>;
  internal::SubCommand* sub_;
};

class SubCommand
    : private builder_internal::SimpleBuilder<internal::SubCommand> {
 public:
  explicit SubCommand(std::string name, const char* help = {}) {
    this->SetObject(internal::SubCommand::Create(std::move(name)));
    if (help) this->GetObject()->SetHelpDoc(help);
  }
  SubCommand& SetAliases(std::vector<std::string> als) {
    this->GetObject()->SetAliases(std::move(als));
    return *this;
  }
  SubCommand& SetHelp(std::string val) {
    this->GetObject()->SetHelpDoc(std::move(val));
    return *this;
  }

 private:
  friend class builder_internal::BuilderAccessor;
};

class SubCommandGroupProxy {
 public:
  SubCommandGroupProxy(internal::SubCommandGroup* group) : group_(group) {}
  template <typename SubCommandT>
  SubCommandProxy AddParser(SubCommandT&& cmd) {
    return group_->AddSubCommand(builder_internal::GetBuiltObject(&cmd));
  }

 private:
  internal::SubCommandGroup* group_;
};

class SubCommandGroup
    : private builder_internal::SimpleBuilder<internal::SubCommandGroup> {
 public:
  SubCommandGroup() { this->SetObject(internal::SubCommandGroup::Create()); }
  SubCommandGroup& SetTitle(std::string val) {
    this->GetObject()->SetTitle(std::move(val));
    return *this;
  }
  SubCommandGroup& SetDescription(std::string val) {
    this->GetObject()->SetDescription(std::move(val));
    return *this;
  }
  SubCommandGroup& SetMetaVar(std::string val) {
    this->GetObject()->SetMetaVar(std::move(val));
    return *this;
  }
  SubCommandGroup& SetHelp(std::string val) {
    this->GetObject()->SetHelpDoc(std::move(val));
    return *this;
  }
  // TODO: this should be stronge-typed.
  SubCommandGroup& SetDest(builder_internal::Dest val) {
    this->GetObject()->SetDest(builder_internal::GetBuiltObject(&val));
    return *this;
  }

 private:
  friend class builder_internal::BuilderAccessor;
};

class ArgumentParser
    : public builder_internal::SupportAddArgumentGroup<ArgumentParser> {
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
  template <typename SubCommandGroupT>
  SubCommandGroupProxy AddSubParsers(SubCommandGroupT&& group) {
    return AddSubCommandGroupImpl(builder_internal::GetBuiltObject(&group));
  }

 private:
  bool ParseArgsImpl(internal::ArgArray args, std::vector<std::string>* out) {
    ARGPARSE_DCHECK(out);
    return controller_->ParseKnownArgs(args, out);
  }
  void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) {
    return controller_->AddArgument(std::move(arg));
  }
  internal::ArgumentGroup* AddArgumentGroupImpl(std::string title) {
    return controller_->AddArgumentGroup(std::move(title));
  }
  internal::SubCommandGroup* AddSubCommandGroupImpl(
      std::unique_ptr<internal::SubCommandGroup> group) {
    return controller_->AddSubCommandGroup(std::move(group));
  }
  internal::OptionsListener* GetOptions() {
    return controller_->GetOptionsListener();
  }

  friend class SupportAddArgument<ArgumentParser>;
  friend class SupportAddArgumentGroup<ArgumentParser>;
  std::unique_ptr<internal::ArgumentController> controller_;
};

template <typename T>
builder_internal::ArgumentBuilder<T> Argument(builder_internal::Names names,
                                              T* dest,
                                              absl::string_view help = {}) {
  return {std::move(names), dest, help};
}

}  // namespace argparse
