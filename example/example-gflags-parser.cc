// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/argparse.h"

int main(int argc, char const *argv[]) {
  using argparse::Argument;

  auto parser = argparse::ArgumentParser();

  parser.SetDescription("a program").SetBugReportEmail("xx@xx.com");
  parser.ParseArgs(argc, argv);

  return 0;
}
