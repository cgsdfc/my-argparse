#include <argparse/argparse.h>
#include <iostream>

int main(int argc, char const* argv[]) {
  auto parser = argparse::ArgumentParser();

  int output;
  int input;

  parser
      .AddArgument(argparse::Argument("output")
                       .Dest(&output)
                       .Help("this is the output file")
                       .MetaVar("OUT"))
      .AddArgument(argparse::Argument("input", &input, "input file"))
      .AddArgument(argparse::Argument("inout", &input, "in-out file"));
}