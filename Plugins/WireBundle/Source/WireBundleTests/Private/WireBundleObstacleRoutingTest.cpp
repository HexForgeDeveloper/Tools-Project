#if WITH_DEV_AUTOMATION_TESTS
#include "WireBundleRoutePlanner.h"
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleObstacleRoutingTest, "WireBundle.Routing.ObstacleCorridors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleObstacleRoutingTest::RunTest(const FString&)
{
    FWireBundleResolvedSettings S;
    S.Movement.bAvoidNodes=true; S.Movement.BundleWidth=240; S.Movement.WireSpacing=20;
    const FGuid Blocker=FGuid::NewGuid(), Source=FGuid::NewGuid(), Target=FGuid::NewGuid();
    TArray<WireBundle::FRouteWireGeometry> Wires;
    for(int32 I=0;I<2;++I)
    {
        WireBundle::FRouteWireGeometry W;
        W.Link.SourceY=I; W.Source=FVector2D(0,I*20-10); W.Target=FVector2D(800,I*20-10);
        W.SourceNode=Source; W.TargetNode=Target; Wires.Add(W);
    }
    const TArray<WireBundle::FRouteObstacle> Walls={{Blocker,FBox2D(FVector2D(340,-60),FVector2D(460,60))}};
    const auto Tangent=[](const FVector2D& A,const FVector2D& B) { return FVector2D(FMath::Abs(B.X-A.X),0); };
    auto Plan=[&](const TArray<WireBundle::FRouteObstacle>& O) { return WireBundle::BuildObstacleRoutePlan(Wires,O,FVector2D(400,0),S,Tangent); };
    const auto Clear=Plan({});
    TestFalse(TEXT("Unobstructed layout is unchanged"),Clear.bBlocked || Clear.bAvoidedObstacles);
    for(int32 Columns:{1,2,3,5})
    {
        S.Movement.RerouteColumns=Columns;
        const auto P=Plan(Walls);
        TestTrue(TEXT("A free corridor is found"),P.bAvoidedObstacles && !P.bBlocked);
        TestEqual(TEXT("Wheel column count is preserved"),P.Lanes[0].Reroutes.Num(),Columns);
        TestTrue(TEXT("Wire spacing is preserved"),FMath::IsNearlyEqual(P.Lanes[1].Entry.Y-P.Lanes[0].Entry.Y,20.0));
        // Independent dense sampling validates the actual curved wire segments,
        // including the endpoint approaches, not just knot centers.
        for(int32 Lane=0;Lane<2;++Lane)
        {
            TArray<FVector2D> Points={Wires[Lane].Source}; Points.Append(P.Lanes[Lane].Reroutes); Points.Add(Wires[Lane].Target);
            for(int32 I=1;I<Points.Num();++I)
            {
                const auto T=Tangent(Points[I-1],Points[I]);
                for(int32 K=0;K<=512;++K)
                { if(Walls[0].Bounds.ExpandBy(24).IsInside(FMath::CubicInterp(Points[I-1],T,Points[I],T,double(K)/512)))
                    { AddError(TEXT("A routed spline intersects the clearance area")); return false; } }
            }
        }
    }
    S.Movement.RerouteColumns=2;
    const auto Both=Plan(Walls);
    TestTrue(TEXT("Equal alternatives choose the upper corridor deterministically"),Both.Center.Y<0);
    auto UpperBlocked=Walls; UpperBlocked.Add({FGuid::NewGuid(),FBox2D(FVector2D(340,-1500),FVector2D(460,-60))});
    const auto Lower=Plan(UpperBlocked);
    TestTrue(TEXT("A blocked upper corridor uses the lower corridor"),!Lower.bBlocked && Lower.Center.Y>0);
    S.Movement.bSnapToGrid=true; S.Movement.GridSize=16;
    const auto Grid=Plan(Walls);
    TestTrue(TEXT("Detour center respects the grid"),FMath::IsNearlyZero(FMath::Fmod(Grid.Center.Y,16.0)));
    S.Movement.MaxRouteDetour=32;
    TestTrue(TEXT("No route in the limit is explicitly blocked"),Plan(Walls).bBlocked);
    S.Movement.bAvoidNodes=false;
    TestFalse(TEXT("Disabled routing keeps legacy behavior"),Plan(Walls).bBlocked);
    S.Movement.bAvoidNodes=true;
    TestFalse(TEXT("Endpoint node bodies are not treated as foreign obstacles"),Plan({{Source,Walls[0].Bounds},{Target,Walls[0].Bounds}}).bBlocked);
    S.Movement.ObstacleClearance=-1; S.Movement.MaxRouteDetour=-1; S.Sanitize();
    TestEqual(TEXT("Clearance is bounded"),S.Movement.ObstacleClearance,8.0f);
    TestEqual(TEXT("Detour is bounded"),S.Movement.MaxRouteDetour,32.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleMultiBendRoutingTest, "WireBundle.Routing.StaggeredObstacles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleMultiBendRoutingTest::RunTest(const FString&)
{
    FWireBundleResolvedSettings S;
    S.Movement.bAvoidNodes=true; S.Movement.BundleWidth=800; S.Movement.WireSpacing=20;
    S.Movement.MaxRouteDetour=200;
    TArray<WireBundle::FRouteWireGeometry> Wires;
    for(int32 I=0;I<2;++I)
    {
        WireBundle::FRouteWireGeometry W;
        W.Link.SourceY=I; W.Source=FVector2D(0,I*20-10); W.Target=FVector2D(1000,I*20-10); Wires.Add(W);
    }
    const TArray<WireBundle::FRouteObstacle> Walls={
        {FGuid::NewGuid(),FBox2D(FVector2D(240,-800),FVector2D(360,50))},
        {FGuid::NewGuid(),FBox2D(FVector2D(640,-50),FVector2D(760,800))}};
    const auto Tangent=[](const FVector2D& A,const FVector2D& B) { return FVector2D(FMath::Abs(B.X-A.X),0); };
    auto Plan=[&] { return WireBundle::BuildObstacleRoutePlan(Wires,Walls,FVector2D(500,0),S,Tangent); };
    S.Movement.bAllowExtraBends=false;
    TestTrue(TEXT("No common horizontal corridor clears staggered walls"),Plan().bBlocked);
    S.Movement.bAllowExtraBends=true;
    for(int32 Columns:{1,2,3})
    {
        S.Movement.RerouteColumns=Columns;
        const auto P=Plan();
        if(!TestFalse(TEXT("Multi-bend path clears staggered walls"),P.bBlocked)) { return false; }
        TestTrue(TEXT("Additional knots are reported"),P.ExtraBends>0);
        TestEqual(TEXT("Requested columns remain the base count"),P.Columns,Columns);
        TestEqual(TEXT("Extra knots count matches the preview"),P.Lanes[0].Reroutes.Num(),Columns+P.ExtraBends);
        TestTrue(TEXT("Same inputs produce the same route"),Plan().Lanes[0].Reroutes==P.Lanes[0].Reroutes);
        const auto Base=WireBundle::BuildParallelRoutePlan({Wires[0].Link,Wires[1].Link},FVector2D(500,0),S);
        for(const auto& Column:Base.Lanes[0].Reroutes)
        { TestTrue(TEXT("Every requested column is retained"),P.Lanes[0].Reroutes.ContainsByPredicate([&](FVector2D V) { return V.X==Column.X; })); }
        for(int32 L=0;L<P.Lanes.Num();++L)
        {
            TArray<FVector2D> Points={Wires[L].Source}; Points.Append(P.Lanes[L].Reroutes); Points.Add(Wires[L].Target);
            for(int32 I=0;I<P.Lanes[L].Reroutes.Num();++I)
            {
                const auto V=P.Lanes[L].Reroutes[I];
                TestTrue(TEXT("All new knots obey the detour bound"),FMath::Abs(V.Y-(L-0.5)*20)<=200);
                TestTrue(TEXT("Spacing is preserved at every bend"),FMath::IsNearlyEqual(P.Lanes[1].Reroutes[I].Y-P.Lanes[0].Reroutes[I].Y,20.0));
            }
            for(int32 I=1;I<Points.Num();++I)
            {
                TestTrue(TEXT("No backward knot segments"),Points[I].X>=Points[I-1].X);
                const auto T=Tangent(Points[I-1],Points[I]);
                for(int32 K=0;K<=1024;++K)
                {
                    const auto V=FMath::CubicInterp(Points[I-1],T,Points[I],T,double(K)/1024);
                    for(const auto& Wall:Walls)
                    { if(Wall.Bounds.ExpandBy(24).IsInside(V)) { AddError(TEXT("A multi-bend spline intersects an obstacle")); return false; } }
                }
            }
        }
    }
    S.Movement.RerouteColumns=2; S.Movement.MaxRoutingReroutes=2;
    TestTrue(TEXT("A knot limit rejects the entire route"),Plan().bBlocked);
    S.Movement.MaxRoutingReroutes=16; S.Movement.bSnapToGrid=true; S.Movement.GridSize=16;
    const auto Grid=Plan();
    TestFalse(TEXT("Multi-bend route supports grid snapping"),Grid.bBlocked);
    for(const auto& V:Grid.Lanes[0].Reroutes)
    { TestTrue(TEXT("Bend row centers snap to grid"),FMath::IsNearlyZero(FMath::Fmod(V.Y+10,16.0))); }
    S.Movement.MaxRouteDetour=32;
    TestTrue(TEXT("Insufficient vertical room fails without a partial path"),Plan().bBlocked);
    S.Movement.MaxRoutingReroutes=999; S.Sanitize();
    TestEqual(TEXT("Routing knot budget is clamped"),S.Movement.MaxRoutingReroutes,64);
    return true;
}
#endif
