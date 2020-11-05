// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-any.h"

#include "argparse/internal/argparse-test-helper.h"

#include <gtest/gtest.h>

namespace argparse {
namespace internal {

TEST(Any, MakeAnyCallsTheCorrectCtor) {
  auto val = MakeAny<CtorOverload>(double());
  EXPECT_TRUE(AnyCast<CtorOverload>(*val).called_ctor ==
              CtorOverload::kDouble);
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

TEST(Any, AnyCanWrapMoveOnlyType) {
  auto val = MakeAny<MoveOnlyType>(1);
  EXPECT_TRUE(AnyCast<MoveOnlyType>(std::move(val)).val == 1);
}

}  // namespace internal
}  // namespace argparse
