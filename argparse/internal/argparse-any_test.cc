// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-any.h"

#include <sstream>  // for a move-only type.

#include "gtest/gtest.h"

namespace argparse {
namespace internal {
namespace testing_internal {

struct SomeType {};
template <typename T>
class AnyTest : public ::testing::Test {
 protected:
};

using AnyTestTypes = ::testing::Types<int, double, bool, std::string>;
TYPED_TEST_SUITE(AnyTest, AnyTestTypes);

TYPED_TEST(AnyTest, TypeIs) {
  auto any = MakeAny<TypeParam>();
  EXPECT_TRUE(any->template TypeIs<TypeParam>());
  EXPECT_FALSE(any->template TypeIs<SomeType>());
}

TYPED_TEST(AnyTest, MakeAny) {
  auto any = MakeAny<TypeParam>();
  static_cast<void>(any);
}

TYPED_TEST(AnyTest, AnyCast) {
  TypeParam default_value{};
  auto any = MakeAny<TypeParam>();

  EXPECT_EQ(AnyCast<TypeParam>(*any), default_value);
  EXPECT_EQ(*AnyCast<TypeParam>(any.get()), default_value);
}

TEST(NonTypedAnyTest, DestructorDidRun) {
  struct FlipWhenDtorRun {
    bool *value_outside_;
    FlipWhenDtorRun(bool* value_outside) : value_outside_(value_outside) {}
    ~FlipWhenDtorRun() {
      *value_outside_ = !*value_outside_;
    }
  };

  bool value = false;
  auto any = MakeAny<FlipWhenDtorRun>(&value);
  any.reset();
  EXPECT_TRUE(value);
}

TEST(NonTypedAnyTest, CanHoldMoveOnlyType) {
  using MoveOnly = std::ostringstream;
  auto any = MakeAny<MoveOnly>();
  constexpr auto kDataToStream = "Data";
  AnyCast<MoveOnly>(*any) << kDataToStream;
  EXPECT_EQ(AnyCast<MoveOnly>(*any).str(), kDataToStream);
}

// TYPED_TEST(AnyTest, TakeValueAndDiscard) {
//   auto any = MakeAny<TypeParam>();
//   auto copy = AnyCast<TypeParam>(*any);
//   auto value = TakeValueAndDiscard<TypeParam>(&any);
//   EXPECT_TRUE(!any);
//   EXPECT_EQ(copy, value);
// }

}  // namespace testing_internal
}  // namespace internal
}  // namespace argparse
