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

// std::unique_ptr<NamesInfo> NamesInfo::CreatePositional(std::string in) {
//   return absl::make_unique<PositionalName>(std::move(in));
// }

// std::unique_ptr<NamesInfo> NamesInfo::CreateOptional(
//     const std::vector<std::string>& in) {
//   return absl::make_unique<OptionalNames>(in);
// }

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

// // TODO: Move these logic NamesInfo.
// Names::Names(std::string name) {
//   if (name[0] == '-') {
//     // This is in fact an option.
//     std::vector<std::string> names{std::move(name)};
//     this->SetObject(internal::NamesInfo::CreateOptional(std::move(names)));
//     return;
//   }
//   ARGPARSE_CHECK_F(internal::IsValidPositionalName(name),
//                    "Not a valid positional name: %s", name.c_str());
//   this->SetObject(internal::NamesInfo::CreatePositional(std::move(name)));
// }

// Names::Names(std::initializer_list<std::string> names) {
//   ARGPARSE_CHECK_F(names.size(), "At least one name must be provided");
//   this->SetObject(internal::NamesInfo::CreateOptional(names));
// }


std::unique_ptr<NamesInfo> NamesInfo::CreateFromStr(absl::string_view name) {
  // if (names.size() == 1 && IsValidPositionalName())
  return {};
}

std::unique_ptr<NamesInfo> NamesInfo::CreateFromStrings(
    std::initializer_list<absl::string_view> names) {
  // if (names.size() == 1 && IsValidPositionalName())
  return {};
}

bool NamesInfo::IsValidPositionalName(absl::string_view name) {
  if (name.empty() || !absl::ascii_isalpha(name[0])) return false;
  return std::all_of(name.begin() + 1, name.end(), [](char c) {
    return absl::ascii_isalnum(c) || c == '-' || c == '_';
  });
}

bool NamesInfo::IsValidOptionalName(absl::string_view name) {
  auto len = name.size();
  if (len < 2 || name[0] != '-') return false;
  if (len == 2)  // This rules out -?, -* -@ -= --
    return absl::ascii_isalnum(name[1]);

  return std::all_of(name.begin() + 2, name.end(), [](char c) {
    return c == '-' || c == '_' || absl::ascii_isalnum(c);
  });
}

NamesInfo::NamesInfo(absl::string_view name) {
  ARGPARSE_CHECK_F(IsValidOptionalName(name) || IsValidPositionalName(name),
                   "'%s' is invalid name", name.data());
  AddName(name);
}

NamesInfo::NamesInfo(std::initializer_list<absl::string_view> names) {
  ARGPARSE_DCHECK(names.size());
  for (auto name : names) AddName(name);
}

}  // namespace internal
}  // namespace argparse
