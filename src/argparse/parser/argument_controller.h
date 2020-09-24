#pragma once

namespace argparse {

// Main options passed to the parser.
struct OptionsInfo {
  int flags = 0;
  // TODO: may change some of these to std::string to allow dynamic generated
  // content.
  const char* program_version = {};
  const char* description = {};
  const char* after_doc = {};
  const char* domain = {};
  const char* email = {};
  ProgramVersionCallback program_version_callback;
  HelpFilterCallback help_filter;
};

// Combination of Holder and Parser. ArgumentParser should be impl'ed in terms
// of this.
class ArgumentController {
 public:
  virtual ~ArgumentController() {}
  virtual ArgumentHolder* GetMainHolder() = 0;
  virtual SubCommandHolder* GetSubCommandHolder() = 0;
  virtual void SetOptions(std::unique_ptr<OptionsInfo> info) = 0;
  virtual Parser* GetParser() = 0;
  static std::unique_ptr<ArgumentController> Create();
};

}  // namespace argparse