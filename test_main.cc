#include <iostream>

#include "argparse/argparse-conversion-result.h"
#include "argparse/argparse.h"
#include "argparse/internal/argparse-gflags-parser.h"

int g_verbose_level;
int g_log_level;

int main(int argc, char const* argv[]) {
  std::vector<int> arr;
  argparse::internal::ArgWithDest<std::vector<int>> arg(&arr);
  arg.SetConstValue(0);

  auto parser = argparse::ArgumentParser();

  parser.SetDescription("a program").SetBugReportEmail("xx@xx.com");

  parser.AddArgument(argparse::Argument("--verbose")
                         .SetDest(&g_verbose_level)
                         .SetDefaultValue(1)
                         .SetAction([](argparse::ConversionResult val) {}));

  parser.AddArgument(argparse::Argument("--log-level").SetDest(&g_log_level));
  parser.ParseArgs(argc, argv);
}