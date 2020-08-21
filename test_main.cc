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
  // auto func = stl_number_parser<double>()();
  // func()

  ArgumentParser parser(Options()
                            .description("a test program")
                            .bug_address("xxx@xxx.com"));
                            // .version([](FILE* f, ArgpState state) {
                            //   std::fprintf(f, "%d.%d.%d\n", 1, 2, 3);
                            // }));

  std::string output;
  parser.add_argument("output", &output, "output to this file");
  std::string input;
  parser.add_argument("input", &input, "input file");

  parser.parse_args(argc, argv);
  std::cout << "output: " << output << '\n';
  std::cout << "input: " << input << '\n';

}
