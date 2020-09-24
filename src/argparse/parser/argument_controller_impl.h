#pragma once

namespace argparse {

class ArgumentControllerImpl : public ArgumentController {
 public:
  explicit ArgumentControllerImpl(
      std::unique_ptr<ParserFactory> parser_factory);

  ArgumentHolder* GetMainHolder() override { return main_holder_.get(); }
  SubCommandHolder* GetSubCommandHolder() override {
    return subcmd_holder_.get();
  }

  void SetOptions(std::unique_ptr<OptionsInfo> info) override {
    SetDirty(true);
    options_info_ = std::move(info);
  }

 private:
  // Listen to events of argumentholder and subcommand holder.
  class ListenerImpl;

  void SetDirty(bool dirty) { dirty_ = dirty; }
  bool dirty() const { return dirty_; }
  Parser* GetParser() override {
    if (dirty() || !parser_) {
      SetDirty(false);
      parser_ = parser_factory_->CreateParser(nullptr);
    }
    return parser_.get();
  }

  bool dirty_ = false;
  std::unique_ptr<ParserFactory> parser_factory_;
  std::unique_ptr<Parser> parser_;
  std::unique_ptr<OptionsInfo> options_info_;
  std::unique_ptr<ArgumentHolder> main_holder_;
  std::unique_ptr<SubCommandHolder> subcmd_holder_;
};

class ArgumentControllerImpl::ListenerImpl : public ArgumentHolder::Listener,
                                             public SubCommandHolder::Listener {
 public:
  explicit ListenerImpl(ArgumentControllerImpl* impl) : impl_(impl) {}

 private:
  void MarkDirty() { impl_->SetDirty(true); }
  void OnAddArgument(Argument*) override { MarkDirty(); }
  void OnAddArgumentGroup(ArgumentGroup*) override { MarkDirty(); }
  void OnAddSubCommand(SubCommand*) override { MarkDirty(); }
  void OnAddSubCommandGroup(SubCommandGroup*) override { MarkDirty(); }

  ArgumentControllerImpl* impl_;
};
}  // namespace argparse
