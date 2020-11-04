// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-gflags-parser.h"

#include "argparse/internal/argparse-argument-container.h"

namespace argparse {
namespace internal {
namespace gflags_parser_internal {

namespace {

using GflagsTypeList = internal::TypeList<bool, gflags::int32, gflags::int64,
                                          gflags::uint64, double, std::string>;

bool IsValidNamesInfo(NamesInfo* info) { return info->GetNameCount() == 1; }

absl::string_view StripPrefixChars(absl::string_view str) {
  auto i = str.find_first_not_of(NamesInfo::kOptionalPrefixChar);
  ARGPARSE_INTERNAL_DCHECK(i != absl::string_view::npos, "");
  return str.substr(i);
}

RegisterParams CreateRegisterParams(Argument* arg) {
  RegisterParams params;
  params.name = StripPrefixChars(arg->GetNames()->GetOptionalName()).data();
  params.help = arg->GetHelpDoc().data();
  params.filename = "";
  params.current_value = arg->GetDest()->GetDestPtr();
  params.default_value = const_cast<Any*>(arg->GetDefaultValue());
  return params;
}

template <typename FlagType>
void RegisterGlagsArgument(const RegisterParams& params) {
  gflags::FlagRegisterer(params.name, params.help, params.filename,
                         params.current_value.Cast<FlagType>(),
                         internal::AnyCast<FlagType>(params.default_value));
}

template <typename... Types>
bool IsGflagsSupportedTypeImpl(std::type_index type, TypeList<Types...>) {
  static const std::type_index kValidTypes[] = {typeid(Types)...};
  return std::find(std::begin(kValidTypes), std::end(kValidTypes), type) !=
         std::end(kValidTypes);
}

bool IsGflagsSupportedType(std::type_index type) {
  return IsGflagsSupportedTypeImpl(type, GflagsTypeList{});
}

template <typename... Types>
GflagsRegisterMap CreateRegisterMap(TypeList<Types...>) {
  return GflagsRegisterMap{{typeid(Types), &RegisterGlagsArgument<Types>}...};
}

}  // namespace

GflagsParser::GflagsParser()
    : register_map_(CreateRegisterMap(GflagsTypeList{})) {}

bool GflagsParser::ParseKnownArgs(ArgArray args,
                                  std::vector<std::string>* unparsed_args) {
  int argc = args.GetArgc();
  char** argv = args.GetArgv();

  auto rv = gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (unparsed_args) {
    for (int i = 0; i < argc; ++i) unparsed_args->push_back(argv[i]);
    return rv == 0;
  }
  return true;
}

void GflagsParser::Initialize(ArgumentContainer* container) {
  auto* main_holder = container->GetMainHolder();

  if (main_holder->GetDefaultGroup(ArgumentGroup::kPositionalGroupIndex)
          ->GetArgumentCount()) {
    ARGPARSE_INTERNAL_LOG(WARNING, "Positional arguments are not supported");
  }

  auto* group =
      main_holder->GetDefaultGroup(ArgumentGroup::kOptionalGroupIndex);
  if (!group->GetArgumentCount()) return;  // No Argument at all.

  for (std::size_t i = 0; i < group->GetArgumentCount(); ++i) {
    auto* arg = group->GetArgument(i);
    ARGPARSE_INTERNAL_DCHECK(arg->IsOptional(), "Should all be optional");

    if (!IsValidNamesInfo(arg->GetNames())) {
      ARGPARSE_INTERNAL_LOG(WARNING, "Aliases are not supported");
      continue;
    }

    ARGPARSE_INTERNAL_DCHECK(arg->GetDefaultValue(),
                             "Default value must be set");

    auto dest_type = arg->GetDest()->GetType();
    if (!IsGflagsSupportedType(dest_type)) {
      ARGPARSE_INTERNAL_LOG(WARNING,
                            "DestType of this argument is not supported");
      continue;
    }

    auto iter = register_map_.find(dest_type);
    ARGPARSE_INTERNAL_DCHECK(iter != register_map_.end(), "");
    auto params = CreateRegisterParams(arg);
    (iter->second)(params);
  }
}

GflagsParser::~GflagsParser() { gflags::ShutDownCommandLineFlags(); }

}  // namespace gflags_parser_internal

// TODO: statie registration.
std::unique_ptr<ArgumentParser> ArgumentParser::CreateDefault() {
  return absl::make_unique<gflags_parser_internal::GflagsParser>();
}

}  // namespace internal
}  // namespace argparse
