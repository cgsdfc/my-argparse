// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <memory.h>

#include "absl/strings/string_view.h"
#include "argparse/internal/argparse-operations.h"

namespace argparse {
namespace internal {

enum class ActionKind {
  kNoAction,
  kStore,
  kStoreConst,
  kStoreTrue,
  kStoreFalse,
  kAppend,
  kAppendConst,
  kCount,
  kPrintHelp,
  kPrintUsage,
  kCustom,
};

enum class TypeKind {
  kNothing,
  kParse,
  kOpen,
  kCustom,
};

class NamesInfo {
 public:
  virtual ~NamesInfo() {}
  virtual bool IsOption() = 0;
  virtual unsigned GetLongNamesCount() { return 0; }
  virtual unsigned GetShortNamesCount() { return 0; }
  bool HasLongNames() { return GetLongNamesCount(); }
  bool HasShortNames() { return GetShortNamesCount(); }

  virtual std::string GetDefaultMetaVar() = 0;

  enum NameKind {
    kLongName,
    kShortName,
    kPosName,
    kAllNames,
  };

  // Visit each name of the optional argument.
  virtual void ForEachName(NameKind name_kind,
                           std::function<void(const std::string&)> callback) {}

  virtual absl::string_view GetName() = 0;

  static std::unique_ptr<NamesInfo> CreatePositional(std::string in);
  static std::unique_ptr<NamesInfo> CreateOptional(
      const std::vector<std::string>& in);
};

class NumArgsInfo {
 public:
  virtual ~NumArgsInfo() {}
  // Run() checks if num is valid by returning bool.
  // If invalid, error msg will be set.
  virtual bool Run(unsigned num, std::string* errmsg) = 0;
  static std::unique_ptr<NumArgsInfo> CreateFromFlag(char flag);
  static std::unique_ptr<NumArgsInfo> CreateFromNum(int num);
};

class DestInfo {
 public:
  OpaquePtr GetDestPtr() const { return dest_ptr_; }
  Operations* GetOperations() const { return operations_; }
  // Query the Operations of value-type of T, if any.
  Operations* GetValueTypeOps() const {
    return GetOperations()->GetValueTypeOps();
  }
  std::type_index GetType() const { return dest_ptr_.type(); }

  template <typename T>
  static std::unique_ptr<DestInfo> CreateFromPtr(T* ptr);

 private:
  template <typename T>
  explicit DestInfo(T* ptr)
      : dest_ptr_(ptr), operations_(Operations::GetInstance<T>()) {}

  OpaquePtr dest_ptr_;
  Operations* operations_;
};

class CallbackClient {
 public:
  virtual ~CallbackClient() {}
  virtual std::unique_ptr<Any> GetData() = 0;
  virtual OpaquePtr GetDestPtr() = 0;
  virtual const Any* GetConstValue() = 0;
  virtual void PrintHelp() = 0;
  virtual void PrintUsage() = 0;
};

class ActionInfo {
 public:
  virtual ~ActionInfo() {}
  virtual void Run(std::unique_ptr<Any> data) {}
  virtual void Run(CallbackClient*) {}

  static std::unique_ptr<ActionInfo> CreateDefault(ActionKind action_kind,
                                                   Operations* ops);
  static std::unique_ptr<ActionInfo> CreateFromCallback(ActionFunction cb);

  static std::unique_ptr<ActionInfo> CreateBuiltinAction(
      ActionKind action_kind, DestInfo* dest, const Any* const_value);
  template <typename T>
  static std::unique_ptr<ActionInfo> CreateCallbackAction(
      ActionCallback<T> func);
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
  using ActionWithDest::ActionWithDest;
  void Run(std::unique_ptr<Any> data) override {
    GetOps()->Store(GetPtr(), std::move(data));
  }
};

// An action that runs a user-supplied callback.
template <typename T>
class CallbackAction final : public ActionInfo {
 public:
  using CallbackType = ActionCallback<T>;
  explicit CallbackAction(CallbackType&& cb) : callback_(std::move(cb)) {}
  void Run(std::unique_ptr<Any> data) override {
    callback_(AnyCast<T>(std::move(data)));
  }

private:
  CallbackType callback_;
};

class TypeInfo {
 public:
  virtual ~TypeInfo() {}
  virtual void Run(const std::string& in, OpsResult* out) = 0;

  // Default version: parse a single string into value.
  static std::unique_ptr<TypeInfo> CreateDefault(Operations* ops);
  // Open a file.
  static std::unique_ptr<TypeInfo> CreateFileType(Operations* ops,
                                                  OpenMode mode);
  // Invoke user's callback.
  static std::unique_ptr<TypeInfo> CreateFromCallback(TypeFunction cb);

  template <typename T>
  static std::unique_ptr<TypeInfo> CreateCallbackType(TypeCallback<T> cb);

  explicit TypeInfo(Operations* ops) : operations_(ops) {}
  Operations* GetOps() const { return operations_; }
  std::string GetTypeHint() const { return GetOps()->GetTypeHint(); }

 private:
  Operations* operations_;
};

template <typename T>
class CallbackTypeInfo : public TypeInfo {
 public:
  using CallbackType = TypeCallback<T>;
  explicit CallbackTypeInfo(CallbackType&& cb)
      : TypeInfo(Operations::GetInstance<T>()), callback_(std::move(cb)) {}

  void Run(const std::string& in, OpsResult* out) override {
    T return_value;
    bool rv = callback_(in, &return_value);
    *out = OpsResult(rv ? ConversionSuccess(std::move_if_noexcept(return_value))
                        : ConversionFailure());
  }

 private:
  CallbackType callback_;
};

// If we can make Operations indexable from type_index, then only an opaque-ptr
// is needed here.
template <typename T>
std::unique_ptr<DestInfo> DestInfo::CreateFromPtr(T* ptr) {
  ARGPARSE_CHECK_F(ptr, "Pointer passed to dest() must not be null.");
  return absl::WrapUnique(new DestInfo(ptr));
}

template <typename T>
std::unique_ptr<ActionInfo> ActionInfo::CreateCallbackAction(
    ActionCallback<T> func) {
  return absl::make_unique<CallbackAction<T>>(std::move(func));
}

template <typename T>
std::unique_ptr<TypeInfo> TypeInfo::CreateCallbackType(TypeCallback<T> cb) {
  return absl::make_unique<CallbackTypeInfo<T>>(std::move(cb));
}

}  // namespace internal
}  // namespace argparse
