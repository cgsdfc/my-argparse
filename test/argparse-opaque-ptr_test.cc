// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "argparse/internal/argparse-opaque-ptr.h"

#include <gtest/gtest.h>

namespace argparse {
namespace internal {
namespace {

TEST(OpaquePtr, DefaultCtorWorks) {
  OpaquePtr ptr;
  EXPECT_TRUE(!ptr);
  EXPECT_TRUE(ptr.type() == typeid(void));
  EXPECT_TRUE(ptr.raw_value() == nullptr);
}

TEST(OpaquePtr, CanBeConvertedFromNullptr) {
  OpaquePtr ptr_a = nullptr;
  OpaquePtr ptr_b(nullptr);
}

TEST(OpaquePtr, NullptrCtorWorks) {
  OpaquePtr ptr = nullptr;
  EXPECT_TRUE(!ptr);
  EXPECT_TRUE(ptr.type() == typeid(void));
  EXPECT_TRUE(ptr.raw_value() == nullptr);
}

TEST(OpaquePtr, TemplateCtorWorks) {
  int val = 0;
  OpaquePtr ptr(&val);
  EXPECT_TRUE(ptr.type() == typeid(val));
  EXPECT_TRUE(ptr.raw_value() == &val);
}

TEST(OpaquePtr, CastWorks) {
  int val = 0;
  OpaquePtr ptr(&val);
  EXPECT_TRUE(ptr.Cast<int>() == &val);
  EXPECT_TRUE(ptr.Cast<int>() == ptr.raw_value());
}

TEST(OpaquePtr, GetValue) {
  int val = 0;
  OpaquePtr ptr(&val);
  EXPECT_TRUE(ptr.GetValue<int>() == 0);
  EXPECT_TRUE(const_cast<const OpaquePtr&>(ptr).GetValue<int>() == 0);
}

TEST(OpaquePtr, PutValue) {
  int val = 0;
  OpaquePtr ptr(&val);

  ptr.PutValue<int>(1);
  EXPECT_TRUE(ptr.GetValue<int>() == 1);
  EXPECT_TRUE(val == 1);
}

TEST(OpaquePtr, OperatorEqComparesRawValue) {
  OpaquePtr a, b;
  int val;
  OpaquePtr c(&val);

  EXPECT_TRUE(a == a);
  EXPECT_TRUE(a == b);
  EXPECT_TRUE(a != c);
  EXPECT_TRUE(b != c);
}

TEST(OpaquePtr, ResetWithNullptr) {
  int val;
  OpaquePtr ptr(&val);
  ptr.Reset(nullptr);
  EXPECT_TRUE(!ptr);
  EXPECT_TRUE(ptr.type() == OpaquePtr().type());
}

TEST(OpaquePtr, ResetWithPtr) {
  OpaquePtr ptr;
  int val;
  ptr.Reset(&val);
  EXPECT_TRUE(ptr.raw_value() == &val);
  EXPECT_TRUE(ptr.type() == typeid(val));
}

TEST(OpaquePtr, Swap) {
  int val_a, val_b;
  OpaquePtr ptr_a(&val_a), ptr_b(&val_b);

  EXPECT_TRUE(ptr_a.raw_value() == &val_a);
  EXPECT_TRUE(ptr_b.raw_value() == &val_b);

  ptr_a.Swap(ptr_b);
  EXPECT_TRUE(ptr_a.raw_value() == &val_b);
  EXPECT_TRUE(ptr_b.raw_value() == &val_a);
}

}  // namespace
}  // namespace internal
}  // namespace argparse
