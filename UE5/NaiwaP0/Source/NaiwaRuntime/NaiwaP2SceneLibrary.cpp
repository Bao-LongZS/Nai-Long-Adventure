#include "NaiwaP2SceneLibrary.h"

namespace NaiwaP2
{
    namespace
    {
        FBox2 Box(const TCHAR* Id, const double X1, const double X2,
                  const double Z1, const double Z2,
                  const TArray<FString>& Kills = {}, const double RequiredRadius = 0.0,
                  const bool bBreakable = false)
        {
            FBox2 Result;
            Result.Id = Id;
            Result.MinX = X1;
            Result.MaxX = X2;
            Result.MinZ = Z1;
            Result.MaxZ = Z2;
            Result.Kills = Kills;
            Result.RequiredRadius = RequiredRadius;
            Result.bBreakable = bBreakable;
            return Result;
        }

        FBox2 Goal(const TCHAR* Id, const double CenterX, const double CenterZ,
                   const double Width = 0.85, const double Height = 0.95)
        {
            FBox2 Result = Box(Id, CenterX - Width / 2.0, CenterX + Width / 2.0,
                               CenterZ - Height / 2.0, CenterZ + Height / 2.0,
                               {FString(Id)}, 0.0, false);
            Result.bGoal = true;
            return Result;
        }

        void Add(FScene& Scene, FBox2 Value)
        {
            if (Value.bGoal)
            {
                Scene.GoalIds.Add(Value.Id);
            }
            Scene.Boxes.Add(MoveTemp(Value));
        }
    }

    FScene FSceneLibrary::Make21()
    {
        FScene Scene;
        Scene.LevelId = TEXT("2-1");
        Scene.MaxX = 18.0;
        Add(Scene, Box(TEXT("W.wall"), 5.40, 6.60, 0.00, 0.80));
        Add(Scene, Box(TEXT("P.col"), 11.05, 11.95, 0.00, 3.60, {TEXT("P1")}, 0.0, true));
        Add(Scene, Box(TEXT("T.base"), 13.90, 17.10, 0.00, 0.30));
        Add(Scene, Box(TEXT("T.c1"), 13.95, 14.25, 0.30, 2.20, {}, 0.0, true));
        Add(Scene, Box(TEXT("T.c2"), 14.75, 15.05, 0.30, 2.20, {TEXT("T1")}, 0.0, true));
        Add(Scene, Box(TEXT("T.c3"), 15.95, 16.25, 0.30, 2.20, {TEXT("T1")}, 0.0, true));
        Add(Scene, Box(TEXT("T.c4"), 16.75, 17.05, 0.30, 2.20, {}, 0.0, true));
        Add(Scene, Box(TEXT("T.d2"), 13.90, 17.10, 2.20, 2.50, {}, 0.0, true));
        Add(Scene, Box(TEXT("T.c5"), 13.95, 14.30, 2.50, 4.20,
                       {TEXT("T1"), TEXT("T2"), TEXT("T3")}, 0.0, true));
        Add(Scene, Box(TEXT("T.c6"), 16.70, 17.05, 2.50, 4.20,
                       {TEXT("T1"), TEXT("T2"), TEXT("T3")}, 0.0, true));
        // Keep the deck as its own rigid body, with a real 5 cm bearing overlap
        // over each post. The previous 10 cm gaps required locked joints and made
        // the entire upper wall behave as one immovable assembly.
        Add(Scene, Box(TEXT("T.d3"), 14.25, 16.75, 4.20, 4.50, {}, 0.0, true));
        Add(Scene, Box(TEXT("T.post"), 15.35, 15.65, 4.50, 5.50, {}, 0.0, true));
        Add(Scene, Box(TEXT("T.cap"), 15.00, 16.00, 5.50, 5.80, {}, 0.0, true));
        Add(Scene, Goal(TEXT("T1"), 15.50, 0.78));
        Add(Scene, Goal(TEXT("T2"), 14.88, 2.98));
        Add(Scene, Goal(TEXT("T3"), 16.12, 2.98));
        Add(Scene, Goal(TEXT("P1"), 11.50, 4.08));
        return Scene;
    }

    FScene FSceneLibrary::Make22(const double HighPlatformApex)
    {
        FScene Scene;
        Scene.LevelId = TEXT("2-2");
        Scene.MaxX = 16.8;
        Add(Scene, Box(TEXT("B.base"), 8.95, 11.15, 0.00, 0.30));
        Add(Scene, Box(TEXT("B.c1"), 9.10, 9.40, 0.30, 1.10,
                       {TEXT("B1"), TEXT("B2")}, 0.0, true));
        Add(Scene, Box(TEXT("B.c2"), 10.70, 11.00, 0.30, 1.10,
                       {TEXT("B1"), TEXT("B2")}, 0.0, true));
        Add(Scene, Box(TEXT("B.deck"), 9.00, 11.10, 1.10, 1.40,
                       {TEXT("B1"), TEXT("B2")}, 0.0, true));
        Add(Scene, Box(TEXT("E.base"), 11.60, 14.20, 0.00, 0.30));
        Add(Scene, Box(TEXT("E.c1"), 11.75, 12.05, 0.30, 2.20, {}, 0.0, true));
        Add(Scene, Box(TEXT("E.c2"), 12.80, 13.10, 0.30, 2.20, {}, 0.0, true));
        Add(Scene, Box(TEXT("E.c3"), 13.75, 14.05, 0.30, 2.20, {}, 0.0, true));
        Add(Scene, Box(TEXT("E.d2"), 11.65, 14.15, 2.20, 2.50,
                       {TEXT("E1"), TEXT("E2")}, 0.0, true));
        Add(Scene, Box(TEXT("E.c4"), 11.80, 12.10, 2.50, 3.70, {}, 0.0, true));
        Add(Scene, Box(TEXT("E.c5"), 13.70, 14.00, 2.50, 3.70, {}, 0.0, true));
        Add(Scene, Box(TEXT("E.d3"), 11.95, 13.85, 3.70, 4.00,
                       {TEXT("E3"), TEXT("E4")}, 0.70, true));
        Add(Scene, Box(TEXT("A.col"), 15.00, 15.60, 0.00, HighPlatformApex - 0.30));
        Add(Scene, Box(TEXT("A.plate"), 14.40, 16.20, HighPlatformApex - 0.30,
                       HighPlatformApex));
        Add(Scene, Box(TEXT("A.rim"), 14.40, 14.62, HighPlatformApex,
                       HighPlatformApex + 1.55, {TEXT("A1")}, 0.0, true));
        Add(Scene, Goal(TEXT("C1"), 6.40, 0.62));
        Add(Scene, Goal(TEXT("C2"), 7.40, 0.62));
        Add(Scene, Goal(TEXT("C3"), 8.40, 0.62));
        Add(Scene, Goal(TEXT("B1"), 9.60, 1.875));
        Add(Scene, Goal(TEXT("B2"), 10.50, 1.875));
        Add(Scene, Goal(TEXT("E1"), 12.40, 2.98));
        Add(Scene, Goal(TEXT("E2"), 13.45, 2.98));
        Add(Scene, Goal(TEXT("E3"), 12.40, 4.475));
        Add(Scene, Goal(TEXT("E4"), 13.45, 4.475));
        Add(Scene, Goal(TEXT("A1"), 15.30, HighPlatformApex + 0.475));
        return Scene;
    }

    FScene FSceneLibrary::Make23(const int32 Stage)
    {
        const int32 NormalizedStage = Stage == 2 ? 2 : 1;
        FScene Scene;
        Scene.LevelId = TEXT("2-3");
        Scene.MaxX = 21.6;
        if (NormalizedStage == 1)
        {
            Add(Scene, Box(TEXT("A.leg1"), 6.35, 6.60, 0.00, 1.40,
                           {TEXT("A1"), TEXT("A2"), TEXT("A3")}, 0.0, true));
            Add(Scene, Box(TEXT("A.leg2"), 8.20, 8.45, 0.00, 1.40,
                           {TEXT("A1"), TEXT("A2"), TEXT("A3")}, 0.0, true));
            Add(Scene, Box(TEXT("A.deck"), 6.20, 8.60, 1.40, 1.70,
                           {TEXT("A1"), TEXT("A2"), TEXT("A3")}, 0.0, true));
            Add(Scene, Goal(TEXT("A1"), 6.60, 2.175));
            Add(Scene, Goal(TEXT("A2"), 7.40, 2.175));
            Add(Scene, Goal(TEXT("A3"), 8.20, 2.175));
        }
        Add(Scene, Box(TEXT("T.base"), 11.00, 14.00, 0.00, 0.30));
        Add(Scene, Box(TEXT("T.c1"), 11.15, 11.45, 0.30, 2.20,
                       {TEXT("T1"), TEXT("T2"), TEXT("T3"), TEXT("T4")}, 0.0, true));
        Add(Scene, Box(TEXT("T.c2"), 12.35, 12.65, 0.30, 2.20,
                       {TEXT("T1"), TEXT("T2"), TEXT("T3"), TEXT("T4")}, 0.0, true));
        Add(Scene, Box(TEXT("T.c3"), 13.55, 13.85, 0.30, 2.20,
                       {TEXT("T1"), TEXT("T2"), TEXT("T3"), TEXT("T4")}, 0.0, true));
        Add(Scene, Box(TEXT("T.d2"), 11.05, 13.95, 2.20, 2.50,
                       {TEXT("T3"), TEXT("T4"), TEXT("T5")}, 0.70, true));
        Add(Scene, Box(TEXT("T.c4"), 11.20, 11.50, 2.50, 4.10,
                       {TEXT("T3"), TEXT("T4")}, 0.0, true));
        Add(Scene, Box(TEXT("T.c5"), 13.50, 13.80, 2.50, 4.10,
                       {TEXT("T3"), TEXT("T4")}, 0.0, true));
        Add(Scene, Box(TEXT("T.d3"), 11.35, 13.65, 4.10, 4.40, {}, 0.0, true));
        Add(Scene, Box(TEXT("T.cap"), 12.10, 12.90, 4.40, 4.95, {TEXT("T5")}, 0.0, true));
        Add(Scene, Box(TEXT("S.rope1"), 15.10, 15.26, 4.55, 7.10,
                       {TEXT("S1"), TEXT("S2")}, 0.0, true));
        Add(Scene, Box(TEXT("S.rope2"), 17.34, 17.50, 4.55, 7.10,
                       {TEXT("S1"), TEXT("S2")}, 0.0, true));
        Add(Scene, Box(TEXT("S.deck"), 14.95, 17.65, 4.25, 4.55, {}, 0.0, true));
        Add(Scene, Box(TEXT("R.wall"), 18.60, 19.20, 0.00, 2.00));
        // Keep the upper tower goals between column collision boxes; their earlier
        // centers overlapped T.c2, T.c3, and T.c5 once those pieces became rigid bodies.
        Add(Scene, Goal(TEXT("T1"), 11.95, 0.78));
        Add(Scene, Goal(TEXT("T2"), 13.05, 0.78));
        Add(Scene, Goal(TEXT("T3"), 11.95, 2.98));
        Add(Scene, Goal(TEXT("T4"), 13.20, 2.98));
        Add(Scene, Goal(TEXT("T5"), 12.50, 5.43));
        Add(Scene, Goal(TEXT("S1"), 15.70, 5.03));
        Add(Scene, Goal(TEXT("S2"), 16.90, 5.03));
        Add(Scene, Goal(TEXT("R1"), 19.90, 0.78));
        Add(Scene, Goal(TEXT("R2"), 20.40, 0.78));
        return Scene;
    }

    FScene FSceneLibrary::Make(const FString& LevelId, const int32 Stage)
    {
        if (LevelId == TEXT("2-1"))
        {
            return Make21();
        }
        if (LevelId == TEXT("2-2"))
        {
            return Make22();
        }
        if (LevelId == TEXT("2-3"))
        {
            return Make23(Stage);
        }
        return FScene();
    }

    FAmmoRule FSceneLibrary::GetAmmoRule(const FString& LevelId)
    {
        FAmmoRule Rule;
        if (LevelId == TEXT("2-1"))
        {
            Rule.EggList.Add(EEggKind::Base);
            Rule.TotalShots = 3;
            Rule.bRepeatAllowed = true;
            Rule.bHasFreeRetry = true;
            return Rule;
        }

        Rule.EggList = {EEggKind::Base, EEggKind::Chick, EEggKind::Mouse,
                        EEggKind::Soy, EEggKind::Tea};
        Rule.TotalShots = LevelId == TEXT("2-2") ? 5 : 4;
        Rule.bRepeatAllowed = false;
        Rule.bHasFreeRetry = LevelId == TEXT("2-2");
        return Rule;
    }
}
