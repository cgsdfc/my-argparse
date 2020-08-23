#include <argparse/argparse.h>
#include <iostream>

using namespace argparse;

struct Appendable {};

template <>
struct AppendTraits<Appendable> : std::true_type {
  using ValueType = int;
  static void Append(Appendable*, int i);
};

static_assert(IsActionSupported<Appendable, Actions::kAppend>{});

int main(int argc, char const* argv[]) {
  // std::ofstream ofs;
  // std::any a;
  // a = std::move(ofs);
  // a.emplace<std::ofstream>("aaaaa");

  // auto a = std::make_any<std::ofstream>();

  ArgumentParser parser(Options()
                            .description("a test program")
                            .email("xxx@xxx.com")
                            .version([](FILE* f, argp_state*) {
                              std::fprintf(f, "%d.%d.%d\n", 1, 2, 3);
                            }));

  std::string output;
  parser.add_argument("output", &output, "output to this file");
  int input;
  parser.add_argument("input", &input, "input file")
      .type([](const std::string& in, Result<int>* out) { out->set_value(1); })
      .action([](int* out, Result<int> in) { *out = in.get_value(); });

  parser.parse_args(argc, argv);
  std::cout << "output: " << output << '\n';
  std::cout << "input: " << input << '\n';

}
