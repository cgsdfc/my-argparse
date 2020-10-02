// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <gtest/gtest.h>

#include "argparse/argparse-builder.h"
#include "argparse-test-helper.h"

namespace argparse {
namespace {

TEST(AnyValue, CtorShouldCreateAny) {
  AnyValue any_value(PlainType{});
  auto val = GetBuiltObject(&any_value);
  EXPECT_TRUE(val);
  EXPECT_TRUE(val->GetType() == typeid(PlainType));
}

}  // namespace

}  // namespace argparse
