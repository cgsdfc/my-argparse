// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT


#include "absl/base/internal/raw_logging.h"

#define ARGPARSE_INTERNAL_LOG(severity, ...) ABSL_RAW_LOG(severity, __VA_ARGS__)

#define ARGPARSE_INTERNAL_CHECK(condition, message) \
  ABSL_RAW_CHECK(condition, message)

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

