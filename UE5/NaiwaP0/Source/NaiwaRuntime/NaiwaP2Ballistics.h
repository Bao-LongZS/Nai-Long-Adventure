#pragma once

#include "NaiwaP2Types.h"

namespace NaiwaP2
{
    class FBallistics
    {
    public:
        static FShotState MakeLaunchState(double AngleDegrees, double Power,
                                          const FEggSpec& Egg = FEggSpec());

        // M1a reference path. Each state is recorded at a fixed-step boundary;
        // position uses the old velocity and gravity updates after position.
        static FTrajectoryResult TraceEmpty(const FShotState& InitialState,
                                            double StepSeconds = FixedStepSeconds,
                                            double Gravity = GravityMPerS2,
                                            double GroundCenterZ = GroundCenterZM,
                                            int32 MaximumSteps = 10000);

        static FShotResult Shoot(const FScene& Scene, double AngleDegrees, double Power,
                                 const FEggSpec& Egg = GetEggSpec(EEggKind::Base),
                                 int32 MaximumSteps = 10000,
                                 int32 ClickTick = INDEX_NONE);
    };
}
