// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-parse-traits.h"

#include "gtest/gtest.h"

namespace argparse {
namespace internal {
namespace testing_internal {

// TODO: use ParamsTest.
TEST(ParseTraits, ParseBool) {
  bool value;
  EXPECT_TRUE(Parse("true", &value));
  EXPECT_TRUE(value);
}

TEST(ParseTraits, ParseInt) {
  int value;
  EXPECT_TRUE(Parse("10", &value));
  EXPECT_TRUE(10 == value);
}

}  // namespace testing_internal
}  // namespace internal
}  // namespace argparse
