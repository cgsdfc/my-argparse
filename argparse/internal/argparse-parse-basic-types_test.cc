// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "absl/strings/ascii.h"
#include "argparse/internal/argparse-logging.h"
#include "argparse/internal/argparse-parse-traits.h"
#include "gtest/gtest.h"

// Test parsing of basic types like char, bool, int.

namespace argparse {
namespace internal {
namespace testing_internal {

template <typename T>
struct ParseTestData {
  std::string input;
  T output;
};

template <typename T>
using DataPointList = std::vector<ParseTestData<T>>;

template <typename T>
void GetDataPointsImpl(DataPointList<T>*) {
  ARGPARSE_INTERNAL_LOG(WARNING, "No data provided");
}

void GetDataPointsImpl(DataPointList<bool>* list) {
  list->assign({
      {"true", true},
      {"t", true},
      {"false", false},
      {"f", false},
  });
}

void GetDataPointsImpl(DataPointList<char>* list) {
  for (int i = 0; i < 256; ++i) {
    if (absl::ascii_isprint(i)) {
      char ch = i;
      list->push_back({std::string(1, ch), ch});
    }
  }
}

void GetDataPointsImpl(DataPointList<std::string>* list) {
  constexpr const char* kStrings[] = {
      "", "a", "ab", "abc", "abcd",
  };
  for (auto str : kStrings) {
    list->push_back({str, str});
  }
}

template <typename T>
class ParseTest : public ::testing::Test {
 public:
  void SetUp() override { GetDataPointsImpl(&data_points_); }

  const DataPointList<T>& GetDataPoints() const { return data_points_; }

 private:
  DataPointList<T> data_points_;
};

using BasicTypes = ::testing::Types<bool, char, int, unsigned, std::string>;

TYPED_TEST_SUITE(ParseTest, BasicTypes);

TYPED_TEST(ParseTest, PositiveCase) {
  EXPECT_TRUE(internal::IsParseDefined<TypeParam>::value);

  for (const auto& data : this->GetDataPoints()) {
    TypeParam result;
    EXPECT_TRUE(internal::Parse(data.input, &result));
    EXPECT_EQ(result, data.output);
  }
}

}  // namespace testing_internal
}  // namespace internal
}  // namespace argparse
