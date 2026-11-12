#pragma once

#include "CoreMinimal.h"

/** Logical actor categories used by the deterministic arena simulation. */
enum class ENaiwaP4EntityKind : uint8
{
	Player,
	Enemy,
	Boss,
	Projectile
};

/** Art direction is deliberately limited to front/back/side; side art can be mirrored. */
enum class ENaiwaP4Facing : uint8
{
	Front,
	Back,
	Side
};

/** Presentation cue only. It must never feed back into collision or simulation state. */
enum class ENaiwaP4ActionCue : uint8
{
	Idle,
	Move,
	Attack,
	Dash,
	Defend,
	Hit,
	Laugh,
	Death
};

enum class ENaiwaP4RunResult : uint8
{
	InProgress,
	Victory,
	Defeat
};

enum class ENaiwaP4BossAttack : uint8
{
	Charge,
	LidSlam
};

struct FNaiwaP4InputCommand
{
	/** Held analog/digital movement for one fixed simulation tick. */
	FVector2D MoveAxis = FVector2D::ZeroVector;
	/** J melee press edge plus held state; the simulation owns its timing and damage. */
	bool bMeleePressed = false;
	bool bMeleeHeld = false;
	bool bBlockHeld = false;
	/** Latched action edges; consumed on exactly one fixed simulation tick. */
	bool bDashPressed = false;
	bool bUltimatePressed = false;
};

struct FNaiwaP4EntityState
{
	int32 EntityId = INDEX_NONE;
	ENaiwaP4EntityKind Kind = ENaiwaP4EntityKind::Enemy;
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Velocity = FVector2D::ZeroVector;
	float HeightZ = 0.0f;
	int32 HP = 1;
	int32 MaxHP = 1;
	float CollisionRadius = 32.0f;
	ENaiwaP4Facing Facing = ENaiwaP4Facing::Front;
	bool bFacingRight = true;
	ENaiwaP4ActionCue ActionCue = ENaiwaP4ActionCue::Idle;
	int32 ActionTick = 0;
	bool bAlive = true;

	// Player combat state. Energy uses hundredths so hit-based gain is deterministic.
	int32 EnergyHundredths = 0;
	int32 AttackCooldownTicksRemaining = 0;
	int32 AttackCueTicksRemaining = 0;
	int32 MeleeHitTicksRemaining = 0;
	int32 MeleeContactTicksRemaining = 0;
	int32 MeleeInputBufferTicksRemaining = 0;
	int32 DashInputBufferTicksRemaining = 0;
	int32 UltimateInputBufferTicksRemaining = 0;
	int32 ProjectileAutoFireCooldownTicksRemaining = 0;
	int32 PlayerProjectileDamage = 0;
	int32 MeleeDamage = 20;
	int32 MeleeDamageBonusPerRank = 0;
	int32 InvulnerabilityTicksRemaining = 0;
	int32 HitCueTicksRemaining = 0;
	int32 LaughTicksRemaining = 0;
	int32 DashCharges = 0;
	int32 DashChargesMax = 0;
	int32 DashRechargeTicksRemaining = 0;
	int32 DashTicksRemaining = 0;
	int32 DashRecoveryTicksRemaining = 0;
	int32 DashRecoveryDurationTicks = 0;
	FVector2D DashDirection = FVector2D::ZeroVector;
	TArray<int32> DashHitEntityIds;
	int32 ProjectileBouncesPerShot = 0;
	float ProjectileBounceDamageScale = 0.4f;
	bool bGainEnergyWhenHit = false;
	int32 EnergyOnHitHundredths = 0;

	// Runtime projectile state; projectile entities share the deterministic entity list.
	int32 ProjectileOwnerEntityId = INDEX_NONE;
	int32 ProjectileDamage = 0;
	int32 ProjectileLifetimeTicksRemaining = 0;
	int32 ProjectileBouncesRemaining = 0;
	TArray<int32> ProjectileHitEntityIds;
	bool bEnemyProjectile = false;
	float ProjectileExplosionRadius = 0.0f;
	float ProjectileSlowMultiplier = 1.0f;
	int32 ProjectileSlowDurationTicks = 0;

	// W2 graybox and W3 table-driven enemy state share the deterministic chase/attack fields.
	bool bTrainingEnemy = false;
	/** Enemy spawned by the table-driven W3 wave director. */
	bool bWaveEnemy = false;
	bool bWaveBoss = false;
	FName EnemyTypeId;
	FName BehaviorId;
	int32 SpawnWaveIndex = 0;
	bool bFrontShield75 = false;
	FVector2D EnemySpawnPosition = FVector2D::ZeroVector;
	float EnemyMoveSpeed = 0.0f;
	float EnemyAttackRange = 0.0f;
	int32 EnemyDamage = 0;
	int32 EnemyAttackCooldownTicksRemaining = 0;
	int32 EnemyTelegraphTicksRemaining = 0;
	int32 EnemyAttackIntervalTicks = 0;
	int32 EnemyTelegraphDurationTicks = 0;
	float EnemyAbilityRadius = 0.0f;
	int32 EnemyAbilityDurationTicks = 0;
	float EnemyProjectileSpeed = 0.0f;
	float EnemySlowMultiplier = 1.0f;
	int32 EnemySlowDurationTicks = 0;
	FName EnemySummonId;
	int32 EnemySummonCount = 0;
	int32 EnemySpecialActionTicksRemaining = 0;
	int32 EnemySpecialActionDurationTicks = 0;
	FVector2D EnemySpecialStartPosition = FVector2D::ZeroVector;
	FVector2D EnemySpecialTargetPosition = FVector2D::ZeroVector;
	bool bEnemySpecialActionActive = false;
	ENaiwaP4BossAttack BossAttack = ENaiwaP4BossAttack::Charge;
	FVector2D BossLockedTargetPosition = FVector2D::ZeroVector;
	FVector2D BossChargeStartPosition = FVector2D::ZeroVector;
	FVector2D BossChargeEndPosition = FVector2D::ZeroVector;
	int32 BossChargeTicksRemaining = 0;
	int32 BossChargeDurationTicks = 0;
	bool bBossChargeHitPlayer = false;
	int32 BossPhase = 1;
	int32 BossPhaseTransitionTicksRemaining = 0;
	FName BossId;
	FName CurrentBossAttackId;
	int32 BossAttackSequenceIndex = 0;
	int32 BossAttackTicksRemaining = 0;
	int32 BossAttackHitTicksRemaining = 0;
	int32 BossAttackHitIndex = 0;
	int32 BossBreakWindowTicksRemaining = 0;
	int32 BossActionStunTicksRemaining = 0;
	int32 BossContactDamageTicksRemaining = 0;
	FVector2D BossAttackStartPosition = FVector2D::ZeroVector;
	FVector2D BossAttackDirection = FVector2D::ZeroVector;
	bool bBossAttackHitPlayer = false;
	bool bBossAttackHitWall = false;
	bool bBossShieldActive = false;
	float BossDamageMultiplier = 1.0f;
	float BossAttackIntervalMultiplier = 1.0f;
	float BossBackDamageMultiplier = 1.0f;
	float BossBreakDamageMultiplier = 1.0f;
	FName BossActiveAttackGeometry;
	float BossActiveAttackExtentX = 0.0f;
	float BossActiveAttackExtentY = 0.0f;
	int32 BossActiveAttackDamage = 0;
	int32 BossActiveAttackDamageHit2 = 0;
	int32 BossActiveAttackDamageHit3 = 0;
	int32 BossActiveAttackHitCount = 0;
	int32 BossActiveAttackPhaseTwoHitCount = 0;
	int32 BossActiveAttackProjectileCount = 0;
	float BossActiveAttackHitIntervalSeconds = 0.0f;
	float BossActiveAttackProjectileSpeed = 0.0f;
	float BossActiveAttackMoveSpeed = 0.0f;
	float BossActiveAttackContactIntervalSeconds = 0.0f;
	float BossActiveAttackBreakWindowSeconds = 0.0f;
	float BossActiveAttackBreakDamageMultiplier = 1.0f;
	float BossActiveAttackBackDamageMultiplier = 1.0f;
	FName BossActiveAttackKind;
	TArray<FName> BossActiveSummonEnemyIds;
	int32 BossActiveSummonCount = 0;
	float SlowMultiplier = 1.0f;
	int32 SlowTicksRemaining = 0;
	int32 TrainingRespawnTicksRemaining = 0;
};

struct FNaiwaP4DamageEvent
{
	int32 SourceEntityId = INDEX_NONE;
	int32 TargetEntityId = INDEX_NONE;
	int32 Damage = 0;
	FVector2D ImpactPosition = FVector2D::ZeroVector;
};

struct FNaiwaP4Event
{
	FName EventId;
	int32 SourceEntityId = INDEX_NONE;
	int32 TargetEntityId = INDEX_NONE;
	int64 Tick = 0;
	FVector2D Position = FVector2D::ZeroVector;
};
