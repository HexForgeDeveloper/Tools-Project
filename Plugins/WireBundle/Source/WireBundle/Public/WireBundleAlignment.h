#pragma once
#include "CoreMinimal.h"
class SGraphPanel;
namespace WireBundle
{
WIREBUNDLE_API bool CanAlignVertical(const TSharedRef<SGraphPanel>& Panel);
WIREBUNDLE_API bool AlignVertical(const TSharedRef<SGraphPanel>& Panel);
}
