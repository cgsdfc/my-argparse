// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-argp-parser.h"

namespace argparse {

GNUArgpParser::Context::Context(const Argument* argument,
                                const char* value,
                                argp_state* state)
    : has_value_(bool(value)), arg_(argument), state_(state) {
  if (has_value_)
    this->value_.assign(value);
}

error_t GNUArgpParser::DoParse(int key, char* arg, argp_state* state) {
  // Positional argument.
  // if (key == ARGP_KEY_ARG) {
  //   const int arg_num = state->arg_num;
  //   if (Argument* argument = context_->FindPositionalArgument(arg_num)) {
  //     RunCallback(argument, arg, state);
  //     return 0;
  //   }
  //   // Too many arguments.
  //   if (state->arg_num >= positional_count())
  //     state.ErrorF("Too many positional arguments. Expected %d, got %d",
  //                  (int)positional_count(), (int)state->arg_num);
  //   return ARGP_ERR_UNKNOWN;
  // }

  // // Next most frequent handling is options.
  // if ((key & kSpecialKeyMask) == 0) {
  //   // This isn't a special key, but rather an option.
  //   Argument* argument = delegate_->FindOptionalArgument(key);
  //   if (!argument)
  //     return ARGP_ERR_UNKNOWN;
  //   RunCallback(argument, arg, state);
  //   return 0;
  // }

  // // No more commandline args, do some post-processing.
  // if (key == ARGP_KEY_END) {
  //   // No enough args.
  //   if (state->arg_num < positional_count())
  //     state.ErrorF("Not enough positional arguments. Expected %d, got %d",
  //                  (int)positional_count(), (int)state->arg_num);
  // }

  // // Remaining args (not parsed). Collect them or turn it into an error.
  // if (key == ARGP_KEY_ARGS) {
  //   return 0;
  // }

  // return 0;
}

void ArgpCompiler::CompileGroup(ArgumentGroup* group,
                                std::vector<argp_option>* out) {
  if (!group->GetArgumentCount())
    return;
  argp_option opt{};
  opt.group = group_to_id_[group];
  opt.doc = group->GetHeader().data();
  out->push_back(opt);
}

void ArgpCompiler::CompileArgument(Argument* arg,
                                   std::vector<argp_option>* out) {
  // argp_option opt{};
  // opt.doc = arg->GetHelpDoc();
  // opt.group = FindGroup(arg->GetGroup());
  // // opt.name = name();

  // if (!arg->IsOption()) {
  //   // positional means none-zero in only doc and name, and flag should be
  //   // OPTION_DOC.
  //   opt.flags = OPTION_DOC;
  //   return out->push_back(opt);
  // }

  // // opt.arg = arg();
  // opt.key = FindArgument(arg);
  // out->push_back(opt);

  // // TODO: handle alias correctly. Add all aliases.
  // auto* info = arg->GetNamesInfo();
  // for (auto first = info->long_names.begin() + 1, last =
  // info->long_names.end();
  //      first != last; ++first) {
  //   argp_option opt_alias;
  //   std::memcpy(&opt_alias, &opt, sizeof(argp_option));
  //   opt.name = first->c_str();
  //   opt.flags = OPTION_ALIAS;
  //   out->push_back(opt_alias);
  // }
}

void ArgpCompiler::CompileOptions(std::vector<argp_option>* out) {
  holder_->ForEachGroup(
      [this, out](ArgumentGroup* g) { return CompileGroup(g, out); });
  holder_->ForEachArgument(
      [this, out](Argument* arg) { return CompileArgument(arg, out); });
  out->push_back({});
}

void ArgpCompiler::InitArgument(Argument* arg) {
  // const auto& short_names = arg->GetNamesInfo()->short_names;
  // int key = short_names.empty() ? next_arg_key_++ : short_names[0];
  // argument_to_id_[arg] = key;
}

void ArgpCompiler::InitGroup(ArgumentGroup* group) {
  // There is no need to allocate special ids for default groups.
  // 1. argp sorts groups in id order.
  // 2. groups are in their added order.
  // 3. Holder always adds default groups at first.
  // 4. So default group is always presented at first.
  group_to_id_[group] = next_group_id_++;
}

void ArgpCompiler::Initialize() {
  // Gen keys for args.
  holder_->ForEachArgument([this](Argument* arg) { return InitArgument(arg); });
  // Gen ids for groups.
  holder_->ForEachGroup([this](ArgumentGroup* group) {
    // TODO: pos/opt default group..
    return InitGroup(group);
  });
}

void ArgpCompiler::CompileUsageFor(Argument* arg, std::ostream& os) {
  // if (!arg->IsOption()) {
  //   os << arg->GetNamesInfo()->meta_var;
  //   return;
  // }
  // os << '[';
  // std::size_t i = 0;
  // const auto& long_names = arg->GetNamesInfo()->long_names;
  // const auto& short_names = arg->GetNamesInfo()->short_names;
  // const auto size = long_names.size() + short_names.size();

  // for (; i < size; ++i) {
  //   if (i < long_names.size()) {
  //     os << "--" << long_names[i];
  //   } else {
  //     os << '-' << short_names[i - long_names.size()];
  //   }
  //   if (i < size - 1)
  //     os << '|';
  // }

  // if (!arg->IsRequired())
  //   os << '[';
  // os << '=' << arg->GetNamesInfo()->meta_var;
  // if (!arg->IsRequired())
  //   os << ']';
  // os << ']';
}

void ArgpCompiler::CompileUsage(std::string* out) {
  std::vector<Argument*> args;
  holder_->SortArguments(&args);

  // join the dump of each arg with a space.
  std::ostringstream os;
  for (std::size_t i = 0, size = args.size(); i < size; ++i) {
    CompileUsageFor(args[i], os);
    if (i != size - 1)
      os << ' ';
  }

  *out = os.str();
}

void ArgpCompiler::CompileArgumentIndexes(ArgpIndexesInfo* out) {
  holder_->ForEachArgument([this, out](Argument* arg) {
    if (arg->IsOption()) {
      out->optionals.emplace(argument_to_id_[arg], arg);
    } else {
      out->positionals.push_back(arg);
    }
  });
}

}  // namespace argparse
