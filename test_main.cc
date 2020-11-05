#include <iostream>

#include "argparse/argparse-conversion-result.h"
#include "argparse/argparse.h"

int g_verbose_level;
int g_log_level;

using argparse::Argument;

int main(int argc, char const* argv[]) {

  auto parser = argparse::ArgumentParser();

  parser.SetDescription("a program").SetBugReportEmail("xx@xx.com");
  parser.AddArgument(Argument("-verbose", &g_verbose_level)
                         .SetDefaultValue(20)
                         .SetHelp("Whether to be very verbose"));

  parser.ParseArgs(argc, argv);
  printf("verbose: %d\n", g_verbose_level);
}