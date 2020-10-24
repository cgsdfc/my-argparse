#include "argparse/internal/argparse-argument-container.h"

namespace argparse {
namespace internal {

ArgumentContainer::ArgumentContainer(Delegate* delegate)
    : delegate_(delegate), main_holder_(this) {}

ArgumentController::ArgumentController()
    : container_(new ArgumentContainer(this)),
      parser_(ArgumentParser::CreateDefault()) {}

}  // namespace internal
}  // namespace argparse
