#pragma once

#include <string>

#include "argparse/argparse-conversion-result.h"
#include "argparse/internal/argparse-any.h"

// Define the OpsResult class.
namespace argparse {
namespace internal {

struct OpsResult {
  bool has_error = false;
  std::unique_ptr<Any> value;  // null if error.
  std::string errmsg;

  explicit OpsResult(ConversionResult result) {
    has_error = result.HasError();
    value = result.ReleaseValue();
    errmsg = result.ReleaseError();
  }
};

}  // namespace internal
}  // namespace argparse
