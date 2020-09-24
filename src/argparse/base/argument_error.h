#pragma once

#include <stdexcept>

namespace argparse {
// Throw this exception will cause an error msg to be printed (via what()).
class ArgumentError final : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

}  // namespace argparse
