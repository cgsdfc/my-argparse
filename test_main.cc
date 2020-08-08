#include <argparse/argparse.h>
#include <iostream>

using namespace argparse;

int main(int argc, char const* argv[]) {
  ArgumentParser parser(Options()
                            .description("a test program")
                            .bug_address("xxx@xxx.com")
                            .version("0.0.0"));

  std::string output;
  parser.add_argument("output", &output, "output to this file").arg();
  std::string input;
  parser.add_argument("--input", &input, "input file");

  parser.parse_args(argc, argv);
}
