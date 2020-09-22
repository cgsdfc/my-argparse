#include <argparse/argparse.h>
#include <iostream>

using namespace argparse;

struct Appendable {};

template <>
struct AppendTraits<Appendable> {
  using ValueType = int;
  static void Append(Appendable*, int i);
};


struct NoMovable {
  NoMovable(NoMovable&&) = delete;
  NoMovable(const NoMovable&) { std::cout << "copy"; }
  NoMovable() {}
};

namespace ap = argparse;

int main(int argc, char const* argv[]) {
  ArgumentParser parser;

  std::string output;
  parser.add_argument(
      ArgumentBuilder({"a", "b"})
          .dest(&output)
          .const_value(std::string("path"))
          .type([](const std::string& in) -> std::string { return ""; })
          .action("store_const")
          .help("output to this file")
          .meta_var("OUT")
          .Build());

  auto subparsers = parser.add_subparsers(SubParsersBuilder()
                                              .description("blabal")
                                              .title("xxx")
                                              .help("sub-commands")
                                              .Build());

  auto create_parser = subparsers.add_parser("create");
  // create_parser.add_argument()
  // int input;
  // parser.add_argument("input", &input, "input");
}
