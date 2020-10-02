// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <gtest/gtest.h>

#include "argparse/argparse-builder.h"
#include "argparse-test-helper.h"

namespace argparse {
namespace {

TEST(AnyValue, Build) {
  AnyValue any_value(PlainType{});
  auto val = GetBuiltObject(&any_value);
  EXPECT_TRUE(val);
  EXPECT_TRUE(val->GetType() == typeid(PlainType));
}

TEST(TypeCallback, BuildFromLambda) {
  TypeCallback cb([](const std::string& in, Result<int>* out) {});
  auto val = GetBuiltObject(&cb);
  EXPECT_TRUE(val);
  EXPECT_TRUE(val->GetTypeHint() == "int");
}

}  // namespace
}  // namespace argparse
