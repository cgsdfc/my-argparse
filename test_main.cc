#include <argparse/argparse.h>
#include <iostream>

using namespace argparse;

struct Appendable {};

template <>
struct AppendTraits<Appendable> {
  using ValueType = int;
  static void Append(Appendable*, int i);
};


struct FU {
  void operator()(bool);
  int operator()(int);
};

int main(int argc, char const* argv[]) {
  ArgumentParser parser;

  // parser.add_argument("out")
  //     .action([](int* a, Result<int> b) {})
  //     .type([](const std::string& in) { return false; });

  std::cout << Format(1) << '\n';
  std::cout << Format(false) << '\n';
  std::cout << Format(std::ostringstream()) << '\n';
}
