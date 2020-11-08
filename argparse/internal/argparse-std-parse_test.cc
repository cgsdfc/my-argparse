// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-std-parse.h"

#include "gtest/gtest.h"

namespace argparse {
namespace internal {
namespace testing_internal {

TEST(StdParse, ParseInt) {
  int val;
  EXPECT_TRUE(StdParse("10", &val));
  EXPECT_EQ(val, 10);
}

}  // namespace testing_internal
}  // namespace internal
}  // namespace argparse
