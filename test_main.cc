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
                         .SetRequired(true)
                         .SetDefaultValue(20)
                         .SetMetaVar("VERY_VERBOSE")
                         .SetHelp("Whether to be very verbose"));

  parser.AddArgumentGroup("").AddArgument(Argument("output", &g_log_level));
  parser.AddArgument("output", &g_log_level, "Logging level");

  parser.ParseArgs(argc, argv);
}