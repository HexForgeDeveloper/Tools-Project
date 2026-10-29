#pragma once
#include "CoreMinimal.h"
#include "Layout/SlateRect.h"

namespace WireBundle
{
inline double ShapeToolbarButtonRadius(double IconRadius) { return FMath::Max(14.0, IconRadius + 6.0); }
inline bool ShapeToolbarHit(FVector2D Center, FVector2D Cursor, double IconRadius)
{
    const double R = ShapeToolbarButtonRadius(IconRadius);
    return FMath::Abs(Cursor.X-Center.X)<=R && FMath::Abs(Cursor.Y-Center.Y)<=R;
}
// Desktop coordinates keep painting and input consistent in floating windows.
inline TArray<FVector2D> ShapeToolbarHandles(FVector2D SelectionTop, const FSlateRect& View, double IconRadius)
{
    const double R = ShapeToolbarButtonRadius(IconRadius), Step = 2*R+6, Margin = 8;
    const bool bHorizontal = View.Right-View.Left >= 2*Margin+6*Step;
    const double Width = bHorizontal ? 5*Step+2*R : 2*R;
    const double Height = bHorizontal ? 2*R : 5*Step+2*R;
    const double X = FMath::Clamp(SelectionTop.X-Width*0.5, double(View.Left)+Margin,
        FMath::Max(double(View.Left)+Margin,double(View.Right)-Margin-Width));
    const double Y = FMath::Clamp(SelectionTop.Y-Height-28, double(View.Top)+Margin,
        FMath::Max(double(View.Top)+Margin,double(View.Bottom)-Margin-Height-24));
    TArray<FVector2D> Result;
    for (int32 I=0; I<6; ++I) { Result.Add(FVector2D(X+R+(bHorizontal?I*Step:0),Y+R+(bHorizontal?0:I*Step))); }
    return Result;
}
}
