#include "argparse/parser/argument_controller_impl.h"

namespace argparse {

std::unique_ptr<ArgumentController> ArgumentController::Create() {
  // if (g_parser_factory_callback)
  //   return std::make_unique<ArgumentControllerImpl>(
  //       g_parser_factory_callback());
  return nullptr;
}

ArgumentControllerImpl::ArgumentControllerImpl(
    std::unique_ptr<ParserFactory> parser_factory)
    : parser_factory_(std::move(parser_factory)),
      main_holder_(ArgumentHolder::Create()),
      subcmd_holder_(SubCommandHolder::Create()) {
  main_holder_->SetListener(std::make_unique<ListenerImpl>(this));
  subcmd_holder_->SetListener(std::make_unique<ListenerImpl>(this));
}

}  // namespace argparse
