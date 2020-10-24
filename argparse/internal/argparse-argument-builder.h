// Copyright (c) 2020 Feng Cong
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "argparse/internal/argparse-info.h"
#include "argparse/internal/argparse-port.h"

namespace argparse {
namespace internal {

class Argument;

// This class handles Argument creation.
// It understands user' options and tries to create an argument correctly.
// Its necessity originates from the fact that some computation is unavoidable
// between creating XXXInfos and getting what user gives us. For example, user's
// action only tells us some string, but the actual performing of the action
// needs an Operation, which can only be found from DestInfo. Meanwhile, impl
// can choose to ignore some of user's options if the parser don't support it
// and create their own impl of Argument to fit their parser. In a word, this
// abstraction is right needed.
class ArgumentBuilder {
 public:
  virtual ~ArgumentBuilder() {}
  virtual void SetNames(std::unique_ptr<NamesInfo> info) = 0;
  virtual void SetDest(std::unique_ptr<DestInfo> info) = 0;
  virtual void SetActionString(const char* str) = 0;
  virtual void SetActionInfo(std::unique_ptr<ActionInfo> info) = 0;
  virtual void SetTypeFileType(OpenMode mode) = 0;
  virtual void SetTypeInfo(std::unique_ptr<TypeInfo> info) = 0;
  virtual void SetNumArgs(std::unique_ptr<NumArgsInfo> info) = 0;
  virtual void SetConstValue(std::unique_ptr<Any> val) = 0;
  virtual void SetDefaultValue(std::unique_ptr<Any> val) = 0;
  virtual void SetRequired(bool val) = 0;
  virtual void SetHelp(std::string val) = 0;
  virtual void SetMetaVar(std::string val) = 0;
  virtual std::unique_ptr<Argument> CreateArgument() = 0;

  static std::unique_ptr<ArgumentBuilder> Create();
};

}  // namespace internal
}  // namespace argparse
