#include <gtest/gtest.h>

#include "argparse/argparse-utils.h"

namespace argparse {
namespace {

struct CtorOverload {
  enum {
    kCtorDouble,
    kCtorInt,
    kCtorChar,
  };
  int called_ctor;
  explicit CtorOverload(double) : called_ctor(kCtorDouble) {}
  explicit CtorOverload(int) : called_ctor(kCtorInt) {}
  explicit CtorOverload(char) : called_ctor(kCtorChar) {}
};

TEST(Any, MakeAnyCallsTheCorrectCtor) {
  auto val = MakeAny<CtorOverload>(double());
  EXPECT_TRUE(AnyCast<CtorOverload>(*val).called_ctor ==
              CtorOverload::kCtorDouble);
}

TEST(Any, AnyImplHasCorrectType) {
  auto val = MakeAny<int>(1);
  EXPECT_TRUE(val->GetType() == typeid(int));
}

TEST(Any, AnyCastMoveFormWorks) {
  auto val = MakeAny<int>(1);
  EXPECT_TRUE(AnyCast<int>(std::move(val)) == 1);
}

TEST(Any, AnyCastConstFormWorks) {
  auto val = MakeAny<int>(1);
  EXPECT_TRUE(AnyCast<int>(*val) == 1);
}

struct MoveOnlyType {
  explicit MoveOnlyType(int val) { this->val = val; }
  MoveOnlyType(MoveOnlyType&&) = default;
  bool operator==(const MoveOnlyType& that) const { return val == that.val; }
  int val;
};

static_assert(std::is_move_constructible<MoveOnlyType>{});
static_assert(!std::is_copy_constructible<MoveOnlyType>{});

TEST(Any, AnyCanWrapMoveOnlyType) {
  auto val = MakeAny<MoveOnlyType>(1);
  EXPECT_TRUE(AnyCast<MoveOnlyType>(std::move(val)).val == 1);
}

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

TEST(TypeName, WorksForTypicalTypes) {
  EXPECT_TRUE(TypeName<int>() == "int");
  EXPECT_TRUE(TypeName<double>() == "double");
  EXPECT_TRUE(TypeName<char>() == "char");
}

}  // namespace
}  // namespace argparse
