// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-argp-parser.h"

#include <cstring>

#include "argparse/internal/argparse-argument-container.h"

namespace argparse {
namespace internal {
namespace argp_parser_internal {

namespace {

// I'm afraid there is some tricky magic about initializing a C struct to 0 in
// C++. So this abstraction should work for all cases.
constexpr struct argp_option EmptyOption() { return {0}; }

}  // namespace

ArgpParser::ArgpParser() {
  memset(&parser_, 0, sizeof(parser_));
  parser_.parser = [](int key, char* arg, struct argp_state* state) {
    auto* self = reinterpret_cast<ArgpParser*>(state->input);
    return self->Parse(key, arg, state);
  };
}

// Clear these staled pointers.
ArgpParser::~ArgpParser() {
  argp_program_version = nullptr;
  argp_program_bug_address = nullptr;
}

void ArgpParser::AppendGroupOption(ArgumentGroup* group) {
  auto option = EmptyOption();
  option.group = next_group_id_++;
  option.doc = group->GetTitle().data();
  options_.push_back(option);
}

void ArgpParser::AppendPositionalArgument(Argument* arg) {
  auto option = EmptyOption();
  option.doc = arg->GetHelpDoc().data();
  option.name = arg->GetNames()->GetPositionalName().data();
  option.flags = OPTION_DOC;
  options_.push_back(option);
}

void ArgpParser::AppendOptionalArgument(Argument* arg) {
  auto option = EmptyOption();
  auto option_key = next_option_id_++;
  optional_args_.insert({option_key, arg});

  option.arg = arg->GetMetaVar().data();
  option.doc = arg->GetHelpDoc().data();
  option.key = option_key;
  option.name =
      NamesInfo::StripPrefixChars(arg->GetNames()->GetOptionalName()).data();
  options_.push_back(option);
}

void ArgpParser::AppendArgument(Argument* arg) {
  return arg->IsOptional() ? AppendOptionalArgument(arg)
                           : AppendPositionalArgument(arg);
}

void ArgpParser::Initialize(ArgumentContainer* container) {
  auto* main_holder = container->GetMainHolder();
  auto total_count = main_holder->GetTotalArgumentCount() +
                     main_holder->GetArgumentGroupCount();
  options_.reserve(total_count);

  for (size_t i = 0; i < main_holder->GetArgumentGroupCount(); ++i) {
    auto* group = main_holder->GetArgumentGroup(i);
    if (!group->GetArgumentCount()) continue;
    AppendGroupOption(group);
    for (size_t j = 0; j < group->GetArgumentCount();++j) {
      auto* arg = group->GetArgument(j);
      AppendArgument(arg);
    }
  }

  options_.push_back(EmptyOption());
  parser_.options = options_.data();
}

void ArgpParser::SetOption(ParserOptions key, absl::string_view value) {
  switch (key) {
    case ParserOptions::kBugReportEmail:
      SetBugReportEmail(value);
      break;
    case ParserOptions::kProgramVersion:
      SetProgramVersion(value);
      break;
    case ParserOptions::kDescription:
      SetDescription(value);
    default:
      break;
  }
}

void ArgpParser::SetBugReportEmail(absl::string_view value) {
  bug_address_ = static_cast<std::string>(value);
  argp_program_bug_address = bug_address_.data();
}

void ArgpParser::SetProgramVersion(absl::string_view value) {
  // Make sure these two variable syn'ed.
  program_version_ = static_cast<std::string>(value);
  argp_program_version = program_version_.data();
}

void ArgpParser::SetDescription(absl::string_view value) {
  description_ = static_cast<std::string>(value);
}

bool ArgpParser::ParseKnownArgs(ArgArray args, std::vector<std::string>* out) {
  auto rv =
      argp_parse(&parser_, args.GetArgc(), args.GetArgv(), 0, nullptr, this);
  return !!rv;
}

error_t ArgpParser::Parse(int key, char* arg, struct argp_state* state) {
  return 0;
}

}  // namespace argp_parser_internal

std::unique_ptr<ArgumentParser> ArgumentParser::CreateDefault() {
  return absl::make_unique<argp_parser_internal::ArgpParser>();
}

}  // namespace internal
}  // namespace argparse
