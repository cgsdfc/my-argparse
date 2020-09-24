#pragma once

#include <ostream>
#include <string>

#include "argparse/base/common.h"

namespace argparse {

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

inline std::ostream& operator<<(std::ostream& os, const StringView& in) {
  return os << in.data();
}

}  // namespace argparse
