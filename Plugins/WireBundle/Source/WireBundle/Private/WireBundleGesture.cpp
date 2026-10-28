#include "WireBundleGesture.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "SGraphPanel.h"
#include "SGraphPin.h"
#include "SGraphNode.h"
#include "GraphEditorSettings.h"
#include "ConnectionDrawingPolicy.h"
#include "EdGraph/EdGraph.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SWindow.h"
#include "Styling/CoreStyle.h"
#include "K2Node_Knot.h"
#include "MaterialGraphNode_Knot.h"
#include "WireBundleCoordinates.h"
#include "WireBundleRoutePlanner.h"
#include "WireBundleRegistry.h"
#include "Types/PaintArgs.h"

namespace WireBundle
{
static double Cross(const FVector2D& A, const FVector2D& B)
{
    return A.X * B.Y - A.Y * B.X;
}

bool CrossesWire(const FVector2D& Start, const FVector2D& End,
    const TArray<FVector2D>& Wire, double Tolerance, double MinimumDragDistance)
{
    if ((End - Start).SizeSquared() < FMath::Square(MinimumDragDistance)) { return false; }
    const FVector2D Direction = End - Start;
    for (int32 Index = 1; Index < Wire.Num(); ++Index)
    {
        const FVector2D A = Wire[Index - 1];
        const FVector2D B = Wire[Index];
        const FVector2D Segment = B - A;
        const double Denom = Cross(Direction, Segment);
        if (!FMath::IsNearlyZero(Denom))
        {
            const double T = Cross(A - Start, Segment) / Denom;
            const double U = Cross(A - Start, Direction) / Denom;
            if (T >= 0.0 && T <= 1.0 && U >= 0.0 && U <= 1.0) { return true; }
        }
        const double Distance = FMath::Min(
            FMath::Min((FMath::ClosestPointOnSegment2D(Start, A, B) - Start).SizeSquared(),
                       (FMath::ClosestPointOnSegment2D(End, A, B) - End).SizeSquared()),
            FMath::Min((FMath::ClosestPointOnSegment2D(A, Start, End) - A).SizeSquared(),
                       (FMath::ClosestPointOnSegment2D(B, Start, End) - B).SizeSquared()));
        if (Distance <= FMath::Square(Tolerance)) { return true; }
    }
    return false;
}

TArray<FLink> FindCrossedLinks(const TArray<FWireSnapshot>& Wires,
    const FVector2D& Start, const FVector2D& End, const FWireBundleGestureSettings& Settings)
{
    TArray<FLink> Result;
    for (const FWireSnapshot& Wire : Wires)
    {
        if (CrossesWire(Start, End, Wire.Points, Settings.WireHitTolerance, Settings.MinimumDragDistance))
        {
            // A curve may cross the gesture more than once; add each edge once.
            Result.Add(Wire.Link);
        }
    }
    return Result;
}

static TSharedPtr<SGraphPanel> PanelAt(FSlateApplication& App, const FVector2D& Position)
{
    const FWidgetPath Path = App.LocateWindowUnderMouse(Position, App.GetInteractiveTopLevelWindows());
    for (int32 Index = Path.Widgets.Num() - 1; Index >= 0; --Index)
    {
        if (Path.Widgets[Index].Widget->GetType() == FName(TEXT("SGraphPanel")))
        {
            return StaticCastSharedRef<SGraphPanel>(Path.Widgets[Index].Widget);
        }
    }
    return nullptr;
}

static TArray<FWireSnapshot> Snapshot(const TSharedRef<SGraphPanel>& Panel, const FWireBundleGestureSettings& Gesture)
{
    TSet<TSharedRef<SWidget>> Pins;
    Panel->GetAllPins(Pins);
    TMap<UEdGraphPin*, FGeometry> Geometries;
    for (const TSharedRef<SWidget>& Widget : Pins)
    {
        const TSharedRef<SGraphPin> PinWidget = StaticCastSharedRef<SGraphPin>(Widget);
        UEdGraphPin* Pin = PinWidget->GetPinObj();
        const FGeometry& Geometry = Widget->GetCachedGeometry();
        if (Pin && !Pin->bOrphanedPin && Widget->GetVisibility().IsVisible()
            && Geometry.GetLocalSize().X > 0.0f && Geometry.GetLocalSize().Y > 0.0f)
        {
            Geometries.Add(Pin, Geometry);
        }
    }
    TArray<FWireSnapshot> Result;
    const UGraphEditorSettings* Settings = GetDefault<UGraphEditorSettings>();
    TMap<UEdGraphNode*, bool> ReversedKnots;
    auto IsReversed = [&Geometries, &ReversedKnots](UEdGraphNode* Node)
    {
        if (!Node->IsA<UK2Node_Knot>() && !Node->IsA<UMaterialGraphNode_Knot>()) { return false; }
        if (const bool* Cached = ReversedKnots.Find(Node)) { return *Cached; }
        FVector2D Left = FVector2D::ZeroVector;
        FVector2D Right = FVector2D::ZeroVector;
        FVector2D Center = FVector2D::ZeroVector;
        int32 LeftCount = 0, RightCount = 0;
        bool bHasCenter = false;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (const FGeometry* Own = Geometries.Find(Pin))
            {
                Center = FGeometryHelper::CenterOf(*Own);
                bHasCenter = true;
            }
            for (UEdGraphPin* Linked : Pin->LinkedTo)
            {
                if (const FGeometry* Other = Geometries.Find(Linked))
                {
                    if (Pin->Direction == EGPD_Input) { Left += FGeometryHelper::CenterOf(*Other); ++LeftCount; }
                    else { Right += FGeometryHelper::CenterOf(*Other); ++RightCount; }
                }
            }
        }
        if (LeftCount) { Left /= LeftCount; }
        if (RightCount) { Right /= RightCount; }
        const bool bReversed = LeftCount && RightCount ? Right.X < Left.X
            : bHasCenter && (LeftCount ? Center.X < Left.X : RightCount && Right.X < Center.X);
        ReversedKnots.Add(Node, bReversed);
        return bReversed;
    };
    for (const auto& Pair : Geometries)
    {
        UEdGraphPin* Output = Pair.Key;
        if (Output->Direction != EGPD_Output) { continue; }
        for (UEdGraphPin* Input : Output->LinkedTo)
        {
            const FGeometry* InputGeometry = Geometries.Find(Input);
            if (!InputGeometry) { continue; }
            const FVector2D A = FGeometryHelper::VerticalMiddleRightOf(Pair.Value) - FVector2D(4, 0);
            const FVector2D B = FGeometryHelper::VerticalMiddleLeftOf(*InputGeometry) + FVector2D(4, 0);
            const FVector2D Tangent = Settings->ComputeSplineTangent(A, B);
            const FVector2D StartTangent = IsReversed(Output->GetOwningNode()) ? -Tangent : Tangent;
            const FVector2D EndTangent = IsReversed(Input->GetOwningNode()) ? -Tangent : Tangent;
            FWireSnapshot Wire;
            Wire.Link = {FEdGraphPinReference(Output), FEdGraphPinReference(Input), static_cast<int32>(FMath::RoundToInt(A.Y))};
            // Sampling actual pin geometry follows pan, zoom and DPI scaling.
            const int32 Steps = Gesture.CurveSamples;
            for (int32 Index = 0; Index <= Steps; ++Index)
            {
                Wire.Points.Add(FMath::CubicInterp(A, StartTangent, B, EndTangent, double(Index) / Steps));
            }
            Result.Add(MoveTemp(Wire));
        }
    }
    Result.StableSort([](const FWireSnapshot& A, const FWireSnapshot& B)
    {
        if (A.Link.SourceY != B.Link.SourceY) { return A.Link.SourceY < B.Link.SourceY; }
        const UEdGraphPin* APin = A.Link.Output.Get();
        const UEdGraphPin* BPin = B.Link.Output.Get();
        return APin && BPin && APin->PinId.ToString() < BPin->PinId.ToString();
    });
    return Result;
}

struct FGestureState
{
    FWireBundleResolvedSettings Config;
    TWeakPtr<SGraphPanel> Panel;
    FVector2D Start;
    FVector2D End;
    FSlateRect Clip;
    TArray<FWireSnapshot> Wires;
};

class SGesturePreview final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SGesturePreview) {} SLATE_END_ARGS()
    void Construct(const FArguments&, TSharedRef<FGestureState> InState)
    {
        State = InState;
        SetVisibility(EVisibility::HitTestInvisible);
    }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect&,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override
    {
        if (!State) { return Layer; }
        const FVector2D WindowOrigin = Args.GetWindowToDesktopTransform();
        const auto ToLocal = [&Geometry, &WindowOrigin](const FVector2D& Screen)
        { return DesktopToPaintLocal(Screen, WindowOrigin, Geometry); };
        Elements.PushClip(FSlateClippingZone(DesktopToPaintClip(State->Clip, WindowOrigin)));
        int32 Count = 0;
        for (const FWireSnapshot& Wire : State->Wires)
        {
            if (!CrossesWire(State->Start, State->End, Wire.Points, State->Config.Gesture.WireHitTolerance, State->Config.Gesture.MinimumDragDistance)) { continue; }
            ++Count;
            if (!State->Config.Appearance.bHighlightCrossedWires) { continue; }
            TArray<FVector2D> Points;
            for (const FVector2D& Point : Wire.Points) { Points.Add(ToLocal(Point)); }
            FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Points,
                ESlateDrawEffect::None, State->Config.Appearance.WireHighlightColor, true, State->Config.Appearance.HighlightThickness);
        }
        // Use graph coordinates for layout, desktop coordinates for projection,
        // and window paint coordinates only at the final drawing boundary.
        bool bExisting = false;
        if (const auto Panel = State->Panel.Pin())
        {
            const auto Links = FindCrossedLinks(State->Wires, State->Start, State->End, State->Config.Gesture);
            const auto Records = ReadBundles(Panel->GetGraphObj());
            for (const FLink& Link : Links)
            {
                const auto Output = Link.Output.Get();
                const auto Input = Link.Input.Get();
                for (const auto& Record : Records)
                {
                    if ((Output && BundleOwnsNode(Record, Output->GetOwningNode()))
                        || (Input && BundleOwnsNode(Record, Input->GetOwningNode()))) { bExisting = true; }
                }
            }
            if (!bExisting && State->Config.Appearance.bShowRoutePreview
                && Links.Num() >= State->Config.Gesture.MinimumWireCount)
            {
                const FGeometry& PanelGeometry = Panel->GetCachedGeometry();
                const FVector2D Center = Panel->PanelCoordToGraphCoord(
                    PanelGeometry.AbsoluteToLocal((State->Start + State->End) * 0.5));
                const auto Plan = BuildParallelRoutePlan(Links, Center, State->Config);
                const auto Project = [&](const FVector2D& GraphPoint)
                { return PanelGeometry.LocalToAbsolute((GraphPoint - Panel->GetViewOffset()) * Panel->GetZoomAmount()); };
                const auto DrawSegment = [&](const FVector2D& A, const FVector2D& B)
                {
                    const FVector2D Tangent = GetDefault<UGraphEditorSettings>()->ComputeSplineTangent(A, B);
                    TArray<FVector2D> Points;
                    for (int32 Step = 0; Step <= State->Config.Gesture.CurveSamples; ++Step)
                    {
                        Points.Add(ToLocal(FMath::CubicInterp(A, Tangent, B, Tangent,
                            double(Step) / State->Config.Gesture.CurveSamples)));
                    }
                    FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Points,
                        ESlateDrawEffect::None, State->Config.Appearance.RoutePreviewColor,
                        true, State->Config.Appearance.RoutePreviewThickness);
                };
                for (const auto& Lane : Plan.Lanes)
                {
                    const auto Wire = State->Wires.FindByPredicate([&](const FWireSnapshot& W)
                    { return W.Link.Output == Lane.Link.Output && W.Link.Input == Lane.Link.Input; });
                    if (!Wire || Wire->Points.IsEmpty()) { continue; }
                    TArray<FVector2D> Markers;
                    FVector2D Previous = Wire->Points[0];
                    for (const auto& Knot : Lane.Reroutes)
                    {
                        const auto Point = Project(Knot);
                        DrawSegment(Previous, Point); Previous = Point;
                        Markers.Add(ToLocal(Point));
                    }
                    DrawSegment(Previous, Wire->Points.Last());
                    // Every planned reroute is marked, including intermediate columns.
                    for (const FVector2D& Point : Markers)
                    {
                        const double R = State->Config.Appearance.MarkerRadius;
                        const TArray<FVector2D> Diamond = {Point + FVector2D(0, -R), Point + FVector2D(R, 0),
                            Point + FVector2D(0, R), Point + FVector2D(-R, 0), Point + FVector2D(0, -R)};
                        FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Diamond,
                            ESlateDrawEffect::None, State->Config.Appearance.RoutePreviewColor, true,
                            State->Config.Appearance.RoutePreviewThickness);
                    }
                }
            }
        }
        const TArray<FVector2D> Line = { ToLocal(State->Start), ToLocal(State->End) };
        const FLinearColor Color = Count >= State->Config.Gesture.MinimumWireCount ? State->Config.Appearance.ReadyColor : State->Config.Appearance.PendingColor;
        FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Line,
            ESlateDrawEffect::None, Color, true, State->Config.Appearance.GuideThickness);
        // A zero-length line on mouse-down is invisible. Mark its screen-space
        // origin immediately and keep a crosshair exactly under the live cursor.
        if (State->Config.Appearance.bShowCursorMarkers)
        for (const FVector2D& Point : Line)
        {
            const TArray<FVector2D> Horizontal = {Point - FVector2D(State->Config.Appearance.MarkerRadius, 0), Point + FVector2D(State->Config.Appearance.MarkerRadius, 0)};
            const TArray<FVector2D> Vertical = {Point - FVector2D(0, State->Config.Appearance.MarkerRadius), Point + FVector2D(0, State->Config.Appearance.MarkerRadius)};
            FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Horizontal,
                ESlateDrawEffect::None, Color, true, 1.5f);
            FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Vertical,
                ESlateDrawEffect::None, Color, true, 1.5f);
        }
        if (State->Config.Appearance.bShowGestureHint)
        FSlateDrawElement::MakeText(Elements, Layer + 2,
            Geometry.ToPaintGeometry(FVector2D(320, 24), FSlateLayoutTransform(Line.Last() + State->Config.Appearance.HintOffset)),
            FString::Printf(TEXT("%d wires | %d column(s) | Wheel: columns | %s | %s cancels"), Count, State->Config.Movement.RerouteColumns,
                bExisting ? TEXT("Release to inspect bundle") : TEXT("Release to bundle"),
                *State->Config.Gesture.CancelKey.GetDisplayName().ToString()),
            FCoreStyle::GetDefaultFontStyle("Regular", State->Config.Appearance.HintFontSize), ESlateDrawEffect::None, Color);
        Elements.PopClip();
        return Layer + 2;
    }
private:
    TSharedPtr<FGestureState> State;
};

class FInput final : public IInputProcessor
{
    TWeakPtr<SGraphPanel> ActivePanel;
    TWeakPtr<SWindow> ActiveWindow;
    TSharedPtr<FGestureState> State;
    TSharedPtr<SGesturePreview> Preview;

    void Cancel()
    {
        if (const TSharedPtr<SWindow> Window = ActiveWindow.Pin())
        {
            if (Preview) { Window->RemoveOverlaySlot(Preview.ToSharedRef()); }
        }
        Preview.Reset();
        State.Reset();
        ActiveWindow.Reset();
        ActivePanel.Reset();
    }
public:
    virtual ~FInput() override { Cancel(); }
    virtual void Tick(float, FSlateApplication& App, TSharedRef<ICursor>) override
    {
        if (State && (!App.IsActive() || !ActivePanel.IsValid()
            || !App.GetPressedMouseButtons().Contains(State->Config.Gesture.GestureButton)))
        {
            Cancel();
            return;
        }
        if (State)
        {
            // Read the Slate desktop cursor every frame, including frames with
            // no routed mouse-move event. Never use graph/node positions here.
            const FVector2D Cursor = App.GetCursorPos();
            if (!State->End.Equals(Cursor))
            {
                State->End = Cursor;
                if (Preview) { Preview->Invalidate(EInvalidateWidgetReason::Paint); }
            }
        }
    }
    virtual bool HandleKeyDownEvent(FSlateApplication& App, const FKeyEvent& Event) override
    {
        if (State)
        {
            if (Event.GetKey() == State->Config.Gesture.CancelKey) { Cancel(); }
            return true;
        }
        const FWireBundleGestureSettings Input = ResolveWireBundleSettings().Gesture;
        const bool bArrow = Event.GetKey() == EKeys::Left || Event.GetKey() == EKeys::Right
            || Event.GetKey() == EKeys::Up || Event.GetKey() == EKeys::Down;
        if (Event.IsCommandDown() || Event.IsControlDown() != Input.bRequireControl
            || Event.IsAltDown() != Input.bRequireAlt
            || (!bArrow && Event.IsShiftDown() != Input.bRequireShift)
            || (bArrow && Input.bRequireShift && !Event.IsShiftDown())) { return false; }
        if (!bArrow && (Event.IsRepeat() || (Event.GetKey() != Input.BundleKey && Event.GetKey() != Input.AdoptKey))) { return false; }
        const TSharedPtr<SGraphPanel> Panel = PanelAt(App, App.GetCursorPos());
        if (!Panel || !CanEdit(Panel.ToSharedRef())) { return false; }
        if (Event.GetKey() == Input.AdoptKey) { Adopt(Panel.ToSharedRef()); return true; }
        if (bArrow)
        {
            const FVector2D Direction(Event.GetKey() == EKeys::Right ? 1 : Event.GetKey() == EKeys::Left ? -1 : 0,
                Event.GetKey() == EKeys::Down ? 1 : Event.GetKey() == EKeys::Up ? -1 : 0);
            return Nudge(Panel.ToSharedRef(), Direction, Event.IsShiftDown() && !Input.bRequireShift);
        }
        Bundle(Panel.ToSharedRef(), Panel->PanelCoordToGraphCoord(
            Panel->GetCachedGeometry().AbsoluteToLocal(App.GetCursorPos())));
        return true;
    }
    virtual bool HandleMouseButtonDownEvent(FSlateApplication& App, const FPointerEvent& Event) override
    {
        if (State) { Cancel(); return true; }
        const FWireBundleResolvedSettings Config = ResolveWireBundleSettings();
        if (Event.GetEffectingButton() != Config.Gesture.GestureButton || Event.IsCommandDown()
            || Event.IsControlDown() != Config.Gesture.bRequireControl
            || Event.IsAltDown() != Config.Gesture.bRequireAlt
            || Event.IsShiftDown() != Config.Gesture.bRequireShift) { return false; }
        const TSharedPtr<SGraphPanel> Panel = PanelAt(App, Event.GetScreenSpacePosition());
        if (!Panel || !CanEdit(Panel.ToSharedRef())) { return false; }
        const TSharedPtr<SWindow> Window = App.FindWidgetWindow(Panel.ToSharedRef());
        if (!Window) { return false; }
        State = MakeShared<FGestureState>();
        State->Config = Config;
        State->Panel = Panel;
        State->Start = State->End = Event.GetScreenSpacePosition();
        State->Clip = Panel->GetCachedGeometry().GetLayoutBoundingRect();
        State->Wires = Snapshot(Panel.ToSharedRef(), Config.Gesture);
        ActivePanel = Panel;
        ActiveWindow = Window;
        Preview = SNew(SGesturePreview, State.ToSharedRef());
        Window->AddOverlaySlot(1000)[Preview.ToSharedRef()];
        return true;
    }
    virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent& Event) override
    {
        if (!State) { return false; }
        State->End = Event.GetScreenSpacePosition();
        if (Preview) { Preview->Invalidate(EInvalidateWidgetReason::Paint); }
        return true;
    }
    virtual bool HandleMouseButtonUpEvent(FSlateApplication& App, const FPointerEvent& Event) override
    {
        if (!State) { return false; }
        const TSharedPtr<SGraphPanel> Panel = ActivePanel.Pin();
        if (Event.GetEffectingButton() == State->Config.Gesture.GestureButton && Panel
            && PanelAt(App, Event.GetScreenSpacePosition()) == Panel)
        {
            const FVector2D End = Event.GetScreenSpacePosition();
            const TArray<FLink> Links = FindCrossedLinks(State->Wires, State->Start, End, State->Config.Gesture);
            const FVector2D Center = Panel->PanelCoordToGraphCoord(Panel->GetCachedGeometry().AbsoluteToLocal((State->Start + End) * 0.5));
            const FWireBundleResolvedSettings Config = State->Config;
            Cancel();
            if (Links.Num() >= Config.Gesture.MinimumWireCount) { Bundle(Panel.ToSharedRef(), Center, &Links, &Config); }
        }
        else { Cancel(); }
        return true;
    }
    virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication&, const FPointerEvent& Event, const FPointerEvent*) override
    {
        if (!State) { return false; }
        if (State->Config.Gesture.bWheelChangesColumns && !FMath::IsNearlyZero(Event.GetWheelDelta()))
        {
            State->Config.Movement.RerouteColumns = FMath::Clamp(State->Config.Movement.RerouteColumns
                + (Event.GetWheelDelta() > 0 ? 1 : -1), 1, State->Config.Movement.MaxRerouteColumns);
            if (Preview) { Preview->Invalidate(EInvalidateWidgetReason::Paint); }
        }
        // The wheel changes layout, never zooms the graph during an active gesture.
        return true;
    }
};

TSharedRef<IInputProcessor> CreateInput() { return MakeShared<FInput>(); }
}
