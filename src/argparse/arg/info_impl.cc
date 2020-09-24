#include "argparse/arg/info_impl.h"

namespace argparse {

void DefaultActionInfo::Run(CallbackClient* client) {
  auto dest_ptr = client->GetDestPtr();
  auto data = client->GetData();

  switch (action_kind_) {
    case ActionKind::kNoAction:
      break;
    case ActionKind::kStore:
      ops_->Store(dest_ptr, std::move(data));
      break;
    case ActionKind::kStoreTrue:
    case ActionKind::kStoreFalse:
    case ActionKind::kStoreConst:
      ops_->StoreConst(dest_ptr, *client->GetConstValue());
      break;
    case ActionKind::kAppend:
      ops_->Append(dest_ptr, std::move(data));
      break;
    case ActionKind::kAppendConst:
      ops_->AppendConst(dest_ptr, *client->GetConstValue());
      break;
    case ActionKind::kPrintHelp:
      client->PrintHelp();
      break;
    case ActionKind::kPrintUsage:
      client->PrintUsage();
      break;
    case ActionKind::kCustom:
      break;
    case ActionKind::kCount:
      ops_->Count(dest_ptr);
      break;
  }
}

}  // namespace argparse
