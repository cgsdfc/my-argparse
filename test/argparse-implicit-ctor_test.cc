// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <gtest/gtest.h>

#include "argparse/argparse.h"

namespace argparse {
namespace {

TEST(AnyValue, CtorShouldCreateAny) {
  auto val = AnyValue(PlainType{}).Release();
  EXPECT_TRUE(val->GetType() == typeid(PlainType));
}

}  // namespace

}  // namespace argparse
