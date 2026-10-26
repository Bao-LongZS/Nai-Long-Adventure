#pragma once

#include "CoreMinimal.h"

namespace NaiwaP2
{
    constexpr double GravityMPerS2 = 15.68;
    constexpr double LaunchSpeedMPerS = 27.0;
    constexpr double PouchHeightM = 1.80;
    constexpr double FixedStepSeconds = 0.004;
    constexpr double BaseProjectileRadiusM = 0.44;
    constexpr double GroundCenterZM = -0.60;
    constexpr double GoalRollDistanceM = 0.55;
    constexpr double TeaBlastRadiusM = 1.50;
    constexpr double ChickVerticalImpulseMPerS = 7.5;
    constexpr double MouseSplitHalfAngleDegrees = 13.0;
    constexpr double MouseSplitSpeedMultiplier = 0.70;
    constexpr double MouseSplitRadiusMultiplier = 0.72;

    enum class EEggKind : uint8
    {
        Base,
        Chick,
        Mouse,
        Soy,
        Tea
    };

    struct FEggSpec
    {
        EEggKind Kind = EEggKind::Base;
        double SpeedMultiplier = 1.0;
        double RadiusMultiplier = 1.0;
        double InitialVerticalVelocityMPerS = 0.0;
    };

    struct FShotState
    {
        double X = 0.0;
        double Z = PouchHeightM;
        double VelocityX = 0.0;
        double VelocityZ = 0.0;
        double Radius = BaseProjectileRadiusM;
    };

    struct FTrajectoryState
    {
        int32 Tick = 0;
        double X = 0.0;
        double Z = PouchHeightM;
        double VelocityX = 0.0;
        double VelocityZ = 0.0;
    };

    struct FTrajectoryResult
    {
        TArray<FTrajectoryState> States;
        bool bCompleted = false;
        FString FailureReason;

        const FTrajectoryState* GetFinalState() const
        {
            return States.IsEmpty() ? nullptr : &States.Last();
        }
    };

    struct FShotHit
    {
        FString BoxId;
        double X = 0.0;
        double Z = 0.0;
        int32 Tick = 0;
        bool bGoal = false;
    };

    struct FProjectilePath
    {
        int32 StartTick = 0;
        TArray<FTrajectoryState> States;
    };

    struct FShotResult
    {
        TArray<FTrajectoryState> States;
        TArray<FProjectilePath> SplitPaths;
        TArray<FShotHit> Hits;
        TArray<FString> RawKilled;
        int32 TotalTicks = 0;
        int32 SkillTick = INDEX_NONE;
        bool bSkillTriggered = false;
        bool bCompleted = false;
        bool bStoppedByStructure = false;
        FString FailureReason;
    };

    struct FBox2
    {
        FString Id;
        double MinX = 0.0;
        double MaxX = 0.0;
        double MinZ = 0.0;
        double MaxZ = 0.0;
        double RequiredRadius = 0.0;
        bool bGoal = false;
        bool bBreakable = false;
        TArray<FString> Kills;

        bool ContainsDiscCenter(const double X, const double Z, const double Radius) const
        {
            return MinX - Radius <= X && X <= MaxX + Radius &&
                   MinZ - Radius <= Z && Z <= MaxZ + Radius;
        }
    };

    struct FScene
    {
        FString LevelId;
        double MaxX = 0.0;
        TArray<FBox2> Boxes;
        TArray<FString> GoalIds;
    };

    struct FAmmoRule
    {
        TArray<EEggKind> EggList;
        int32 TotalShots = 0;
        bool bRepeatAllowed = false;
        bool bHasFreeRetry = false;
    };

    inline FEggSpec GetEggSpec(const EEggKind Kind)
    {
        FEggSpec Result;
        Result.Kind = Kind;
        switch (Kind)
        {
        case EEggKind::Chick:
        case EEggKind::Mouse:
        case EEggKind::Tea:
            break;
        case EEggKind::Soy:
            Result.SpeedMultiplier = 0.84;
            Result.RadiusMultiplier = 1.90;
            break;
        case EEggKind::Base:
        default:
            break;
        }
        return Result;
    }

    inline const TCHAR* GetEggKindLabel(const EEggKind Kind)
    {
        switch (Kind)
        {
        case EEggKind::Chick: return TEXT("CHICK");
        case EEggKind::Mouse: return TEXT("MOUSE");
        case EEggKind::Soy: return TEXT("SOY");
        case EEggKind::Tea: return TEXT("TEA");
        case EEggKind::Base:
        default: return TEXT("BASE");
        }
    }
}
