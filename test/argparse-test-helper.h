// Copyright (c) 2020 Feng Cong
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

namespace argparse {
namespace {

struct PlainType {};

struct MoveOnlyType {
  explicit MoveOnlyType(int val) { this->val = val; }
  MoveOnlyType(MoveOnlyType&&) = default;
  bool operator==(const MoveOnlyType& that) const { return val == that.val; }
  int val;
};

static_assert(std::is_move_constructible<MoveOnlyType>{});
static_assert(!std::is_copy_constructible<MoveOnlyType>{});

struct CtorOverload {
  enum {
    kDouble,
    kInt,
    kChar,
  };
  int called_ctor;
  explicit CtorOverload(double) : called_ctor(kDouble) {}
  explicit CtorOverload(int) : called_ctor(kInt) {}
  explicit CtorOverload(char) : called_ctor(kChar) {}
};

}  // namespace
}  // namespace argparse
