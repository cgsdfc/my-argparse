// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-internal.h"

namespace argparse {
namespace internal {
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
  template <typename Builder>
  static auto GetBuilder(Builder* builder) -> decltype(builder->GetBuilder()) {
    return builder->GetBuilder();
  }
};

// Call this function on a builder object to obtain the built object.
template <typename Builder>
auto Build(Builder* b) -> decltype(BuilderAccessor::Build(b)) {
  return BuilderAccessor::Build(b);
}

// template <typename Builder>
// auto GetBuilder
// Creator of DestInfo. For those that need a DestInfo, just take Dest
// as an arg.
class Dest : private SimpleBuilder<internal::DestInfo> {
 public:
  template <typename T>
  Dest(T* ptr) {
    this->SetObject(internal::DestInfo::CreateFromPtr(ptr));
  }

 private:
  friend class BuilderAccessor;
};

class NameOrNames final : private SimpleBuilder<internal::NamesInfo> {
 public:
  NameOrNames(absl::string_view name) {
    this->SetObject(internal::NamesInfo::CreateSingleName(name));
  }
  NameOrNames(std::initializer_list<absl::string_view> names) {
    this->SetObject(internal::NamesInfo::CreateOptionalNames(names));
  }

 private:
  friend class BuilderAccessor;
};

class FlagOrNumber final : private SimpleBuilder<internal::NumArgsInfo> {
 public:
  FlagOrNumber(int number) {
    this->SetObject(internal::NumArgsInfo::CreateNumber(number));
  }
  FlagOrNumber(char flag) {
    this->SetObject(internal::NumArgsInfo::CreateFlag(flag));
  }

 private:
  friend class BuilderAccessor;
};

class AnyValue : private SimpleBuilder<internal::Any> {
 public:
  template <typename T,
            absl::enable_if_t<!std::is_convertible<T, AnyValue>{}>* = nullptr>
  AnyValue(T&& val) {
    auto obj = internal::MakeAny<absl::decay_t<T>>(std::forward<T>(val));
    this->SetObject(std::move(obj));
  }

 private:
  friend class BuilderAccessor;
};

#define ARGPARSE_BUILDER_INTERNAL_COMMON()                         \
  template <typename Method, typename Arg>                         \
  ABSL_MUST_USE_RESULT Derived& Invoke(Method method, Arg&& arg) { \
    auto* self = static_cast<Derived*>(this);                      \
    (BuilderAccessor::GetBuilder(self)->*method)(std::move(arg));  \
    return *self;                                                  \
  }                                                                \
  static constexpr bool kForceSemiColon = false

// Component of a type-saft Argument's methods.
template <typename Derived>
class NonTypeMethodsBase {
 public:
  Derived& Help(std::string val) {
    return Invoke(&ArgumentBuilder::SetHelp, std::move(val));
  }
  Derived& Required(bool val) {
    return Invoke(&ArgumentBuilder::SetRequired, val);
  }
  Derived& MetaVar(std::string val) {
    return Invoke(&ArgumentBuilder::SetMetaVar, std::move(val));
  }
  Derived& NumArgs(FlagOrNumber num_args) {
    return Invoke(&ArgumentBuilder::SetNumArgs,
                  BuilderAccessor::Build(&num_args));
  }

 private:
  ARGPARSE_BUILDER_INTERNAL_COMMON();
};

// Some value_type methods are added according to T.
// For example, `FileType()` is added for a file type, and EnumType() is added
// for an enum type.
// Methods common to all value-type is put into `ValueTypeMethodsBase`.
template <typename Derived, typename T, typename = void>
class SelectValueTypeMethods {};

template <typename Derived, typename T>
class SelectValueTypeMethods<Derived, T,
                             absl::enable_if_t<std::is_enum<T>::value>> {
 public:
  // TODO:
  Derived& EnumType(internal::EnumValues<T> values) {
    return Invoke(&ArgumentBuilder::SetTypeInfo,
                  TypeInfo::CreateEnumType(values));
  }

 private:
  ARGPARSE_BUILDER_INTERNAL_COMMON();
};

template <typename Derived, typename T>
class SelectValueTypeMethods<
    Derived, T, absl::enable_if_t<internal::IsOpenDefined<T>::value>> {
 public:
  Derived& FileType(absl::string_view mode) {
    return Invoke(&ArgumentBuilder::SetTypeFileType, mode);
  }

 private:
  ARGPARSE_BUILDER_INTERNAL_COMMON();
};

// Methods added here are common to all value-types.
template <typename Derived, typename T>
class ValueTypeMethodsBase : public SelectValueTypeMethods<Derived, T> {
 public:
  Derived& Action(absl::string_view str) {
    return Invoke(&ArgumentBuilder::SetActionString, str);
  }
  Derived& Action(ActionCallback<T>&& func) {
    return Invoke(&ArgumentBuilder::SetActionInfo,
                  ActionInfo::CreateCallbackAction(std::move(func)));
  }
  // To implement append_const and store_const.
  Derived& ConstValue(T&& value) {
    return Invoke(&ArgumentBuilder::SetConstValue,
                  MakeAny<T>(std::move(value)));
  }
  Derived& Type(TypeCallback<T>&& func) {
    return Invoke(&ArgumentBuilder::SetTypeInfo,
                  TypeInfo::CreateCallbackType(std::move(func)));
  }

 private:
  ARGPARSE_BUILDER_INTERNAL_COMMON();
};

// For T that does not have a ValueType.
struct FakeValueTypeMethodsBase {
  void Action() = delete;
  void Type() = delete;
  void ConstValue() = delete;
};

template <typename Derived, typename T, typename U = internal::ValueTypeOf<T>>
class TypeMethodsBase : public ValueTypeMethodsBase<Derived, U> {};

template <typename Derived, typename T>
class TypeMethodsBase<Derived, T, void> : public FakeValueTypeMethodsBase {};

// DestMethodsBase add the methods bound to the type of dest.
template <typename Derived, typename T>
class DestMethodsBase : public ValueTypeMethodsBase<Derived, T> {
 public:
  Derived& DefaultValue(T&& value) {
    return Invoke(&ArgumentBuilder::SetDefaultValue,
                  MakeAny<T>(std::move(value)));
  }

 private:
  ARGPARSE_BUILDER_INTERNAL_COMMON();
};

// Incorperate all the typed methods (methods that depend on T) into one class.
template <typename Derived, typename T>
class TypedMethodsBase : public DestMethodsBase<Derived, T>,
                         public TypeMethodsBase<Derived, T> {
 private:
  using DestBase = DestMethodsBase<Derived, T>;
  using TypeBase = TypeMethodsBase<Derived, T>;

 public:
  using DestBase::Action;
  using TypeBase::Action;

  using DestBase::Type;
  using TypeBase::Type;

  using DestBase::ConstValue;
  using TypeBase::ConstValue;
};

template <typename T>
class ArgumentBuilderProxy
    : public NonTypeMethodsBase<ArgumentBuilderProxy<T>>,
      public TypedMethodsBase<ArgumentBuilderProxy<T>, T> {
 public:
  ArgumentBuilderProxy(NameOrNames names, T* ptr) {
    GetBuilder()->SetDest(DestInfo::CreateFromPtr(ptr));
    GetBuilder()->SetNames(BuilderAccessor::Build(&names));
  }

 private:
  // For BuilderAccessor::Build()
  std::unique_ptr<internal::Argument> Build() { return GetBuilder()->Build(); }
  // For BuilderAccessor::GetBuilder()
  internal::ArgumentBuilder* GetBuilder() { return builder_.get(); }

  friend class BuilderAccessor;
  std::unique_ptr<internal::ArgumentBuilder> builder_ =
      internal::ArgumentBuilder::Create();
};

// This is a helper that provides add_argument().
// For derived, void AddArgumentImpl(std::unique_ptr<internal::Argument>) should
// be implemented.
template <typename Derived>
class SupportAddArgument {
 public:
  // Use it with Argument().
  template <typename T>
  Derived& AddArgument(T&& arg) {
    auto* self = static_cast<Derived*>(this);
    self->AddArgumentImpl(Build(&arg));
    return *self;
  }
};

// ArgumentGroup: a group of arguments that share the same title.
class ArgumentGroupProxy final : public SupportAddArgument<ArgumentGroupProxy> {
 public:
  ArgumentGroupProxy(internal::ArgumentGroup* group) : group_(group) {}

 private:
  // SupportAddArgument implementation.
  void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) {
    group_->AddArgument(std::move(arg));
  }

  friend class SupportAddArgument<ArgumentGroupProxy>;
  internal::ArgumentGroup* group_;
};

// If we can do add_argument_group(), add_argument() is always possible.
// For derived, void AddArgumentGroupImpl(std::string) should be implemented.
template <typename Derived>
class SupportAddArgumentGroup : public SupportAddArgument<Derived> {
 public:
  // Add an argument group to this object.
  ArgumentGroupProxy AddArgumentGroup(absl::string_view title) {
    auto* self = static_cast<Derived*>(this);
    return self->AddArgumentGroupImpl(title);
  }
};

class SubCommandProxy final
    : public builder_internal::SupportAddArgumentGroup<SubCommandProxy> {
 public:
  SubCommandProxy(internal::SubCommand* sub) : sub_(sub) {}

 private:
  // SupportAddArgument:
  void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) {
    return sub_->GetHolder()->AddArgument(std::move(arg));
  }
  // SupportAddArgumentGroup:
  internal::ArgumentGroup* AddArgumentGroupImpl(absl::string_view title) {
    return sub_->GetHolder()->AddArgumentGroup(title);
  }
  friend class SupportAddArgumentGroup<SubCommandProxy>;
  internal::SubCommand* sub_;
};

class SubCommand final
    : private builder_internal::SimpleBuilder<internal::SubCommand> {
 public:
  explicit SubCommand(std::string name, const char* help = {}) {
    this->SetObject(internal::SubCommand::Create(std::move(name)));
    if (help) this->GetObject()->SetHelp(help);
  }
  SubCommand& Aliases(std::vector<std::string> als) {
    this->GetObject()->SetAliases(std::move(als));
    return *this;
  }
  SubCommand& Help(std::string val) {
    this->GetObject()->SetHelp(std::move(val));
    return *this;
  }

 private:
  friend class builder_internal::BuilderAccessor;
};

class SubCommandGroupProxy final {
 public:
  SubCommandGroupProxy(internal::SubCommandGroup* group) : group_(group) {}
  template <typename SubCommandT>
  SubCommandProxy AddParser(SubCommandT&& cmd) {
    return group_->AddSubCommand(builder_internal::Build(&cmd));
  }

 private:
  internal::SubCommandGroup* group_;
};

class SubCommandGroup final
    : private builder_internal::SimpleBuilder<internal::SubCommandGroup> {
 public:
  SubCommandGroup() { this->SetObject(internal::SubCommandGroup::Create()); }
  SubCommandGroup& Title(std::string val) {
    this->GetObject()->SetTitle(std::move(val));
    return *this;
  }
  SubCommandGroup& Description(std::string val) {
    this->GetObject()->SetDescription(std::move(val));
    return *this;
  }
  SubCommandGroup& MetaVar(std::string val) {
    this->GetObject()->SetMetaVar(std::move(val));
    return *this;
  }
  SubCommandGroup& Help(std::string val) {
    this->GetObject()->SetHelpDoc(std::move(val));
    return *this;
  }
  // TODO: this should be stronge-typed.
  SubCommandGroup& Dest(builder_internal::Dest val) {
    this->GetObject()->SetDest(builder_internal::Build(&val));
    return *this;
  }

 private:
  friend class builder_internal::BuilderAccessor;
};

class ArgumentParser final
    : public builder_internal::SupportAddArgumentGroup<ArgumentParser> {
 public:
  ArgumentParser() = default;

  ArgumentParser& Description(std::string val) {
    controller_.SetOption(internal::ParserOptions::kDescription,
                          std::move(val));
    return *this;
  }
  ArgumentParser& ProgramVersion(std::string val) {
    controller_.SetOption(internal::ParserOptions::kProgramVersion,
                          std::move(val));
    return *this;
  }
  ArgumentParser& BugReportEmail(std::string val) {
    controller_.SetOption(internal::ParserOptions::kBugReportEmail,
                          std::move(val));
    return *this;
  }
  ArgumentParser& ProgramName(std::string& val) {
    controller_.SetOption(internal::ParserOptions::kProgramName,
                          std::move(val));
    return *this;
  }
  ArgumentParser& ProgramUsage(std::string& val) {
    controller_.SetOption(internal::ParserOptions::kProgramUsage,
                          std::move(val));
    return *this;
  }
  void ParseArgs(int argc, const char** argv) {
    ParseArgsImpl(internal::ArgArray(argc, argv), nullptr);
  }
  void ParseArgs(internal::ArgVector args) {
    ParseArgsImpl(internal::ArgArray(args), nullptr);
  }
  bool ParseKnownArgs(int argc, const char** argv,
                      std::vector<std::string>* out) {
    return ParseArgsImpl(internal::ArgArray(argc, argv), out);
  }
  bool ParseKnownArgs(internal::ArgVector args, std::vector<std::string>* out) {
    return ParseArgsImpl(internal::ArgArray(args), out);
  }
  template <typename SubCommandGroupT>
  SubCommandGroupProxy AddSubParsers(SubCommandGroupT&& group) {
    return AddSubCommandGroupImpl(builder_internal::Build(&group));
  }

 private:
  bool ParseArgsImpl(internal::ArgArray args, std::vector<std::string>* out) {
    return controller_.ParseKnownArgs(args, out);
  }
  void AddArgumentImpl(std::unique_ptr<internal::Argument> arg) {
    return controller_.AddArgument(std::move(arg));
  }
  internal::ArgumentGroup* AddArgumentGroupImpl(absl::string_view title) {
    return controller_.AddArgumentGroup(title);
  }
  internal::SubCommandGroup* AddSubCommandGroupImpl(
      std::unique_ptr<internal::SubCommandGroup> group) {
    return controller_.AddSubCommandGroup(std::move(group));
  }

  friend class SupportAddArgument<ArgumentParser>;
  friend class SupportAddArgumentGroup<ArgumentParser>;
  internal::ArgumentController controller_;
};

}  // namespace builder_internal
}  // namespace internal

using ArgumentParser = internal::builder_internal::ArgumentParser;

template <typename T>
internal::builder_internal::ArgumentBuilderProxy<T> Argument(
    absl::string_view name, T* dest) {
  return {name, dest};
}

template <typename T>
internal::builder_internal::ArgumentBuilderProxy<T> Argument(
    std::initializer_list<absl::string_view> names, T* dest) {
  return {names, dest};
}

#undef ARGPARSE_BUILDER_INTERNAL_COMMON

}  // namespace argparse
