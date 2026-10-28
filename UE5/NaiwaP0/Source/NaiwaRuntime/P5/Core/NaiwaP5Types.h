#pragma once

#include "CoreMinimal.h"

enum class ENaiwaP5LandingGrade : uint8
{
	Perfect,
	Good,
	Fall
};

enum class ENaiwaP5RunPhase : uint8
{
	NotStarted,
	Running,
	Paused,
	Finished,
	Swallowed
};

struct FNaiwaP5InputCommand
{
	bool bJumpHeld = false;
	bool bJumpPressed = false;
	bool bJumpReleased = false;
	bool bRotateLeft = false;
	bool bRotateRight = false;
};

struct FNaiwaP5TrackSpeedSegment
{
	int32 StartDistanceMm = 0;
	int32 EndDistanceMm = 0;
	int32 BaseSpeedMmPerSecond = 0;
};

enum class ENaiwaP5RouteEventKind : uint8
{
	Coin,
	Tree,
	Rock,
	IceGap,
	Kangaroo,
	CliffLaunch,
	CliffGap
};

struct FNaiwaP5RouteEvent
{
	int32 Id = INDEX_NONE;
	int32 DistanceMm = 0;
	ENaiwaP5RouteEventKind Kind = ENaiwaP5RouteEventKind::Coin;
	int32 RequiredClearanceMm = 0;
	int32 HitGapPenaltyMm = 0;
	int32 WidthMm = 0;
};

enum class ENaiwaP5SurfaceKind : uint8
{
	Snow,
	Ice,
	JumpRamp
};

struct FNaiwaP5RouteSegment
{
	int32 StartDistanceMm = 0;
	int32 EndDistanceMm = 0;
	int32 BaseSpeedMmPerSecond = 0;
	int32 StartHeightMm = 0;
	int32 EndHeightMm = 0;
	ENaiwaP5SurfaceKind Surface = ENaiwaP5SurfaceKind::Snow;
};

struct FNaiwaP5RouteFixture
{
	FName RouteId = NAME_None;
	int32 Version = 1;
	int32 RouteLengthMm = 0;
	TArray<FNaiwaP5RouteSegment> Segments;
	TArray<FNaiwaP5RouteEvent> Events;
	uint32 FixtureHash = 0;
};

struct FNaiwaP5AbilityEvent
{
	FName AbilityId = NAME_None;
	FName TriggerId = NAME_None;
	int64 Tick = 0;
	int32 DistanceMm = 0;
	int32 Value = 0;
};

/** Numeric route fixture used by M0. Geometry is authored separately. */
struct FNaiwaP5BaselineFixture
{
	FName RouteId = NAME_None;
	int32 Version = 1;
	int32 RouteLengthMm = 0;
	TArray<FNaiwaP5TrackSpeedSegment> SpeedSegments;
	uint32 FixtureHash = 0;
};

struct FNaiwaP5SkinRow
{
	FName SkinId = NAME_None;
	FName AbilityId = NAME_None;
	int32 TicketPrice = 0;
	float LandingToleranceDeltaDegrees = 0.0f;
	float LandingSpeedMultiplier = 1.0f;
	float LandingSpeedDurationSeconds = 0.0f;
	float ObstacleWarningDistanceMeters = 0.0f;
	float AvalanchePushbackMeters = 0.0f;
	float AvalanchePushbackCooldownSeconds = 0.0f;
	bool bIgnoresTreeHitPenalty = false;
	bool bIgnoresTreeSlowdown = false;
	FString DisplayName;
	FString AbilityDescription;
};

/** Additive score buckets. The category ceilings mirror the M0 score audit. */
struct FNaiwaP5ScoreLedger
{
	int32 DistancePoints = 0;
	int32 CoinPoints = 0;
	int32 TrickAndComboPoints = 0;
	int32 RidePoints = 0;
	int32 NoFallPoints = 0;

	int32 GetTotalScore() const;
	int32 GetTicketsEarned() const;
};

struct FNaiwaP5RunState
{
	ENaiwaP5RunPhase Phase = ENaiwaP5RunPhase::NotStarted;
	int64 CurrentTick = 0;
	uint32 MasterSeed = 0;
	double ElapsedSeconds = 0.0;
	double DistanceMeters = 0.0;
	double PlayerSpeedMetersPerSecond = 0.0;
	double AvalancheGapMeters = 60.0;
	double JumpChargeSeconds = 0.0;
	double JumpElapsedSeconds = 0.0;
	double JumpHeightMeters = 0.0;
	double JumpVerticalVelocityMetersPerSecond = 0.0;
	double CoyoteElapsedSeconds = 0.0;
	double RotationDegrees = 0.0;
	double FallElapsedSeconds = 0.0;
	double ComboMultiplier = 1.0;
	double LastSuccessfulActionSeconds = -1.0;
	FName EquippedSkinId = FName(TEXT("NaiwaSkin_Base"));
	FNaiwaP5ScoreLedger Score;
	TArray<int32> CollectedCoinIds;
	TArray<int32> ResolvedObstacleIds;
	TArray<FNaiwaP5AbilityEvent> AbilityEvents;
	double LandingSpeedBoostSeconds = 0.0;
	double TrickBoostSeconds = 0.0;
	double AvalanchePushbackCooldownSeconds = 0.0;
	double RideElapsedSeconds = 0.0;
	double RideStartDistanceMeters = 0.0;
	double RideDeliveryElapsedSeconds = 0.0;
	double DismountElapsedSeconds = 0.0;
	double ObstacleWarningDistanceMeters = 0.0;
	int32 ObstacleWarningEventId = INDEX_NONE;
	int32 CompletedFlips = 0;
	int32 TrickBoostLevel = 0;
	int32 DeferredCatchUpSteps = 0;
	int32 Falls = 0;
	bool bM2PrototypeRoute = false;
	bool bRiding = false;
	bool bRidePouchAvailable = false;
	bool bRidePouchUsedThisRun = false;
	bool bRideDeliveryCanceled = false;
	bool bRideDeliveryAwarded = false;
	bool bRideSpeedBoostActive = false;
	bool bDismounted = false;
	bool bObstacleWarning = false;
	bool bSettlementSaved = false;
	bool bJumpCharging = false;
	bool bJumpBuffered = false;
	bool bAirborne = false;
	bool bOnSnow = true;
	bool bFallen = false;
	bool bFallInvulnerable = false;
	bool bEverFell = false;
	bool bAvalancheWarning = false;
	bool bWasSwallowed = false;
};
