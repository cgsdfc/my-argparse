// #include <argparse/argparse.h>
#include <iostream>

struct A {
  // A&& foo() && {
  //   return std::move(*this);
  // }
};

void bar(A);

int main(int argc, char const* argv[]) {
  bar(A{});
}
