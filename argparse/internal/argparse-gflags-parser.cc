// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-gflags-parser.h"

#include "argparse/internal/argparse-argument-container.h"

namespace argparse {
namespace internal {
namespace gflags_parser_internal {

using GflagsTypeList = TypeList<bool, gflags::int32, gflags::int64,
                                gflags::uint64, double, std::string>;

constexpr char kGflagParserName[] = "gflags-parser";

inline const char* GetGflagsSupportedTypeAsString() {
  // Hand-rolled one is better than computed one.
  return "bool, int32, int64, uint64, double, std::string";
}

static bool IsValidNamesInfo(NamesInfo* info) {
  return info->GetNameCount() == 1;
}

// std::false_type Or(std::initializer_list<std::false_type>);
// std::true_type Or(std::initializer_list<bool>);

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

    (iter->second)(arg);
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
