// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/argparse-result.h"

#include "argparse-test-helper.h"
#include <gtest/gtest.h>

namespace argparse {
namespace {

TEST(Result, WhenDefaultConstructedTheStateIsCorrect) {
  Result<int> res;
  EXPECT_TRUE(res.empty());
  EXPECT_TRUE(!res.has_value());
  EXPECT_TRUE(!res.has_error());
}

TEST(Result, WhenValueConstructedTheStateIsCorrect) {
  Result<int> res(1);
  EXPECT_TRUE(res.has_value());
  EXPECT_TRUE(!res.has_error());
  EXPECT_TRUE(!res.empty());
  EXPECT_TRUE(res.get_value() == 1);
}

TEST(Result, DefaultConstructedAndThenMutateTheState) {
  Result<int> res;
  EXPECT_TRUE(res.empty());

  res.set_value(1);
  EXPECT_TRUE(res.has_value());
  EXPECT_TRUE(!res.has_error());
  EXPECT_TRUE(!res.empty());
  EXPECT_TRUE(res.get_value() == 1);

  res.set_error("err");
  EXPECT_TRUE(res.has_error());
  EXPECT_TRUE(!res.has_value());
  EXPECT_TRUE(!res.empty());
  EXPECT_TRUE(res.get_error() == "err");

  res.reset();
  EXPECT_TRUE(res.empty());
  EXPECT_TRUE(!res.has_value());
  EXPECT_TRUE(!res.has_error());
}

TEST(Result, AfterReleasingValueTheStateIsEmpty) {
  Result<int> res(1);
  EXPECT_TRUE(res.release_value() == 1);
  EXPECT_TRUE(res.empty());
}

TEST(Result, AfterReleasingErrMsgTheStateIsEmpty) {
  Result<int> res;
  res.set_error("err");
  EXPECT_TRUE(res.release_error() == "err");
  EXPECT_TRUE(res.empty());
}

TEST(Result, AssignmentWorksAsSetValue) {
  Result<int> res;
  res = 1;
  EXPECT_TRUE(res.has_value());
  EXPECT_TRUE(res.get_value() == 1);
}

TEST(Result, WorksForMoveOnlyType) {
  Result<MoveOnlyType> res;
  res.set_value(MoveOnlyType(1));
  EXPECT_TRUE(res.has_value());
  EXPECT_TRUE(res.get_value() == MoveOnlyType(1));
}

// TEST(TypeName, WorksForTypicalTypes) {
//   EXPECT_TRUE(TypeName<int>() == "int");
//   EXPECT_TRUE(TypeName<double>() == "double");
//   EXPECT_TRUE(TypeName<char>() == "char");
// }

}  // namespace
}  // namespace argparse
