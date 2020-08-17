#include <argparse/argparse.h>
#include <iostream>

using namespace argparse;

struct Appendable {};

template <>
struct AppendTraits<Appendable, int> {
  using value_type = int;
  static void Append(Appendable*, int&& i);
};

int main(int argc, char const* argv[]) {
  std::cout << ActionIsSupported<std::vector<int>, Actions::kAppend>{};
  std::cout << ActionIsSupported<int, Actions::kAppend> {};
  std::cout << ActionIsSupported<std::list<int>, Actions::kAppend> {};
  std::cout << ActionIsSupported<Appendable, Actions::kAppend> {};
  using ti = decltype(typeid(int));

  ArgumentParser parser(Options()
                            .description("a test program")
                            .bug_address("xxx@xxx.com"));
                            // .version([](FILE* f, ArgpState state) {
                            //   std::fprintf(f, "%d.%d.%d\n", 1, 2, 3);
                            // }));

  std::string output;
  parser.add_argument("output", &output, "output to this file").arg();
  std::string input;
  parser.add_argument({"--input", "-i", "--input-file"}, &input, "input file");

  auto student = parser.add_argument_group("student");
  student.add_argument("--sid").help("student id");
  // xxx: this is not supported.
  student.add_argument("--cid");
  parser.add_argument("file");

  parser.parse_args(argc, argv);
}
