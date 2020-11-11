// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/argparse.h"

int main(int argc, char const *argv[]) {
  using argparse::Argument;

  auto parser = argparse::ArgumentParser();

  // This example uses the output of the command `clang-format --help`.
  parser.Description(
      "OVERVIEW: A tool to format "
      "C/C++/Java/JavaScript/Objective-C/Protobuf/C# code.");

  FILE* file;
  parser.AddArgument(Argument("--file", &file).FileType("w"));

  bool Werror;
  parser.AddArgument(Argument("--Werror", &Werror)
                         .Help("If set, changes formatting warnings to errors.")
                         .DefaultValue(false));

  bool Wno_error;
  parser.AddArgument(
      Argument("--Wno-error", &Wno_error)
          .Help("If set, unknown format options are only warned about. "
                "This can be used to enable formatting, even if the "
                "configuration contains unknown (newer) options.")
          .DefaultValue(false));

  std::string assume_filename;
  parser.AddArgument(
      Argument("--assume-filename", &assume_filename)
          .Help(
              "Override filename used to determine the language. "
              "When reading from stdin, clang-format assumes this filename to "
              "determine the language.")
          .DefaultValue(""));

  int cursor;
  parser.AddArgument(Argument("--cursor", &cursor)
                         .Help("The position of the cursor when invoking "
                               "clang-format from an editor integration")
                         .DefaultValue(0));

  parser.ParseArgs(argc, argv);

  return 0;
}
