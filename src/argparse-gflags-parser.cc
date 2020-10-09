// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <gflags/gflags.h>

#include "argparse/internal/argparse-internal.h"

#define ARGPARSE_UNSUPPORTED_METHOD(object) \
  ARGPARSE_CHECK_F(false, "%s is not supported by %s", __func__, (object))

namespace argparse {
namespace internal {

inline StringView StripLeading(StringView sv, char ch) {
  auto str = sv.data();
  auto size = sv.size();
  for (; size != 0 && *str == ch; ++str, --size)
    ;
  return StringView(str, size);
}

class RegisterHelper {
 public:
  virtual void Run(StringView name, StringView help, Any* default_value,
                   OpaquePtr ptr) = 0;
  virtual ~RegisterHelper() {}
};

template <typename T>
class RegisterHelperImpl : public RegisterHelper {
 public:
  void Run(StringView name, StringView help, Any* default_value,
           OpaquePtr ptr) override {
    gflags::FlagRegisterer(name.data(), help.data(), nullptr, ptr.Cast<T>(),
                           AnyCast<T>(default_value));
  }
};

class GflagsArgumentParser : public ArgumentParser {
 public:
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
    // Arg should only have one optional name.
    ARGPARSE_CHECK_F(IsValidNamesInfo(arg->GetNamesInfo()),
                     "%s only accept optional argument without alias",
                     kGflagParserName);
    RegisterFlagForArgument(arg);
  }

  bool ParseKnownArgs(ArgArray args, std::vector<std::string>*) override {
    int argc = args.argc();
    char** argv = args.argv();
    gflags::ParseCommandLineFlags(&argc, &argv, remove_flags_);
    return true;
  }

  std::unique_ptr<OptionsListener> CreateOptionsListener() override;

 private:
  // Holds something that gflags needs but don't store.
  class FlagStorage;
  class OptionsListenerImpl;

  static constexpr char kGflagParserName[] = "gflags parser";

  static bool IsValidNamesInfo(NamesInfo* info) {
    if (!info->IsOption()) return false;
    auto count_all = info->GetLongNamesCount() + info->GetShortNamesCount();
    return count_all == 1;
  }

  void RegisterFlagForArgument(Argument* arg) {
    auto type = arg->GetDest()->GetType();
    auto register_func = register_func_map_[type].get();
    // TODO: make dest can get type name.
    ARGPARSE_CHECK_F(register_func, "Invalid type of Argument's dest");

    // A default value is needed.
    ARGPARSE_CHECK_F(arg->GetDefaultValue(), "Default value must be set");

    auto name = StripLeading(arg->GetNamesInfo()->GetName(), '-');
    auto help = arg->GetHelpDoc();
    register_func->Run(name, help, const_cast<Any*>(arg->GetDefaultValue()),
                       arg->GetDest()->GetDestPtr());
  }

  //   template <typename T>
  //   static void RegisterFuncImpl(Argument* arg) {
  //     auto* current_storage = arg->GetDest()->GetDestPtr().Cast<T>();

  //     auto* defval_storage = AnyCast<T>(arg->GetDefaultValue());

  //     gflags::FlagRegisterer(name.data(), help.data(), nullptr,
  //     current_storage,
  //                            const_cast<T*>(defval_storage));
  //   }

  //   using RegisterFunc = void (*)(Argument*);

  std::map<std::type_index, std::unique_ptr<RegisterHelper>> register_func_map_;
  bool remove_flags_ = false;
};

class GflagsArgumentParser::OptionsListenerImpl : public OptionsListener {
 public:
  void SetProgramUsage(std::string val) override {
    gflags::SetUsageMessage(val);
  }
  void SetProgramVersion(std::string val) override {
    gflags::SetVersionString(val);
  }

 private:
};

}  // namespace internal
}  // namespace argparse
