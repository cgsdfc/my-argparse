// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/argparse-builder.h"

#include <gtest/gtest.h>

#include "argparse/internal/argparse-test-helper.h"

namespace argparse {
namespace {

TEST(AnyValue, Build) {
  AnyValue any_value(PlainType{});
  auto val = builder_internal::Build(&any_value);
  EXPECT_TRUE(val);
  EXPECT_TRUE(val->GetType() == typeid(PlainType));
}

}  // namespace
}  // namespace argparse
