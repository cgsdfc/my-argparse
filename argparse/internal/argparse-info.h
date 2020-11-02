// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <memory.h>
#include <initializer_list>

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

bool IsValidPositionalName(const std::string& name);

// A valid option name is long or short option name and not '--', '-'.
// This is only checked once and true for good.
bool IsValidOptionName(const std::string& name);

// These two predicates must be called only when IsValidOptionName() holds.
inline bool IsLongOptionName(const std::string& name) {
  ARGPARSE_DCHECK(IsValidOptionName(name));
  return name.size() > 2;
}

inline bool IsShortOptionName(const std::string& name) {
  ARGPARSE_DCHECK(IsValidOptionName(name));
  return name.size() == 2;
}

class NamesInfo {
 public:
  // For iterating all names.
  std::size_t GetNameCount() const { return names_.size(); }

  absl::string_view GetName(std::size_t i) const { return names_[i]; }

  // Tells whether this is a positional name -- like 'output'.
  bool IsPositional() const;
  // Return the name if this is a positional name, or die.
  absl::string_view GetPositionalName() const;

  // Tells whether this is an (or a list of) optional name(s).
  bool IsOptional() const { return flags_ & kIsOptional; }
  // // For an optional name, return 
  // absl::string_view GetFirstOptionalName() const;

  // Flag is a special optional that only has one short optional name, like
  // '-c'.
  bool IsFlag() const;

  // For a positional, this is the positional name.
  // For an optional, this is the first long name (or first short name).
  absl::string_view GetRepresentativeName() const;

  // Invoke a callback for each name that satisfies the predicate.
  // Example:
  // names->ForEachNameThat(&NamesInfo::IsOptionalName, [](absl::string_view
  // name) {});
  template <typename Callback>
  void ForEachNameThat(bool (*predicate)(absl::string_view),
                       Callback&& cb) const {
    for (std::size_t i = 0; i < GetNameCount(); ++i) {
      auto name = GetName(i);
      if (predicate(name)) cb(name);
    }
  }

  // Various predicates about a name.
  static bool IsOptionalName(absl::string_view name);
  static bool IsPositionalName(absl::string_view name);
  static bool IsLongOptionalName(absl::string_view name);
  static bool IsShortOptionalName(absl::string_view name);

  static std::unique_ptr<NamesInfo> CreatePositional(absl::string_view name);

  static std::unique_ptr<NamesInfo> CreateFromStr(absl::string_view name);
  static std::unique_ptr<NamesInfo> CreateFromStrings(
      std::initializer_list<absl::string_view> names);

  std::string GetDefaultMetaVar() const {}

 private:
  struct PositionalTag {};
  struct OptionalTag {};

  NamesInfo(absl::string_view name, PositionalTag);

  enum {
    kIsPositional = 0,
    kIsOptional = 1,
    kIsFlag = 2,
  };
  int flags_ = 0;
  absl::InlinedVector<std::string, 1> names_;
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

  static std::unique_ptr<ActionInfo> CreateBuiltinAction(
      ActionKind action_kind, DestInfo* dest, const Any* const_value);
  template <typename T>
  static std::unique_ptr<ActionInfo> CreateCallbackAction(
      ActionCallback<T> func);
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

  template <typename T>
  static std::unique_ptr<TypeInfo> CreateCallbackType(TypeCallback<T> cb);

  explicit TypeInfo(Operations* ops) : operations_(ops) {}
  Operations* GetOps() const { return operations_; }
  std::string GetTypeHint() const { return GetOps()->GetTypeHint(); }

 private:
  Operations* operations_;
};

namespace info_internal {

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

}  // namespace info_internal

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
  using info_internal::CallbackAction;
  return absl::make_unique<CallbackAction<T>>(std::move(func));
}

template <typename T>
std::unique_ptr<TypeInfo> TypeInfo::CreateCallbackType(TypeCallback<T> cb) {
  using info_internal::CallbackTypeInfo;
  return absl::make_unique<CallbackTypeInfo<T>>(std::move(cb));
}

}  // namespace internal
}  // namespace argparse
