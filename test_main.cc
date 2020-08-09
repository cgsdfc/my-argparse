#include <argparse/argparse.h>
#include <iostream>

using namespace argparse;

int main(int argc, char const* argv[]) {
  ArgumentParser parser(Options()
                            .description("a test program")
                            .bug_address("xxx@xxx.com")
                            .version([](FILE* f, ArgpState* state) {
                              std::fprintf(f, "%d.%d.%d\n", 1, 2, 3);
                            }));

  std::string output;
  parser.add_argument("output", &output, "output to this file").arg();
  std::string input;
  parser.add_argument("--input", &input, "input file");

  parser.parse_args(argc, argv);
}
