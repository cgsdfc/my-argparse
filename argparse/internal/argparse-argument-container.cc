#include "argparse/internal/argparse-argument-container.h"

namespace argparse {
namespace internal {

ArgumentContainer::ArgumentContainer() {}

ArgumentController::ArgumentController()
    : container_(new ArgumentContainer),
      parser_(ArgumentParser::CreateDefault()) {}

}  // namespace internal
}  // namespace argparse
