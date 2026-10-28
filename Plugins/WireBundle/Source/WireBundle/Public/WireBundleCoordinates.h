#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"

namespace WireBundle
{
	// Slate's input/tick geometry is in desktop space, while OnPaint geometry is
	// window-relative. Subtract FPaintArgs' window origin exactly once before
	// converting to widget-local coordinates. Geometry handles DPI/layout scaling.
	inline FVector2D DesktopToPaintLocal(const FVector2D& DesktopPoint,
		const FVector2D& WindowDesktopOrigin, const FGeometry& PaintGeometry)
	{
		return PaintGeometry.AbsoluteToLocal(DesktopPoint - WindowDesktopOrigin);
	}

	inline FSlateRect DesktopToPaintClip(const FSlateRect& DesktopClip,
		const FVector2D&								   WindowDesktopOrigin)
	{
		return FSlateRect(DesktopClip.Left - WindowDesktopOrigin.X,
			DesktopClip.Top - WindowDesktopOrigin.Y,
			DesktopClip.Right - WindowDesktopOrigin.X,
			DesktopClip.Bottom - WindowDesktopOrigin.Y);
	}
} // namespace WireBundle
