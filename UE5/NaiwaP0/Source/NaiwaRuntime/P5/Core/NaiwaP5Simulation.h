#pragma once

#include "NaiwaP5Types.h"

namespace NaiwaP5Rules
{
	inline constexpr double FixedStepSeconds = 1.0 / 60.0;
	inline constexpr int32 MaxCatchUpSteps = 4;
	inline constexpr double MaxPlayerSpeedMetersPerSecond = 26.0;
	inline constexpr double TrickBoostTier1MaxSpeedMetersPerSecond = 32.0;
	inline constexpr double TrickBoostTier2MaxSpeedMetersPerSecond = 38.0;
	inline constexpr double AvalancheSpeedMetersPerSecond = 12.0;
	inline constexpr double AvalancheGapMaxMeters = 60.0;
	inline constexpr double AvalancheWarningGapMeters = 20.0;

	inline constexpr double JumpLaunchVelocityMetersPerSecond = 7.4;
	inline constexpr double JumpHoldAccelerationMetersPerSecondSquared = 20.0;
	inline constexpr double JumpGravityMetersPerSecondSquared = 16.0;
	inline constexpr double JumpMaxHoldSeconds = 0.30;
	inline constexpr double RideJumpVelocityMultiplier = 1.225;
	inline constexpr double CliffLaunchVelocityMetersPerSecond = 13.0;
	inline constexpr double JumpBufferSeconds = 0.18;
	inline constexpr double CoyoteTimeSeconds = 0.16;

	inline constexpr double RotationSpeedDegreesPerSecond = 760.0;
	inline constexpr double MaxRotationDegrees = 720.0;
	inline constexpr double PerfectLandingToleranceDegrees = 15.0;
	inline constexpr double GoodLandingToleranceDegrees = 45.0;

	inline constexpr double FallHardStunSeconds = 1.5;
	inline constexpr double FallRecoverySeconds = 1.0;
	inline constexpr double FallSpeedFraction = 0.45;
	inline constexpr double FallRecoveredSpeedFraction = 0.85;
	inline constexpr double FallInvulnerabilitySeconds = 0.5;

	inline constexpr double HitGapPenaltyMeters = 3.0;
	inline constexpr double FallGapPenaltyMeters = 8.0;
	inline constexpr double NahaoPushbackMeters = 6.0;
	inline constexpr double NahaoCooldownSeconds = 8.0;

	inline constexpr double ComboWindowSeconds = 0.6;
	inline constexpr double ComboMultiplierStep = 0.1;
	inline constexpr double ComboMultiplierMax = 3.0;
	inline constexpr int32 PointsPerCompletedFlip = 150;
	inline constexpr int32 PointsPerCoin = 50;
	inline constexpr int32 MaxCoinCount = 36;
	inline constexpr int32 NoFallBonusPoints = 1500;
	inline constexpr int32 MaxDistancePoints = 1800;
	inline constexpr int32 MaxTrickAndComboPoints = 3000;
	inline constexpr int32 MaxRidePoints = 900;
	inline constexpr int32 MaxRunScore = 9000;
	inline constexpr int32 PointsPerTicket = 50;
	inline constexpr double TrickBoostTier1Multiplier = 1.15;
	inline constexpr double TrickBoostTier2Multiplier = 1.30;
	inline constexpr double TrickBoostTier1DurationSeconds = 4.0;
	inline constexpr double TrickBoostTier2DurationSeconds = 6.0;
}

class FNaiwaP5Simulation
{
public:
	static int8 ResolveRotationAxis(bool bRotateLeft, bool bRotateRight);
	static double GetSegmentSpeedMetersPerSecond(const FNaiwaP5BaselineFixture& Fixture, int32 DistanceMm);
	static double GetJumpTimeToLandingSeconds(double HeightMeters, double VerticalVelocityMetersPerSecond);
	static double AdvanceRotationDegrees(double CurrentDegrees, int8 Axis, double DeltaSeconds);
	static double WrapAngleDegrees(double Degrees);
	static ENaiwaP5LandingGrade ClassifyLanding(double RotationDegrees);
	static double AdvanceAvalancheGapMeters(double CurrentGapMeters, double PlayerSpeedMetersPerSecond,
		double DeltaSeconds, double HitPenaltyMeters, double SkillPushbackMeters, bool& bOutSwallowed);
	static double GetFallSpeedMetersPerSecond(double TerrainSpeedMetersPerSecond, double FallElapsedSeconds);
	static bool IsFallInvulnerable(double FallElapsedSeconds);
	static double AdvanceComboMultiplier(double CurrentMultiplier, double SecondsSinceLastAction, bool bHasLastAction);
	static int32 ScoreCompletedFlips(int32 CompletedFlips, double ComboMultiplier);
};
