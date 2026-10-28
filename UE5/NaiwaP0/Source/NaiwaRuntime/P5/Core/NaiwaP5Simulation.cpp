#include "NaiwaP5Simulation.h"

int32 FNaiwaP5ScoreLedger::GetTotalScore() const
{
	const int32 SafeDistance = FMath::Clamp(DistancePoints, 0, NaiwaP5Rules::MaxDistancePoints);
	const int32 SafeCoins = FMath::Clamp(CoinPoints, 0,
		NaiwaP5Rules::MaxCoinCount * NaiwaP5Rules::PointsPerCoin);
	const int32 SafeTricks = FMath::Clamp(TrickAndComboPoints, 0, NaiwaP5Rules::MaxTrickAndComboPoints);
	const int32 SafeRide = FMath::Clamp(RidePoints, 0, NaiwaP5Rules::MaxRidePoints);
	const int32 SafeNoFall = FMath::Clamp(NoFallPoints, 0, NaiwaP5Rules::NoFallBonusPoints);
	return FMath::Clamp(SafeDistance + SafeCoins + SafeTricks + SafeRide + SafeNoFall, 0,
		NaiwaP5Rules::MaxRunScore);
}

int32 FNaiwaP5ScoreLedger::GetTicketsEarned() const
{
	return GetTotalScore() / NaiwaP5Rules::PointsPerTicket;
}

int8 FNaiwaP5Simulation::ResolveRotationAxis(const bool bRotateLeft, const bool bRotateRight)
{
	if (bRotateLeft == bRotateRight)
	{
		return 0;
	}
	return bRotateLeft ? -1 : 1;
}

double FNaiwaP5Simulation::GetSegmentSpeedMetersPerSecond(const FNaiwaP5BaselineFixture& Fixture,
	const int32 DistanceMm)
{
	if (Fixture.SpeedSegments.IsEmpty() || Fixture.RouteLengthMm <= 0)
	{
		return 0.0;
	}
	const int32 SafeDistanceMm = FMath::Clamp(DistanceMm, 0, Fixture.RouteLengthMm - 1);
	for (const FNaiwaP5TrackSpeedSegment& Segment : Fixture.SpeedSegments)
	{
		if (SafeDistanceMm >= Segment.StartDistanceMm && SafeDistanceMm < Segment.EndDistanceMm)
		{
			return static_cast<double>(Segment.BaseSpeedMmPerSecond) / 1000.0;
		}
	}
	return static_cast<double>(Fixture.SpeedSegments.Last().BaseSpeedMmPerSecond) / 1000.0;
}

double FNaiwaP5Simulation::GetJumpTimeToLandingSeconds(const double HeightMeters,
	const double VerticalVelocityMetersPerSecond)
{
	const double SafeHeight = FMath::Max(0.0, HeightMeters);
	const double Gravity = NaiwaP5Rules::JumpGravityMetersPerSecondSquared;
	const double Discriminant = FMath::Max(0.0,
		VerticalVelocityMetersPerSecond * VerticalVelocityMetersPerSecond + 2.0 * Gravity * SafeHeight);
	return FMath::Max(0.0, (VerticalVelocityMetersPerSecond + FMath::Sqrt(Discriminant)) / Gravity);
}

double FNaiwaP5Simulation::AdvanceRotationDegrees(const double CurrentDegrees, const int8 Axis,
	const double DeltaSeconds)
{
	const double SafeDelta = FMath::Max(0.0, DeltaSeconds);
	const double SafeAxis = FMath::Clamp(static_cast<double>(Axis), -1.0, 1.0);
	return FMath::Clamp(CurrentDegrees + SafeAxis * NaiwaP5Rules::RotationSpeedDegreesPerSecond * SafeDelta,
		-NaiwaP5Rules::MaxRotationDegrees, NaiwaP5Rules::MaxRotationDegrees);
}

double FNaiwaP5Simulation::WrapAngleDegrees(const double Degrees)
{
	double Wrapped = FMath::Fmod(Degrees + 180.0, 360.0);
	if (Wrapped < 0.0)
	{
		Wrapped += 360.0;
	}
	return Wrapped - 180.0;
}

ENaiwaP5LandingGrade FNaiwaP5Simulation::ClassifyLanding(const double RotationDegrees)
{
	const double AbsoluteAngle = FMath::Abs(WrapAngleDegrees(RotationDegrees));
	if (AbsoluteAngle <= NaiwaP5Rules::PerfectLandingToleranceDegrees)
	{
		return ENaiwaP5LandingGrade::Perfect;
	}
	if (AbsoluteAngle <= NaiwaP5Rules::GoodLandingToleranceDegrees)
	{
		return ENaiwaP5LandingGrade::Good;
	}
	return ENaiwaP5LandingGrade::Fall;
}

double FNaiwaP5Simulation::AdvanceAvalancheGapMeters(const double CurrentGapMeters,
	const double PlayerSpeedMetersPerSecond, const double DeltaSeconds, const double HitPenaltyMeters,
	const double SkillPushbackMeters, bool& bOutSwallowed)
{
	const double SafeDelta = FMath::Max(0.0, DeltaSeconds);
	const double RawGap = CurrentGapMeters +
		(PlayerSpeedMetersPerSecond - NaiwaP5Rules::AvalancheSpeedMetersPerSecond) * SafeDelta -
		FMath::Max(0.0, HitPenaltyMeters) + FMath::Max(0.0, SkillPushbackMeters);
	bOutSwallowed = CurrentGapMeters <= 0.0 || RawGap <= 0.0;
	return FMath::Clamp(RawGap, 0.0, NaiwaP5Rules::AvalancheGapMaxMeters);
}

double FNaiwaP5Simulation::GetFallSpeedMetersPerSecond(const double TerrainSpeedMetersPerSecond,
	const double FallElapsedSeconds)
{
	const double SafeBaseSpeed = FMath::Max(0.0, TerrainSpeedMetersPerSecond);
	if (FallElapsedSeconds < NaiwaP5Rules::FallHardStunSeconds)
	{
		return SafeBaseSpeed * NaiwaP5Rules::FallSpeedFraction;
	}

	const double RecoveryAlpha = FMath::Clamp((FallElapsedSeconds - NaiwaP5Rules::FallHardStunSeconds) /
		NaiwaP5Rules::FallRecoverySeconds, 0.0, 1.0);
	return SafeBaseSpeed * FMath::Lerp(NaiwaP5Rules::FallSpeedFraction,
		NaiwaP5Rules::FallRecoveredSpeedFraction, RecoveryAlpha);
}

bool FNaiwaP5Simulation::IsFallInvulnerable(const double FallElapsedSeconds)
{
	const double RecoveryEnd = NaiwaP5Rules::FallHardStunSeconds + NaiwaP5Rules::FallRecoverySeconds;
	return FallElapsedSeconds >= RecoveryEnd &&
		FallElapsedSeconds < RecoveryEnd + NaiwaP5Rules::FallInvulnerabilitySeconds;
}

double FNaiwaP5Simulation::AdvanceComboMultiplier(const double CurrentMultiplier,
	const double SecondsSinceLastAction, const bool bHasLastAction)
{
	if (!bHasLastAction || SecondsSinceLastAction < 0.0 ||
		SecondsSinceLastAction > NaiwaP5Rules::ComboWindowSeconds)
	{
		return 1.0;
	}
	return FMath::Min(FMath::Max(1.0, CurrentMultiplier) + NaiwaP5Rules::ComboMultiplierStep,
		NaiwaP5Rules::ComboMultiplierMax);
}

int32 FNaiwaP5Simulation::ScoreCompletedFlips(const int32 CompletedFlips, const double ComboMultiplier)
{
	const int32 SafeFlips = FMath::Max(0, CompletedFlips);
	const double SafeMultiplier = FMath::Clamp(ComboMultiplier, 1.0, NaiwaP5Rules::ComboMultiplierMax);
	return FMath::FloorToInt(static_cast<double>(SafeFlips) * NaiwaP5Rules::PointsPerCompletedFlip * SafeMultiplier);
}
