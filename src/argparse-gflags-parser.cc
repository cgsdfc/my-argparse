// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-gflags-parser.h"

#define ARGPARSE_UNSUPPORTED_METHOD(object) \
  ARGPARSE_CHECK_F(false, "%s is not supported by %s", __func__, (object))

namespace argparse {
namespace internal {

using GflagsTypeList = TypeList<bool, gflags::int32, gflags::int64,
                                gflags::uint64, double, std::string>;

template <typename... Types>
bool IsGflagsSupportedTypeImpl(std::type_index type, TypeList<Types...>) {
  return ((type == typeid(Types)) || ...);
}

bool IsGflagsSupportedType(std::type_index type) {
  return IsGflagsSupportedTypeImpl(type, GflagsTypeList{});
}

template <typename T, typename... Rest>
void JoinTypeNamesImpl(const char* sep, std::ostream& os,
                       TypeList<T, Rest...>) {
  os << TypeName<T>();
  if (sizeof...(Rest)) os << sep;
  JoinTypeNamesImpl(sep, os, TypeList<Rest...>{});
}
void JoinTypeNamesImpl(const char* sep, std::ostream& os, TypeList<>) {}

template <typename... Types>
std::string JoinTypeNames(const char* sep, TypeList<Types...> type_list) {
  std::ostringstream os;
  JoinTypeNamesImpl(sep, os, type_list);
  return os.str();
}

const char* GetGflagsSupportedTypeAsString() {
  // Hand-rolled one is better than computed one.
  return "bool, int32, int64, uint64, double, std::string";
}

constexpr char kGflagParserName[] = "gflags-parser";

inline StringView StripLeading(StringView sv, char ch) {
  auto str = sv.data();
  auto size = sv.size();
  for (; size != 0 && *str == ch; ++str, --size)
    ;
  return StringView(str, size);
}

class GflagsArgument {
 public:
  explicit GflagsArgument(Argument* arg) {
    ARGPARSE_CHECK_F(IsValidNamesInfo(arg->GetNamesInfo()),
                     "%s only accept optional argument without alias",
                     kGflagParserName);
    ARGPARSE_CHECK_F(IsGflagsSupportedType(arg->GetDest()->GetType()),
                     "Not a gflags-supported type. Supported types are:\n%s",
                     GetGflagsSupportedTypeAsString());
    name_ = arg->GetNamesInfo()->GetName().data();
    help_ = arg->GetHelpDoc().data();
    ARGPARSE_DCHECK(arg->GetConstValue());
    filename_ = AnyCast<StringView>(arg->GetConstValue())->data();
    dest_ptr_ = arg->GetDest()->GetDestPtr();
    default_value_ = const_cast<Any*>(arg->GetDefaultValue());
    ARGPARSE_DCHECK(default_value_);
  }

  template <typename FlagType>
  void Register() {
    gflags::FlagRegisterer(name_,                             // name
                           help_,                             // help
                           filename_,                         // filename
                           dest_ptr_.Cast<FlagType>(),        // current_storage
                           AnyCast<FlagType>(default_value_)  // defval_storage
    );
  }

 private:
  static bool IsValidNamesInfo(NamesInfo* info) {
    if (!info->IsOption()) return false;
    auto count_all = info->GetLongNamesCount() + info->GetShortNamesCount();
    return count_all == 1;
  }

  const char* name_;
  const char* help_;
  const char* filename_;
  OpaquePtr dest_ptr_;
  Any* default_value_;
};

using GflagRegisterFunc = void (*)(GflagsArgument*);

template <typename FlagType>
void RegisterGlagsArgument(GflagsArgument* arg) {
  return arg->Register<FlagType>();
}

using GflagsRegisterMap = std::map<std::type_index, GflagRegisterFunc>;

template <typename... Types>
GflagsRegisterMap CreateRegisterMap(TypeList<Types...>) {
  return GflagsRegisterMap{{typeid(Types), &RegisterGlagsArgument<Types>}...};
}

class GflagsParser : public ArgumentParser {
 public:
  GflagsParser() : register_map_(CreateRegisterMap(GflagsTypeList{})) {}

  // TODO: make them empty.
  void AddArgumentGroup(ArgumentGroup*) override {
    ARGPARSE_UNSUPPORTED_METHOD(kGflagParserName);
  }
  void AddSubCommand(SubCommand*) override {
    ARGPARSE_UNSUPPORTED_METHOD(kGflagParserName);
  }
  void AddSubCommandGroup(SubCommandGroup*) override {
    ARGPARSE_UNSUPPORTED_METHOD(kGflagParserName);
  }
  void AddArgument(Argument* arg) override {
    GflagsArgument gflags_arg(arg);
    auto iter = register_map_.find(arg->GetDest()->GetType());
    ARGPARSE_DCHECK(iter != register_map_.end());
    return (*iter->second)(&gflags_arg);
  }

  bool ParseKnownArgs(ArgArray args,
                      std::vector<std::string>* unparsed_args) override {
    int argc = args.argc();
    char** argv = args.argv();
    auto rv = gflags::ParseCommandLineFlags(&argc, &argv, true);
    if (unparsed_args) {
      for (int i = 0; i < argc; ++i) unparsed_args->push_back(argv[i]);
      return rv == 0;
    }
    return true;
  }

  ~GflagsParser() override { gflags::ShutDownCommandLineFlags(); }

  std::unique_ptr<OptionsListener> CreateOptionsListener() override;

 private:
  class OptionsListenerImpl;

  const GflagsRegisterMap register_map_;
};

class GflagsParser::OptionsListenerImpl : public OptionsListener {
 public:
  void SetProgramVersion(std::string val) override {
    gflags::SetVersionString(val);
  }
  void SetDescription(std::string val) override {
    gflags::SetUsageMessage(val);
  }
  void SetEmail(std::string) override {
    ARGPARSE_UNSUPPORTED_METHOD(kGflagParserName);
  }
  void SetProgramName(std::string) override {
    ARGPARSE_UNSUPPORTED_METHOD(kGflagParserName);
  }
  void SetProgramUsage(std::string) override {
    ARGPARSE_UNSUPPORTED_METHOD(kGflagParserName);
  }
};

std::unique_ptr<OptionsListener> GflagsParser::CreateOptionsListener() {
  return std::make_unique<OptionsListenerImpl>();
}

std::unique_ptr<ArgumentParser> GflagsParserFactory::CreateParser() {
  return std::make_unique<GflagsParser>();
}

}  // namespace internal
}  // namespace argparse
