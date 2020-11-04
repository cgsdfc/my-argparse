#include <iostream>

#include "argparse/argparse-conversion-result.h"
#include "argparse/argparse.h"

int g_verbose_level;
int g_log_level;

using argparse::Argument;

int main(int argc, char const* argv[]) {
  for (int i = 0; i < 10; ++i)
    ARGPARSE_INTERNAL_LOG(WARNING, "this is a warning %d", i);

  auto parser = argparse::ArgumentParser();

  parser.SetDescription("a program").SetBugReportEmail("xx@xx.com");
  parser.AddArgument(Argument("-verbose", &g_verbose_level)
                         .SetRequired(true)
                         .SetDefaultValue(20)
                         .SetMetaVar("VERY_VERBOSE")
                         .SetHelp("Whether to be very verbose"));

  parser.ParseArgs(argc, argv);
}