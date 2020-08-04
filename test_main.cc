#include <argparse/argparse.h>
#include <iostream>

using namespace argparse;

Status bar(const Context& c, int* i) {
  return false;
}

int main(int argc, char const* argv[]) {
  auto type_lambda1 = [](const Context& ctx, int* i) -> Status { return false; };
  auto type_lambda2 = [](const Context& ctx) -> int { return 0; };

  Type t1{type_lambda1};
  Type t2{type_lambda2};
  Type t3{std::move(t1)};

  auto action_lambda1 = [](const Context& ctx, int* i) -> Status { return false; };
  auto action_lambda2 = [](const Context& ctx, int* i) {};

  Action a1{action_lambda1};
  Action a2{action_lambda2};

  Action a3{std::move(a1)};

  using Function = detail::function_signature_t<decltype(&bar)>;

  ArgumentHolder holder;

  bool all = false;
  holder.add_argument({"--all"})
      .dest(&all)
      .help("whether or not to do it all")
      .required(true)
      .type([](const Context& ctx) -> bool { return true; });

  ArgumentParser parser(Options()
                            .version("1.2.3")
                            .description("a short program")
                            .bug_address("cgsdfc@126.com")
                            .flags(Flags::kLongOnly));

  std::string name;
  parser.add_argument("name", &name, "what's your name?");
  auto group = parser.add_argument_group("header");

  return 0;
}
