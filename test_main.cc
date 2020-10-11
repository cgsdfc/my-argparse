#include <argparse/argparse.h>
#include <argparse/internal/argparse-gflags-parser.h>

#include <iostream>

int main(int argc, char const* argv[]) {
  argparse::internal::GflagsParserFactory factory;
  auto parser = factory.CreateParser();
}