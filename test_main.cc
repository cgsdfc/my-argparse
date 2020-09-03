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

  std::cout << detail::is_functor<int>{};
  std::cout << detail::is_functor<std::function<void()>>{};
  auto lamb = []() {};
  std::cout << detail::is_functor<decltype(lamb)>{};
  std::cout << detail::is_functor<FU>{};

  auto lamb2 = [](auto x) {};
  std::cout << detail::is_functor<decltype(lamb2)>{};

  parser.add_argument("out")
      .action([](int* a, Result<int> b) {})
      .type([](const std::string& in) { return false; });
}
