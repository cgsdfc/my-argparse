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

// The default of TypeInfo: parse a single string into a value
// using ParseTraits.
class DefaultTypeInfo : public TypeInfo {
 public:
  using TypeInfo::TypeInfo;

  void Run(const std::string& in, OpsResult* out) override {
    ARGPARSE_DCHECK(GetOps()->IsSupported(OpsKind::kParse));
    return GetOps()->Parse(in, out);
  }
};

// TypeInfo that opens a file according to some mode.
class FileTypeInfo : public TypeInfo {
 public:
  FileTypeInfo(Operations* ops, OpenMode mode) : TypeInfo(ops), mode_(mode) {
    ARGPARSE_DCHECK(ops->IsSupported(OpsKind::kOpen));
    ARGPARSE_DCHECK(mode != kModeNoMode);
  }

  void Run(const std::string& in, OpsResult* out) override {
    return GetOps()->Open(in, mode_, out);
  }

 private:
  OpenMode mode_;
};

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

// The base class for all actions that manipulate around a dest.
class ActionWithDest : public ActionInfo {
 public:
  explicit ActionWithDest(DestInfo* dest) : dest_(dest) {
    ARGPARSE_DCHECK(dest);
  }

 protected:
  DestInfo* GetDest() const { return dest_; }
  Operations* GetOps() const { return GetDest()->GetOperations(); }
  OpaquePtr GetPtr() const { return GetDest()->GetDestPtr(); }

 private:
  DestInfo* dest_;
};

class CountAction : public ActionWithDest {
 public:
  using ActionWithDest::ActionWithDest;
  void Run(std::unique_ptr<Any>) override { GetOps()->Count(GetPtr()); }
};

// Actions that don't use the input data, but use a pre-set constant.
class ActionWithConst : public ActionWithDest {
 public:
  ActionWithConst(DestInfo* dest, const Any* const_value)
      : ActionWithDest(dest), const_value_(const_value) {
    ARGPARSE_DCHECK(const_value_);
  }

 protected:
  const Any& GetConstValue() const { return *const_value_; }
  using ActionWithDest::GetDest;

 private:
  const Any* const_value_;
};

class StoreConstAction final : public ActionWithConst {
 public:
  using ActionWithConst::ActionWithConst;
  void Run(std::unique_ptr<Any>) override {
    GetOps()->StoreConst(GetPtr(), GetConstValue());
  }
};

class AppendConstAction final : public ActionWithConst {
 public:
  using ActionWithConst::ActionWithConst;
  void Run(std::unique_ptr<Any>) override {
    GetOps()->AppendConst(GetPtr(), GetConstValue());
  }
};

class AppendAction final : public ActionWithDest {
 public:
  using ActionWithDest::ActionWithDest;
  void Run(std::unique_ptr<Any> data) override {
    GetOps()->Append(GetPtr(), std::move(data));
  }
};

class StoreAction final : public ActionWithDest {
 public:
  // TODO: should check supportness in ctor.
  using ActionWithDest::ActionWithDest;
  void Run(std::unique_ptr<Any> data) override {
    GetOps()->Store(GetPtr(), std::move(data));
  }
};

}  // namespace

std::unique_ptr<TypeInfo> TypeInfo::CreateDefault(Operations* ops) {
  return absl::make_unique<DefaultTypeInfo>(ops);
}

std::unique_ptr<TypeInfo> TypeInfo::CreateFileType(Operations* ops,
                                                   OpenMode mode) {
  return absl::make_unique<FileTypeInfo>(ops, mode);
}

std::unique_ptr<NumArgsInfo> NumArgsInfo::CreateFromFlag(char flag) {
  return absl::make_unique<FlagNumArgsInfo>(flag);
}

std::unique_ptr<NumArgsInfo> NumArgsInfo::CreateFromNum(int num) {
  ARGPARSE_CHECK_F(num >= 0, "nargs number must be >= 0");
  return absl::make_unique<NumberNumArgsInfo>(num);
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

  return std::all_of(name.begin() + 2, name.end(), [](char c) {
    return c == '-' || c == '_' || absl::ascii_isalnum(c);
  });
}

std::unique_ptr<ActionInfo> ActionInfo::CreateBuiltinAction(
    ActionKind action_kind, DestInfo* dest, const Any* const_value) {
  switch (action_kind) {
    case ActionKind::kStore:
      return absl::make_unique<StoreAction>(dest);
    case ActionKind::kAppend:
      return absl::make_unique<AppendAction>(dest);
    case ActionKind::kCount:
      return absl::make_unique<CountAction>(dest);
    case ActionKind::kStoreFalse:
    case ActionKind::kStoreTrue:
      // For these two actions, client should pass a true/false as const_value.
      // And they will be handled as StoreConst.
      ARGPARSE_DCHECK(const_value->GetType() == typeid(bool));
      ABSL_FALLTHROUGH_INTENDED;
    case ActionKind::kStoreConst:
      return absl::make_unique<StoreConstAction>(dest, const_value);
    case ActionKind::kAppendConst:
      return absl::make_unique<AppendConstAction>(dest, const_value);
    default:
      return nullptr;
  }
}

}  // namespace internal
}  // namespace argparse
