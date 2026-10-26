#include "NaiwaP2Ballistics.h"

#include <cmath>

namespace NaiwaP2
{
    FShotState FBallistics::MakeLaunchState(const double AngleDegrees, const double Power,
                                            const FEggSpec& Egg)
    {
        constexpr double Pi = 3.14159265358979323846264338327950288;
        const double AngleRadians = AngleDegrees * Pi / 180.0;
        const double Speed = LaunchSpeedMPerS * Power * Egg.SpeedMultiplier;

        FShotState Result;
        Result.X = 0.0;
        Result.Z = PouchHeightM;
        Result.VelocityX = Speed * std::cos(AngleRadians);
        Result.VelocityZ = Speed * std::sin(AngleRadians) + Egg.InitialVerticalVelocityMPerS;
        Result.Radius = BaseProjectileRadiusM * Egg.RadiusMultiplier;
        return Result;
    }

    FTrajectoryResult FBallistics::TraceEmpty(const FShotState& InitialState,
                                              const double StepSeconds,
                                              const double Gravity,
                                              const double GroundCenterZ,
                                              const int32 MaximumSteps)
    {
        FTrajectoryResult Result;
        if (!(StepSeconds > 0.0) || !(Gravity >= 0.0) || MaximumSteps < 1 ||
            !FMath::IsFinite(InitialState.X) || !FMath::IsFinite(InitialState.Z) ||
            !FMath::IsFinite(InitialState.VelocityX) || !FMath::IsFinite(InitialState.VelocityZ))
        {
            Result.FailureReason = TEXT("invalid_input");
            return Result;
        }

        FTrajectoryState State;
        State.Tick = 0;
        State.X = InitialState.X;
        State.Z = InitialState.Z;
        State.VelocityX = InitialState.VelocityX;
        State.VelocityZ = InitialState.VelocityZ;
        Result.States.Add(State);

        for (int32 Step = 0; Step < MaximumSteps; ++Step)
        {
            if (State.Z <= GroundCenterZ)
            {
                Result.bCompleted = true;
                return Result;
            }

            const double OldVelocityZ = State.VelocityZ;
            State.X += State.VelocityX * StepSeconds;
            State.Z += OldVelocityZ * StepSeconds;
            State.VelocityZ = OldVelocityZ - Gravity * StepSeconds;
            State.Tick = Step + 1;
            Result.States.Add(State);
        }

        Result.FailureReason = TEXT("maximum_steps_exceeded");
        return Result;
    }

    FShotResult FBallistics::Shoot(const FScene& Scene, const double AngleDegrees, const double Power,
                                   const FEggSpec& Egg, const int32 MaximumSteps,
                                   const int32 ClickTick)
    {
        FShotResult Result;
        if (Scene.MaxX <= 0.0 || !(Power > 0.0) || MaximumSteps < 1)
        {
            Result.FailureReason = TEXT("invalid_input");
            return Result;
        }

        const FShotState Launch = MakeLaunchState(AngleDegrees, Power, Egg);
        FTrajectoryState State;
        State.X = Launch.X;
        State.Z = Launch.Z;
        State.VelocityX = Launch.VelocityX;
        State.VelocityZ = Launch.VelocityZ;
        Result.States.Add(State);

        TSet<FString> HitIds;
        TSet<FString> KilledIds;
        double RollFromX = 0.0;
        bool bHasRollOrigin = false;
        bool bTeaBlastApplied = false;
        bool bMouseSplit = false;
        const bool bClickSkill = ClickTick > 0 &&
            (Egg.Kind == EEggKind::Chick || Egg.Kind == EEggKind::Mouse);

        const auto RecordHit = [&Result, &KilledIds](const FBox2& Box, const FTrajectoryState& HitState)
        {
            FShotHit& Hit = Result.Hits.AddDefaulted_GetRef();
            Hit.BoxId = Box.Id;
            Hit.X = HitState.X;
            Hit.Z = HitState.Z;
            Hit.Tick = HitState.Tick;
            Hit.bGoal = Box.bGoal;
            for (const FString& GoalId : Box.Kills)
            {
                KilledIds.Add(GoalId);
            }
        };

        const auto ApplyTeaBlast = [&Scene, &KilledIds](const FTrajectoryState& HitState)
        {
            for (const FBox2& Box : Scene.Boxes)
            {
                if (!Box.bGoal)
                {
                    continue;
                }
                const double GoalX = (Box.MinX + Box.MaxX) * 0.5;
                const double GoalZ = (Box.MinZ + Box.MaxZ) * 0.5;
                if (FMath::Square(GoalX - HitState.X) + FMath::Square(GoalZ - HitState.Z) <=
                    FMath::Square(TeaBlastRadiusM))
                {
                    KilledIds.Add(Box.Id);
                }
            }
        };

        const auto TraceSplitPath = [&Scene, &Result, &KilledIds, &RecordHit,
                                     MaximumSteps](const FTrajectoryState& StartState,
                                                  const double Radius,
                                                  FProjectilePath& Path)
        {
            Path.StartTick = StartState.Tick;
            Path.States.Add(StartState);
            FTrajectoryState ChildState = StartState;
            TSet<FString> ChildHitIds;
            double ChildRollFromX = 0.0;
            bool bChildHasRollOrigin = false;

            for (int32 ChildStep = 0; ChildStep < MaximumSteps; ++ChildStep)
            {
                const double OldVelocityZ = ChildState.VelocityZ;
                ChildState.X += ChildState.VelocityX * FixedStepSeconds;
                ChildState.Z += OldVelocityZ * FixedStepSeconds;
                ChildState.VelocityZ = OldVelocityZ - GravityMPerS2 * FixedStepSeconds;
                ++ChildState.Tick;

                bool bStopChild = ChildState.Z <= GroundCenterZM || ChildState.X >= Scene.MaxX;
                if (!bStopChild)
                {
                    const FBox2* BestBox = nullptr;
                    for (const FBox2& Box : Scene.Boxes)
                    {
                        if (Box.RequiredRadius > Radius + 1.0e-9 ||
                            !Box.ContainsDiscCenter(ChildState.X, ChildState.Z, Radius))
                        {
                            continue;
                        }
                        if (!BestBox || Box.MinX < BestBox->MinX)
                        {
                            BestBox = &Box;
                        }
                    }

                    if (BestBox && !ChildHitIds.Contains(BestBox->Id))
                    {
                        ChildHitIds.Add(BestBox->Id);
                        RecordHit(*BestBox, ChildState);
                        if (!BestBox->bGoal)
                        {
                            Result.bStoppedByStructure = true;
                            bStopChild = true;
                        }
                        else if (!bChildHasRollOrigin)
                        {
                            ChildRollFromX = ChildState.X;
                            bChildHasRollOrigin = true;
                        }
                    }
                    if (bChildHasRollOrigin && ChildState.X - ChildRollFromX > GoalRollDistanceM)
                    {
                        bStopChild = true;
                    }
                }

                Path.States.Add(ChildState);
                if (bStopChild)
                {
                    return true;
                }
            }
            return false;
        };

        for (int32 Step = 0; Step < MaximumSteps; ++Step)
        {
            if (State.Z <= GroundCenterZM || State.X >= Scene.MaxX)
            {
                Result.bCompleted = true;
                break;
            }

            const FBox2* BestBox = nullptr;
            for (const FBox2& Box : Scene.Boxes)
            {
                if (Box.RequiredRadius > Launch.Radius + 1.0e-9 ||
                    !Box.ContainsDiscCenter(State.X, State.Z, Launch.Radius))
                {
                    continue;
                }
                if (!BestBox || Box.MinX < BestBox->MinX)
                {
                    BestBox = &Box;
                }
            }

            if (BestBox && !HitIds.Contains(BestBox->Id))
            {
                HitIds.Add(BestBox->Id);
                RecordHit(*BestBox, State);
                if (Egg.Kind == EEggKind::Tea && !bTeaBlastApplied)
                {
                    ApplyTeaBlast(State);
                    bTeaBlastApplied = true;
                }

                if (!BestBox->bGoal)
                {
                    Result.bStoppedByStructure = true;
                    Result.bCompleted = true;
                    break;
                }
                if (!bHasRollOrigin)
                {
                    RollFromX = State.X;
                    bHasRollOrigin = true;
                }
            }

            if (bHasRollOrigin && State.X - RollFromX > GoalRollDistanceM)
            {
                Result.bCompleted = true;
                break;
            }

            // The fixed-step boundary checks collision/landing first, then
            // applies an accepted click before integrating that step.
            if (bClickSkill && State.Tick == ClickTick)
            {
                Result.bSkillTriggered = true;
                Result.SkillTick = State.Tick;
                if (Egg.Kind == EEggKind::Chick)
                {
                    State.VelocityZ += ChickVerticalImpulseMPerS;
                    Result.States.Last() = State;
                }
                else
                {
                    const double Speed = FMath::Sqrt(FMath::Square(State.VelocityX) +
                        FMath::Square(State.VelocityZ)) * MouseSplitSpeedMultiplier;
                    const double Angle = FMath::Atan2(State.VelocityZ, State.VelocityX);
                    constexpr double DegreesToRadians = 0.0174532925199432957692;
                    bool bAllPathsCompleted = true;
                    for (const double OffsetDegrees : {-MouseSplitHalfAngleDegrees, 0.0, MouseSplitHalfAngleDegrees})
                    {
                        const double ChildAngle = Angle + OffsetDegrees * DegreesToRadians;
                        FTrajectoryState ChildStart = State;
                        ChildStart.VelocityX = Speed * FMath::Cos(ChildAngle);
                        ChildStart.VelocityZ = Speed * FMath::Sin(ChildAngle);
                        FProjectilePath& Path = Result.SplitPaths.AddDefaulted_GetRef();
                        bAllPathsCompleted = TraceSplitPath(
                            ChildStart, Launch.Radius * MouseSplitRadiusMultiplier, Path) && bAllPathsCompleted;
                    }
                    bMouseSplit = true;
                    Result.bCompleted = bAllPathsCompleted;
                    Result.TotalTicks = State.Tick;
                    for (const FProjectilePath& Path : Result.SplitPaths)
                    {
                        if (!Path.States.IsEmpty())
                        {
                            Result.TotalTicks = FMath::Max(Result.TotalTicks, Path.States.Last().Tick);
                        }
                    }
                    break;
                }
            }

            const double OldVelocityZ = State.VelocityZ;
            State.X += State.VelocityX * FixedStepSeconds;
            State.Z += OldVelocityZ * FixedStepSeconds;
            State.VelocityZ = OldVelocityZ - GravityMPerS2 * FixedStepSeconds;
            ++State.Tick;
            Result.States.Add(State);
        }

        if (!bMouseSplit && !Result.bCompleted)
        {
            Result.FailureReason = TEXT("maximum_steps_exceeded");
            return Result;
        }
        if (!bMouseSplit)
        {
            Result.TotalTicks = Result.States.IsEmpty() ? 0 : Result.States.Last().Tick;
        }

        Result.RawKilled = KilledIds.Array();
        Result.RawKilled.Sort();
        return Result;
    }
}
