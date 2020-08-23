#include "argparse/argparse.h"

namespace argparse {

CallbackRunner* CallbackResolverImpl::CreateCallbackRunner() {
  if (!dest_) {
    // everything is null.
    if (!custom_action_)  // The user don't give any action and dest, the type
                          // is discarded if given.
      return new DummyCallbackRunner();
    if (!custom_type_) {
      DCHECK(custom_action_);
      // The user migh just want to print something.
      custom_type_.reset(new NullTypeCallback());
    }
    return new CallbackRunnerImpl(std::move(custom_type_),
                                  std::move(custom_action_));
  }

  if (action_ == Actions::kNoAction)
    action_ = Actions::kStore;

  // The factory is used as a fall-back when user don't specify.
  auto* factory = dest_->CreateFactory(action_);
  // if we need the factory (action_ or type_ is null), but it is invalid.
  DCHECK2((custom_action_ && custom_type_) || factory,
          "The provided action is not supported by the type of dest");

  if (!custom_action_) {
    // user does not provide an action callback, we need to infer.
    custom_action_.reset(factory->CreateActionCallback());
  }

  // If there is a dest, send it to action anyway (may not be needed by the
  // action, but we generally don't know that.).
  custom_action_->SetDest(dest_.get());

  if (value_.has_value()) {
    // If user gave us a value, send it to the action.
    custom_action_->SetConstValue(std::move(value_));
  }

  if (!custom_type_) {
    custom_type_.reset(factory->CreateTypeCallback());
  }
  DCHECK2(custom_action_->WorksWith(dest_.get(), custom_type_.get()),
          "The provide dest, action and type are not compatible");
  return new CallbackRunnerImpl(std::move(custom_type_),
                                std::move(custom_action_));
}

void Names::InitOptions(std::initializer_list<const char*> names) {
  DCHECK2(names.size(), "At least one name must be provided");
  is_option = true;

  for (auto name : names) {
    std::size_t len = std::strlen(name);
    DCHECK2(IsValidOptionName(name, len), "Not a valid option name!");
    if (IsLongOptionName(name, len)) {
      // Strip leading '-' at most twice.
      for (int i = 0; *name == '-' && i < 2; ++i) {
        ++name;
        --len;
      }
      long_names.emplace_back(name, len);
    } else {
      short_names.push_back(name[1]);
    }
  }
  if (long_names.size())
    meta_var = ToUpper(long_names[0]);
  else
    meta_var = ToUpper({&short_names[0], 1});
}

// ArgumentImpl:
ArgumentImpl::ArgumentImpl(Delegate* delegate, const Names& names, int group)
    : callback_resolver_(new CallbackResolverImpl()),
      delegate_(delegate),
      group_(group) {
  InitNames(names);
  InitKey(names.is_option);
  delegate_->OnArgumentCreated(this);
}

void ArgumentImpl::CompileToArgpOptions(
    std::vector<argp_option>* options) const {
  argp_option opt{};
  opt.doc = doc();
  opt.group = group();
  opt.name = name();
  if (!is_option()) {
    // positional means none-zero in only doc and name, and flag should be
    // OPTION_DOC.
    opt.flags = OPTION_DOC;
    return options->push_back(opt);
  }
  opt.arg = arg();
  opt.key = key();
  options->push_back(opt);
  // TODO: handle alias correctly. Add all aliases.
  for (auto first = long_names_.begin() + 1, last = long_names_.end();
       first != last; ++first) {
    argp_option opt_alias;
    std::memcpy(&opt_alias, &opt, sizeof(argp_option));
    opt.name = first->c_str();
    opt.flags = OPTION_ALIAS;
    options->push_back(opt_alias);
  }
}

bool ArgumentImpl::CompareArguments(const ArgumentImpl* a,
                                    const ArgumentImpl* b) {
  // options go before positionals.
  if (a->is_option() != b->is_option())
    return int(a->is_option()) > int(b->is_option());

  // positional compares on their names.
  if (!a->is_option() && !b->is_option()) {
    DCHECK(a->name() && b->name());
    return std::strcmp(a->name(), b->name()) < 0;
  }

  // required option goes first.
  if (a->is_required() != b->is_required())
    return int(a->is_required()) > int(b->is_required());

  // short-only option goes before the rest.
  if (bool(a->name()) != bool(b->name()))
    return bool(a->name()) < bool(b->name());

  // a and b are both short-only option.
  if (!a->name() && !b->name())
    return a->key() < b->key();

  // a and b are both long option.
  DCHECK(a->name() && b->name());
  return std::strcmp(a->name(), b->name()) < 0;
}

void ArgumentImpl::FormatArgsDoc(std::ostream& os) const {
  if (!is_option()) {
    os << meta_var();
    return;
  }
  os << '[';
  std::size_t i = 0;
  const auto size = long_names_.size() + short_names_.size();

  for (; i < size; ++i) {
    if (i < long_names_.size()) {
      os << "--" << long_names_[i];
    } else {
      os << '-' << short_names_[i - long_names_.size()];
    }
    if (i < size - 1)
      os << '|';
  }

  if (!is_required())
    os << '[';
  os << '=' << meta_var();
  if (!is_required())
    os << ']';
  os << ']';
}

void ArgumentHolderImpl::CompileToArgpOptions(
    std::vector<argp_option>* options) {
  if (arguments_.empty())
    return options->push_back({});

  const unsigned option_size =
      arguments_.size() + groups_.size() + kAverageAliasCount + 1;
  options->reserve(option_size);

  for (const Group& group : groups_) {
    group.CompileToArgpOption(options);
  }

  // TODO: if there is not pos/opt at all but there are two groups, will argp
  // still print these empty groups?
  for (Argument& arg : arguments_) {
    arg.InitCallback();
    arg.CompileToArgpOptions(options);
  }
  // Only when at least one opt/pos presents should we generate their groups.
  options->push_back({});

  // PrintArgpOptionArray(*options);
}

void ArgumentHolderImpl::GenerateArgsDoc(std::string* args_doc) {
  std::vector<Argument*> args(arguments_.size());
  std::transform(arguments_.begin(), arguments_.end(), args.begin(),
                 [](ArgumentImpl& arg) { return &arg; });
  std::sort(args.begin(), args.end(),
            [](Argument* a, Argument* b) { return a->AppearsBefore(b); });

  // join the dump of each arg with a space.
  std::ostringstream os;
  for (std::size_t i = 0, size = args.size(); i < size; ++i) {
    args[i]->FormatArgsDoc(os);
    if (i != size - 1)
      os << ' ';
    args_doc->assign(os.str());
  }
}

ArgpParserImpl::ArgpParserImpl(ArgpParser::Delegate* delegate)
    : delegate_(delegate) {
  argp_.parser = &ArgpParserImpl::ArgpParserCallbackImpl;
  positional_count_ = delegate_->PositionalArgumentCount();
  delegate_->CompileToArgpOptions(&argp_options_);
  argp_.options = argp_options_.data();
  delegate_->GenerateArgsDoc(&args_doc_);
  argp_.args_doc = args_doc_.c_str();
}

void ArgpParserImpl::Init(const Options& options) {
  if (options.program_version)
    ::argp_program_version = options.program_version;
  if (options.program_version_callback)
    ::argp_program_version_hook = options.program_version_callback;
  if (options.bug_address)
    ::argp_program_bug_address = options.bug_address;
  if (options.help_filter) {
    argp_.help_filter = &ArgpHelpFilterCallbackImpl;
    help_filter_ = options.help_filter;
  }

  // TODO: may check domain?
  set_argp_domain(options.domain);
  AddFlags(options.flags);

  // Generate the program doc.
  if (options.description) {
    program_doc_ = options.description;
  }
  if (options.after_doc) {
    program_doc_.append({'\v'});
    program_doc_.append(options.after_doc);
  }

  if (!program_doc_.empty())
    set_doc(program_doc_.c_str());
}

error_t ArgpParserImpl::DoParse(int key, char* arg, ArgpState state) {
  // Positional argument.
  if (key == ARGP_KEY_ARG) {
    const int arg_num = state->arg_num;
    if (Argument* argument = delegate_->FindPositionalArgument(arg_num)) {
      InvokeUserCallback(argument, arg, state);
      return 0;
    }
    // Too many arguments.
    if (state->arg_num >= positional_count())
      state.ErrorF("Too many positional arguments. Expected %d, got %d",
                   (int)positional_count(), (int)state->arg_num);
    return ARGP_ERR_UNKNOWN;
  }

  // Next most frequent handling is options.
  if ((key & kSpecialKeyMask) == 0) {
    // This isn't a special key, but rather an option.
    Argument* argument = delegate_->FindOptionalArgument(key);
    if (!argument)
      return ARGP_ERR_UNKNOWN;
    InvokeUserCallback(argument, arg, state);
    return 0;
  }

  // No more commandline args, do some post-processing.
  if (key == ARGP_KEY_END) {
    // No enough args.
    if (state->arg_num < positional_count())
      state.ErrorF("No enough positional arguments. Expected %d, got %d",
                   (int)positional_count(), (int)state->arg_num);
  }

  // Remaining args (not parsed). Collect them or turn it into an error.
  if (key == ARGP_KEY_ARGS) {
    return 0;
  }

  return 0;
}

char* ArgpParserImpl::ArgpHelpFilterCallbackImpl(int key,
                                                 const char* text,
                                                 void* input) {
  if (!input || !text)
    return (char*)text;
  auto* self = reinterpret_cast<ArgpParserImpl*>(input);
  DCHECK2(self->help_filter_,
          "should only be called if user install help filter!");
  auto* arg = self->delegate_->FindOptionalArgument(key);
  DCHECK2(arg, "argp calls us with unknown key!");

  std::string repl(text);
  HelpFilterResult result = std::invoke(self->help_filter_, *arg, &repl);
  switch (result) {
    case HelpFilterResult::kKeep:
      return const_cast<char*>(text);
    case HelpFilterResult::kDrop:
      return nullptr;
    case HelpFilterResult::kReplace: {
      char* s = (char*)std::malloc(1 + repl.size());
      return std::strcpy(s, repl.c_str());
    }
  }
}
}  // namespace argparse
