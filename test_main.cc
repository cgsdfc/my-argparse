#include <iostream>

#include "argparse/argparse-conversion-result.h"
#include "argparse/argparse.h"

int g_verbose_level;
int g_log_level;

using argparse::Argument;

int main(int argc, char const* argv[]) {
  ARGPARSE_INTERNAL_LOG(INFO, "%d %s", 11, "abcd");

  auto parser = argparse::ArgumentParser();

  parser.SetDescription("a program").SetBugReportEmail("xx@xx.com");
  parser.AddArgument(Argument("-verbose", &g_verbose_level)
                         .SetRequired(true)
                         .SetDefaultValue(20)
                         .SetMetaVar("VERY_VERBOSE")
                         .SetHelp("Whether to be very verbose"));

  parser.ParseArgs(argc, argv);
}