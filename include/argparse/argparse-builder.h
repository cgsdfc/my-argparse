// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-internal.h"

namespace argparse {
class BuilderAccessor;

// Base class of builders for some kind of object T.
template <typename T>
class BuilderOf {
 protected:
  // Derived can override this.
  using ObjectType = T;
  BuilderOf() = default;
  explicit BuilderOf(std::unique_ptr<T> obj) : object_(std::move(obj)) {}
  void SetObject(std::unique_ptr<T> obj) { object_ = std::move(obj); }
  T* GetObject() { return object_.get(); }

  // Default impl of Build(). Derived can override this.
  std::unique_ptr<ObjectType> Build() { return std::move(object_); }
  bool HasObject() const { return object_ != nullptr; }

 private:
  friend class BuilderAccessor;
  std::unique_ptr<T> object_;
};

// Friends of every Builder class to access their Build() method.
struct BuilderAccessor {
  template <typename Builder>
  static auto Build(Builder* builder) -> decltype(builder->Build()) {
    return builder->Build();
  }
};

template <typename Builder>
auto GetBuiltObject(Builder* b) -> decltype(BuilderAccessor::Build(b)) {
  return BuilderAccessor::Build(b);
}

class AnyValue : private BuilderOf<internal::Any> {
 public:
  template <typename T,
            std::enable_if_t<!std::is_convertible<T, AnyValue>{}>* = nullptr>
  AnyValue(T&& val) {
    auto obj = internal::MakeAny<std::decay_t<T>>(std::forward<T>(val));
    this->SetObject(std::move(obj));
  }

 private:
  friend class BuilderAccessor;
};

class TypeCallback : private BuilderOf<internal::TypeCallback> {
 public:
  template <typename T, std::enable_if_t<internal::IsCallback<T>{}>* = nullptr>
  TypeCallback(T&& cb) {
    this->SetObject(internal::MakeTypeCallback(std::forward<T>(cb)));
  }

 private:
  friend class BuilderAccessor;
};

class ActionCallback : private BuilderOf<internal::ActionCallback> {
 public:
  template <typename T, std::enable_if_t<internal::IsCallback<T>{}>* = nullptr>
  ActionCallback(T&& cb) {
    this->SetObject(internal::MakeActionCallback(std::forward<T>(cb)));
  }

 private:
  friend class BuilderAccessor;
};

// Creator of DestInfo. For those that need a DestInfo, just take Dest
// as an arg.
class Dest : private BuilderOf<internal::DestInfo> {
 public:
  template <typename T>
  Dest(T* ptr) {
    this->SetObject(internal::DestInfo::CreateFromPtr(ptr));
  }
  Dest() = default;
  using BuilderOf<internal::DestInfo>::HasObject;

 private:
  friend class BuilderAccessor;
};

class Names : private BuilderOf<internal::NamesInfo> {
 public:
  Names(const char* name) : Names(std::string(name)) {
    ARGPARSE_CHECK_F(name, "name should not be null");
  }
  Names(std::string name);
  Names(std::initializer_list<std::string> names);

 private:
  friend class BuilderAccessor;
};

}  // namespace argparse
