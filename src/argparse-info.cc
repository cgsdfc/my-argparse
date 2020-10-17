// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-info.h"

#include "absl/strings/ascii.h"
#include "absl/strings/str_replace.h"

namespace argparse {
namespace internal {

namespace {

class NumberNumArgsInfo : public NumArgsInfo {
 public:
  explicit NumberNumArgsInfo(unsigned num) : num_(num) {}
  bool Run(unsigned in, std::string* errmsg) override {
    if (in == num_) return true;
    std::ostringstream os;
    os << "expected " << num_ << " values, got " << in;
    *errmsg = os.str();
    return false;
  }

 private:
  const unsigned num_;
};

class FlagNumArgsInfo : public NumArgsInfo {
 public:
  explicit FlagNumArgsInfo(char flag);
  bool Run(unsigned in, std::string* errmsg) override;

 private:
  const char flag_;
};

bool IsValidNumArgsFlag(char in) { return in == '+' || in == '*' || in == '+'; }

const char* FlagToString(char flag) {
  switch (flag) {
    case '+':
      return "one or more";
    case '?':
      return "zero or one";
    case '*':
      return "zero or more";
    default:
      ARGPARSE_DCHECK(false);
  }
}

bool FlagNumArgsInfo::Run(unsigned in, std::string* errmsg) {
  bool ok = false;
  switch (flag_) {
    case '+':
      ok = in >= 1;
      break;
    case '?':
      ok = in == 0 || in == 1;
      break;
    case '*':
      ok = true;
    default:
      ARGPARSE_DCHECK(false);
  }
  if (ok) return true;
  std::ostringstream os;
  os << "expected " << FlagToString(flag_) << " values, got " << in;
  *errmsg = os.str();
  return false;
}

FlagNumArgsInfo::FlagNumArgsInfo(char flag) : flag_(flag) {
  ARGPARSE_CHECK_F(IsValidNumArgsFlag(flag), "Not a valid flag to nargs: %c",
                   flag);
}

// ActionInfo for builtin actions like store and append.
class DefaultActionInfo : public ActionInfo {
 public:
  DefaultActionInfo(ActionKind action_kind, Operations* ops)
      : action_kind_(action_kind), ops_(ops) {}

  void Run(CallbackClient* client) override;

 private:
  // Since kind of action is too much, we use a switch instead of subclasses.
  ActionKind action_kind_;
  Operations* ops_;
};

// Adapt an ActionCallback to ActionInfo.
class ActionCallbackInfo : public ActionInfo {
 public:
  explicit ActionCallbackInfo(std::unique_ptr<ActionCallback> cb)
      : action_callback_(std::move(cb)) {}

  void Run(CallbackClient* client) override {
    return action_callback_->Run(client->GetDestPtr(), client->GetData());
  }

 private:
  std::unique_ptr<ActionCallback> action_callback_;
};

// The default of TypeInfo: parse a single string into a value
// using ParseTraits.
class DefaultTypeInfo : public TypeInfo {
 public:
  explicit DefaultTypeInfo(Operations* ops) : ops_(ops) {}

  void Run(const std::string& in, OpsResult* out) override {
    return ops_->Parse(in, out);
  }

  std::string GetTypeHint() override { return ops_->GetTypeHint(); }

 private:
  Operations* ops_;
};

// TypeInfo that opens a file according to some mode.
class FileTypeInfo : public TypeInfo {
 public:
  // TODO: set up cache of Operations objs..
  FileTypeInfo(Operations* ops, OpenMode mode) : ops_(ops), mode_(mode) {
    ARGPARSE_DCHECK(mode != kModeNoMode);
  }

  void Run(const std::string& in, OpsResult* out) override {
    return ops_->Open(in, mode_, out);
  }

  std::string GetTypeHint() override { return ops_->GetTypeHint(); }

 private:
  Operations* ops_;
  OpenMode mode_;
};

// TypeInfo that runs user's callback.
class TypeCallbackInfo : public TypeInfo {
 public:
  explicit TypeCallbackInfo(std::unique_ptr<TypeCallback> cb)
      : type_callback_(std::move(cb)) {}

  void Run(const std::string& in, OpsResult* out) override {
    return type_callback_->Run(in, out);
  }

  std::string GetTypeHint() override { return type_callback_->GetTypeHint(); }

 private:
  std::unique_ptr<TypeCallback> type_callback_;
};

void DefaultActionInfo::Run(CallbackClient* client) {
  auto dest_ptr = client->GetDestPtr();
  auto data = client->GetData();

  switch (action_kind_) {
    case ActionKind::kNoAction:
      break;
    case ActionKind::kStore:
      ops_->Store(dest_ptr, std::move(data));
      break;
    case ActionKind::kStoreTrue:
    case ActionKind::kStoreFalse:
    case ActionKind::kStoreConst:
      ops_->StoreConst(dest_ptr, *client->GetConstValue());
      break;
    case ActionKind::kAppend:
      ops_->Append(dest_ptr, std::move(data));
      break;
    case ActionKind::kAppendConst:
      ops_->AppendConst(dest_ptr, *client->GetConstValue());
      break;
    case ActionKind::kPrintHelp:
      client->PrintHelp();
      break;
    case ActionKind::kPrintUsage:
      client->PrintUsage();
      break;
    case ActionKind::kCustom:
      break;
    case ActionKind::kCount:
      ops_->Count(dest_ptr);
      break;
  }
}

class PositionalName : public NamesInfo {
 public:
  explicit PositionalName(std::string name) : name_(std::move(name)) {}

  bool IsOption() override { return false; }
  std::string GetDefaultMetaVar() override {
    return absl::AsciiStrToUpper(name_);
  }
  void ForEachName(NameKind name_kind,
                   std::function<void(const std::string&)> callback) override {
    if (name_kind == kPosName) callback(name_);
  }
  absl::string_view GetName() override { return name_; }

 private:
  std::string name_;
};

class OptionalNames : public NamesInfo {
 public:
  explicit OptionalNames(const std::vector<std::string>& names) {
    for (auto& name : names) {
      ARGPARSE_CHECK_F(IsValidOptionName(name), "Not a valid option name: %s",
                       name.c_str());
      if (IsLongOptionName(name)) {
        long_names_.push_back(name);
      } else {
        ARGPARSE_DCHECK(IsShortOptionName(name));
        short_names_.push_back(name);
      }
    }
  }

  bool IsOption() override { return true; }
  unsigned GetLongNamesCount() override { return long_names_.size(); }
  unsigned GetShortNamesCount() override { return short_names_.size(); }

  std::string GetDefaultMetaVar() override {
    std::string in =
        long_names_.empty() ? short_names_.front() : long_names_.front();
    absl::StrReplaceAll({{"-", "_"}}, &in);
    absl::AsciiStrToUpper(&in);
    return in;
  }

  void ForEachName(NameKind name_kind,
                   std::function<void(const std::string&)> callback) override {
    switch (name_kind) {
      case kPosName:
        return;
      case kLongName: {
        for (auto& name : long_names_) callback(name);
        break;
      }
      case kShortName: {
        for (auto& name : short_names_) callback(name);
        break;
      }
      case kAllNames: {
        for (auto& name : long_names_) callback(name);
        for (auto& name : short_names_) callback(name);
        break;
      }
      default:
        break;
    }
  }

  absl::string_view GetName() override {
    const auto& name =
        long_names_.empty() ? short_names_.front() : long_names_.front();
    return name;
  }

 private:
  std::vector<std::string> long_names_;
  std::vector<std::string> short_names_;
};

}  // namespace

std::unique_ptr<TypeInfo> TypeInfo::CreateDefault(Operations* ops) {
  return absl::make_unique<DefaultTypeInfo>(ops);
}

std::unique_ptr<TypeInfo> TypeInfo::CreateFileType(Operations* ops,
                                                   OpenMode mode) {
  return absl::make_unique<FileTypeInfo>(ops, mode);
}

// Invoke user's callback.
std::unique_ptr<TypeInfo> TypeInfo::CreateFromCallback(
    std::unique_ptr<TypeCallback> cb) {
  return absl::make_unique<TypeCallbackInfo>(std::move(cb));
}

std::unique_ptr<NumArgsInfo> NumArgsInfo::CreateFromFlag(char flag) {
  return absl::make_unique<FlagNumArgsInfo>(flag);
}

std::unique_ptr<NumArgsInfo> NumArgsInfo::CreateFromNum(int num) {
  ARGPARSE_CHECK_F(num >= 0, "nargs number must be >= 0");
  return absl::make_unique<NumberNumArgsInfo>(num);
}

std::unique_ptr<ActionInfo> ActionInfo::CreateDefault(ActionKind action_kind,
                                                      Operations* ops) {
  return absl::make_unique<DefaultActionInfo>(action_kind, ops);
}

std::unique_ptr<ActionInfo> ActionInfo::CreateFromCallback(
    std::unique_ptr<ActionCallback> cb) {
  return absl::make_unique<ActionCallbackInfo>(std::move(cb));
}

std::unique_ptr<NamesInfo> NamesInfo::CreatePositional(std::string in) {
  return absl::make_unique<PositionalName>(std::move(in));
}

std::unique_ptr<NamesInfo> NamesInfo::CreateOptional(
    const std::vector<std::string>& in) {
  return absl::make_unique<OptionalNames>(in);
}

bool IsValidPositionalName(const std::string& name) {
  if (name.size() == 0 || !absl::ascii_isalpha(name[0])) return false;

  return std::all_of(name.begin() + 1, name.end(), [](char c) {
    return absl::ascii_isalnum(c) || c == '-' || c == '_';
  });
}

bool IsValidOptionName(const std::string& name) {
  auto len = name.size();
  if (len < 2 || name[0] != '-') return false;
  if (len == 2)  // This rules out -?, -* -@ -= --
    return absl::ascii_isalnum(name[1]);
  // check for long-ness.
  // TODO: fixthis.
  ARGPARSE_CHECK_F(
      name[1] == '-',
      "Single-dash long option (i.e., -jar) is not supported. Please use "
      "GNU-style long option (double-dash)");

  return std::all_of(name.begin() + 2, name.end(), [](char c) {
    return c == '-' || c == '_' || absl::ascii_isalnum(c);
  });
}

}  // namespace internal
}  // namespace argparse
