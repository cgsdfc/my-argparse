// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "absl/memory/memory.h"
#include "absl/meta/type_traits.h"
#include "absl/utility/utility.h"
#include "argparse/internal/argparse-logging.h"

namespace argparse {
namespace internal {
namespace any_internal {

// Our version of any.
class Any {
 public:
  ~Any() {
    ARGPARSE_INTERNAL_DCHECK(destructor_func_, "");
    destructor_func_(this);
  }

  template <typename T>
  bool TypeIs() const;

 protected:
  using DestructorFunc = void (*)(Any*);
  explicit Any(DestructorFunc dtor) : destructor_func_(dtor) {}
  DestructorFunc destructor_func() const { return destructor_func_; }

 private:
  DestructorFunc destructor_func_;
};

template <typename T, typename... Args>
void ConstructAt(T* p, Args&&... args) {
  ::new (p) T{std::forward<Args>(args)...};
}

template <typename T>
void DestroyAt(T* p) {
  p->~T();
}

template <typename T>
class AnyImpl final : public Any {
 private:
  static_assert(!std::is_array<T>::value, "T must not be array type");

 public:
  template <typename... Args>
  explicit AnyImpl(absl::in_place_t, Args&&... args)
      : Any(GetDestructorFunc()) {
    ConstructAt(GetPtr(), std::forward<Args>(args)...);
  }

  T* GetPtr() & { return reinterpret_cast<T*>(&storage_); }
  const T* GetConstPtr() const& {
    return reinterpret_cast<const T*>(&storage_);
  }
  T& GetRef() & { return *GetPtr(); }
  const T& GetConstRef() const& { return *GetConstPtr(); }
  T&& GetRValueRef() && { return std::move(GetRef()); }
  const T&& GetRValueConstRef() const&& { return std::move(GetConstRef()); }

  static AnyImpl* FromPtr(Any* self) {
    ARGPARSE_INTERNAL_DCHECK(self, "Nullptr passed to FromPtr()");
    ARGPARSE_INTERNAL_DCHECK(self->TypeIs<T>(), "FromPtr(): Type mismatched");
    return static_cast<AnyImpl*>(self);
  }

  static const AnyImpl* FromConstPtr(const Any* self) {
    return FromPtr(const_cast<Any*>(self));
  }

  static AnyImpl& FromRef(Any& self) {
    return *FromPtr(&self);  //
  }

  static const AnyImpl& FromConstRef(const Any& self) {
    return FromRef(const_cast<Any&>(self));
  }

  static AnyImpl&& FromRValueRef(Any&& self) {
    return std::move(FromRef(self));
  }

  static const AnyImpl&& FromRValueConstRef(const Any&& self) {
    return std::move(FromConstRef(self));
  }

  static constexpr DestructorFunc GetDestructorFunc() {
    return &DestructorFuncImpl;
  }

 private:
  static void DestructorFuncImpl(Any* self) {
    DestroyAt(FromPtr(self)->GetPtr());
  }

  typename std::aligned_storage<sizeof(T), alignof(T)>::type storage_;
};

template <typename T>
bool Any::TypeIs() const {
  return destructor_func() == AnyImpl<T>::GetDestructorFunc();
}

template <typename T, typename... Args>
std::unique_ptr<Any> MakeAny(Args&&... args) {
  return absl::make_unique<AnyImpl<T>>(absl::in_place,
                                       std::forward<Args>(args)...);
}

template <typename T>
const T& AnyCast(const Any& any) {
  return AnyImpl<T>::FromConstRef(any).GetConstRef();
}

template <typename T>
T& AnyCast(Any& any) {
  return AnyImpl<T>::FromRef(any).GetRef();
}

template <typename T>
const T* AnyCast(const Any* any) {
  return AnyImpl<T>::FromConstPtr(any)->GetConstPtr();
}

template <typename T>
T* AnyCast(Any* any) {
  return AnyImpl<T>::FromPtr(any)->GetPtr();
}

template <typename T>
T&& AnyCast(Any&& any) {
  return AnyImpl<T>::FromRValueRef(std::move(any)).GetRValueRef();
}

}  // namespace any_internal

using any_internal::Any;
using any_internal::AnyCast;
using any_internal::MakeAny;

template <typename T>
ABSL_MUST_USE_RESULT T TakeValueAndDiscard(std::unique_ptr<Any>* any) {
  auto tmp = std::move(*any);
  return std::move_if_noexcept(*AnyCast<T>(tmp.get()));
}

}  // namespace internal
}  // namespace argparse
