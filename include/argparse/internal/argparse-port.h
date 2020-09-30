#pragma once

#define ARGPARSE_SOURCE_LOCATION_CURRENT() \
  (SourceLocation{__LINE__, __FILE__, __func__})

#define ARGPARSE_CHECK_IMPL(condition, format, ...)                         \
  do {                                                                      \
    if (!static_cast<bool>(condition))                                      \
      ::argparse::internal::CheckFailed(ARGPARSE_SOURCE_LOCATION_CURRENT(), \
                                        (format), ##__VA_ARGS__);           \
  } while (0)

// Perform a runtime check for user's error.
#define ARGPARSE_CHECK_F(expr, format, ...) \
  ARGPARSE_CHECK_IMPL((expr), (format), ##__VA_ARGS__)

// If no format, use the stringified expr.
#define ARGPARSE_CHECK(expr) ARGPARSE_CHECK_IMPL((expr), "%s", #expr)

#ifdef NDEBUG  // Not debug
#define ARGPARSE_DCHECK(expr) ((void)(expr))
#define ARGPARSE_DCHECK_F(expr, format, ...) ((void)(expr))
#else
#define ARGPARSE_DCHECK(expr) ARGPARSE_CHECK(expr)
#define ARGPARSE_DCHECK_F(expr, format, ...) \
  ARGPARSE_CHECK_F(expr, format, ##__VA_ARGS__)
#endif

namespace argparse {

// File open mode. This is not enum class since we do & | on it.
enum OpenMode {
  kModeNoMode = 0x0,
  kModeRead = 1,
  kModeWrite = 2,
  kModeAppend = 4,
  kModeTruncate = 8,
  kModeBinary = 16,
};

namespace internal {

// When an meaningless type is needed.
struct NoneType {};

struct SourceLocation {
  int line;
  const char* filename;
  const char* function;
};

[[noreturn]] void CheckFailed(SourceLocation loc, const char* fmt, ...);

OpenMode CharsToMode(const char* str);
std::string ModeToChars(OpenMode mode);

OpenMode StreamModeToMode(std::ios_base::openmode stream_mode);
std::ios_base::openmode ModeToStreamMode(OpenMode m);

struct OpsResult {
  bool has_error = false;
  std::unique_ptr<Any> value;  // null if error.
  std::string errmsg;
};

// Throw this exception will cause an error msg to be printed (via what()).
class ArgumentError final : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

}  // namespace internal
}  // namespace argparse
