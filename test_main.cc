#include <iostream>

#include "argparse/argparse-conversion-result.h"
#include "argparse/argparse.h"
#include "argparse/internal/argparse-gflags-parser.h"

int g_verbose_level;
int g_log_level;

using argparse::Argument;

int main(int argc, char const* argv[]) {
  auto parser = argparse::ArgumentParser();

  parser.SetDescription("a program").SetBugReportEmail("xx@xx.com");
  parser.AddArgument(Argument(&g_verbose_level, "-verbose")
                         .SetRequired(true)
                         .SetDefaultValue(20)
                         .SetMetaVar("VERY_VERBOSE")
                         .SetHelp("Whether to be very verbose"));

  parser.ParseArgs(argc, argv);
}