#pragma once

#include "WireBundleAssetData.h"
#include "WireBundleModel.h"

class UEdGraph;
class UEdGraphNode;

namespace WireBundle
{
enum class EBundleHealth : uint8 { Valid, Changed, MissingEndpoints, MissingReroutes, UnsupportedVersion };

WIREBUNDLE_API FGuid NodeIdentity(const UEdGraphNode* Node);
WIREBUNDLE_API FWireBundlePinIdentity IdentifyPin(const UEdGraphPin* Pin);
WIREBUNDLE_API UEdGraphPin* ResolvePin(UEdGraph* Graph, const FWireBundlePinIdentity& Identity);
WIREBUNDLE_API FWireBundleRecord NewRecord(UEdGraph* Graph);
WIREBUNDLE_API bool SupportsBundleStore(UEdGraph* Graph);
WIREBUNDLE_API TArray<FWireBundleRecord> ReadBundles(UEdGraph* Graph);
WIREBUNDLE_API bool RemoveBundleRecord(UEdGraph* Graph, const FGuid& Id);
WIREBUNDLE_API bool StoreBundle(UEdGraph* Graph, const FWireBundleRecord& Record);
WIREBUNDLE_API EBundleHealth ValidateBundle(UEdGraph* Graph, const FWireBundleRecord& Record);
WIREBUNDLE_API bool FindBundleForNode(UEdGraph* Graph, const UEdGraphNode* Node, FWireBundleRecord& OutRecord);
WIREBUNDLE_API bool BundleOwnsNode(const FWireBundleRecord& Record, const UEdGraphNode* Node);
WIREBUNDLE_API void CaptureReroutes(FWireBundleStoredWire& Wire, const TArray<UEdGraphNode*>& Nodes);
WIREBUNDLE_API bool BuildAdoptionRecord(UEdGraph* Graph, const TArray<UEdGraphNode*>& Selected,
    FWireBundleRecord& OutRecord, FText& OutError);
}
