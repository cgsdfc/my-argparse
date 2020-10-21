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
  virtual void Run(CallbackClient*) = 0;
  static std::unique_ptr<ActionInfo> CreateDefault(ActionKind action_kind,
                                                   Operations* ops);
  static std::unique_ptr<ActionInfo> CreateFromCallback(ActionFunction cb);
};

// The base class for all actions that manipulate around a dest.
class ActionWithDest : public ActionInfo {
 protected:
  explicit ActionWithDest(DestInfo* dest) : dest_(dest) {
    ARGPARSE_DCHECK(dest);
  }
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
 protected:
  ActionWithConst(DestInfo* dest, const Any* const_value);
  const Any& GetConstValue() const;
  using ActionWithDest::GetDest;

 private:
  const Any* const_value_;
};

class StoreConstAction : public ActionWithConst {
 public:
  using ActionWithConst::ActionWithConst;
  void Run(std::unique_ptr<Any>) override {
    GetOps()->StoreConst(GetPtr(), GetConstValue());
  }
};

class AppendConstAction : public ActionWithConst {
 public:
  using ActionWithConst::ActionWithConst;
  void Run(std::unique_ptr<Any>) override {
    GetOps()->AppendConst(GetPtr(), GetConstValue());
  }
};

class AppendAction : public ActionWithDest {
 public:
  using ActionWithDest::ActionWithDest;
  void Run(std::unique_ptr<Any> data) override {
    GetOps()->Append(GetPtr(), std::move(data));
  }
};

class StoreAction : public ActionWithDest {
 public:
  using ActionWithDest::ActionWithDest;
  void Run(std::unique_ptr<Any> data) override {
    GetOps()->Store(GetPtr(), std::move(data));
  }
};

class TypeInfo {
 public:
  virtual ~TypeInfo() {}
  virtual void Run(const std::string& in, OpsResult* out) = 0;
  // TODO: typehint should be inferred from dest.
  virtual std::string GetTypeHint() { return {}; }

  // Default version: parse a single string into value.
  static std::unique_ptr<TypeInfo> CreateDefault(Operations* ops);
  // Open a file.
  static std::unique_ptr<TypeInfo> CreateFileType(Operations* ops,
                                                  OpenMode mode);
  // Invoke user's callback.
  static std::unique_ptr<TypeInfo> CreateFromCallback(TypeFunction cb);
};

// If we can make Operations indexable from type_index, then only an opaque-ptr
// is needed here.
template <typename T>
std::unique_ptr<DestInfo> DestInfo::CreateFromPtr(T* ptr) {
  ARGPARSE_CHECK_F(ptr, "Pointer passed to dest() must not be null.");
  return absl::WrapUnique(new DestInfo(ptr));
}

}  // namespace internal
}  // namespace argparse
