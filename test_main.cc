#include <argparse/argparse.h>
#include <argparse/internal/argparse-gflags-parser.h>

#include <iostream>

int g_verbose_level;
int g_log_level;

int main(int argc, char const* argv[]) {
  argparse::ArgumentParser()
      .SetDescription("a program")
      .SetBugReportEmail("xx@xx.com")
      .AddArgument(argparse::Argument("--verbose")
                       .SetDest(&g_verbose_level)
                       .SetDefaultValue(1))
      .AddArgument(argparse::Argument("--log-level").SetDest(&g_log_level))
      .ParseArgs(argc, argv);
}