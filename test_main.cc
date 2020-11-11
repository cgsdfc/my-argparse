// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/argparse.h"

int g_verbose_level;
int g_log_level;

using argparse::Argument;

int main(int argc, char const* argv[]) {
  auto parser = argparse::ArgumentParser();

  parser.Description("a program").BugReportEmail("xx@xx.com");
  parser.AddArgument(
      Argument("--verbose", &g_verbose_level).Help("Verbose").MetaVar("V"));

  parser.ParseArgs(argc, argv);
  printf("verbose: %d\n", g_verbose_level);
}