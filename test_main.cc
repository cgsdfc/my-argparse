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
  // ArgumentParser parser;
  std::cout << IsOpsSupported<OpsKind::kCount, double>{};

  // std::string output;
  // parser.add(ap::argument("output")
  //                .dest(&output)
  //                .const_value(std::string("path"))
  //                .type([](const std::string& in) -> std::string { return ""; })
  //                .action("store_const")
  //                .help("output to this file")
  //                .meta_var("OUT"));

  // int input;
  // parser.add_argument("input", &input, "input");
  ARGPARSE_DCHECK_F(false, "a string %s", "hhh");
}
