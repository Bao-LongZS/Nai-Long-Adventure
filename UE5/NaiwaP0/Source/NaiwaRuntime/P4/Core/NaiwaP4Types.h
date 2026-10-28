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

struct FNaiwaP4InputCommand
{
	/** Held analog/digital movement for one fixed simulation tick. */
	FVector2D MoveAxis = FVector2D::ZeroVector;
	/** Attack press edge plus held state; the simulation owns cadence and effects. */
	bool bAttackPressed = false;
	bool bAttackHeld = false;
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
	ENaiwaP4Facing Facing = ENaiwaP4Facing::Front;
	bool bFacingRight = true;
	ENaiwaP4ActionCue ActionCue = ENaiwaP4ActionCue::Idle;
	int32 ActionTick = 0;
	bool bAlive = true;
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
