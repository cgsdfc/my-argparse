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
  return info->IsOptional() && info->GetNameCount() == 1;
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

GflagsArgument::GflagsArgument(Argument* arg) {
  ARGPARSE_CHECK_F(IsValidNamesInfo(arg->GetNamesInfo()),
                   "%s only accept optional argument without alias",
                   kGflagParserName);
  ARGPARSE_CHECK_F(IsGflagsSupportedType(arg->GetDest()->GetType()),
                   "Not a gflags-supported type. Supported types are:\n%s",
                   GetGflagsSupportedTypeAsString());
  // name_ = arg->GetNamesInfo()->GetName().data();
  help_ = arg->GetHelpDoc().data();
  // ARGPARSE_DCHECK(arg->GetConstValue());
  // filename_ = AnyCast<absl::string_view>(arg->GetConstValue())->data();
  filename_ = "";
  dest_ptr_ = arg->GetDest()->GetDestPtr();
  default_value_ = const_cast<Any*>(arg->GetDefaultValue());
  ARGPARSE_DCHECK(default_value_);
}

GflagsParser::GflagsParser()
    : register_map_(CreateRegisterMap(GflagsTypeList{})) {}

bool GflagsParser::ParseKnownArgs(ArgArray args,
                                  std::vector<std::string>* unparsed_args) {
  int argc = args.argc();
  char** argv = args.argv();
  auto rv = gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (unparsed_args) {
    for (int i = 0; i < argc; ++i) unparsed_args->push_back(argv[i]);
    return rv == 0;
  }
  return true;
}

void GflagsParser::Initialize(ArgumentContainer* container) {
  auto* main_holder = container->GetMainHolder();
  // Only default optional group is valid.
  // if (main_holder->GetDefaultGroup(ArgumentGroup::kPositionalGroupIndex)
  //         ->GetArgumentCount())
  //   return false;  // Positional arg not supported.

  auto* group =
      main_holder->GetDefaultGroup(ArgumentGroup::kOptionalGroupIndex);
  if (!group->GetArgumentCount()) return;  // No Argument at all.

  for (std::size_t i = 0; i < group->GetArgumentCount(); ++i) {
    auto* arg = group->GetArgument(i);
    if (arg->GetNamesInfo()->IsPositional()) continue;

    auto dest_type = arg->GetDest()->GetType();
    // TODO: may give a warning instead..
    ARGPARSE_CHECK_F(IsGflagsSupportedType(dest_type),
                     "type '%s' is not supported by gflags",
                     arg->GetDest()->GetOperations()->GetTypeName().data());

    auto iter = register_map_.find(dest_type);
    ARGPARSE_DCHECK(iter != register_map_.end());
    // TODO: this should allow further checking.
    GflagsArgument gflags_arg{arg};
    (iter->second)(&gflags_arg);
  }
}

GflagsParser::~GflagsParser() { gflags::ShutDownCommandLineFlags(); }

}  // namespace gflags_parser_internal

std::unique_ptr<ArgumentParser> ArgumentParser::CreateDefault() {
  return absl::make_unique<gflags_parser_internal::GflagsParser>();
}


}  // namespace internal
}  // namespace argparse
