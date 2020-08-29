#include <argparse/argparse.h>
#include <iostream>

using namespace argparse;

struct Appendable {};

template <>
struct AppendTraits<Appendable> {
  using ValueType = int;
  static void Append(Appendable*, int i);
};


int main(int argc, char const* argv[]) {

  ArgumentParser parser(Options()
                            .description("a test program")
                            .email("xxx@xxx.com")
                            .version([](FILE* f, argp_state*) {
                              std::fprintf(f, "%d.%d.%d\n", 1, 2, 3);
                            }));

  std::ofstream output;
  parser.add_argument("output", &output, "output to this file");

  parser.parse_args(argc, argv);

}
