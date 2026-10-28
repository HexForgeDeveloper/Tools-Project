#pragma once
#include "CoreMinimal.h"
namespace WireBundle
{
FDelegateHandle RegisterMenus();
void UnregisterMenus(FDelegateHandle Handle);
}
