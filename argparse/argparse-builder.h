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
};

// Call this function on a builder object to obtain the built object.
template <typename Builder>
auto Build(Builder* b) -> decltype(BuilderAccessor::Build(b)) {
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

 private:
  friend class BuilderAccessor;
};

class NameOrNames final : private SimpleBuilder<internal::NamesInfo> {
 public:
  NameOrNames(const char* name) {
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

template <typename Derived>
class MethodsBase {
 protected:
  template <typename T>
  using BuilderMethod = void (internal::ArgumentBuilder::*) (T);

  template <typename T>
  Derived& Invoke(BuilderMethod<T> method, T value) {
    builder()->*method(std::move(value));
    return derived_this();
  }

 private:
  Derived& derived_this() { return static_cast<Derived&>(*this); }
  internal::ArgumentBuilder* builder() { return derived_this().GetBuilder(); }
};

template <typename Derived>
class NonTypeMethodsBase : public MethodsBase<Derived> {
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
  Derived& FlagOrNumber(FlagOrNumber num_args) {
    return Invoke(&ArgumentBuilder::SetNumArgs,
                  builder_internal::Build(&num_args));
  }

 private:
  using MethodsBase<Derived>::Invoke;
};

// template <typename T, typename Derived>
// class ActionMethodsBase : public MethodsBase<Derived> {
// public:
//   Derived& Action(ActionCallback<T>&& func) {
//     builder()->SetActionInfo(
//         internal::ActionInfo::CreateCallbackAction(std::move(func)));
//     return derived_this();
//   }
//   Derived& Action(const char* str) {
//     builder()->SetActionString(str);
//     return derived_this();
//   }
//   private:
// };

template <typename T, typename Derived>
class DestTypeMethods {
 public:
  Derived& ConstValue(T&& value) {
    builder()->SetConstValue(internal::MakeAny<T>(std::move(value)));
    return derived_this();
  }
  Derived& DefaultValue(T&& value) {
    builder()->SetDefaultValue(internal::MakeAny<T>(std::move(value)));
    return derived_this();
  }
  Derived& Action(ActionCallback<T>&& func) {
    builder()->SetActionInfo(
        internal::ActionInfo::CreateCallbackAction(std::move(func)));
    return derived_this();
  }
  Derived& Action(const char* str) {
    builder()->SetActionString(str);
    return derived_this();
  }
  Derived& Type(TypeCallback<T>&& func) {
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
  Derived& ValueTypeConst(ValueType&& value) {
    builder()->SetConstValue(internal::MakeAny<ValueType>(std::move(value)));
    return derived_this();
  }
  Derived& ValueTypeCallback(TypeCallback<ValueType>&& func) {
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

template <typename T, typename Derived, bool = internal::IsOpenDefined<T>{}>
class FileTypeMethods {
 public:
  Derived& FileType(absl::string_view mode) {
    builder()->SetTypeFileType(mode);
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
class ArgumentBuilderProxy final
    : public NonTypeMethodsBase<ArgumentBuilderProxy<T>>,
      public DestTypeMethods<T, ArgumentBuilderProxy<T>>,
      public ValueTypeMethods<T, ArgumentBuilderProxy<T>>,
      public FileTypeMethods<T, ArgumentBuilderProxy<T>> {
 public:
  ArgumentBuilderProxy(NameOrNames names, T* ptr)
      : builder_(internal::ArgumentBuilder::Create()) {
    GetBuilder()->SetDest(internal::DestInfo::CreateFromPtr(ptr));
    GetBuilder()->SetNames(builder_internal::Build(&names));
  }

 private:
  friend class NonTypeMethodsBase<ArgumentBuilderProxy<T>>;
  friend class DestTypeMethods<T, ArgumentBuilderProxy<T>>;
  friend class ValueTypeMethods<T, ArgumentBuilderProxy<T>>;
  friend class FileTypeMethods<T, ArgumentBuilderProxy<T>>;
  friend class BuilderAccessor;

  // For being a Builder.
  std::unique_ptr<internal::Argument> Build() { return GetBuilder()->Build(); }
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
  Derived& AddArgument(NameOrNames names, T* dest,
                       absl::string_view help = {}) {
    ArgumentBuilderProxy<T> builder(std::move(names), dest, help);
    return AddArgument(builder);
  }
  // Use it with Argument().
  template <typename T>
  Derived& AddArgument(T&& arg) {
    auto* self = static_cast<Derived*>(this);
    self->AddArgumentImpl(Build(&arg));
    return *self;
  }
};

// ArgumentGroup: a group of arguments that share the same title.
class ArgumentGroup final : public SupportAddArgument<ArgumentGroup> {
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
  internal::ArgumentGroup* AddArgumentGroupImpl(std::string title) {
    return sub_->GetHolder()->AddArgumentGroup(std::move(title));
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
  internal::ArgumentGroup* AddArgumentGroupImpl(std::string title) {
    return controller_.AddArgumentGroup(std::move(title));
  }
  internal::SubCommandGroup* AddSubCommandGroupImpl(
      std::unique_ptr<internal::SubCommandGroup> group) {
    return controller_.AddSubCommandGroup(std::move(group));
  }

  friend class SupportAddArgument<ArgumentParser>;
  friend class SupportAddArgumentGroup<ArgumentParser>;
  internal::ArgumentController controller_;
};

template < typename T>
ArgumentBuilderProxy<T> Argument(NameOrNames names, T* dest) {
  return {std::move(names), dest};
}

}  // namespace builder_internal
}  // namespace internal

using internal::builder_internal::Argument;
using internal::builder_internal::ArgumentParser;

}  // namespace argparse
