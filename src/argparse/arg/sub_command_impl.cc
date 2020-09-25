#include "argparse/arg/sub_command_impl.h"

namespace argparse {
std::unique_ptr<SubCommandHolder> SubCommandHolder::Create() {
  return std::make_unique<SubCommandHolderImpl>();
}

}  // namespace argparse
