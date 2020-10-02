// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

namespace argparse {

// Base class of builders for some kind of object T.
template <typename T>
class BuilderOf {
 protected:
  using ObjectType = T;
  BuilderOf() = default;
  explicit BuilderOf(std::unique_ptr<T> obj) : object_(std::move(obj)) {}
  void SetObject(std::unique_ptr<T> obj) { object_ = std::move(obj); }
  T* GetObject() { return object_.get(); }
  // Default impl of Build().
  std::unique_ptr<T> Build() { return std::move(object_); }

 private:
  std::unique_ptr<T> object_;
};

// Friends of every Builder class to access their Build() method.
struct BuilderAccessor {
  template <typename Builder>
  static auto Build(Builder* builder) -> decltype(builder->Build()) {
    return builder->Build();
  }
};

class AnyValue : private BuilderOf<internal::Any> {
 public:
  template <typename T,
            std::enable_if_t<!std::is_convertible<T, AnyValue>{}>* = 0>
  AnyValue(T&& val) {
    auto obj = internal::MakeAny<std::decay_t<T>>(std::forward<T>(val));
    this->SetObject(std::move(obj));
  }

 private:
  friend class BuilderAccessor;
};

class TypeCallback : private BuilderOf<internal::TypeCallback> {
 public:
  template <typename T, std::enable_if_t<internal::IsCallback<T>{}>* = 0>
  TypeCallback(T&& cb) {
    this->SetObject(internal::MakeTypeCallback(std::forward<T>(cb)));
  }

 private:
  friend class BuilderAccessor;
};

class ActionCallback : private BuilderOf<internal::ActionCallback> {
 public:
  template <typename T, std::enable_if_t<internal::IsCallback<T>{}>* = 0>
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
