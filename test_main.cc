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

int main(int argc, char const* argv[]) {
  ArgumentParser parser;
  NoMovable v{};
  
  NoMovable b(std::move_if_noexcept(v));
}
