#pragma once

namespace argparse {

class Argument;
class ArgumentGroup;
class AddArgumentHelper;

class CallbackRunner {
 public:
  // Communicate with outside when callback is fired.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual bool GetValue(std::string* out) = 0;
    virtual void OnCallbackError(const std::string& errmsg) = 0;
    virtual void OnPrintUsage() = 0;
    virtual void OnPrintHelp() = 0;
  };
  // Before the callback is run, allow default value to be set.
  virtual void InitCallback() {}
  virtual void RunCallback(std::unique_ptr<Delegate> delegate) = 0;
  virtual ~CallbackRunner() {}
};

// Return value of help filter function.
enum class HelpFilterResult {
  kKeep,
  kDrop,
  kReplace,
};

using HelpFilterCallback =
    std::function<HelpFilterResult(const Argument&, std::string* text)>;

// XXX: this depends on argp and is not general.
// In fact, people only need to pass in a std::string.
using ProgramVersionCallback = void (*)(std::FILE*, argp_state*);

////////////////////////////////////////
// End of interfaces. Begin of Impls. //
////////////////////////////////////////

ActionKind StringToActions(const std::string& str);

class FileType {
 public:
  explicit FileType(const char* mode) : mode_(CharsToMode(mode)) {}
  explicit FileType(std::ios_base::openmode mode)
      : mode_(StreamModeToMode(mode)) {}
  OpenMode mode() const { return mode_; }

 private:
  OpenMode mode_;
};

struct Names {
  std::unique_ptr<NamesInfo> info;
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
  Options& help_filter(HelpFilterCallback cb) {
    info->help_filter = std::move(cb);
    return *this;
  }

  std::unique_ptr<OptionsInfo> info;
};

// Creator of DestInfo. For those that need a DestInfo, just take Dest
// as an arg.
struct Dest {
  std::unique_ptr<DestInfo> info;
  template <typename T>
  Dest(T* ptr) : info(DestInfo::CreateFromPtr(ptr)) {}
};

class ArgumentBuilder {
 public:
  explicit ArgumentBuilder(Names names) : factory_(ArgumentFactory::Create()) {
    ARGPARSE_DCHECK(names.info);
    factory_->SetNames(std::move(names.info));
  }

  // TODO: Fix the typeinfo/actioninfo deduction.
  ArgumentBuilder& dest(Dest dest) {
    factory_->SetDest(std::move(dest.info));
    return *this;
  }
  ArgumentBuilder& action(const char* str) {
    factory_->SetActionString(str);
    return *this;
  }
  template <typename Callback>
  ArgumentBuilder& action(Callback&& cb) {
    factory_->SetActionCallback(MakeActionCallback(std::forward<Callback>(cb)));
    return *this;
  }
  template <typename Callback>
  ArgumentBuilder& type(Callback&& cb) {
    factory_->SetTypeCallback(MakeTypeCallback(std::forward<Callback>(cb)));
    return *this;
  }
  template <typename T>
  ArgumentBuilder& type() {
    factory_->SetTypeOperations(CreateOperations<T>());
    return *this;
  }
  ArgumentBuilder& type(FileType file_type) {
    factory_->SetTypeFileType(file_type.mode());
    return *this;
  }
  template <typename T>
  ArgumentBuilder& const_value(T&& val) {
    using Type = std::decay_t<T>;
    factory_->SetConstValue(MakeAny<Type>(std::forward<T>(val)));
    return *this;
  }
  template <typename T>
  ArgumentBuilder& default_value(T&& val) {
    using Type = std::decay_t<T>;
    factory_->SetDefaultValue(MakeAny<Type>(std::forward<T>(val)));
    return *this;
  }
  ArgumentBuilder& help(std::string val) {
    factory_->SetHelp(std::move(val));
    return *this;
  }
  ArgumentBuilder& required(bool val) {
    factory_->SetRequired(val);
    return *this;
  }
  ArgumentBuilder& meta_var(std::string val) {
    factory_->SetMetaVar(std::move(val));
    return *this;
  }
  ArgumentBuilder& nargs(int num) {
    factory_->SetNumArgsNumber(num);
    return *this;
  }
  ArgumentBuilder& nargs(char flag) {
    factory_->SetNumArgsFlag(flag);
    return *this;
  }

  std::unique_ptr<Argument> Build() { return factory_->CreateArgument(); }

 private:
  std::unique_ptr<ArgumentFactory> factory_;
};

// This is a helper that provides add_argument().
class AddArgumentHelper {
 public:
  // add_argument(ArgumentBuilder(...).Build());
  void add_argument(std::unique_ptr<Argument> arg) {
    return AddArgumentImpl(std::move(arg));
  }
  virtual ~AddArgumentHelper() {}

 private:
  virtual void AddArgumentImpl(std::unique_ptr<Argument> arg) {}
};

class argument_group : public AddArgumentHelper {
 public:
  explicit argument_group(ArgumentGroup* group) : group_(group) {}

 private:
  void AddArgumentImpl(std::unique_ptr<Argument> arg) override {
    return group_->AddArgument(std::move(arg));
  }

  ArgumentGroup* group_;
};

// If we can do add_argument_group(), add_argument() is always possible.
class AddArgumentGroupHelper : public AddArgumentHelper {
 public:
  argument_group add_argument_group(const char* header) {
    ARGPARSE_DCHECK(header);
    return argument_group(AddArgumentGroupImpl(header));
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

class SubParserGroup;

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

class MainParserHelper;

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
