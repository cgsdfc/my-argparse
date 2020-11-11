#pragma once

#include <string>

#include "absl/status/statusor.h"
#include "argparse/internal/argparse-any.h"

// Define the OpsResult class.
namespace argparse {
namespace internal {

using StatusOrAny = absl::StatusOr<std::unique_ptr<Any>>;

struct OpsResult {
  bool has_error = false;
  std::unique_ptr<Any> value;  // null if error.
  std::string errmsg;

};

}  // namespace internal
}  // namespace argparse
