#include <argparse/argparse.h>

using namespace argparse;

int main(int argc, char const* argv[]) {
  ArgumentHolder parser;
  int val;

  parser.add_argument("foo", &val).help("This is a help").required(true);
  return 0;
}
