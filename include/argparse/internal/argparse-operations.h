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
#include "argparse/internal/argparse-ops-result.h"
#include "argparse/internal/argparse-port.h"

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
  virtual void Parse(const std::string& in, OpsResult* out) = 0;
  virtual void Open(const std::string& in, OpenMode, OpsResult* out) = 0;
  virtual bool IsSupported(OpsKind ops) = 0;
  virtual absl::string_view GetTypeName() = 0;
  virtual std::string GetTypeHint() = 0;
  virtual const std::type_info& GetTypeInfo() = 0;
  virtual std::string FormatValue(const Any& val) = 0;
  virtual ~Operations() {}

  template <typename T>
  static Operations* GetOps();

  template <typename T>
  static Operations* GetValueTypeOps();
};

template <typename T>
using TypeCallbackPrototype = void(const std::string&, Result<T>*);

// There is an alternative for those using exception.
// You convert string into T and throw ArgumentError if something bad happened.
template <typename T>
using TypeCallbackPrototypeThrows = T(const std::string&);

// The prototype for action. An action normally does not report errors.
template <typename T, typename V>
using ActionCallbackPrototype = void(T*, Result<V>);

// Can only take a Result without dest.
// template <typename T, typename V>
// using ActionCallbackPrototypeNoDest = void(Result<V>);

// This is the type eraser of user's callback to the type() option.
class TypeCallback {
 public:
  virtual ~TypeCallback() {}
  virtual void Run(const std::string& in, OpsResult* out) = 0;
  virtual std::string GetTypeHint() = 0;
};

// Similar to TypeCallback, but is for action().
class ActionCallback {
 public:
  virtual ~ActionCallback() {}
  virtual void Run(OpaquePtr dest, std::unique_ptr<Any> data) = 0;
};

// Extracted the bool value from AppendTraits.
template <typename T>
struct IsAppendSupported
    : portability::bool_constant<bool(AppendTraits<T>::Run)> {};

template <typename T, bool = IsAppendSupported<T>{}>
struct IsAppendConstSupportedImpl;
template <typename T>
struct IsAppendConstSupportedImpl<T, false> : std::false_type {};
template <typename T>
struct IsAppendConstSupportedImpl<T, true>
    : std::is_copy_assignable<ValueTypeOf<T>> {};

template <typename T>
struct IsAppendConstSupported : IsAppendConstSupportedImpl<T> {};

template <typename T>
struct IsOpenSupported : portability::bool_constant<bool(OpenTraits<T>::Run)> {
};

template <OpsKind Ops, typename T>
struct IsOpsSupported : std::false_type {};

template <typename T>
struct IsOpsSupported<OpsKind::kStore, T>
    : portability::bool_constant<std::is_copy_assignable<T>{} ||
                                 std::is_move_assignable<T>{}> {};

template <typename T>
struct IsOpsSupported<OpsKind::kStoreConst, T> : std::is_copy_assignable<T> {};

template <typename T>
struct IsOpsSupported<OpsKind::kAppend, T> : IsAppendSupported<T> {};

template <typename T>
struct IsOpsSupported<OpsKind::kAppendConst, T> : IsAppendConstSupported<T> {};

template <typename T>
struct IsOpsSupported<OpsKind::kCount, T> : std::is_integral<T> {};

template <typename T>
struct IsOpsSupported<OpsKind::kParse, T>
    : portability::bool_constant<bool(ParseTraits<T>::Run)> {};

template <typename T>
struct IsOpsSupported<OpsKind::kOpen, T> : IsOpenSupported<T> {};

// Put the code used only in this module here.
namespace operations_internal {

template <typename T>
void ConvertResults(Result<T>* in, OpsResult* out) {
  out->has_error = in->has_error();
  if (out->has_error) {
    out->errmsg = in->ReleaseError();
  } else if (in->has_value()) {
    out->value = MakeAny<T>(in->ReleaseValue());
  }
}

template <OpsKind Ops, typename T, bool = IsOpsSupported<Ops, T>{}>
struct OpsMethod;

template <OpsKind Ops, typename T>
struct OpsMethod<Ops, T, false> {
  template <typename... Args>
  static void Run(Args&&...) {
    ARGPARSE_CHECK_F(
        false,
        "Operation %s is not supported by type %s. Please specialize one of "
        "AppendTraits, ParseTraits, TypeHintTraits, FormatTraits, and "
        "OpenTraits, or pass in a callback.",
        OpsToString(Ops), TypeName<T>());
  }
};

template <typename T>
struct OpsMethod<OpsKind::kStore, T, true> {
  static void Run(OpaquePtr dest, std::unique_ptr<Any> data) {
    if (data) {
      auto value = AnyCast<T>(std::move(data));
      dest.PutValue(std::move_if_noexcept(value));
    }
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
  static void Run(const std::string& in, OpsResult* out) {
    auto conversion_result = ParseTraits<T>::Run(in);
    *out = OpsResult(std::move(conversion_result));
  }
};

template <typename T>
struct OpsMethod<OpsKind::kOpen, T, true> {
  static void Run(const std::string& in, OpenMode mode, OpsResult* out) {
    Result<T> result;
    OpenTraits<T>::Run(in, mode, &result);
    ConvertResults(&result, out);
  }
};

template <typename T, std::size_t... OpsIndices>
bool OpsIsSupportedImpl(OpsKind ops, absl::index_sequence<OpsIndices...>) {
  static constexpr bool kFlagArray[] = {
      (IsOpsSupported<static_cast<OpsKind>(OpsIndices), T>{})...};
  return kFlagArray[std::size_t(ops)];
}

template <typename T>
class OperationsImpl : public Operations {
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
  void Parse(const std::string& in, OpsResult* out) override {
    return OpsMethod<OpsKind::kParse, T>::Run(in, out);
  }
  void Open(const std::string& in, OpenMode mode, OpsResult* out) override {
    return OpsMethod<OpsKind::kOpen, T>::Run(in, mode, out);
  }
  bool IsSupported(OpsKind ops) override {
    constexpr auto kMaxOpsKind = static_cast<std::size_t>(OpsKind::kMaxOpsKind);
    return OpsIsSupportedImpl<T>(ops, absl::make_index_sequence<kMaxOpsKind>{});
  }
  absl::string_view GetTypeName() override { return TypeName<T>(); }
  std::string GetTypeHint() override { return TypeHintTraits<T>::Run(); }
  std::string FormatValue(const Any& val) override {
    return FormatTraits<T>::Run(AnyCast<T>(val));
  }
  const std::type_info& GetTypeInfo() override { return typeid(T); }
};

template <typename T, bool = IsAppendSupported<T>{}>
struct GetValueTypeOpsImpl;

template <typename T>
struct GetValueTypeOpsImpl<T, false> {
  static Operations* Run() { return nullptr; }
};
template <typename T>
struct GetValueTypeOpsImpl<T, true> {
  static Operations* Run() { return new OperationsImpl<ValueTypeOf<T>>; }
};

template <typename T>
Operations* GetOpsImpl() {
  static auto* g_operations = new OperationsImpl<T>;
  return g_operations;
}

template <typename T>
class TypeCallbackImpl : public TypeCallback {
 public:
  using CallbackType = std::function<TypeCallbackPrototype<T>>;
  explicit TypeCallbackImpl(CallbackType cb) : callback_(std::move(cb)) {}

  void Run(const std::string& in, OpsResult* out) override {
    // Result<T> result;
    // std::invoke(callback_, in, &result);
    // ConvertResults(&result, out);
  }

  std::string GetTypeHint() override { return TypeHint<T>(); }

 private:
  CallbackType callback_;
};

// Provided by user's callable obj.
template <typename T, typename V>
class CustomActionCallback : public ActionCallback {
 public:
  using CallbackType = std::function<ActionCallbackPrototype<T, V>>;
  explicit CustomActionCallback(CallbackType cb) : callback_(std::move(cb)) {
    ARGPARSE_DCHECK(callback_);
  }

 private:
  void Run(OpaquePtr dest_ptr, std::unique_ptr<Any> data) override {
    Result<V> result(AnyCast<V>(std::move(data)));
    auto* obj = dest_ptr.template Cast<T>();
    callback_(obj, std::move(result));
  }

  CallbackType callback_;
};

/// Strip the class from a method type
template <typename T>
struct RemoveClass {};
template <typename C, typename R, typename... A>
struct RemoveClass<R (C::*)(A...)> {
  using type = R(A...);
};
template <typename C, typename R, typename... A>
struct RemoveClass<R (C::*)(A...) const> {
  using type = R(A...);
};
template <typename F>
struct StripFunctionObject {
  using type = typename RemoveClass<decltype(&F::operator())>::type;
};

// Extracts the function signature from a function, function pointer or lambda.
template <typename Func, typename F = absl::remove_reference_t<Func>>
using FunctionSignature = absl::conditional_t<
    std::is_function<F>::value, F,
    typename absl::conditional_t<
        std::is_pointer<F>::value || std::is_member_pointer<F>::value,
        std::remove_pointer<F>, StripFunctionObject<F>>::type>;

template <typename T>
struct IsFunctionPointer : std::is_function<absl::remove_pointer_t<T>> {};

template <typename T, typename SFINAE = void>
struct IsFunctor : std::false_type {};

// Note: this will fail on auto lambda and overloaded operator().
// But you should not use these as input to callback.
template <typename T>
struct IsFunctor<T, absl::void_t<decltype(&T::operator())>> : std::true_type {};

template <typename Callback, typename T>
std::unique_ptr<TypeCallback> MakeTypeCallbackImpl(Callback&& cb,
                                                   TypeCallbackPrototype<T>*) {
  return absl::make_unique<TypeCallbackImpl<T>>(std::forward<Callback>(cb));
}

template <typename Callback, typename T>
std::unique_ptr<TypeCallback> MakeTypeCallbackImpl(
    Callback&& cb, TypeCallbackPrototypeThrows<T>*) {
  auto wrapped_cb = [cb](const std::string& in, Result<T>* out) {
    try {
      *out = cb(in);
    } catch (const ArgumentError& e) {
      out->SetError(e.what());
    }
  };
  return absl::make_unique<TypeCallbackImpl<T>>(std::move(wrapped_cb));
}

template <typename Callback, typename T, typename V>
std::unique_ptr<ActionCallback> MakeActionCallbackImpl(
    Callback&& cb, ActionCallbackPrototype<T, V>*) {
  return absl::make_unique<CustomActionCallback<T, V>>(
      std::forward<Callback>(cb));
}

}  // namespace operations_internal

template <typename T>
Operations* Operations::GetOps() {
  return operations_internal::GetOpsImpl<T>();
}

template <typename T>
Operations* Operations::GetValueTypeOps() {
  return operations_internal::GetValueTypeOpsImpl<T>::Run();
}

template <typename Func, typename F = absl::decay_t<Func>>
struct IsCallback
    : portability::bool_constant<operations_internal::IsFunctionPointer<F>{} ||
                                 operations_internal::IsFunctor<F>{}> {};

template <typename Callback>
std::unique_ptr<TypeCallback> MakeTypeCallback(Callback&& cb) {
  using namespace operations_internal;
  return MakeTypeCallbackImpl(std::forward<Callback>(cb),
                              (FunctionSignature<Callback>*)nullptr);
}

template <typename Callback>
std::unique_ptr<ActionCallback> MakeActionCallback(Callback&& cb) {
  using namespace operations_internal;
  return MakeActionCallbackImpl(std::forward<Callback>(cb),
                                (FunctionSignature<Callback>*)nullptr);
}

}  // namespace internal
}  // namespace argparse
