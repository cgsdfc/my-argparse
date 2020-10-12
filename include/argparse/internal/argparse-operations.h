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

// inline unsigned  GetMaxOpsKind() {return static_cast}
// inline constexpr std::size_t kMaxOpsKind = std::size_t(OpsKind::kOpen) + 1;

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
  virtual StringView GetTypeName() = 0;
  virtual std::string GetTypeHint() = 0;
  virtual const std::type_info& GetTypeInfo() = 0;
  virtual std::string FormatValue(const Any& val) = 0;
  virtual ~Operations() {}
};

// How to create a vtable?
class OpsFactory {
 public:
  virtual std::unique_ptr<Operations> CreateOps() = 0;
  // If this type has a concept of value_type, create a handle.
  virtual std::unique_ptr<Operations> CreateValueTypeOps() = 0;
  virtual ~OpsFactory() {}
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
struct OpsImpl;

template <OpsKind Ops, typename T>
struct OpsImpl<Ops, T, false> {
  template <typename... Args>
  static void Run(Args&&...) {
    ARGPARSE_CHECK_F(
        false,
        "Operation %s is not supported by type %s. Please specialize one of "
        "AppendTraits, ParseTraits and OpenTraits, or pass in a callback.",
        OpsToString(Ops), TypeName<T>());
  }
};

template <typename T>
struct OpsImpl<OpsKind::kStore, T, true> {
  static void Run(OpaquePtr dest, std::unique_ptr<Any> data) {
    if (data) {
      auto value = AnyCast<T>(std::move(data));
      dest.PutValue(std::move_if_noexcept(value));
    }
  }
};

template <typename T>
struct OpsImpl<OpsKind::kStoreConst, T, true> {
  static void Run(OpaquePtr dest, const Any& data) {
    dest.PutValue(AnyCast<T>(data));
  }
};

template <typename T>
struct OpsImpl<OpsKind::kAppend, T, true> {
  static void Run(OpaquePtr dest, std::unique_ptr<Any> data) {
    if (data) {
      auto* ptr = dest.Cast<T>();
      auto value = AnyCast<ValueTypeOf<T>>(std::move(data));
      AppendTraits<T>::Run(ptr, std::move_if_noexcept(value));
    }
  }
};

template <typename T>
struct OpsImpl<OpsKind::kAppendConst, T, true> {
  static void Run(OpaquePtr dest, const Any& data) {
    auto* ptr = dest.Cast<T>();
    auto value = AnyCast<ValueTypeOf<T>>(data);
    AppendTraits<T>::Run(ptr, value);
  }
};

template <typename T>
struct OpsImpl<OpsKind::kCount, T, true> {
  static void Run(OpaquePtr dest) {
    auto* ptr = dest.Cast<T>();
    ++(*ptr);
  }
};

template <typename T>
struct OpsImpl<OpsKind::kParse, T, true> {
  static void Run(const std::string& in, OpsResult* out) {
    Result<T> result;
    ParseTraits<T>::Run(in, &result);
    ConvertResults(&result, out);
  }
};

template <typename T>
struct OpsImpl<OpsKind::kOpen, T, true> {
  static void Run(const std::string& in, OpenMode mode, OpsResult* out) {
    Result<T> result;
    OpenTraits<T>::Run(in, mode, &result);
    ConvertResults(&result, out);
  }
};

template <typename T, std::size_t... OpsIndices>
bool OpsIsSupportedImpl(OpsKind ops,
                        portability::index_sequence<OpsIndices...>) {
  static constexpr std::size_t kMaxOps = sizeof...(OpsIndices);
  static constexpr bool kArray[kMaxOps] = {
      (IsOpsSupported<static_cast<OpsKind>(OpsIndices), T>{})...};
  auto index = std::size_t(ops);
  ARGPARSE_DCHECK(index < kMaxOps);
  return kArray[index];
}

template <typename T>
class OperationsImpl : public Operations {
 public:
  void Store(OpaquePtr dest, std::unique_ptr<Any> data) override {
    OpsImpl<OpsKind::kStore, T>::Run(dest, std::move(data));
  }
  void StoreConst(OpaquePtr dest, const Any& data) override {
    OpsImpl<OpsKind::kStoreConst, T>::Run(dest, data);
  }
  void Append(OpaquePtr dest, std::unique_ptr<Any> data) override {
    OpsImpl<OpsKind::kAppend, T>::Run(dest, std::move(data));
  }
  void AppendConst(OpaquePtr dest, const Any& data) override {
    OpsImpl<OpsKind::kAppendConst, T>::Run(dest, data);
  }
  void Count(OpaquePtr dest) override {
    OpsImpl<OpsKind::kCount, T>::Run(dest);
  }
  void Parse(const std::string& in, OpsResult* out) override {
    OpsImpl<OpsKind::kParse, T>::Run(in, out);
  }
  void Open(const std::string& in, OpenMode mode, OpsResult* out) override {
    OpsImpl<OpsKind::kOpen, T>::Run(in, mode, out);
  }
  bool IsSupported(OpsKind ops) override {
    const auto kMaxOpsKind = static_cast<std::size_t>(OpsKind::kMaxOpsKind);
    return OpsIsSupportedImpl<T>(
        ops, portability::make_index_sequence<kMaxOpsKind>{});
  }
  StringView GetTypeName() override { return TypeName<T>(); }
  std::string GetTypeHint() override { return TypeHintTraits<T>::Run(); }
  std::string FormatValue(const Any& val) override {
    return FormatTraits<T>::Run(AnyCast<T>(val));
  }
  const std::type_info& GetTypeInfo() override { return typeid(T); }
};

template <typename T>
std::unique_ptr<Operations> CreateOperations() {
  return portability::make_unique<OperationsImpl<T>>();
}

template <typename T, bool = IsAppendSupported<T>{}>
struct CreateValueTypeOpsImpl;

template <typename T>
struct CreateValueTypeOpsImpl<T, false> {
  static std::unique_ptr<Operations> Run() { return nullptr; }
};
template <typename T>
struct CreateValueTypeOpsImpl<T, true> {
  static std::unique_ptr<Operations> Run() {
    return CreateOperations<ValueTypeOf<T>>();
  }
};

template <typename T>
class OpsFactoryImpl : public OpsFactory {
 public:
  std::unique_ptr<Operations> CreateOps() override {
    return CreateOperations<T>();
  }
  std::unique_ptr<Operations> CreateValueTypeOps() override {
    return CreateValueTypeOpsImpl<T>::Run();
  }
};

template <typename T>
std::unique_ptr<OpsFactory> CreateOpsFactory() {
  return portability::make_unique<OpsFactoryImpl<T>>();
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
template <typename Func, typename F = portability::remove_reference_t<Func>>
using FunctionSignature = portability::conditional_t<
    std::is_function<F>::value, F,
    typename portability::conditional_t<
        std::is_pointer<F>::value || std::is_member_pointer<F>::value,
        std::remove_pointer<F>, StripFunctionObject<F>>::type>;

template <typename T>
struct IsFunctionPointer : std::is_function<portability::remove_pointer_t<T>> {
};

template <typename T, typename SFINAE = void>
struct IsFunctor : std::false_type {};

// Note: this will fail on auto lambda and overloaded operator().
// But you should not use these as input to callback.
template <typename T>
struct IsFunctor<T, std::void_t<decltype(&T::operator())>> : std::true_type {};

template <typename Func, typename F = portability::decay_t<Func>>
struct IsCallback
    : portability::bool_constant<IsFunctionPointer<F>{} || IsFunctor<F>{}> {};

template <typename Callback, typename T>
std::unique_ptr<TypeCallback> MakeTypeCallbackImpl(Callback&& cb,
                                                   TypeCallbackPrototype<T>*) {
  return portability::make_unique<TypeCallbackImpl<T>>(
      std::forward<Callback>(cb));
}

template <typename Callback, typename T>
std::unique_ptr<TypeCallback> MakeTypeCallbackImpl(
    Callback&& cb, TypeCallbackPrototypeThrows<T>*) {
  auto wrapped_cb = [cb](const std::string& in, Result<T>* out) {
    try {
      *out = portability::invoke(cb, in);
    } catch (const ArgumentError& e) {
      out->SetError(e.what());
    }
  };
  return portability::make_unique<TypeCallbackImpl<T>>(std::move(wrapped_cb));
}

template <typename Callback>
std::unique_ptr<TypeCallback> MakeTypeCallback(Callback&& cb) {
  return MakeTypeCallbackImpl(std::forward<Callback>(cb),
                              (FunctionSignature<Callback>*)nullptr);
}

template <typename Callback, typename T, typename V>
std::unique_ptr<ActionCallback> MakeActionCallbackImpl(
    Callback&& cb, ActionCallbackPrototype<T, V>*) {
  return portability::make_unique<CustomActionCallback<T, V>>(
      std::forward<Callback>(cb));
}

template <typename Callback>
std::unique_ptr<ActionCallback> MakeActionCallback(Callback&& cb) {
  return MakeActionCallbackImpl(std::forward<Callback>(cb),
                                (FunctionSignature<Callback>*)nullptr);
}

}  // namespace internal
}  // namespace argparse
