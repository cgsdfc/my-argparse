#pragma once

namespace argparse {

// Main options passed to the parser.
// TODO: rename to ParserOptions.
struct OptionsInfo {
  int flags = 0;
  // TODO: may change some of these to std::string to allow dynamic generated
  // content.
  const char* program_version = {};
  const char* description = {};
  const char* after_doc = {};
  const char* domain = {};
  const char* email = {};
  // ProgramVersionCallback program_version_callback;
  // HelpFilterCallback help_filter;
};

// Parser contains everythings it needs to parse arguments.
class Parser {
 public:
  virtual ~Parser() {}
  // Parse args, if rest is null, exit on error. Otherwise put unknown ones into
  // rest and return status code.
  virtual bool ParseKnownArgs(ArgArray args, std::vector<std::string>* out) = 0;
};

class ParserFactory {
 public:
  // Interaction when creating parser.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual std::unique_ptr<OptionsInfo> GetOptions() = 0;
    virtual ArgumentHolder* GetMainHolder() = 0;
    virtual SubCommandHolder* GetSubCommandHolder() = 0;
  };

  virtual ~ParserFactory() {}
  virtual std::unique_ptr<Parser> CreateParser(
      std::unique_ptr<Delegate> delegate) = 0;

  using Callback = std::unique_ptr<ParserFactory> (*)();
  static void RegisterCallback(Callback callback);
};

}  // namespace argparse
