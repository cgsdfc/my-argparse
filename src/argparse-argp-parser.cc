// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <argp.h>

#include "argparse/internal/argparse-internal.h"

namespace argparse {
namespace internal {

class ArgumentNamespace {
public:
    virtual ~ArgumentNamespace() {}
    virtual Argument* 
};

class SubCommandNamespace {

};

class ArgpArgumentParser : public ArgumentParser {
 public:
  void AddArgument(Argument* arg) override {}
};

}  // namespace internal
}  // namespace argparse
