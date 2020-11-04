#include <iostream>

#include "argparse/argparse-conversion-result.h"
#include "argparse/argparse.h"

int g_verbose_level;
int g_log_level;

using argparse::Argument;

int main(int argc, char const* argv[]) {
  std::string long_str(600, 'a');
  ARGPARSE_INTERNAL_LOG(ERROR, "%s", long_str.data());


  auto parser = argparse::ArgumentParser();

  parser.SetDescription("a program").SetBugReportEmail("xx@xx.com");
  parser.AddArgument(Argument("-verbose", &g_verbose_level)
                         .SetRequired(true)
                         .SetDefaultValue(20)
                         .SetMetaVar("VERY_VERBOSE")
                         .SetHelp("Whether to be very verbose"));

  parser.ParseArgs(argc, argv);
}