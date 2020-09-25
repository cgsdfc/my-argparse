#pragma once

#include <type_traits>

// Forward declaration.
namespace argparse {

enum class OpsKind {
  kStore,
  kStoreConst,
  kAppend,
  kAppendConst,
  kCount,
  kParse,
  kOpen,
};

inline constexpr std::size_t kMaxOpsKind = std::size_t(OpsKind::kOpen) + 1;

const char* OpsToString(OpsKind ops);

template <OpsKind Ops, typename T>
struct IsOpsSupported;

template <OpsKind Ops, typename T, bool Supported = IsOpsSupported<Ops, T>{}>
struct OpsImpl;

template <typename T>
struct IsAppendSupported;

template <typename T, bool = IsAppendSupported<T>{}>
struct IsAppendConstSupported;

// wstring is not supported now.
}  // namespace argparse
