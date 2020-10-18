#include <argparse/argparse-conversion-result.h>
#include <argparse/argparse.h>
#include <argparse/internal/argparse-gflags-parser.h>

#include <iostream>

int g_verbose_level;
int g_log_level;

int main(int argc, char const* argv[]) {
  auto parser = argparse::ArgumentParser();

  parser.SetDescription("a program").SetBugReportEmail("xx@xx.com");

  parser.AddArgument(argparse::Argument("--verbose")
                         .SetDest(&g_verbose_level)
                         .SetDefaultValue(1)
                         .SetAction([](argparse::ConversionResult val) {
                           if (val.HasValue() && int(val) > 10)
                             g_verbose_level = val.GetValue<int>();
                         }));

  parser.AddArgument(argparse::Argument("--log-level").SetDest(&g_log_level));
  parser.ParseArgs(argc, argv);
}