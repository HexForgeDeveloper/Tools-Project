#if WITH_DEV_AUTOMATION_TESTS
#include "WireBundleShapeToolbar.h"
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleShapeToolbarTest, "WireBundle.Shape.ToolbarLayoutAndHitTargets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleShapeToolbarTest::RunTest(const FString&)
{
    for (const FVector2D Origin : {FVector2D::ZeroVector,FVector2D(380,120),FVector2D(-1920,80)})
    {
        for (const FVector2D Size : {FVector2D(900,600),FVector2D(220,600)})
        {
            const FSlateRect View(Origin.X,Origin.Y,Origin.X+Size.X,Origin.Y+Size.Y);
            for (const FVector2D Anchor : {Origin,Origin+Size,Origin+Size*0.5})
            {
                const auto Handles=WireBundle::ShapeToolbarHandles(Anchor,View,7);
                TestEqual(TEXT("All six actions have a slot"),Handles.Num(),6);
                const double R=WireBundle::ShapeToolbarButtonRadius(7);
                for(int32 I=0;I<Handles.Num();++I)
                {
                    const auto H=Handles[I];
                    TestTrue(TEXT("Button stays inside viewport"),H.X-R>=View.Left && H.X+R<=View.Right && H.Y-R>=View.Top && H.Y+R<=View.Bottom);
                    TestTrue(TEXT("Visible button corners accept clicks"),WireBundle::ShapeToolbarHit(H,H+FVector2D(R-1,R-1),7));
                    TestFalse(TEXT("Outside the button does not capture"),WireBundle::ShapeToolbarHit(H,H+FVector2D(R+1,0),7));
                    for(int32 J=I+1;J<Handles.Num();++J)
                    { TestFalse(TEXT("Buttons have disjoint hit targets"),WireBundle::ShapeToolbarHit(H,Handles[J],7)); }
                }
                if(Size.X>500 && Anchor==Origin+Size*0.5)
                { TestTrue(TEXT("Toolbar is above selected nodes"),Handles[0].Y+R<Anchor.Y-12); }
            }
        }
    }
    return true;
}
#endif
