#include "argparse/base/string_view.h"

#include <cstring>

namespace argparse {

StringView::StringView(const char* data)
    : StringView(data, std::strlen(data)) {}

std::unique_ptr<char[]> StringView::ToCharArray() const {
  auto dest = std::make_unique<char[]>(size() + 1);
  std::char_traits<char>::copy(dest.get(), data(), size());
  dest[size()] = 0;
  return dest;
}

int StringView::Compare(const StringView& a, const StringView& b) {
  return std::char_traits<char>::compare(a.data(), b.data(),
                                         std::min(a.size(), b.size()));
}

StringView::StringView(const char* data, std::size_t size)
    : data_(data), size_(size) {
  ARGPARSE_DCHECK_F(data, "data shouldn't be null");
  ARGPARSE_DCHECK(std::strlen(data) == size);
}

}  // namespace argparse
