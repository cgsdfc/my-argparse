#include "argparse/internal/argparse-subcommand.h"

namespace argparse {
namespace internal {

SubCommand::SubCommand(Delegate* delegate)
    : delegate_(delegate), holder_(this) {
  // Place for the primary name.
  names_.resize(1);
}

}  // namespace internal
}  // namespace argparse
