#include "argparse/internal/argparse-argument-container.h"

namespace argparse {
namespace internal {

ArgumentContainer::ArgumentContainer(Delegate* delegate)
    : delegate_(delegate), main_holder_(this) {}

}  // namespace internal
}  // namespace argparse
