// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/argparse.h"

#include "absl/strings/str_format.h"

using argparse::Argument;

void print_string(absl::string_view str) {
  std::cout << str << '\n';
}

int main(int argc, char const* argv[]) {
  print_string(absl::StrFormat("%d-%d-%d", 10, 20, 30));
  printf("%s\n", std::string{"20"}.c_str());
}