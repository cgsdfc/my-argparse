#pragma once

// Utility classes
namespace argparse {

// Our version of any.
class Any {
 public:
  virtual ~Any() {}
  virtual std::type_index GetType() const = 0;
};

template <typename T>
class AnyImpl : public Any {
 public:
  explicit AnyImpl(T&& val) : value_(std::move(val)) {}
  explicit AnyImpl(const T& val) : value_(val) {}
  template <typename... Args>
  explicit AnyImpl(std::in_place_type_t<T>, Args&&... args)
      : value_(std::forward<Args>(args)...) {}

  ~AnyImpl() override {}
  std::type_index GetType() const override { return typeid(T); }

  T ReleaseValue() { return std::move_if_noexcept(value_); }
  const T& value() const { return value_; }

  static AnyImpl* FromAny(Any* any) {
    ARGPARSE_DCHECK(any && any->GetType() == typeid(T));
    return static_cast<AnyImpl*>(any);
  }
  static const AnyImpl& FromAny(const Any& any) {
    ARGPARSE_DCHECK(any.GetType() == typeid(T));
    return static_cast<const AnyImpl&>(any);
  }

 private:
  T value_;
};

template <typename T, typename... Args>
std::unique_ptr<Any> MakeAny(Args&&... args) {
  return std::make_unique<AnyImpl<T>>(std::in_place_type<T>,
                                      std::forward<Args>(args)...);
}

// template <typename T, typename... Args>
// AnyImpl<T> MakeAnyOnStack(Args&&... args) {
//   return AnyImpl<T>(std::forward<Args>(args)...);
// }

template <typename T>
T AnyCast(std::unique_ptr<Any> any) {
  ARGPARSE_DCHECK(any);
  return AnyImpl<T>::FromAny(any.get())->ReleaseValue();
}

template <typename T>
T AnyCast(const Any& any) {
  return AnyImpl<T>::FromAny(any).value();
}

class ArgArray {
 public:
  ArgArray(int argc, const char** argv)
      : argc_(argc), argv_(const_cast<char**>(argv)) {}
  ArgArray(std::vector<const char*>& args)
      : ArgArray(args.size(), args.data()) {}

  int argc() const { return argc_; }
  std::size_t size() const { return argc(); }

  char** argv() const { return argv_; }
  char* operator[](std::size_t i) {
    ARGPARSE_DCHECK(i < argc());
    return argv()[i];
  }

 private:
  int argc_;
  char** argv_;
};

// This is a type-erased type-safe void* wrapper.
// Result<T> handles user' returned value and error using a union.
template <typename T>
class Result {
 public:
  // Default is empty (!has_value && !has_error).
  Result() { ARGPARSE_DCHECK(empty()); }
  // To hold a value.
  explicit Result(T&& val) : data_(std::move(val)) {
    ARGPARSE_DCHECK(has_value());
  }
  explicit Result(const T& val) : data_(val) { ARGPARSE_DCHECK(has_value()); }

  // For now just use default. If T can't be moved, will it still work?
  Result(Result&&) = default;
  Result& operator=(Result&&) = default;

  bool has_value() const { return data_.index() == kValueIndex; }
  bool has_error() const { return data_.index() == kErrorMsgIndex; }

  void set_error(const std::string& msg) {
    data_.template emplace<kErrorMsgIndex>(msg);
    ARGPARSE_DCHECK(has_error());
  }
  void set_error(std::string&& msg) {
    data_.template emplace<kErrorMsgIndex>(std::move(msg));
    ARGPARSE_DCHECK(has_error());
  }
  void set_value(const T& val) {
    data_.template emplace<kValueIndex>(val);
    ARGPARSE_DCHECK(has_value());
  }
  void set_value(T&& val) {
    data_.template emplace<kValueIndex>(std::move(val));
    ARGPARSE_DCHECK(has_value());
  }

  Result& operator=(const T& val) {
    set_value(val);
    return *this;
  }
  Result& operator=(T&& val) {
    set_value(std::move(val));
    return *this;
  }

  // Release the err-msg (if any). Keep in error state.
  std::string release_error() {
    ARGPARSE_DCHECK(has_error());
    // We know that std::string is moveable.
    return std::get<kErrorMsgIndex>(std::move(data_));
  }
  const std::string& get_error() const {
    ARGPARSE_DCHECK(has_error());
    return std::get<kErrorMsgIndex>(data_);
  }

  // Release the value, Keep in value state.
  T release_value() {
    ARGPARSE_DCHECK(has_value());
    // But we don't know T is moveable or not.
    return std::get<kValueIndex>(std::move_if_noexcept(data_));
  }
  const T& get_value() const {
    ARGPARSE_DCHECK(has_value());
    return std::get<kValueIndex>(data_);
  }
  // Goes back to empty state.
  void reset() {
    data_.template emplace<kEmptyIndex>(NoneType{});
    ARGPARSE_DCHECK(empty());
  }

 private:
  bool empty() const { return kEmptyIndex == data_.index(); }
  enum Indices {
    kEmptyIndex,
    kErrorMsgIndex,
    kValueIndex,
  };
  std::variant<NoneType, std::string, T> data_;
};

class DestPtr {
 public:
  template <typename T>
  explicit DestPtr(T* ptr) : type_(typeid(T)), ptr_(ptr) {}
  DestPtr() = default;

  // Copy content to out.
  template <typename T>
  const T& load() const {
    ARGPARSE_DCHECK(type_ == typeid(T));
    return *reinterpret_cast<const T*>(ptr_);
  }
  template <typename T>
  void load(T* out) const {
    *out = load<T>();
  }

  template <typename T>
  T* load_ptr() const {
    ARGPARSE_DCHECK(type_ == typeid(T));
    return reinterpret_cast<T*>(ptr_);
  }
  template <typename T>
  void load_ptr(T** ptr_out) const {
    *ptr_out = load_ptr<T>();
  }

  template <typename T>
  void store(T&& val) {
    using Type = std::remove_reference_t<T>;
    ARGPARSE_DCHECK(type_ == typeid(Type));
    *reinterpret_cast<T*>(ptr_) = std::forward<T>(val);
  }

  template <typename T>
  void reset(T* ptr) {
    DestPtr that(ptr);
    swap(that);
  }

  void swap(DestPtr& that) {
    std::swap(this->type_, that.type_);
    std::swap(this->ptr_, that.ptr_);
  }

  explicit operator bool() const { return !!ptr_; }

  std::type_index type() const { return type_; }
  void* ptr() const { return ptr_; }

 private:
  std::type_index type_ = typeid(NoneType);
  void* ptr_ = nullptr;
};

// Like std::string_view, but may be more suit our needs.
class StringView {
 public:
  StringView(const StringView&) = default;
  StringView& operator=(const StringView&) = default;

  StringView() = delete;
  StringView(std::string&&) = delete;

  // data should be non-null and null-terminated.
  StringView(const char* data);

  StringView(const std::string& in) : StringView(in.data(), in.size()) {}

  // Should be selected if data is a string literal.
  template <std::size_t N>
  StringView(const char (&data)[N]) : StringView(data, N - 1) {}

  std::size_t size() const { return size_; }
  bool empty() const { return 0 == size(); }
  const char* data() const {
    ARGPARSE_DCHECK(data_);
    return data_;
  }

  std::string ToString() const { return std::string(data_, size_); }
  std::unique_ptr<char[]> ToCharArray() const;

  static int Compare(const StringView& a, const StringView& b);
  bool operator<(const StringView& that) const {
    return Compare(*this, that) < 0;
  }
  bool operator==(const StringView& that) const {
    return Compare(*this, that) == 0;
  }

 private:
  // data should be non-null and null-terminated.
  StringView(const char* data, std::size_t size);

  // Not default-constructible.
  const char* data_;
  std::size_t size_;
};

std::ostream& operator<<(std::ostream& os, const StringView& in);

}  // namespace argparse
