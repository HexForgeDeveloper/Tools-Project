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

namespace WireBundle
{
static double Cross(const FVector2D& A, const FVector2D& B)
{
    return A.X * B.Y - A.Y * B.X;
}

bool CrossesWire(const FVector2D& Start, const FVector2D& End,
    const TArray<FVector2D>& Wire, double Tolerance)
{
    if ((End - Start).SizeSquared() < 64.0) { return false; }
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
    const FVector2D& Start, const FVector2D& End)
{
    TArray<FLink> Result;
    for (const FWireSnapshot& Wire : Wires)
    {
        if (CrossesWire(Start, End, Wire.Points))
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

static TArray<FWireSnapshot> Snapshot(const TSharedRef<SGraphPanel>& Panel)
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
            constexpr int32 Steps = 96;
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
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override
    {
        if (!State) { return Layer; }
        Elements.PushClip(FSlateClippingZone(State->Clip));
        int32 Count = 0;
        for (const FWireSnapshot& Wire : State->Wires)
        {
            if (!CrossesWire(State->Start, State->End, Wire.Points)) { continue; }
            ++Count;
            TArray<FVector2D> Points;
            for (const FVector2D& Point : Wire.Points) { Points.Add(Geometry.AbsoluteToLocal(Point)); }
            FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Points,
                ESlateDrawEffect::None, FLinearColor(0.1f, 0.8f, 1.0f, 0.8f), true, 3.0f);
        }
        const TArray<FVector2D> Line = { Geometry.AbsoluteToLocal(State->Start), Geometry.AbsoluteToLocal(State->End) };
        const FLinearColor Color = Count >= 2 ? FLinearColor(0.15f, 1.0f, 0.55f) : FLinearColor(1.0f, 0.65f, 0.15f);
        FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Line,
            ESlateDrawEffect::None, Color, true, 2.0f);
        FSlateDrawElement::MakeText(Elements, Layer + 2,
            Geometry.ToPaintGeometry(FVector2D(320, 24), FSlateLayoutTransform(Line.Last() + FVector2D(16, 12))),
            FString::Printf(TEXT("%d wires | Release to bundle | Esc cancels"), Count),
            FCoreStyle::GetDefaultFontStyle("Regular", 11), ESlateDrawEffect::None, Color);
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
            || !App.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))) { Cancel(); }
    }
    virtual bool HandleKeyDownEvent(FSlateApplication& App, const FKeyEvent& Event) override
    {
        if (State)
        {
            if (Event.GetKey() == EKeys::Escape) { Cancel(); }
            return true;
        }
        if (Event.IsRepeat() || Event.GetKey() != EKeys::B || !Event.IsControlDown()
            || !Event.IsAltDown() || Event.IsShiftDown()) { return false; }
        const TSharedPtr<SGraphPanel> Panel = PanelAt(App, App.GetCursorPos());
        if (!Panel || !CanEdit(Panel.ToSharedRef())) { return false; }
        Bundle(Panel.ToSharedRef(), Panel->PanelCoordToGraphCoord(
            Panel->GetCachedGeometry().AbsoluteToLocal(App.GetCursorPos())));
        return true;
    }
    virtual bool HandleMouseButtonDownEvent(FSlateApplication& App, const FPointerEvent& Event) override
    {
        if (State) { Cancel(); return true; }
        if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !Event.IsControlDown()
            || !Event.IsAltDown() || Event.IsShiftDown()) { return false; }
        const TSharedPtr<SGraphPanel> Panel = PanelAt(App, Event.GetScreenSpacePosition());
        if (!Panel || !CanEdit(Panel.ToSharedRef())) { return false; }
        const TSharedPtr<SWindow> Window = App.FindWidgetWindow(Panel.ToSharedRef());
        if (!Window) { return false; }
        State = MakeShared<FGestureState>();
        State->Start = State->End = Event.GetScreenSpacePosition();
        State->Clip = Panel->GetCachedGeometry().GetLayoutBoundingRect();
        State->Wires = Snapshot(Panel.ToSharedRef());
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
        if (Event.GetEffectingButton() == EKeys::LeftMouseButton && Panel
            && PanelAt(App, Event.GetScreenSpacePosition()) == Panel)
        {
            const FVector2D End = Event.GetScreenSpacePosition();
            const TArray<FLink> Links = FindCrossedLinks(State->Wires, State->Start, End);
            const FVector2D Center = Panel->PanelCoordToGraphCoord(Panel->GetCachedGeometry().AbsoluteToLocal((State->Start + End) * 0.5));
            Cancel();
            if (Links.Num() >= 2) { Bundle(Panel.ToSharedRef(), Center, &Links); }
        }
        else { Cancel(); }
        return true;
    }
    virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication&, const FPointerEvent&, const FPointerEvent*) override
    {
        // Keep the geometry snapshot fixed until release or cancellation.
        return State.IsValid();
    }
};

TSharedRef<IInputProcessor> CreateInput() { return MakeShared<FInput>(); }
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleIntersectionTest, "WireBundle.Gesture.Intersections",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleIntersectionTest::RunTest(const FString&)
{
    const TArray<FVector2D> Wire = {FVector2D(0, 50), FVector2D(100, 50)};
    TestTrue(TEXT("Crossing"), WireBundle::CrossesWire(FVector2D(50, 0), FVector2D(50, 100), Wire));
    TestTrue(TEXT("Reverse gesture"), WireBundle::CrossesWire(FVector2D(50, 100), FVector2D(50, 0), Wire));
    TestFalse(TEXT("Nearby non-crossing wire"), WireBundle::CrossesWire(FVector2D(50, 0), FVector2D(50, 40), Wire));
    TestTrue(TEXT("Touching endpoint"), WireBundle::CrossesWire(FVector2D(50, 0), FVector2D(50, 50), Wire));
    TestTrue(TEXT("Tolerance"), WireBundle::CrossesWire(FVector2D(50, 0), FVector2D(50, 48), Wire));
    TestFalse(TEXT("Click is not a gesture"), WireBundle::CrossesWire(FVector2D(50, 50), FVector2D(50, 50), Wire));
    TestFalse(TEXT("Short drag is not a gesture"), WireBundle::CrossesWire(FVector2D(50, 48), FVector2D(50, 52), Wire));
    TestTrue(TEXT("Collinear overlap"), WireBundle::CrossesWire(FVector2D(20, 50), FVector2D(80, 50), Wire));
    TestFalse(TEXT("Collinear disjoint"), WireBundle::CrossesWire(FVector2D(120, 50), FVector2D(180, 50), Wire));
    const TArray<FVector2D> Curve = {FVector2D(0, 0), FVector2D(50, 100), FVector2D(100, 0)};
    TestTrue(TEXT("Curved wire crossing"), WireBundle::CrossesWire(FVector2D(0, 50), FVector2D(100, 50), Curve));
    WireBundle::FWireSnapshot Snapshot;
    Snapshot.Points = Curve;
    TestEqual(TEXT("Multiple crossings select one edge"), WireBundle::FindCrossedLinks({Snapshot}, FVector2D(0, 50), FVector2D(100, 50)).Num(), 1);
    return true;
}
#endif
