#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NaiwaP4DataTypes.generated.h"

class UTexture2D;

/** Table-driven enemy archetype data consumed by the deterministic W3 enemy brain. */
USTRUCT(BlueprintType)
struct FNaiwaP4EnemyRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy")
	FName BehaviorId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="1"))
	int32 MaxHP = 36;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="1"))
	int32 Damage = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="0"))
	float MoveSpeed = 132.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="1"))
	float AttackRange = 72.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="0.05"))
	float AttackIntervalSeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="0.05"))
	float TelegraphSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="1"))
	float CollisionRadius = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy")
	bool bFrontShield75 = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="0"))
	float AbilityRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="0"))
	float AbilityDurationSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="0"))
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="0", ClampMax="1"))
	float SlowMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="0"))
	float SlowDurationSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy")
	FName SummonEnemyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="0"))
	int32 SummonCount = 0;
};

/** One data row in the ordered wave spawn plan; repeated rows compose a wave. */
USTRUCT(BlueprintType)
struct FNaiwaP4WaveSpawnRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Wave", meta=(ClampMin="1"))
	int32 WaveIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Wave", meta=(ClampMin="0"))
	int32 SpawnOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Wave")
	FName EnemyTypeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Wave", meta=(ClampMin="1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Wave", meta=(ClampMin="0"))
	float SpawnIntervalSeconds = 0.55f;
};

/** Beta6 chapter projection row. Alpha chapter 1 remains a frozen runtime baseline. */
USTRUCT(BlueprintType)
struct FNaiwaP4Beta6ChapterRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Chapter", meta=(ClampMin="1", ClampMax="10"))
	int32 ChapterIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Chapter")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Chapter")
	FName ChapterType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Chapter")
	FName ThemeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Chapter", meta=(ClampMin="1"))
	int32 WaveCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Chapter", meta=(ClampMin="0"))
	int32 OrdinaryEnemyCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Chapter")
	FName BossId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Chapter", meta=(ClampMin="1"))
	int32 DurationTargetSeconds = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Chapter")
	FName DataSource;
};

/** Beta6 scene-theme projection; design prop tokens are not runtime asset bindings, and the status row still controls readiness. */
USTRUCT(BlueprintType)
struct FNaiwaP4Beta6ThemeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Theme")
	FName ThemeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Theme")
	FName SceneBase;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Theme")
	FName SceneTint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Theme")
	FName SceneLight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Theme")
	TArray<FName> DesignPropIds;

	/** Scene plate used by the isolated Beta6 test presenter; chapter gates still control playability. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Theme")
	TSoftObjectPtr<UTexture2D> BackdropTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Theme")
	FName RuntimeAssetBindingsStatus;
};

/** Beta6 chapter-wave composition. Unconfigured intervals must not drive runtime spawning. */
USTRUCT(BlueprintType)
struct FNaiwaP4Beta6WaveSpawnRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Wave", meta=(ClampMin="1", ClampMax="10"))
	int32 ChapterIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Wave", meta=(ClampMin="1"))
	int32 WaveIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Wave", meta=(ClampMin="0"))
	int32 SpawnOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Wave")
	FName EnemyTypeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Wave", meta=(ClampMin="1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Wave", meta=(ClampMin="0"))
	float SpawnIntervalSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Wave")
	bool bSpawnIntervalConfigured = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Wave")
	FName SourceKind;
};

/** Beta6 enemy projection. Runtime behavior must reject rows without configured movement/attack tuning. */
USTRUCT(BlueprintType)
struct FNaiwaP4Beta6EnemyRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy")
	FName EnemyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy", meta=(ClampMin="1"))
	int32 MaxHP = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy", meta=(ClampMin="1"))
	int32 Damage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy", meta=(ClampMin="0"))
	float TelegraphSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy")
	FName BehaviorId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy")
	int32 ThreatLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy")
	int32 DesignDebutChapter = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy")
	int32 DebutChapter = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy")
	FName DebutSource;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy")
	FName DesignSource;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy")
	FString SilhouetteNotes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy", meta=(ClampMin="0"))
	float MoveSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy", meta=(ClampMin="0"))
	float AttackRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy", meta=(ClampMin="0"))
	float AttackIntervalSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy", meta=(ClampMin="0"))
	float CollisionRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy")
	bool bFrontShield75 = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy", meta=(ClampMin="0"))
	float AbilityRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy", meta=(ClampMin="0"))
	float AbilityDurationSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy", meta=(ClampMin="0"))
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy", meta=(ClampMin="0", ClampMax="1"))
	float SlowMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy", meta=(ClampMin="0"))
	float SlowDurationSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy")
	FName SummonEnemyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy", meta=(ClampMin="0"))
	int32 SummonCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy")
	bool bRuntimeTuningConfigured = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Enemy")
	FName RuntimeTuningStatus;
};

/** Beta6 Boss design projection; attack IDs are not executable until detail rows are authored. */
USTRUCT(BlueprintType)
struct FNaiwaP4Beta6BossRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss")
	FName BossId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="1", ClampMax="6"))
	int32 ChapterIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="1"))
	int32 MaxHP = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="0", ClampMax="1"))
	float PhaseTwoAt = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="0"))
	float TelegraphSecondsMinimum = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss")
	TArray<FName> AttackIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss")
	FText BreakWindowDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="0"))
	float BreakWindowSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="1"))
	float BreakDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="1"))
	float BackDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss")
	FText IntroLine;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss")
	FText PhaseTwoLine;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss")
	FText DeathLine;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss")
	FText EventLine;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss")
	FText GagDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss")
	bool bRuntimeAttackTuningConfigured = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss")
	FName RuntimeAttackTuningStatus;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="0"))
	float MoveSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="1"))
	float AttackRange = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="1"))
	float CollisionRadius = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="0"))
	float PhaseTwoTransitionSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="1"))
	float PhaseTwoDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="0.01", ClampMax="1"))
	float PhaseTwoAttackIntervalMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="0"))
	int32 PhaseTwoGagDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="0"))
	float PhaseTwoGagRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss", meta=(ClampMin="0"))
	float PhaseTwoGagStunSeconds = 0.0f;
};

/** One ordered Beta6 Boss attack contract; blank parameters keep the attack disabled at runtime. */
USTRUCT(BlueprintType)
struct FNaiwaP4Beta6BossAttackRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack")
	FName BossId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack")
	FName AttackId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="1"))
	int32 AttackOrder = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack")
	FName PhaseId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack")
	FName DesignActionKind;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	float TelegraphSecondsMinimum = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	int32 Damage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	float ActiveDurationSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	float RecoverySeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack")
	FName HitboxGeometry;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	float HitboxExtentX = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	float HitboxExtentY = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	float BreakWindowSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="1"))
	float BreakDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="1"))
	float BackDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack")
	bool bBreakWindowConfigured = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack")
	bool bSummonsEnemies = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack")
	TArray<FName> SummonEnemyIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	int32 SummonCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack")
	bool bRuntimeTuningConfigured = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack")
	FName RuntimeTuningStatus;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	int32 HitCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	float HitIntervalSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	int32 DamageHit2 = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	int32 DamageHit3 = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	int32 PhaseTwoHitCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	float MoveSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Beta6|Boss Attack", meta=(ClampMin="0"))
	float ContactDamageIntervalSeconds = 0.0f;
};

/** Upgrade row shape used by the W1 import probe and the P4 upgrade registry. */
USTRUCT(BlueprintType)
struct FNaiwaP4UpgradeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FText TriggerRule;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FText ComboRule;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FName CategoryId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FText CategoryLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FName EffectKind;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	float P0 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	float P1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	float P2 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	bool bUnique = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	TArray<FName> PrerequisiteCardIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	TArray<FName> ExcludedCardIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FName RedlineGroup;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FName Rarity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	TSoftObjectPtr<UTexture2D> IconSoftRef;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	TSoftObjectPtr<UTexture2D> CategoryMarkSoftRef;
};

/** Runtime presentation metadata generated from the approved A1 import manifest. */
USTRUCT(BlueprintType)
struct FNaiwaP4A1FrameRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Animation")
	FName ActionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Animation")
	FName PoseId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Animation")
	FName ViewId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Animation")
	TSoftObjectPtr<UTexture2D> FrameTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Animation", meta=(ClampMin="0", ClampMax="723"))
	int32 FootAnchorYPx = 662;
};

/** One transform-only W4 effect preview frame and its authored timing metadata. */
USTRUCT(BlueprintType)
struct FNaiwaP4VfxFrameRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|VFX")
	FName EffectId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|VFX", meta=(ClampMin="0"))
	int32 FrameIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|VFX", meta=(ClampMin="0.001"))
	float DurationSeconds = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|VFX")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|VFX")
	TSoftObjectPtr<UTexture2D> FrameTexture;
};

/** One W4 Boss action preview frame. This table remains art-preview data, not gameplay state. */
USTRUCT(BlueprintType)
struct FNaiwaP4BossFrameRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Boss Art")
	FName StateId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Boss Art")
	FName ViewId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Boss Art", meta=(ClampMin="0"))
	int32 FrameIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Boss Art", meta=(ClampMin="0.001"))
	float DurationSeconds = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Boss Art")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Boss Art")
	TSoftObjectPtr<UTexture2D> FrameTexture;
};
