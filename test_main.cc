#include <iostream>

#include "argparse/argparse-conversion-result.h"
#include "argparse/argparse.h"
#include "argparse/internal/argparse-gflags-parser.h"

int g_verbose_level;
int g_log_level;

int main(int argc, char const* argv[]) {
  std::vector<int> arr;
  using argparse::internal::argument_internal::Argument;

  Argument<std::vector<int>>(&arr)
      // .SetConstValue(0)
      .SetDefaultValue({})
      .SetRequired(false)
      .SetHelp("aaaaa");

  Argument<int> var(&g_verbose_level);

  FILE* f;
  Argument<FILE*> var_f(&f);
  var_f.SetFileType(argparse::FileType("xxx"));

  auto parser = argparse::ArgumentParser();

  parser.SetDescription("a program").SetBugReportEmail("xx@xx.com");

  parser.AddArgument(argparse::Argument("--verbose")
                         .SetDest(&g_verbose_level)
                         .SetDefaultValue(1)
                         .SetAction([](argparse::ConversionResult val) {}));

  parser.AddArgument(argparse::Argument("--log-level").SetDest(&g_log_level));
  parser.ParseArgs(argc, argv);
}