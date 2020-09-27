#include <gtest/gtest.h>

#include "argparse/argparse-utils.h"

namespace argparse {
namespace {

TEST(Any, MakeAny) {}

TEST(Any, AnyCast) {}

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

struct MoveOnlyType {
  explicit MoveOnlyType(int val) { this->val = val; }
  MoveOnlyType(MoveOnlyType&&) = default;
  bool operator==(const MoveOnlyType& that) const { return val == that.val; }
  int val;
};

static_assert(std::is_move_constructible<MoveOnlyType>{});
static_assert(!std::is_copy_constructible<MoveOnlyType>{});

TEST(Result, WorksForMoveOnlyType) {
  Result<MoveOnlyType> res;
  res.set_value(MoveOnlyType(1));
  EXPECT_TRUE(res.has_value());
  EXPECT_TRUE(res.get_value() == MoveOnlyType(1));
}

}  // namespace
}  // namespace argparse
