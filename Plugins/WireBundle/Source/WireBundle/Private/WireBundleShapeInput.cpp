#include "WireBundleShape.h"
#include "WireBundleGesture.h"
#include "WireBundleCoordinates.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "SGraphPanel.h"
#include "EdGraph/EdGraph.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SWindow.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "Styling/CoreStyle.h"

namespace WireBundle
{
struct FShapeState
{
    TWeakPtr<SGraphPanel> Panel;
    FShapeSnapshot Shape;
    FWireBundleResolvedSettings Config;
    bool bDragging = false;
    EShapeHandle Handle = EShapeHandle::Move;
    FVector2D Start, Delta = FVector2D::ZeroVector;
};
static TSharedPtr<SGraphPanel> ShapePanelAt(FSlateApplication& App, const FVector2D& Cursor)
{
    const auto Path = App.LocateWindowUnderMouse(Cursor, App.GetInteractiveTopLevelWindows());
    for (int32 I = Path.Widgets.Num()-1; I >= 0; --I)
    { if (Path.Widgets[I].Widget->GetType() == FName("SGraphPanel")) { return StaticCastSharedRef<SGraphPanel>(Path.Widgets[I].Widget); } }
    return nullptr;
}
static TArray<FVector2D> ShapeHandles(const FShapeState& State)
{
    const auto Panel = State.Panel.Pin();
    if (!Panel) { return {}; }
    const auto P = State.bDragging ? PlanShapeEdit(State.Shape, State.Handle, State.Delta, State.Config.Movement) : State.Shape.Positions;
    if (P.Num() != State.Shape.Positions.Num() || State.Shape.NumRows() < 1) { return {}; }
    FVector2D Center = FVector2D::ZeroVector;
    double Left = DBL_MAX, Right = -DBL_MAX, Bottom = -DBL_MAX;
    for (int32 Row=0; Row<State.Shape.NumRows(); ++Row)
    {
        double Y=0;
        for (int32 I=State.Shape.RowBegin(Row); I<State.Shape.RowEnd(Row); ++I)
        {
            const auto Point=P[I]+FVector2D(21,12);
            Y+=Point.Y; Bottom=FMath::Max(Bottom,Point.Y);
            Left=FMath::Min(Left,Point.X); Right=FMath::Max(Right,Point.X);
        }
        Center.Y+=Y/(State.Shape.RowEnd(Row)-State.Shape.RowBegin(Row));
    }
    Center.Y/=State.Shape.NumRows(); Center.X=(Left+Right)*0.5;
    auto Project = [&](const FVector2D& Point)
    { return Panel->GetCachedGeometry().LocalToAbsolute((Point - Panel->GetViewOffset()) * Panel->GetZoomAmount()); };
    return {Project(Center), Project(FVector2D(Left, Center.Y)) - FVector2D(0, 26),
        Project(FVector2D(Right, Center.Y)) - FVector2D(0, 26), Project(FVector2D(Center.X, Bottom)) + FVector2D(0, 32)};
}
class SShapeOverlay final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SShapeOverlay) {} SLATE_END_ARGS()
    void Construct(const FArguments&, TSharedRef<FShapeState> InState) { State = InState; SetVisibility(EVisibility::HitTestInvisible); }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect&,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override
    {
        const auto Panel = State->Panel.Pin();
        if (!Panel) { return Layer; }
        const auto Handles = ShapeHandles(*State);
        if (Handles.Num() != 4) { return Layer; }
        const FVector2D Origin = Args.GetWindowToDesktopTransform();
        auto Local = [&](const FVector2D& P) { return DesktopToPaintLocal(P, Origin, Geometry); };
        auto Draw = [&](const TArray<FVector2D>& Desktop, const FLinearColor& Color, float Width)
        {
            TArray<FVector2D> Points;
            for (const auto& P : Desktop) { Points.Add(Local(P)); }
            FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Width);
        };
        Elements.PushClip(FSlateClippingZone(DesktopToPaintClip(Panel->GetCachedGeometry().GetLayoutBoundingRect(), Origin)));
        const auto Color = State->Config.Appearance.ShapeHandleColor;
        const double R = State->Config.Appearance.ShapeHandleRadius;
        for (int32 I = 0; I < Handles.Num(); ++I)
        {
            if ((!State->Shape.HasWidth() && (I == 1 || I == 2)) || (I==3 && State->Shape.NumRows()<2)) { continue; }
            const FVector2D H = Handles[I];
            if (I == 0)
            {
                Draw({H-FVector2D(R,0), H+FVector2D(R,0)}, Color, 2);
                Draw({H-FVector2D(0,R), H+FVector2D(0,R)}, Color, 2);
                for (const FVector2D D : {FVector2D(1,0),FVector2D(-1,0),FVector2D(0,1),FVector2D(0,-1)})
                {
                    const FVector2D Tip = H + D * R, Side(-D.Y, D.X);
                    Draw({Tip-D*4+Side*3,Tip,Tip-D*4-Side*3},Color,2);
                }
            }
            else { Draw({H+FVector2D(0,-R),H+FVector2D(R,0),H+FVector2D(0,R),H+FVector2D(-R,0),H+FVector2D(0,-R)},Color,2); }
        }
        if (State->bDragging)
        {
            const auto P = PlanShapeEdit(State->Shape, State->Handle, State->Delta, State->Config.Movement);
            auto Project = [&](const FVector2D& Point)
            { return Panel->GetCachedGeometry().LocalToAbsolute((Point+FVector2D(21,12)-Panel->GetViewOffset())*Panel->GetZoomAmount()); };
            for (int32 Row=0; Row<State->Shape.NumRows(); ++Row)
            {
                TArray<FVector2D> Line;
                for (int32 I=State->Shape.RowBegin(Row); I<State->Shape.RowEnd(Row); ++I)
                {
                    const auto H=Project(P[I]); Line.Add(H);
                    Draw({H+FVector2D(-4,-4),H+FVector2D(4,-4),H+FVector2D(4,4),H+FVector2D(-4,4),H+FVector2D(-4,-4)},Color,1.5f);
                }
                if (!State->Shape.bSelectionOnly && Line.Num()>1) { Draw(Line, State->Config.Appearance.RoutePreviewColor, State->Config.Appearance.RoutePreviewThickness); }
            }
            if (State->Shape.bSelectionOnly)
            {
                TMap<UEdGraphNode*,int32> Indices;
                for (int32 I=0; I<State->Shape.Nodes.Num(); ++I) { Indices.Add(State->Shape.Nodes[I].Get(),I); }
                for (const auto& PinLinks:State->Shape.Topology)
                {
                    const auto Pin=PinLinks.Pin.Get();
                    if (!Pin || Pin->Direction!=EGPD_Output) { continue; }
                    const auto A=Indices.Find(Pin->GetOwningNode());
                    if (!A) { continue; }
                    for (const auto& Link:PinLinks.Links)
                    {
                        const auto Other=Link.Get();
                        const auto B=Other ? Indices.Find(Other->GetOwningNode()) : nullptr;
                        if (B) { Draw({Project(P[*A]),Project(P[*B])},State->Config.Appearance.RoutePreviewColor,State->Config.Appearance.RoutePreviewThickness); }
                    }
                }
            }
        }
        if (State->Config.Appearance.bShowGestureHint)
        {
            const FString Hint = State->bDragging
                ? FString::Printf(TEXT("Release to apply | %s cancels"), *State->Config.Gesture.CancelKey.GetDisplayName().ToString())
                : State->Shape.NumRows()<2 ? (State->Shape.HasWidth() ? TEXT("Center: move | Sides: width") : TEXT("Center: move"))
                : !State->Shape.HasWidth() ? TEXT("Center: move | Bottom: spacing")
                    : TEXT("Center: move | Sides: width | Bottom: spacing");
            FSlateDrawElement::MakeText(Elements, Layer+1,
                Geometry.ToPaintGeometry(FVector2D(420,24), FSlateLayoutTransform(Local(Handles[0]-FVector2D(100,58)))),
                Hint, FCoreStyle::GetDefaultFontStyle("Regular", State->Config.Appearance.HintFontSize), ESlateDrawEffect::None, Color);
        }
        Elements.PopClip(); return Layer+1;
    }
private:
    TSharedPtr<FShapeState> State;
};
class FShapeInput final : public IInputProcessor
{
    TSharedPtr<FShapeState> State;
    TWeakPtr<SWindow> Window;
    TSharedPtr<SShapeOverlay> Overlay;
    void Cancel()
    {
        if (auto W = Window.Pin()) { if (Overlay) { W->RemoveOverlaySlot(Overlay.ToSharedRef()); } }
        Overlay.Reset(); State.Reset(); Window.Reset();
    }
    void Update(const FVector2D& Cursor)
    {
        if (auto Panel = State->Panel.Pin())
        {
            const auto& G = Panel->GetCachedGeometry();
            State->Delta = Panel->PanelCoordToGraphCoord(G.AbsoluteToLocal(Cursor)) - Panel->PanelCoordToGraphCoord(G.AbsoluteToLocal(State->Start));
            Overlay->Invalidate(EInvalidateWidgetReason::Paint);
        }
    }
public:
    virtual ~FShapeInput() override { Cancel(); }
    virtual void Tick(float, FSlateApplication& App, TSharedRef<ICursor>) override
    {
        if (!App.IsActive()) { Cancel(); return; }
        if (State && State->bDragging)
        {
            auto Panel = State->Panel.Pin();
            if (!Panel || !CanEdit(Panel.ToSharedRef()) || !App.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton)) { Cancel(); return; }
            Update(App.GetCursorPos()); return;
        }
        const auto Config = ResolveWireBundleSettings();
        const auto Panel = ShapePanelAt(App, App.GetCursorPos());
        if (!Config.Movement.bEnableMouseEditing || !Panel || !CanEdit(Panel.ToSharedRef()) || App.GetPressedMouseButtons().Num()) { Cancel(); return; }
        FShapeSnapshot Shape;
        if (!CaptureSelectedShape(Panel.ToSharedRef(),Shape)) { Cancel(); return; }
        auto W = App.FindWidgetWindow(Panel.ToSharedRef());
        if (!W) { Cancel(); return; }
        if (!State || State->Panel.Pin() != Panel || Window.Pin() != W)
        {
            Cancel(); State = MakeShared<FShapeState>(); State->Panel = Panel; Window = W;
            Overlay = SNew(SShapeOverlay, State.ToSharedRef()); W->AddOverlaySlot(1001)[Overlay.ToSharedRef()];
        }
        State->Shape = MoveTemp(Shape); State->Config = Config; Overlay->Invalidate(EInvalidateWidgetReason::Paint);
    }
    virtual bool HandleMouseButtonDownEvent(FSlateApplication& App, const FPointerEvent& Event) override
    {
        if (!State) { return false; }
        if (State->bDragging) { Cancel(); return true; }
        if (Event.GetEffectingButton() != EKeys::LeftMouseButton || Event.IsControlDown() || Event.IsAltDown() || Event.IsShiftDown() || Event.IsCommandDown()) { return false; }
        const auto Panel = State->Panel.Pin();
        if (!Panel || ShapePanelAt(App, Event.GetScreenSpacePosition()) != Panel || !CanEdit(Panel.ToSharedRef())) { return false; }
        const auto Handles = ShapeHandles(*State);
        for (int32 I = 0; I < Handles.Num(); ++I)
        {
            if ((!State->Shape.HasWidth() && (I == 1 || I == 2)) || (I==3 && State->Shape.NumRows()<2)) { continue; }
            if (FVector2D::DistSquared(Handles[I], Event.GetScreenSpacePosition()) <= FMath::Square(State->Config.Appearance.ShapeHandleRadius+3))
            {
                State->bDragging = true; State->Handle = EShapeHandle(I); State->Start = Event.GetScreenSpacePosition(); State->Delta = FVector2D::ZeroVector; return true;
            }
        }
        return false;
    }
    virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent& Event) override
    { if (!State || !State->bDragging) { return false; } Update(Event.GetScreenSpacePosition()); return true; }
    virtual bool HandleMouseButtonUpEvent(FSlateApplication& App, const FPointerEvent& Event) override
    {
        if (!State || !State->bDragging) { return false; }
        const auto Panel = State->Panel.Pin();
        if (Event.GetEffectingButton() == EKeys::LeftMouseButton && Panel && ShapePanelAt(App, Event.GetScreenSpacePosition()) == Panel)
        { Update(Event.GetScreenSpacePosition()); CommitShapeEdit(Panel.ToSharedRef(), State->Shape, State->Handle, State->Delta, State->Config.Movement); }
        Cancel(); return true;
    }
    virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
    { if (!State || !State->bDragging) { return false; } if (Event.GetKey() == State->Config.Gesture.CancelKey) { Cancel(); } return true; }
    virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication&, const FPointerEvent&, const FPointerEvent*) override
    { return State && State->bDragging; }
};
TSharedRef<IInputProcessor> CreateShapeInput() { return MakeShared<FShapeInput>(); }
}
