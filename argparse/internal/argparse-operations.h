// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <map>
#include <memory>
#include <sstream>
#include <string>

#include "argparse/argparse-traits.h"
#include "argparse/internal/argparse-any.h"
#include "argparse/internal/argparse-opaque-ptr.h"
#include "argparse/internal/argparse-parse-traits.h"
#include "argparse/internal/argparse-ops-result.h"

// This file impls Operations class using the traits defined by users.
namespace argparse {
namespace internal {

enum class OpsKind {
  kStore,
  kStoreConst,
  kAppend,
  kAppendConst,
  kCount,
  kParse,
  kOpen,
  kMaxOpsKind,
};

const char* OpsToString(OpsKind ops);

// A handle to the function table.
class Operations {
 public:
  // For actions:
  virtual void Store(OpaquePtr dest, std::unique_ptr<Any> data) = 0;
  virtual void StoreConst(OpaquePtr dest, const Any& data) = 0;
  virtual void Append(OpaquePtr dest, std::unique_ptr<Any> data) = 0;
  virtual void AppendConst(OpaquePtr dest, const Any& data) = 0;
  virtual void Count(OpaquePtr dest) = 0;
  // For types:
  virtual void Parse(absl::string_view in, OpsResult* out) = 0;
  virtual void Open(absl::string_view in, absl::string_view mode,
                    OpsResult* out) = 0;
  virtual bool IsSupported(OpsKind ops) = 0;
  virtual absl::string_view GetTypeName() = 0;
  virtual std::string GetTypeHint() = 0;
  virtual const std::type_info& GetTypeInfo() = 0;
  virtual std::string FormatValue(const Any& val) = 0;
  virtual Operations* GetValueTypeOps() = 0;
  virtual ~Operations() {}

  template <typename T>
  static Operations* GetInstance();
};

// Extracted the bool value from AppendTraits.
template <typename T>
struct IsAppendSupported
    : std::integral_constant<bool, bool(AppendTraits<T>::Run)> {};

template <typename T, bool = IsAppendSupported<T>{}>
struct IsAppendConstSupportedImpl;
template <typename T>
struct IsAppendConstSupportedImpl<T, false> : std::false_type {};
template <typename T>
struct IsAppendConstSupportedImpl<T, true>
    : std::is_copy_assignable<ValueTypeOf<T>> {};

template <typename T>
struct IsAppendConstSupported : IsAppendConstSupportedImpl<T> {};

template <OpsKind Ops, typename T>
struct IsOpsSupported : std::false_type {};

template <typename T>
struct IsOpsSupported<OpsKind::kStore, T>
    : std::integral_constant<bool, std::is_copy_assignable<T>{} ||
                                       std::is_move_assignable<T>{}> {};

template <typename T>
struct IsOpsSupported<OpsKind::kStoreConst, T> : std::is_copy_assignable<T> {};

template <typename T>
struct IsOpsSupported<OpsKind::kAppend, T> : IsAppendSupported<T> {};

template <typename T>
struct IsOpsSupported<OpsKind::kAppendConst, T> : IsAppendConstSupported<T> {};

template <typename T>
struct IsOpsSupported<OpsKind::kCount, T>
    : std::integral_constant<bool, std::is_integral<T>{} &&
                                       !std::is_same<T, bool>{}> {};

template <typename T>
struct IsOpsSupported<OpsKind::kParse, T> : internal::IsParseDefined<T> {};

template <typename T>
struct IsOpsSupported<OpsKind::kOpen, T> : IsOpenDefined<T> {};

// Put the code used only in this module here.
namespace operations_internal {

template <typename T>
ABSL_MUST_USE_RESULT T TakeValueAndDiscard(std::unique_ptr<Any> any) {
  ARGPARSE_INTERNAL_DCHECK(any, "");
  return std::move_if_noexcept(AnyCast<T>(*any));
}

template <OpsKind Ops, typename T, bool = IsOpsSupported<Ops, T>{}>
struct OpsMethod;

template <OpsKind Ops, typename T>
struct OpsMethod<Ops, T, false> {
  template <typename... Args>
  static void Run(Args&&...) {}
};

template <typename T>
struct OpsMethod<OpsKind::kStore, T, true> {
  static void Run(OpaquePtr dest, std::unique_ptr<Any> data) {
    if (data) dest.PutValue(TakeValueAndDiscard<T>(std::move(data)));
  }
};

template <typename T>
struct OpsMethod<OpsKind::kStoreConst, T, true> {
  static void Run(OpaquePtr dest, const Any& data) {
    dest.PutValue(AnyCast<T>(data));
  }
};

template <typename T>
struct OpsMethod<OpsKind::kAppend, T, true> {
  static void Run(OpaquePtr dest, std::unique_ptr<Any> data) {
    if (data) {
      auto* ptr = dest.Cast<T>();
      auto value = AnyCast<ValueTypeOf<T>>(std::move(data));
      AppendTraits<T>::Run(ptr, std::move_if_noexcept(value));
    }
  }
};

template <typename T>
struct OpsMethod<OpsKind::kAppendConst, T, true> {
  static void Run(OpaquePtr dest, const Any& data) {
    auto* ptr = dest.Cast<T>();
    auto value = AnyCast<ValueTypeOf<T>>(data);
    AppendTraits<T>::Run(ptr, value);
  }
};

template <typename T>
struct OpsMethod<OpsKind::kCount, T, true> {
  static void Run(OpaquePtr dest) {
    auto* ptr = dest.Cast<T>();
    ++(*ptr);
  }
};

template <typename T>
struct OpsMethod<OpsKind::kParse, T, true> {
  static void Run(absl::string_view in, OpsResult* out) {
    //TODO:
  }
};

template <typename T>
struct OpsMethod<OpsKind::kOpen, T, true> {
  static void Run(absl::string_view in, absl::string_view mode,
                  OpsResult* out) {
    // TODO:
  }
};

template <typename T, std::size_t... OpsIndices>
bool OpsIsSupportedImpl(OpsKind ops, absl::index_sequence<OpsIndices...>) {
  constexpr bool kFlagArray[] = {
      (IsOpsSupported<static_cast<OpsKind>(OpsIndices), T>::value)...};
  return kFlagArray[std::size_t(ops)];
}

template <typename T>
class OperationsImpl final : public Operations {
 public:
  void Store(OpaquePtr dest, std::unique_ptr<Any> data) override {
    return OpsMethod<OpsKind::kStore, T>::Run(dest, std::move(data));
  }
  void StoreConst(OpaquePtr dest, const Any& data) override {
    return OpsMethod<OpsKind::kStoreConst, T>::Run(dest, data);
  }
  void Append(OpaquePtr dest, std::unique_ptr<Any> data) override {
    return OpsMethod<OpsKind::kAppend, T>::Run(dest, std::move(data));
  }
  void AppendConst(OpaquePtr dest, const Any& data) override {
    return OpsMethod<OpsKind::kAppendConst, T>::Run(dest, data);
  }
  void Count(OpaquePtr dest) override {
    return OpsMethod<OpsKind::kCount, T>::Run(dest);
  }
  void Parse(absl::string_view in, OpsResult* out) override {
    return OpsMethod<OpsKind::kParse, T>::Run(in, out);
  }
  void Open(absl::string_view in, absl::string_view mode,
            OpsResult* out) override {
    return OpsMethod<OpsKind::kOpen, T>::Run(in, mode, out);
  }
  bool IsSupported(OpsKind ops) override {
    return OpsIsSupportedImpl<T>(
        ops, absl::make_index_sequence<size_t(OpsKind::kMaxOpsKind)>{});
  }
  absl::string_view GetTypeName() override { return TypeName<T>(); }
  std::string GetTypeHint() override { return TypeHintTraits<T>::Run(); }
  std::string FormatValue(const Any& val) override {
    return FormatTraits<T>::Run(AnyCast<T>(val));
  }
  const std::type_info& GetTypeInfo() override { return typeid(T); }
  // See below.
  Operations* GetValueTypeOps() override;
};

template <typename T>
Operations* GetOperationsInstance();

template <typename T, bool = IsAppendSupported<T>{}>
struct GetValueTypeOperations;

template <typename T>
struct GetValueTypeOperations<T, false> {
  static Operations* Run() { return nullptr; }
};

template <typename T>
struct GetValueTypeOperations<T, true> {
  static Operations* Run() {
    using ValueType = ValueTypeOf<T>;
    return GetOperationsInstance<ValueType>();
  }
};

template <typename T>
Operations* GetOperationsInstance() {
  static OperationsImpl<T> g_operations;
  return &g_operations;
}

template <typename T>
Operations* OperationsImpl<T>::GetValueTypeOps() {
  return GetValueTypeOperations<T>::Run();
}

}  // namespace operations_internal

template <typename T>
Operations* Operations::GetInstance() {
  return operations_internal::GetOperationsInstance<T>();
}

}  // namespace internal
}  // namespace argparse
