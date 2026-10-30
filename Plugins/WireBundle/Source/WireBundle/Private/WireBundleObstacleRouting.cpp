#include "WireBundleRoutePlanner.h"
#include "Algo/Reverse.h"

namespace WireBundle
{
    // Conservative cubic/rectangle intersection: subdivide the Bezier hull.
    // A collision is never missed between sparse preview sample points.
    static bool CurveHits(const FVector2D& A, const FVector2D& B, const FVector2D& C,
        const FVector2D& D, const FBox2D& Box, int32 Depth=0)
    {
        FBox2D Hull(ForceInit); Hull+=A; Hull+=B; Hull+=C; Hull+=D;
        if (!Hull.Intersect(Box)) { return false; }
        if (Depth>=10 || Hull.GetSize().GetMax()<0.5) { return true; }
        const auto AB=(A+B)*0.5, BC=(B+C)*0.5, CD=(C+D)*0.5;
        const auto ABC=(AB+BC)*0.5, BCD=(BC+CD)*0.5, Mid=(ABC+BCD)*0.5;
        return CurveHits(A,AB,ABC,Mid,Box,Depth+1) || CurveHits(Mid,BCD,CD,D,Box,Depth+1);
    }
    static bool TryExtraBends(FBundleRoutePlan& Plan, const TArray<FRouteWireGeometry>& Wires,
        const TArray<FRouteObstacle>& Obstacles, const FWireBundleResolvedSettings& Config,
        TFunctionRef<FVector2D(const FVector2D&, const FVector2D&)> Tangent,
        TFunctionRef<bool(const FBundleRoutePlan&)> ClearPlan)
    {
        if(!Config.Movement.bAllowExtraBends || Plan.Lanes.IsEmpty()) { return false; }
        TArray<FRouteWireGeometry> Sorted;
        double MinX=-DBL_MAX,MaxX=DBL_MAX;
        for(const auto& Lane:Plan.Lanes)
        {
            const auto W=Wires.FindByPredicate([&](const FRouteWireGeometry& Wire)
            { return Wire.Link.Output==Lane.Link.Output && Wire.Link.Input==Lane.Link.Input && Wire.Link.SourceY==Lane.Link.SourceY; });
            if(!W) { return false; }
            Sorted.Add(*W); MinX=FMath::Max(MinX,W->Source.X+8); MaxX=FMath::Min(MaxX,W->Target.X-8);
        }
        if(MinX>=MaxX) { return false; }
        const double Clearance=Config.Movement.ObstacleClearance+21;
        const double Half=(Sorted.Num()-1)*Config.Movement.WireSpacing*0.5;
        const double Max=Config.Movement.MaxRouteDetour;
        const double Grid=Config.Movement.GridSize;
        auto Snap=[&](double V,bool Up)
        { return Config.Movement.bSnapToGrid ? (Up?FMath::CeilToDouble(V/Grid):FMath::FloorToDouble(V/Grid))*Grid : V; };
        TArray<double> Mandatory, Xs, Ys={Plan.Center.Y};
        for(const auto& P:Plan.Lanes[0].Reroutes)
        {
            if(P.X<MinX || P.X>MaxX) { return false; }
            Mandatory.Add(P.X); Xs.Add(P.X);
        }
        const double StartX=Snap(MinX,true), EndX=Snap(MaxX,false);
        if(StartX<=EndX) { Xs.Add(StartX); Xs.Add(EndX); }
        for(const auto& O:Obstacles)
        {
            const auto Box=O.Bounds.ExpandBy(Clearance);
            if(Box.Max.X<MinX || Box.Min.X>MaxX || Box.Max.Y<Plan.Center.Y-Max-Half || Box.Min.Y>Plan.Center.Y+Max+Half) { continue; }
            for(double X:{Snap(Box.Min.X-1,false),Snap(Box.Max.X+1,true)})
            { if(X>=MinX && X<=MaxX) { Xs.Add(X); } }
            for(double Y:{Snap(Box.Min.Y-Half-1,false),Snap(Box.Max.Y+Half+1,true)})
            { if(FMath::Abs(Y-Plan.Center.Y)<=Max) { Ys.Add(Y); } }
        }
        Ys.Add(Snap(Plan.Center.Y-Max,true)); Ys.Add(Snap(Plan.Center.Y+Max,false));
        auto Unique=[](TArray<double>& Values)
        {
            Values.Sort();
            for(int32 I=Values.Num()-1;I>0;--I) { if(FMath::IsNearlyEqual(Values[I],Values[I-1],0.01)) { Values.RemoveAt(I); } }
        };
        Unique(Xs); Unique(Ys);
        // Bound work even in dense graphs. A limit rejects the plan before mutation.
        if(Xs.Num()>64) { return false; }
        Ys.StableSort([&](double A,double B) { return FMath::Abs(A-Plan.Center.Y)==FMath::Abs(B-Plan.Center.Y)?A<B:FMath::Abs(A-Plan.Center.Y)<FMath::Abs(B-Plan.Center.Y); });
        if(Ys.Num()>24) { Ys.SetNum(24); }
        const int32 NY=Ys.Num();
        TArray<TArray<FBox2D>> Boxes; Boxes.SetNum(Sorted.Num());
        for(int32 L=0;L<Sorted.Num();++L)
        {
            for(const auto& O:Obstacles)
            {
                if(O.Node.IsValid() && (O.Node==Sorted[L].SourceNode || O.Node==Sorted[L].TargetNode)) { continue; }
                Boxes[L].Add(O.Bounds.ExpandBy(Clearance));
            }
        }
        int32 Attempts=0;
        auto SegmentClear=[&](int32 L,const FVector2D& A,const FVector2D& B,bool StartReversed=false,bool EndReversed=false)
        {
            if(A.X>B.X) { return false; }
            const auto T=Tangent(A,B);
            for(const auto& Box:Boxes[L])
            { if(CurveHits(A,A+(StartReversed?-T:T)/3,B-(EndReversed?-T:T)/3,B,Box)) { return false; } }
            return true;
        };
        auto EdgeClear=[&](FVector2D A,FVector2D B)
        {
            if(++Attempts>32768) { return false; }
            for(int32 L=0;L<Sorted.Num();++L)
            {
                const FVector2D Offset(0,(L-(Sorted.Num()-1)*0.5)*Config.Movement.WireSpacing);
                if(!SegmentClear(L,A+Offset,B+Offset)) { return false; }
            }
            return true;
        };
        auto EndpointClear=[&](FVector2D P,bool Source)
        {
            for(int32 L=0;L<Sorted.Num();++L)
            {
                const auto Q=P+FVector2D(0,(L-(Sorted.Num()-1)*0.5)*Config.Movement.WireSpacing);
                if(!SegmentClear(L,Source?Sorted[L].Source:Q,Source?Q:Sorted[L].Target,
                    Source?Sorted[L].SourceEndpoint.IsReversed(Q.X,true):false,Source?false:Sorted[L].TargetEndpoint.IsReversed(Q.X,false))) { return false; }
            }
            return true;
        };
        TArray<double> Costs; Costs.Init(DBL_MAX,Xs.Num()*NY);
        TArray<int32> Previous; Previous.Init(INDEX_NONE,Costs.Num());
        for(int32 Y=0;Y<NY;++Y)
        { if(EndpointClear(FVector2D(Xs[0],Ys[Y]),true)) { Costs[Y]=FMath::Abs(Ys[Y]-Plan.Center.Y); } }
        for(int32 X=1;X<Xs.Num();++X)
        {
            for(int32 Y=0;Y<NY;++Y)
            {
                for(int32 PY=0;PY<NY;++PY)
                {
                    const int32 Prev=(X-1)*NY+PY;
                    if(Costs[Prev]==DBL_MAX) { continue; }
                    const FVector2D A(Xs[X-1],Ys[PY]),B(Xs[X],Ys[Y]);
                    const double Cost=Costs[Prev]+(B-A).Size()+(PY==Y?0:16);
                    const int32 Index=X*NY+Y;
                    if(Cost<Costs[Index] && EdgeClear(A,B)) { Costs[Index]=Cost; Previous[Index]=Prev; }
                    if(Attempts>32768) { return false; }
                }
            }
        }
        int32 Best=INDEX_NONE; double BestCost=DBL_MAX;
        for(int32 Y=0;Y<NY;++Y)
        {
            const int32 I=(Xs.Num()-1)*NY+Y;
            const double Cost=Costs[I]+FMath::Abs(Ys[Y]-Plan.Center.Y);
            if(Cost<BestCost && EndpointClear(FVector2D(Xs.Last(),Ys[Y]),false)) { BestCost=Cost; Best=I; }
        }
        if(Best==INDEX_NONE) { return false; }
        TArray<FVector2D> Path; Path.SetNum(Xs.Num());
        for(int32 X=Xs.Num()-1;X>=0;--X) { Path[X]=FVector2D(Xs[X],Ys[Best%NY]); Best=Previous[Best]; }
        auto Candidate=Plan;
        for(int32 L=0;L<Candidate.Lanes.Num();++L)
        {
            auto& Lane=Candidate.Lanes[L]; Lane.Reroutes=Path;
            for(auto& P:Lane.Reroutes) { P.Y+=(L-(Sorted.Num()-1)*0.5)*Config.Movement.WireSpacing; }
        }
        // Remove unnecessary stations, but retain every wheel-selected column.
        for(int32 I=0;I<Candidate.Lanes[0].Reroutes.Num();)
        {
            const double X=Candidate.Lanes[0].Reroutes[I].X;
            if(Mandatory.ContainsByPredicate([&](double V) { return FMath::IsNearlyEqual(X,V,0.01); })) { ++I; continue; }
            auto Reduced=Candidate;
            for(auto& Lane:Reduced.Lanes) { Lane.Reroutes.RemoveAt(I); }
            if(ClearPlan(Reduced)) { Candidate=MoveTemp(Reduced); } else { ++I; }
        }
        if(Candidate.Lanes[0].Reroutes.Num()>Config.Movement.MaxRoutingReroutes || !ClearPlan(Candidate)) { return false; }
        for(auto& Lane:Candidate.Lanes) { Lane.Entry=Lane.Reroutes[0]; Lane.Exit=Lane.Reroutes.Last(); }
        Candidate.ExtraBends=Candidate.Lanes[0].Reroutes.Num()-Candidate.Columns;
        Candidate.bAvoidedObstacles=true; Plan=MoveTemp(Candidate); return true;
    }
    FBundleRoutePlan BuildObstacleRoutePlan(const TArray<FRouteWireGeometry>& Wires,
        const TArray<FRouteObstacle>& Obstacles, const FVector2D& Center,
        const FWireBundleResolvedSettings& Settings,
        TFunctionRef<FVector2D(const FVector2D&, const FVector2D&)> Tangent)
    {
        auto Config=Settings; Config.Sanitize();
        TArray<FLink> Links; for(const auto& W:Wires) { Links.Add(W.Link); }
        auto Base=BuildParallelRoutePlan(Links,Center,Config);
        if(Wires.IsEmpty()) { return Base; }
        const bool bBackward=Wires.ContainsByPredicate([](const FRouteWireGeometry& W) { return W.Source.X>W.Target.X; });
        const bool bForward=Wires.ContainsByPredicate([](const FRouteWireGeometry& W) { return W.Source.X<=W.Target.X; });
        if(bBackward && !bForward)
        {
            // Reflect into the forward search frame. Its origin is already snapped,
            // so mandatory columns and the global grid survive the round trip.
            const double Origin=Base.Center.X;
            auto Mirrored=Wires;
            auto MirrorEndpoint=[&](FRouteEndpointGeometry& E)
            {
                E.CenterX=Origin-E.CenterX;
                E.IncomingX=E.IncomingCount*Origin-E.IncomingX;
                E.OutgoingX=E.OutgoingCount*Origin-E.OutgoingX;
                E.FixedDirection=-E.FixedDirection;
            };
            for(auto& W:Mirrored)
            {
                W.Source.X=Origin-W.Source.X; W.Target.X=Origin-W.Target.X;
                MirrorEndpoint(W.SourceEndpoint); MirrorEndpoint(W.TargetEndpoint);
            }
            auto Boxes=Obstacles;
            for(auto& O:Boxes)
            { O.Bounds=FBox2D(FVector2D(Origin-O.Bounds.Max.X,O.Bounds.Min.Y),FVector2D(Origin-O.Bounds.Min.X,O.Bounds.Max.Y)); }
            auto MirrorConfig=Config; MirrorConfig.Movement.CenterOffset=FVector2D::ZeroVector;
            auto Result=BuildObstacleRoutePlan(Mirrored,Boxes,FVector2D(0,Base.Center.Y),MirrorConfig,[&](const FVector2D& A,const FVector2D& B)
            {
                const auto T=Tangent(FVector2D(Origin-A.X,A.Y),FVector2D(Origin-B.X,B.Y));
                return FVector2D(T.X,-T.Y);
            });
            Result.Center.X=Origin-Result.Center.X;
            for(auto& Lane:Result.Lanes)
            {
                for(auto& P:Lane.Reroutes) { P.X=Origin-P.X; }
                Lane.Entry=Lane.Reroutes[0]; Lane.Exit=Lane.Reroutes.Last();
                Lane.Direction=-1; Lane.bSourceReversed=!Lane.bSourceReversed; Lane.bTargetReversed=!Lane.bTargetReversed;
                for(auto& Direction:Lane.KnotDirections) { Direction=-Direction; }
            }
            return Result;
        }
        auto Finish=[&](FBundleRoutePlan P)
        {
            for(auto& Lane:P.Lanes)
            {
                const auto W=Wires.FindByPredicate([&](const FRouteWireGeometry& G)
                { return G.Link.Output==Lane.Link.Output && G.Link.Input==Lane.Link.Input && G.Link.SourceY==Lane.Link.SourceY; });
                if(!W) { continue; }
                Lane.KnotDirections.Reset();
                for(int32 I=0;I<Lane.Reroutes.Num();++I)
                {
                    const double Before=I==0?W->Source.X:Lane.Reroutes[I-1].X;
                    const double After=I==Lane.Reroutes.Num()-1?W->Target.X:Lane.Reroutes[I+1].X;
                    Lane.KnotDirections.Add(After<Before?-1:1);
                }
                Lane.bSourceReversed=W->SourceEndpoint.IsReversed(Lane.Reroutes[0].X,true);
                Lane.bTargetReversed=W->TargetEndpoint.IsReversed(Lane.Reroutes.Last().X,false);
            }
            return P;
        };
        if(bBackward)
        {
            for(auto& Lane:Base.Lanes)
            {
                const auto W=Wires.FindByPredicate([&](const FRouteWireGeometry& G)
                { return G.Link.Output==Lane.Link.Output && G.Link.Input==Lane.Link.Input && G.Link.SourceY==Lane.Link.SourceY; });
                if(W && W->Source.X>W->Target.X) { Algo::Reverse(Lane.Reroutes); Lane.Direction=-1; Lane.Entry=Lane.Reroutes[0]; Lane.Exit=Lane.Reroutes.Last(); }
            }
            // A shared search cannot certify opposite travel directions in one bundle.
            Base.bBlocked=Config.Movement.bAvoidNodes && !Obstacles.IsEmpty();
            return Finish(Base);
        }
        if (!Config.Movement.bAvoidNodes || Obstacles.IsEmpty()) { return Finish(Base); }
        const double Clearance=Config.Movement.ObstacleClearance+21;
        auto Clear=[&](const FBundleRoutePlan& Plan)
        {
            for(const auto& Lane:Plan.Lanes)
            {
                const auto* Wire=Wires.FindByPredicate([&](const FRouteWireGeometry& W)
                { return W.Link.Output==Lane.Link.Output && W.Link.Input==Lane.Link.Input && W.Link.SourceY==Lane.Link.SourceY; });
                if(!Wire) { return false; }
                TArray<FVector2D> Points; Points.Add(Wire->Source); Points.Append(Lane.Reroutes); Points.Add(Wire->Target);
                for(const auto& Obstacle:Obstacles)
                {
                    if(Obstacle.Node.IsValid() && (Obstacle.Node==Wire->SourceNode || Obstacle.Node==Wire->TargetNode)) { continue; }
                    const auto Box=Obstacle.Bounds.ExpandBy(Clearance);
                    for(int32 I=1;I<Points.Num();++I)
                    {
                        if(Points[I].X<Points[I-1].X) { return false; }
                        const auto T=Tangent(Points[I-1],Points[I]);
                        const bool StartReversed=I==1 ? Wire->SourceEndpoint.IsReversed(Points[1].X,true) : false;
                        const bool EndReversed=I==Points.Num()-1 ? Wire->TargetEndpoint.IsReversed(Points[I-1].X,false) : false;
                        if(CurveHits(Points[I-1],Points[I-1]+(StartReversed?-T:T)/3,Points[I]-(EndReversed?-T:T)/3,Points[I],Box)) { return false; }
                    }
                }
            }
            return true;
        };
        if(Clear(Base)) { return Finish(Base); }
        const double Max=Config.Movement.MaxRouteDetour;
        const double Half=(Wires.Num()-1)*Config.Movement.WireSpacing*0.5;
        TArray<double> Offsets;
        for(const auto& O:Obstacles)
        {
            Offsets.Add(double(O.Bounds.Min.Y)-Clearance-Half-1-Base.Center.Y);
            Offsets.Add(double(O.Bounds.Max.Y)+Clearance+Half+1-Base.Center.Y);
        }
        for(double D=32;D<Max;D+=32) { Offsets.Add(-D); Offsets.Add(D); }
        Offsets.Add(-Max); Offsets.Add(Max);
        Offsets.StableSort([](double A,double B) { return FMath::Abs(A)==FMath::Abs(B) ? A<B : FMath::Abs(A)<FMath::Abs(B); });
        for(double Offset:Offsets)
        {
            if(!FMath::IsFinite(Offset) || FMath::Abs(Offset)>Max) { continue; }
            auto Candidate=Base;
            double Delta=Offset;
            if(Config.Movement.bSnapToGrid)
            {
                const double Grid=Config.Movement.GridSize;
                Delta=(Offset<0 ? FMath::FloorToDouble((Base.Center.Y+Offset)/Grid) : FMath::CeilToDouble((Base.Center.Y+Offset)/Grid))*Grid-Base.Center.Y;
            }
            if(FMath::Abs(Delta)>Max) { continue; }
            Candidate.Center.Y+=Delta;
            for(auto& Lane:Candidate.Lanes)
            {
                for(auto& P:Lane.Reroutes) { P.Y+=Delta; }
                Lane.Entry=Lane.Reroutes[0]; Lane.Exit=Lane.Reroutes.Last();
            }
            if(Clear(Candidate)) { Candidate.bAvoidedObstacles=true; return Finish(Candidate); }
        }
        if(TryExtraBends(Base,Wires,Obstacles,Config,Tangent,Clear)) { return Finish(Base); }
        Base.bBlocked=true;
        return Finish(Base);
    }
} // namespace WireBundle
