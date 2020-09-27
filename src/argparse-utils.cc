#include "argparse/argparse-utils.h"

#include <cxxabi.h>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <typeindex>
#include <cstring>
#include <ostream>

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

std::ostream& operator<<(std::ostream& os, const StringView& in) {
  return os << in.data();
}

static std::string Demangle(const char* mangled_name) {
  std::size_t length;
  int status;
  const char* realname =
      abi::__cxa_demangle(mangled_name, nullptr, &length, &status);

  if (status) {
    static constexpr const char kDemangleFailedSub[] =
        "<error-type(demangle failed)>";
    return kDemangleFailedSub;
  }

  ARGPARSE_DCHECK(realname);
  std::string result(realname, length);
  std::free((void*)realname);
  return result;
}

const char* TypeNameImpl(const std::type_info& type) {
  static std::map<std::type_index, std::string> g_typenames;
  auto iter = g_typenames.find(type);
  if (iter == g_typenames.end()) {
    g_typenames.emplace(type, Demangle(type.name()));
  }
  return g_typenames[type].c_str();
}

void CheckFailed(SourceLocation loc, const char* fmt, ...) {
  std::fprintf(stderr, "Error at %s:%d:%s: ", loc.filename, loc.line,
               loc.function);

  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(stderr, fmt, ap);
  va_end(ap);

  std::fprintf(
      stderr,
      "\n\nPlease check your code and read the documents of argparse.\n\n");
  std::abort();
}

bool IsValidPositionalName(const std::string& name) {
  auto len = name.size();
  if (!len || !std::isalpha(name[0]))
    return false;

  return std::all_of(name.begin() + 1, name.end(), [](char c) {
    return std::isalnum(c) || c == '-' || c == '_';
  });
}

bool IsValidOptionName(const std::string& name) {
  auto len = name.size();
  if (len < 2 || name[0] != '-')
    return false;
  if (len == 2)  // This rules out -?, -* -@ -= --
    return std::isalnum(name[1]);
  // check for long-ness.
  // TODO: fixthis.
  ARGPARSE_CHECK_F(
      name[1] == '-',
      "Single-dash long option (i.e., -jar) is not supported. Please use "
      "GNU-style long option (double-dash)");

  return std::all_of(name.begin() + 2, name.end(), [](char c) {
    return c == '-' || c == '_' || std::isalnum(c);
  });
}

}  // namespace argparse
