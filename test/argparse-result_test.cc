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
  EXPECT_TRUE(res.value() == 1);
}

TEST(Result, DefaultConstructedAndThenMutateTheState) {
  Result<int> res;
  EXPECT_TRUE(res.empty());

  res.SetValue(1);
  EXPECT_TRUE(res.has_value());
  EXPECT_TRUE(!res.has_error());
  EXPECT_TRUE(!res.empty());
  EXPECT_TRUE(res.value() == 1);

  res.SetError("err");
  EXPECT_TRUE(res.has_error());
  EXPECT_TRUE(!res.has_value());
  EXPECT_TRUE(!res.empty());
  EXPECT_TRUE(res.error() == "err");

  res.Reset();
  EXPECT_TRUE(res.empty());
  EXPECT_TRUE(!res.has_value());
  EXPECT_TRUE(!res.has_error());
}

TEST(Result, AfterReleasingValueTheStateIsEmpty) {
  Result<int> res(1);
  EXPECT_TRUE(res.ReleaseValue() == 1);
  EXPECT_TRUE(res.empty());
}

TEST(Result, AfterReleasingErrMsgTheStateIsEmpty) {
  Result<int> res;
  res.SetError("err");
  EXPECT_TRUE(res.ReleaseError() == "err");
  EXPECT_TRUE(res.empty());
}

TEST(Result, AssignmentWorksAsSetValue) {
  Result<int> res;
  res = 1;
  EXPECT_TRUE(res.has_value());
  EXPECT_TRUE(res.value() == 1);
}

TEST(Result, WorksForMoveOnlyType) {
  Result<MoveOnlyType> res;
  res.SetValue(MoveOnlyType(1));
  EXPECT_TRUE(res.has_value());
  EXPECT_TRUE(res.value() == MoveOnlyType(1));
}

TEST(Result, InPlaceConstructorWorks) {
  Result<CtorOverload> res(portability::in_place, double());
  EXPECT_TRUE(res.has_value());
  EXPECT_TRUE(res.value().called_ctor == CtorOverload::kDouble);
}

TEST(Result, EmplaceWorks) {
  Result<CtorOverload> res;
  res.emplace(int());
  EXPECT_TRUE(res.value().called_ctor == CtorOverload::kInt);
}

}  // namespace
}  // namespace argparse
