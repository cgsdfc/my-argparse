// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/argparse-open-mode.h"

#include "argparse/internal/argparse-logging.h"

namespace argparse {

std::string ModeToChars(OpenMode mode) {
  std::string m;
  if (mode & kModeRead) m.append("r");
  if (mode & kModeWrite) m.append("w");
  if (mode & kModeAppend) m.append("a");
  if (mode & kModeBinary) m.append("b");
  return m;
}

std::ios_base::openmode ModeToStreamMode(OpenMode m) {
  std::ios_base::openmode out{};
  if (m & kModeRead) out |= std::ios_base::in;
  if (m & kModeWrite) out |= std::ios_base::out;
  if (m & kModeAppend) out |= std::ios_base::app;
  if (m & kModeTruncate) out |= std::ios_base::trunc;
  if (m & kModeBinary) out |= std::ios_base::binary;
  return out;
}

OpenMode StreamModeToMode(std::ios_base::openmode stream_mode) {
  int m = kModeNoMode;
  if (stream_mode & std::ios_base::in) m |= kModeRead;
  if (stream_mode & std::ios_base::out) m |= kModeWrite;
  if (stream_mode & std::ios_base::app) m |= kModeAppend;
  if (stream_mode & std::ios_base::trunc) m |= kModeTruncate;
  if (stream_mode & std::ios_base::binary) m |= kModeBinary;
  return static_cast<OpenMode>(m);
}

OpenMode CharsToMode(absl::string_view str) {
  int m = kModeNoMode;
  for (char ch : str) {
    switch (ch) {
      case 'r':
        m |= kModeRead;
        break;
      case 'w':
        m |= kModeWrite;
        break;
      case 'a':
        m |= kModeAppend;
        break;
      case 'b':
        m |= kModeBinary;
        break;
      case '+':
        // Valid combs are a+, w+, r+.
        if (m & kModeAppend)
          m |= kModeRead;
        else if (m & kModeWrite)
          m |= kModeRead;
        else if (m & kModeRead)
          m |= kModeWrite;
        else
          ARGPARSE_INTERNAL_LOG(FATAL,
                                "Valid usage of '+' are 'a+', 'w+' and 'r+'");
        break;
    }
  }
  return static_cast<OpenMode>(m);
}

}  // namespace argparse
