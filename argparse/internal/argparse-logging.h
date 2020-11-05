// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "absl/base/attributes.h"
#include "absl/base/log_severity.h"
#include "absl/base/optimization.h"

#define ARGPARSE_INTERNAL_LOG(severity, ...)                                 \
  do {                                                                       \
    constexpr const char* argparse_internal_logging_internal_basename =      \
        ::argparse::internal::logging_internal::Basename(                    \
            __FILE__, sizeof(__FILE__) - 1);                                 \
    ::argparse::internal::logging_internal::Log(                             \
        ARGPARSE_LOGGING_INTERNAL_##severity,                                \
        argparse_internal_logging_internal_basename, __LINE__, __VA_ARGS__); \
  } while (0)

#define ARGPARSE_INTERNAL_CHECK(condition, message)                   \
  do {                                                                \
    if (ABSL_PREDICT_FALSE(!(condition))) {                           \
      ARGPARSE_INTERNAL_LOG(FATAL, "Check %s failed: %s", #condition, \
                            message);                                 \
    }                                                                 \
  } while (0)

// Debug-time checking.
#ifdef NDEBUG  // Not debug
#define ARGPARSE_INTERNAL_DCHECK(condition, message)
#else
#define ARGPARSE_INTERNAL_DCHECK(condition, message) \
  ARGPARSE_INTERNAL_CHECK(condition, message)
#endif

// Legacy alias
#define ARGPARSE_CHECK(condition) ARGPARSE_INTERNAL_CHECK(condition, "")

#define ARGPARSE_CHECK_F(condition, format, ...) \
  ARGPARSE_INTERNAL_CHECK(condition, "")

#define ARGPARSE_DCHECK(condition) ARGPARSE_INTERNAL_DCHECK(condition, "")

#define ARGPARSE_DCHECK_F(condition, format, ...) \
  ARGPARSE_INTERNAL_DCHECK(condition, "")

// These macros are needed because CamalCase VS ALL_CAPS
#define ARGPARSE_LOGGING_INTERNAL_INFO ::absl::LogSeverity::kInfo
#define ARGPARSE_LOGGING_INTERNAL_WARNING ::absl::LogSeverity::kWarning
#define ARGPARSE_LOGGING_INTERNAL_ERROR ::absl::LogSeverity::kError
#define ARGPARSE_LOGGING_INTERNAL_FATAL ::absl::LogSeverity::kFatal

namespace argparse {
namespace internal {
namespace logging_internal {

// From absl/base/internal/raw_logging.h
// Applying Basename() on __FILE__ makes the log message more nice-looking.

// compile-time function to get the "base" filename, that is, the part of
// a filename after the last "/" or "\" path separator.  The search starts at
// the end of the string; the second parameter is the length of the string.
constexpr const char* Basename(const char* fname, int offset) {
  return offset == 0 || fname[offset - 1] == '/' || fname[offset - 1] == '\\'
             ? fname + offset
             : Basename(fname, offset - 1);
}

// The Log() function uses the C's fprintf() to directly feed formatted message
// to stderr. The formatted message is truncated if too long.
void Log(absl::LogSeverity severity, const char* file, int line,
         const char* format, ...);

}  // namespace logging_internal
}  // namespace internal
}  // namespace argparse
