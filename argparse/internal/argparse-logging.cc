// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-logging.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace argparse {
namespace internal {
namespace logging_internal {

// For our short messages, this size is sufficient.
static constexpr size_t kLogMsgBufSize = 512;
static const char kTruncated[] = " ... (message truncated)\n";

void Log(absl::LogSeverity severity, const char* file, int line,
         const char* format, ...) {
  char buf[kLogMsgBufSize];
  const auto size = sizeof(buf);

  va_list ap;
  va_start(ap, format);
  int n = vsnprintf(buf, size, format, ap);
  va_end(ap);

  bool truncated = n < 0 || n >= size;
  fprintf(stderr, "[%s : %d] %s: %s%s\n", file, line,
          absl::LogSeverityName(severity), buf, (truncated ? kTruncated : ""));

  if (severity == absl::LogSeverity::kFatal) {
    abort();
  }
}

}  // namespace logging_internal
}  // namespace internal
}  // namespace argparse
