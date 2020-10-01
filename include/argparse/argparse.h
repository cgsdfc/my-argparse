// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/argparse-traits.h"
#include "argparse/internal/argparse-internal.h"

// Holds public things
namespace argparse {

struct AnyValue {
  std::unique_ptr<internal::Any> value;
  template <typename T,
            std::enable_if_t<!std::is_convertible<T, AnyValue>{}>* = 0>
  AnyValue(T&& val)
      : value(internal::MakeAny<std::decay_t<T>>(std::forward<T>(val))) {}
};

struct TypeCallback {
  std::unique_ptr<internal::TypeCallback> cb;
  template <typename Callback,
            std::enable_if_t<!std::is_convertible<T, TypeCallback>{}>* = 0>
  TypeCallback(Callback&& cb)
      : cb(internal::MakeTypeCallback(std::forward<Callback>(cb))) {}
}

struct ActionCallback {
  std::unique_ptr<internal::ActionCallback> cb;
  template <typename Callback,
            std::enable_if_t<!std::is_convertible<T, ActionCallback>{}>* = 0>
  ActionCallback(Callback&& cb)
      : cb(internal::MakeActionCallback(std::forward<Callback>(cb))) {}
}

// Creator of DestInfo. For those that need a DestInfo, just take Dest
// as an arg.
struct Dest {
  std::unique_ptr<internal::DestInfo> info;
  template <typename T>
  Dest(T* ptr) : info(internal::DestInfo::CreateFromPtr(ptr)) {}
};

class FileType {
 public:
  explicit FileType(const char* mode) : mode_(internal::CharsToMode(mode)) {}
  explicit FileType(std::ios_base::openmode mode)
      : mode_(internal::StreamModeToMode(mode)) {}
  OpenMode mode() const { return mode_; }

 private:
  OpenMode mode_;
};

struct Names {
  std::unique_ptr<internal::NamesInfo> info;
  Names(const char* name) : Names(std::string(name)) { ARGPARSE_DCHECK(name); }
  Names(std::string name);
  Names(std::initializer_list<std::string> names);
};

// Public flags user can use. These are corresponding to the ARGP_XXX flags
// passed to argp_parse().
enum Flags {
  kNoFlags = 0,            // The default.
  kNoHelp = ARGP_NO_HELP,  // Don't produce --help.
  kLongOnly = ARGP_LONG_ONLY,
  kNoExit = ARGP_NO_EXIT,
};

// Options to ArgumentParser constructor.
// TODO: rename to OptionsBuilder and typedef.
struct Options {
  // Only the most common options are listed in this list.
  Options() : info(new OptionsInfo) {}
  Options& version(const char* v) {
    info->program_version = v;
    return *this;
  }
  Options& version(ProgramVersionCallback callback) {
    info->program_version_callback = callback;
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
      builder_->SetHelpDoc(help);
  }

  Argument& dest(Dest dest) {
    builder_->SetDest(std::move(dest.info));
    return *this;
  }
  Argument& action(const char* str) {
    builder_->SetActionString(str);
    return *this;
  }
  Argument& action(ActionCallback cb) {
    builder_->SetActionCallback(std::move(cb.cb));
    return *this;
  }
  Argument& type(TypeCallback cb) {
    builder_->SetTypeCallback(std::move(cb.cb));
    return *this;
  }
  template <typename T>
  Argument& type() {
    builder_->SetTypeOperations(internal::CreateOperations<T>());
    return *this;
  }
  Argument& type(FileType file_type) {
    builder_->SetTypeFileType(file_type.mode());
    return *this;
  }
  Argument& const_value(AnyValue val) {
    builder_->SetConstValue(std::move(val.value));
    return *this;
  }
  Argument& default_value(AnyValue val) {
    builder_->SetDefaultValue(std::move(val.value));
    return *this;
  }
  Argument& help(std::string val) {
    builder_->SetHelp(std::move(val));
    return *this;
  }
  Argument& required(bool val) {
    builder_->SetRequired(val);
    return *this;
  }
  Argument& meta_var(std::string val) {
    builder_->SetMetaVar(std::move(val));
    return *this;
  }
  Argument& nargs(int num) {
    builder_->SetNumArgsNumber(num);
    return *this;
  }
  Argument& nargs(char flag) {
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
  void add_argument(Argument& arg) {
    return add_argument(std::move(arg));
  }
  void add_argument(Argument&& arg) {
    return AddArgumentImpl(arg.Build());
  }
  virtual ~AddArgumentHelper() {}

 private:
  virtual void AddArgumentImpl(std::unique_ptr<Argument> arg) = 0;
};

class ArgumentGroup : public AddArgumentHelper {
 public:
  explicit ArgumentGroup(internal::ArgumentGroup* group) : group_(group) {}
  void add_argument(Argument arg) { group_->AddArgument(arg.Build()); }

 private:
  internal::ArgumentGroup* group_;
};

// If we can do add_argument_group(), add_argument() is always possible.
class AddArgumentGroupHelper : public AddArgumentHelper {
 public:
  ArgumentGroup add_argument_group(const char* header) {
    ARGPARSE_DCHECK(header);
    return ArgumentGroup(AddArgumentGroupImpl(header));
  }

 private:
  virtual ArgumentGroup* AddArgumentGroupImpl(const char* header) = 0;
};

class SubParser : public AddArgumentGroupHelper {
 public:
  explicit SubParser(SubCommand* sub) : sub_(sub) {}

 private:
  void AddArgumentImpl(std::unique_ptr<Argument> arg) override {
    return sub_->GetHolder()->AddArgument(std::move(arg));
  }
  ArgumentGroup* AddArgumentGroupImpl(const char* header) override {
    return sub_->GetHolder()->AddArgumentGroup(header);
  }
  SubCommand* sub_;
};

// Support add(parser("something").aliases({...}).help("..."))
class SubCommandBuilder {
 public:
  explicit SubCommandBuilder(std::string name)
      : cmd_(SubCommand::Create(std::move(name))) {}

  SubCommandBuilder& aliases(std::vector<std::string> als) {
    cmd_->SetAliases(std::move(als));
    return *this;
  }
  SubCommandBuilder& help(std::string val) {
    cmd_->SetHelpDoc(std::move(val));
    return *this;
  }

  std::unique_ptr<SubCommand> Build() {
    ARGPARSE_DCHECK(cmd_);
    return std::move(cmd_);
  }

 private:
  std::unique_ptr<SubCommand> cmd_;
};

class SubParserGroup {
 public:
  explicit SubParserGroup(SubCommandGroup* group) : group_(group) {}

  // Positional.
  SubParser add_parser(std::string name,
                       std::string help = {},
                       std::vector<std::string> aliases = {}) {
    SubCommandBuilder builder(std::move(name));
    builder.help(std::move(help)).aliases(std::move(aliases));
    return add_parser(builder.Build());
  }

  // Builder pattern.
  SubParser add_parser(std::unique_ptr<SubCommand> cmd) {
    auto* cmd_ptr = group_->AddSubCommand(std::move(cmd));
    return SubParser(cmd_ptr);
  }

 private:
  SubCommandGroup* group_;
};

// Support add(subparsers(...))
class SubParsersBuilder {
 public:
  explicit SubParsersBuilder() : group_(SubCommandGroup::Create()) {}

  SubParsersBuilder& title(std::string val) {
    group_->SetTitle(std::move(val));
    return *this;
  }

  SubParsersBuilder& description(std::string val) {
    group_->SetDescription(std::move(val));
    return *this;
  }

  SubParsersBuilder& meta_var(std::string val) {
    group_->SetMetaVar(std::move(val));
    return *this;
  }

  SubParsersBuilder& help(std::string val) {
    group_->SetHelpDoc(std::move(val));
    return *this;
  }

  SubParsersBuilder& dest(Dest val) {
    group_->SetDest(std::move(val.info));
    return *this;
  }

  std::unique_ptr<SubCommandGroup> Build() { return std::move(group_); }

 private:
  std::unique_ptr<SubCommandGroup> group_;
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

  SubParserGroup add_subparsers(std::unique_ptr<SubCommandGroup> group) {
    return SubParserGroup(AddSubParsersImpl(std::move(group)));
  }
  // TODO: More precise signature.
  SubParserGroup add_subparsers(Dest dest, std::string help = {}) {
    SubParsersBuilder builder;
    builder.dest(std::move(dest)).help(std::move(help));
    return add_subparsers(builder.Build());
  }

 private:
  virtual bool ParseArgsImpl(ArgArray args, std::vector<std::string>* out) = 0;
  virtual SubCommandGroup* AddSubParsersImpl(
      std::unique_ptr<SubCommandGroup> group) = 0;
};

class ArgumentParser : public MainParserHelper {
 public:
  ArgumentParser() : controller_(ArgumentController::Create()) {}

  explicit ArgumentParser(Options options) : ArgumentParser() {
    if (options.info)
      controller_->SetOptions(std::move(options.info));
  }

 private:
  bool ParseArgsImpl(ArgArray args, std::vector<std::string>* out) override {
    ARGPARSE_DCHECK(out);
    return controller_->GetParser()->ParseKnownArgs(args, out);
  }
  void AddArgumentImpl(std::unique_ptr<Argument> arg) override {
    return controller_->GetMainHolder()->AddArgument(std::move(arg));
  }
  ArgumentGroup* AddArgumentGroupImpl(const char* header) override {
    return controller_->GetMainHolder()->AddArgumentGroup(header);
  }
  SubCommandGroup* AddSubParsersImpl(
      std::unique_ptr<SubCommandGroup> group) override {
    return controller_->GetSubCommandHolder()->AddSubCommandGroup(
        std::move(group));
  }

  std::unique_ptr<ArgumentController> controller_;
};

}  // namespace argparse
