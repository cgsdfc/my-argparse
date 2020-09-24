#pragma once

#include "argparse/arg/info.h"

namespace argparse {

class NumberNumArgsInfo : public NumArgsInfo {
 public:
  explicit NumberNumArgsInfo(unsigned num) : num_(num) {}
  bool Run(unsigned in, std::string* errmsg) override {
    if (in == num_)
      return true;
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
  explicit FlagNumArgsInfo(char flag) : flag_(flag) {
    ARGPARSE_CHECK_F(IsValidNumArgsFlag(flag), "Not a valid flag to nargs: %c",
                     flag);
  }
  bool Run(unsigned in, std::string* errmsg) override {
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
    if (ok)
      return true;
    std::ostringstream os;
    os << "expected " << FlagToString(flag_) << " values, got " << in;
    *errmsg = os.str();
    return false;
  }
  static bool IsValidNumArgsFlag(char in) {
    return in == '+' || in == '*' || in == '+';
  }
  static const char* FlagToString(char flag) {
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

 private:
  const char flag_;
};

inline std::unique_ptr<NumArgsInfo> NumArgsInfo::CreateFromFlag(char flag) {
  return std::make_unique<FlagNumArgsInfo>(flag);
}

inline std::unique_ptr<NumArgsInfo> NumArgsInfo::CreateFromNum(int num) {
  ARGPARSE_CHECK_F(num >= 0, "nargs number must be >= 0");
  return std::make_unique<NumberNumArgsInfo>(num);
}

class DestInfoImpl : public DestInfo {
 public:
  DestInfoImpl(DestPtr d, std::unique_ptr<OpsFactory> f)
      : dest_ptr_(d), ops_factory_(std::move(f)) {
    ops_ = ops_factory_->CreateOps();
  }

  DestPtr GetDestPtr() override { return dest_ptr_; }
  OpsFactory* GetOpsFactory() override { return ops_factory_.get(); }
  std::string FormatValue(const Any& in) override {
    return ops_->FormatValue(in);
  }

 private:
  DestPtr dest_ptr_;
  std::unique_ptr<OpsFactory> ops_factory_;
  std::unique_ptr<Operations> ops_;
};

// ActionInfo for builtin actions like store and append.
class DefaultActionInfo : public ActionInfo {
 public:
  DefaultActionInfo(ActionKind action_kind, std::unique_ptr<Operations> ops)
      : action_kind_(action_kind), ops_(std::move(ops)) {}

  void Run(CallbackClient* client) override;

 private:
  // Since kind of action is too much, we use a switch instead of subclasses.
  ActionKind action_kind_;
  std::unique_ptr<Operations> ops_;
};

// class TypeLessActionInfo : public

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

inline std::unique_ptr<ActionInfo> ActionInfo::CreateDefault(
    ActionKind action_kind,
    std::unique_ptr<Operations> ops) {
  return std::make_unique<DefaultActionInfo>(action_kind, std::move(ops));
}

inline std::unique_ptr<ActionInfo> ActionInfo::CreateFromCallback(
    std::unique_ptr<ActionCallback> cb) {
  return std::make_unique<ActionCallbackInfo>(std::move(cb));
}

// The default of TypeInfo: parse a single string into a value
// using ParseTraits.
class DefaultTypeInfo : public TypeInfo {
 public:
  explicit DefaultTypeInfo(std::unique_ptr<Operations> ops)
      : ops_(std::move(ops)) {}

  void Run(const std::string& in, OpsResult* out) override {
    return ops_->Parse(in, out);
  }

  std::string GetTypeHint() override { return ops_->GetTypeHint(); }

 private:
  std::unique_ptr<Operations> ops_;
};

// TypeInfo that opens a file according to some mode.
class FileTypeInfo : public TypeInfo {
 public:
  // TODO: set up cache of Operations objs..
  FileTypeInfo(std::unique_ptr<Operations> ops, OpenMode mode)
      : ops_(std::move(ops)), mode_(mode) {
    ARGPARSE_DCHECK(mode != kModeNoMode);
  }

  void Run(const std::string& in, OpsResult* out) override {
    return ops_->Open(in, mode_, out);
  }

  std::string GetTypeHint() override { return ops_->GetTypeHint(); }

 private:
  std::unique_ptr<Operations> ops_;
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

inline std::unique_ptr<TypeInfo> TypeInfo::CreateDefault(
    std::unique_ptr<Operations> ops) {
  return std::make_unique<DefaultTypeInfo>(std::move(ops));
}
inline std::unique_ptr<TypeInfo> TypeInfo::CreateFileType(
    std::unique_ptr<Operations> ops,
    OpenMode mode) {
  return std::make_unique<FileTypeInfo>(std::move(ops), mode);
}
// Invoke user's callback.
inline std::unique_ptr<TypeInfo> TypeInfo::CreateFromCallback(
    std::unique_ptr<TypeCallback> cb) {
  return std::make_unique<TypeCallbackInfo>(std::move(cb));
}

}  // namespace argparse