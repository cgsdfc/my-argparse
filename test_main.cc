#include <argparse/argparse.h>
#include <iostream>

using namespace argparse;

Status bar(const Context& c, int* i) {
  return false;
}

int main(int argc, char const* argv[]) {
  auto type_lambda1 = [](const Context& ctx, int* i) -> Status { return false; };
  auto type_lambda2 = [](const Context& ctx) -> int { return 0; };

  // Type{type_lambda1};
  // Type{type_lambda2};

  auto action_lambda1 = [](const Context& ctx, int* i) -> Status { return false; };
  auto action_lambda2 = [](const Context& ctx, int* i) {};

  // Action{action_lambda1};
  // Action{action_lambda2};

  using Function = detail::function_signature_t<decltype(&bar)>;
  return 0;
}
