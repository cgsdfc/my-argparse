// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/argparse.h"

int g_verbose_level;
int g_log_level;
FILE* f;
std::vector<FILE*> fs;

using argparse::Argument;

int main(int argc, char const* argv[]) {
  auto parser = argparse::ArgumentParser();

  parser.Description("a program").BugReportEmail("xx@xx.com");
  parser.AddArgument(
      Argument("--verbose", &g_verbose_level).Help("Verbose").MetaVar("V"));

  enum class A {
    a, b,
  };

  A a_var;

  Argument("a", &a_var).EnumType({{"a", A::a}, {"b", A::b}});

  parser.ParseArgs(argc, argv);
  printf("verbose: %d\n", g_verbose_level);
}