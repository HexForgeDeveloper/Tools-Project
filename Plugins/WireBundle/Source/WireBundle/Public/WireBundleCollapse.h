#pragma once
#include "CoreMinimal.h"
class SGraphPanel;
namespace WireBundle
{
WIREBUNDLE_API bool CanCollapseReroutes(const TSharedRef<SGraphPanel>& Panel);
WIREBUNDLE_API bool CollapseReroutes(const TSharedRef<SGraphPanel>& Panel);
}
