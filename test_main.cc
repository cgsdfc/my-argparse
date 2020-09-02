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
  ArgumentParser parser;

  // std::ofstream output;
  int output;
  parser.add_argument("output", &output, "output to this file")
      .default_value(0)
      .const_value(1);

  Type t;
  DCHECK(!t.info);
  Type t2(FileType("r"));
  parser.parse_args(argc, argv);

}
