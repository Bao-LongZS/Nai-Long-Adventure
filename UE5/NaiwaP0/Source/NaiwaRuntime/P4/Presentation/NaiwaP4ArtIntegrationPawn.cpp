#include "NaiwaP4ArtIntegrationPawn.h"

#include "../Core/NaiwaP4ArenaState.h"
#include "../Core/NaiwaP4ArenaSubsystem.h"
#include "../Core/NaiwaP4Settings.h"
#include "../Data/NaiwaP4DataTypes.h"
#include "NaiwaP4ArtIntegrationGameMode.h"
#include "NaiwaP4Beta6TestGameMode.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"

namespace
{
	const TCHAR* FrameCatalogPath = TEXT("/Game/Naiwa/P4/Data/DT_P4_A1_RuntimeFrames.DT_P4_A1_RuntimeFrames");
	const TCHAR* VfxFrameCatalogPath = TEXT("/Game/Naiwa/P4/Data/DT_P4_W4_VFX_RuntimePreviewFrames.DT_P4_W4_VFX_RuntimePreviewFrames");
	const TCHAR* BossFrameCatalogPath = TEXT("/Game/Naiwa/P4/Data/DT_P4_W4_Boss_RuntimePreviewFrames.DT_P4_W4_Boss_RuntimePreviewFrames");
	const TCHAR* SpriteMaterialPath = TEXT("/Game/Naiwa/P4/Materials/Alpha/A1/M_P4_A1_SpriteRuntime.M_P4_A1_SpriteRuntime");
	const TCHAR* ArenaBackdropMaterialPath = TEXT("/Game/Naiwa/P4/Materials/Alpha/A2/M_P4_A2_RefinedBackdrop_v02.M_P4_A2_RefinedBackdrop_v02");
	const FName RefinedArenaOwnerTag(TEXT("NaiwaP4A2RefinedArena"));
	const TCHAR* ArenaBackdropTextureParameter = TEXT("P4ArenaSceneTexture");
	const TCHAR* NaiDingStaticPreviewTexturePath = TEXT("/Game/Naiwa/P4/Art/Alpha/W3_Enemies/StaticPreview/T_P4_W3_Naiding_IdleFront_StaticPreview_v01.T_P4_W3_Naiding_IdleFront_StaticPreview_v01");
	const TCHAR* NaiDunStaticPreviewTexturePath = TEXT("/Game/Naiwa/P4/Art/Alpha/W3_Enemies/StaticPreview/T_P4_W3_Naidun_IdleFront_StaticPreview_v01.T_P4_W3_Naidun_IdleFront_StaticPreview_v01");
	const TCHAR* ShadowMaterialPath = TEXT("/Game/Naiwa/P4/Materials/W1/M_P4_W1_SoftShadow_RadialV6.M_P4_W1_SoftShadow_RadialV6");
	const TCHAR* FrameTextureParameter = TEXT("P4FrameTexture");
	constexpr int32 MoveFrameTicks = 8;
	constexpr int32 TwoFrameActionTicks = 7;
	constexpr float CharacterPresentationScale = 0.82f;
	constexpr float SpriteWidthScale = 1.8f;
	constexpr float EnemyStaticPreviewFrameSizePx = 512.0f;
	constexpr float EnemyStaticPreviewVisibleHeightPx = 400.0f;
	constexpr float EnemyStaticPreviewFootAnchorYPx = 466.0f;
	constexpr float NaiDingStaticPreviewCanvasHeightUU = 215.0f;
	constexpr float NaiDunStaticPreviewCanvasHeightUU = 309.0f;
	constexpr float NaiTongBossPreviewCanvasHeightUU = 414.0f;
	constexpr float NaiDingStaticPreviewWidthScale = 0.90f;
	constexpr float NaiDunStaticPreviewWidthScale = 0.82f;
}

ANaiwaP4ArtIntegrationPawn::ANaiwaP4ArtIntegrationPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("P4ArtPreviewRoot"));
	RootComponent = SceneRoot;

	SpritePlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("P4PlayerSprite"));
	SpritePlane->SetupAttachment(SceneRoot);
	SpritePlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpritePlane->SetCastShadow(false);
	SpritePlane->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
	// Preserve the authored pose proportions while applying one presentation scale
	// to make the player read smaller in the fixed-camera arena.
	SpritePlane->SetRelativeScale3D(FVector(SpriteWidthScale * CharacterPresentationScale,
		SpriteHeightUU * CharacterPresentationScale / 100.0f, 1.0f));

	ContactShadow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("P4PlayerContactShadow"));
	ContactShadow->SetupAttachment(SceneRoot);
	ContactShadow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ContactShadow->SetCastShadow(false);
	ContactShadow->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
	ContactShadow->SetRelativeScale3D(FVector(
		1.35f * CharacterPresentationScale, 0.58f * CharacterPresentationScale, 1.0f));

	ArenaBackdrop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("P4ArenaBackdrop"));
	ArenaBackdrop->SetupAttachment(SceneRoot);
	ArenaBackdrop->SetAbsolute(true, true, true);
	ArenaBackdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArenaBackdrop->SetCastShadow(false);
}

void ANaiwaP4ArtIntegrationPawn::BeginPlay()
{
	Super::BeginPlay();

	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane)
	{
		SpritePlane->SetStaticMesh(Plane);
		EnemySpritePlaneMesh = Plane;
		ContactShadow->SetStaticMesh(Plane);
		ArenaBackdrop->SetStaticMesh(Plane);
	}
	ArenaBackdrop->SetVisibility(false);

	const UNaiwaP4Settings* Settings = GetDefault<UNaiwaP4Settings>();
	UWorld* World = GetWorld();
	const ANaiwaP4ArtIntegrationGameMode* PreviewGameMode = World
		? Cast<ANaiwaP4ArtIntegrationGameMode>(World->GetAuthGameMode())
		: nullptr;
	const ANaiwaP4Beta6TestGameMode* Beta6TestGameMode = World
		? Cast<ANaiwaP4Beta6TestGameMode>(World->GetAuthGameMode())
		: nullptr;
	UMaterialInterface* ArenaScene = LoadObject<UMaterialInterface>(nullptr, ArenaBackdropMaterialPath);
	ArenaBackdropMaterialBase = ArenaScene;
	const bool bRefinedArenaMap = World && World->GetWorldSettings()
		&& World->GetWorldSettings()->ActorHasTag(RefinedArenaOwnerTag);
	if (Settings && ArenaBackdrop && ArenaScene && bRefinedArenaMap)
	{
		ArenaBackdrop->SetMaterial(0, ArenaScene);
		ArenaBackdrop->SetVisibility(true);
		const float ArenaDepth = PreviewGameMode
			? PreviewGameMode->GetPreviewArenaDepth()
			: Settings->LogicalArenaDepth;
		const float CameraPitchDegrees = PreviewGameMode
			? PreviewGameMode->GetPreviewCameraPitch()
			: Settings->CameraPitchDegrees;
		ApplyArenaBackdropCameraTransform(ArenaDepth, CameraPitchDegrees);
	}
	else if (Settings && ArenaBackdrop && ArenaScene && Beta6TestGameMode)
	{
		SyncBeta6Backdrop();
	}
	else
	{
		ArenaBackdrop->SetVisibility(false);
		if (bRefinedArenaMap)
		{
			UE_LOG(LogTemp, Warning, TEXT("P4 refined arena background is unavailable: %s"), ArenaBackdropMaterialPath);
		}
		else if (Beta6TestGameMode)
		{
			UE_LOG(LogTemp, Warning, TEXT("P4 Beta6 preview backdrop material is unavailable: %s"), ArenaBackdropMaterialPath);
		}
	}

	if (UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, SpriteMaterialPath))
	{
		SpriteMaterialBase = BaseMaterial;
		SpriteMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		SpritePlane->SetMaterial(0, SpriteMaterial);

		NaiDingStaticPreviewTexture = LoadObject<UTexture2D>(nullptr, NaiDingStaticPreviewTexturePath);
		if (NaiDingStaticPreviewTexture)
		{
			NaiDingStaticPreviewMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			if (NaiDingStaticPreviewMaterial)
			{
				NaiDingStaticPreviewMaterial->SetTextureParameterValue(FrameTextureParameter, NaiDingStaticPreviewTexture);
			}
		}

		NaiDunStaticPreviewTexture = LoadObject<UTexture2D>(nullptr, NaiDunStaticPreviewTexturePath);
		if (NaiDunStaticPreviewTexture)
		{
			NaiDunStaticPreviewMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			if (NaiDunStaticPreviewMaterial)
			{
				NaiDunStaticPreviewMaterial->SetTextureParameterValue(FrameTextureParameter, NaiDunStaticPreviewTexture);
			}
		}

		NaiTongBossPreviewMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("P4 art preview could not load the approved A1 sprite material: %s"), SpriteMaterialPath);
	}
	UE_LOG(LogTemp, Display, TEXT("P4 W3 static enemy preview: Naiding=%s, Naidun=%s"),
		NaiDingStaticPreviewMaterial ? TEXT("art") : TEXT("graybox fallback"),
		NaiDunStaticPreviewMaterial ? TEXT("art") : TEXT("graybox fallback"));

	if (UMaterialInterface* Shadow = LoadObject<UMaterialInterface>(nullptr, ShadowMaterialPath))
	{
		SoftShadowMaterial = Shadow;
		ContactShadow->SetMaterial(0, Shadow);
	}
	else
	{
		ContactShadow->SetVisibility(false);
		UE_LOG(LogTemp, Warning, TEXT("P4 art preview is missing the calibrated W1 soft-shadow material."));
	}
	DamageNumberTextMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/EngineMaterials/UnlitText.UnlitText"));

	GrayboxSphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	GrayboxEnemyMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Naiwa/P4/Materials/Alpha/A2/M_P4_A2_LowObstacle.M_P4_A2_LowObstacle"));
	if (UMaterialInterface* ShapeMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		GrayboxProjectileMaterial = UMaterialInstanceDynamic::Create(ShapeMaterial, this);
		if (GrayboxProjectileMaterial)
		{
			GrayboxProjectileMaterial->SetVectorParameterValue(TEXT("Color"),
				FLinearColor(1.0f, 0.68f, 0.16f, 1.0f));
		}
		NaiDingEnemyMaterial = UMaterialInstanceDynamic::Create(ShapeMaterial, this);
		if (NaiDingEnemyMaterial)
		{
			NaiDingEnemyMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.88f, 0.55f, 0.27f, 1.0f));
		}
		NaiDunEnemyMaterial = UMaterialInstanceDynamic::Create(ShapeMaterial, this);
		if (NaiDunEnemyMaterial)
		{
			NaiDunEnemyMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.39f, 0.62f, 0.36f, 1.0f));
		}
	}

	LoadFrameCatalog();
	LoadVfxCatalog();
	if (UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr)
	{
		LastProcessedVfxEventCount = Arena->GetState().Events.Num();
	}
	SyncFromSimulation();
	LoadBossCatalog();
	SyncGrayboxEntities(0.0f);
	SyncDamageNumbers(0.0f);
	SyncActionVfx();

}

void ANaiwaP4ArtIntegrationPawn::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SyncBeta6Backdrop();
	ProcessVfxEvents();
	BossImpactPoseRemainingSeconds = FMath::Max(0.0f,
		BossImpactPoseRemainingSeconds - FMath::Max(DeltaSeconds, 0.0f));
	BossDeathPoseRemainingSeconds = FMath::Max(0.0f,
		BossDeathPoseRemainingSeconds - FMath::Max(DeltaSeconds, 0.0f));
	if (ImpactPresentationFreezeSecondsRemaining > 0.0f)
	{
		// Briefly hold presentation transforms on impact; simulation remains authoritative and keeps ticking.
		ImpactPresentationFreezeSecondsRemaining = FMath::Max(0.0f,
			ImpactPresentationFreezeSecondsRemaining - FMath::Max(DeltaSeconds, 0.0f));
		return;
	}
	SyncFromSimulation();
	SyncGrayboxEntities(DeltaSeconds);
	SyncDamageNumbers(DeltaSeconds);
	SyncActionVfx();
	SyncActiveVfx(DeltaSeconds);
}

void ANaiwaP4ArtIntegrationPawn::SyncBeta6Backdrop()
{
	UWorld* World = GetWorld();
	const ANaiwaP4Beta6TestGameMode* Beta6TestGameMode = World
		? Cast<ANaiwaP4Beta6TestGameMode>(World->GetAuthGameMode())
		: nullptr;
	const UNaiwaP4Settings* Settings = GetDefault<UNaiwaP4Settings>();
	UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	const FNaiwaP4Beta6ChapterBundle* Chapter = Arena ? Arena->GetCurrentBeta6Chapter() : nullptr;
	if (!Beta6TestGameMode || !Settings || !ArenaBackdrop || !ArenaBackdropMaterialBase)
	{
		return;
	}
	if (!Chapter)
	{
		ArenaBackdrop->SetVisibility(false);
		return;
	}

	const FName ThemeId = Chapter->Theme.ThemeId;
	if (ThemeId.IsNone())
	{
		ArenaBackdrop->SetVisibility(false);
		return;
	}

	if (CurrentBeta6BackdropThemeId != ThemeId || !Beta6ArenaBackdropMaterial)
	{
		UTexture2D* BackdropTexture = Chapter->Theme.BackdropTexture.LoadSynchronous();
		if (!BackdropTexture)
		{
			ArenaBackdrop->SetVisibility(false);
			return;
		}

		Beta6ArenaBackdropMaterial = UMaterialInstanceDynamic::Create(ArenaBackdropMaterialBase, this);
		if (!Beta6ArenaBackdropMaterial)
		{
			ArenaBackdrop->SetVisibility(false);
			return;
		}
		Beta6ArenaBackdropMaterial->SetTextureParameterValue(ArenaBackdropTextureParameter, BackdropTexture);
		CurrentBeta6BackdropThemeId = ThemeId;
		ArenaBackdrop->SetMaterial(0, Beta6ArenaBackdropMaterial);
		ApplyArenaBackdropCameraTransform(Beta6TestGameMode->GetPreviewArenaDepth(),
			Beta6TestGameMode->GetPreviewCameraPitch());
	}
	ArenaBackdrop->SetVisibility(true);
}

void ANaiwaP4ArtIntegrationPawn::ApplyArenaBackdropCameraTransform(
	const float ArenaDepth, const float CameraPitchDegrees)
{
	const UNaiwaP4Settings* Settings = GetDefault<UNaiwaP4Settings>();
	if (!Settings || !ArenaBackdrop)
	{
		return;
	}

	const float PitchRadians = FMath::DegreesToRadians(CameraPitchDegrees);
	const float CameraDistance = Settings->CameraDistance;
	const FVector ArenaCenter(Settings->LogicalArenaWidth * 0.5f, ArenaDepth * 0.5f, 0.0f);
	const FVector CameraOffset(0.0f,
		-CameraDistance * FMath::Cos(PitchRadians),
		 CameraDistance * FMath::Sin(PitchRadians));
	const FVector CameraLocation = ArenaCenter + CameraOffset;
	const FRotator CameraRotation(-CameraPitchDegrees, 90.0f, 0.0f);
	const FVector CameraForward = CameraRotation.Vector().GetSafeNormal();
	const FVector CameraRight = CameraRotation.RotateVector(FVector::RightVector).GetSafeNormal();
	const float BackdropDistance = CameraDistance + 650.0f;
	const float BackdropWidth = 2.0f * BackdropDistance
		* FMath::Tan(FMath::DegreesToRadians(Settings->CameraHorizontalFOVDegrees * 0.5f)) * 1.06f;
	const float BackdropHeight = BackdropWidth * (9.0f / 16.0f);

	// Fixed camera scene plates sit behind the play plane and keep sprites and combat feedback in world space.
	ArenaBackdrop->SetWorldLocation(CameraLocation + CameraForward * BackdropDistance);
	ArenaBackdrop->SetWorldRotation(FRotationMatrix::MakeFromXZ(CameraRight, -CameraForward).Rotator());
	ArenaBackdrop->SetWorldScale3D(FVector(BackdropWidth / 100.0f, BackdropHeight / 100.0f, 1.0f));
}

void ANaiwaP4ArtIntegrationPawn::SyncGrayboxEntities(const float DeltaSeconds)
{
	if (!GrayboxSphereMesh)
	{
		return;
	}
	UWorld* World = GetWorld();
	UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	const UNaiwaP4Settings* Settings = GetDefault<UNaiwaP4Settings>();
	if (!Arena || !Settings)
	{
		return;
	}

	for (const FNaiwaP4EntityState& Entity : Arena->GetState().Entities)
	{
		if (!Entity.bTrainingEnemy && !Entity.bWaveEnemy && !Entity.bWaveBoss
			&& Entity.Kind != ENaiwaP4EntityKind::Projectile)
		{
			continue;
		}
		const bool bHasMilkShotArt = Entity.Kind == ENaiwaP4EntityKind::Projectile
			&& ActiveVfx.ContainsByPredicate([&Entity](const FNaiwaP4ActiveVfx& Active)
			{
				return Active.EffectId == TEXT("fx_milk_shot")
					&& Active.FollowEntityId == Entity.EntityId;
			});

		const bool bNaiDingPreview = Entity.bWaveEnemy
			&& Entity.EnemyTypeId == TEXT("naiding")
			&& EnemySpritePlaneMesh
			&& NaiDingStaticPreviewMaterial;
		const bool bNaiDunPreview = Entity.bWaveEnemy
			&& Entity.EnemyTypeId == TEXT("naidun")
			&& EnemySpritePlaneMesh
			&& NaiDunStaticPreviewMaterial;
		const bool bNaiTongBossPreview = Entity.bWaveBoss
			&& EnemySpritePlaneMesh
			&& NaiTongBossPreviewMaterial
			&& !BossSequences.IsEmpty();
		const bool bUseStaticEnemyPreview = bNaiDingPreview || bNaiDunPreview || bNaiTongBossPreview;
		const FVector2D RenderPosition = Arena->GetSmoothedPresentationPosition(Entity.EntityId, Entity.Position);
		const float WorldX = Settings->LogicalArenaWidth - static_cast<float>(RenderPosition.X);

		UStaticMeshComponent* EnemyShadow = EnemyContactShadows.FindRef(Entity.EntityId);
		if (Entity.Kind != ENaiwaP4EntityKind::Projectile && EnemySpritePlaneMesh && SoftShadowMaterial)
		{
			if (!EnemyShadow)
			{
				EnemyShadow = NewObject<UStaticMeshComponent>(this,
					FName(*FString::Printf(TEXT("P4EnemyContactShadow_%d"), Entity.EntityId)));
				if (EnemyShadow)
				{
					EnemyShadow->SetupAttachment(SceneRoot);
					EnemyShadow->SetAbsolute(true, true, true);
					EnemyShadow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					EnemyShadow->SetCastShadow(false);
					EnemyShadow->SetStaticMesh(EnemySpritePlaneMesh);
					EnemyShadow->SetMaterial(0, SoftShadowMaterial);
					EnemyShadow->RegisterComponent();
					EnemyContactShadows.Add(Entity.EntityId, EnemyShadow);
				}
			}
			if (EnemyShadow)
			{
				EnemyShadow->SetVisibility(Entity.bAlive);
				if (Entity.bAlive)
				{
					const float ShadowRadiusScale = FMath::Max(Entity.CollisionRadius, 1.0f) / 50.0f;
					EnemyShadow->SetWorldLocation(FVector(WorldX, RenderPosition.Y, 1.5f));
					EnemyShadow->SetWorldRotation(FRotator::ZeroRotator);
					EnemyShadow->SetWorldScale3D(FVector(
						1.35f * ShadowRadiusScale * CharacterPresentationScale,
						0.58f * ShadowRadiusScale * CharacterPresentationScale, 1.0f));
				}
			}
		}

		UStaticMeshComponent* Mesh = GrayboxEntityMeshes.FindRef(Entity.EntityId);
		if (!Mesh)
		{
			const FName ComponentName(*FString::Printf(TEXT("P4GrayboxEntity_%d"), Entity.EntityId));
			Mesh = NewObject<UStaticMeshComponent>(this, ComponentName);
			if (!Mesh)
			{
				continue;
			}
			Mesh->SetupAttachment(SceneRoot);
			Mesh->SetAbsolute(true, true, true);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCastShadow(false);
			Mesh->SetStaticMesh(bUseStaticEnemyPreview ? EnemySpritePlaneMesh : GrayboxSphereMesh);
			UMaterialInterface* EntityMaterial = bNaiDingPreview
				? static_cast<UMaterialInterface*>(NaiDingStaticPreviewMaterial.Get())
				: bNaiTongBossPreview
					? static_cast<UMaterialInterface*>(NaiTongBossPreviewMaterial.Get())
				: bNaiDunPreview
					? static_cast<UMaterialInterface*>(NaiDunStaticPreviewMaterial.Get())
					: GrayboxEnemyMaterial.Get();
			if (Entity.Kind == ENaiwaP4EntityKind::Projectile)
			{
				EntityMaterial = GrayboxProjectileMaterial.Get();
			}
			else if (!bUseStaticEnemyPreview && Entity.EnemyTypeId == TEXT("naidun"))
			{
				EntityMaterial = NaiDunEnemyMaterial ? NaiDunEnemyMaterial.Get() : EntityMaterial;
			}
			else if (!bUseStaticEnemyPreview && Entity.bWaveEnemy)
			{
				EntityMaterial = NaiDingEnemyMaterial ? NaiDingEnemyMaterial.Get() : EntityMaterial;
			}
			Mesh->SetMaterial(0, EntityMaterial);
			Mesh->RegisterComponent();
			GrayboxEntityMeshes.Add(Entity.EntityId, Mesh);
		}

		const bool bShowBossDeathPose = Entity.bWaveBoss && !Entity.bAlive
			&& BossDeathPoseRemainingSeconds > 0.0f && bNaiTongBossPreview;
		Mesh->SetVisibility((Entity.bAlive || bShowBossDeathPose) && !bHasMilkShotArt);
		if ((!Entity.bAlive && !bShowBossDeathPose) || bHasMilkShotArt)
		{
			continue;
		}
		const float Radius = Entity.CollisionRadius;
		const float CenterZ = Entity.Kind == ENaiwaP4EntityKind::Projectile ? 48.0f : Radius;
		if (bUseStaticEnemyPreview)
		{
			// Static front-view art is preview-only. Keep a conservative fallback sphere
			// whenever either preview texture/material is unavailable.
			const float CanvasHeightUU = bNaiTongBossPreview
				? NaiTongBossPreviewCanvasHeightUU
				: bNaiDunPreview
					? NaiDunStaticPreviewCanvasHeightUU
					: NaiDingStaticPreviewCanvasHeightUU;
			const float ScaledCanvasHeightUU = CanvasHeightUU * CharacterPresentationScale;
			const float AnchorCenterZ = ((EnemyStaticPreviewFootAnchorYPx / EnemyStaticPreviewFrameSizePx) - 0.5f)
				* ScaledCanvasHeightUU;
			Mesh->SetWorldLocation(FVector(WorldX, RenderPosition.Y, CenterZ + AnchorCenterZ));
			Mesh->SetWorldRotation(FRotator(0.0f, 0.0f, 90.0f));
			const float WidthScale = bNaiTongBossPreview
				? 0.90f
				: bNaiDunPreview
					? NaiDunStaticPreviewWidthScale
					: NaiDingStaticPreviewWidthScale;
			const bool bMirrorSideView = bNaiTongBossPreview
				&& Entity.Facing == ENaiwaP4Facing::Side
				&& Entity.bFacingRight;
			Mesh->SetWorldScale3D(FVector(
				(bMirrorSideView ? -1.0f : 1.0f) * ScaledCanvasHeightUU * WidthScale / 100.0f,
				ScaledCanvasHeightUU / 100.0f, 1.0f));
			if (bNaiTongBossPreview)
			{
				ApplyBossArt(Entity, Mesh, DeltaSeconds);
			}
			continue;
		}

		const float HealthScale = (Entity.bTrainingEnemy || Entity.bWaveEnemy) && Entity.MaxHP > 0
			? 0.65f + 0.35f * static_cast<float>(Entity.HP) / Entity.MaxHP
			: 1.0f;
		const float TelegraphScale = (Entity.bTrainingEnemy || Entity.bWaveEnemy) && Entity.EnemyTelegraphTicksRemaining > 0
			? 1.2f
			: 1.0f;
		Mesh->SetWorldLocation(FVector(WorldX, RenderPosition.Y, CenterZ));
		const float ProjectileScale = Entity.Kind == ENaiwaP4EntityKind::Projectile ? 1.45f : 1.0f;
		const float CharacterScale = Entity.Kind == ENaiwaP4EntityKind::Projectile
			? 1.0f : CharacterPresentationScale;
		Mesh->SetWorldScale3D(FVector(
			Radius / 50.0f * HealthScale * TelegraphScale * ProjectileScale * CharacterScale));
	}
}

void ANaiwaP4ArtIntegrationPawn::SyncDamageNumbers(const float DeltaSeconds)
{
	constexpr float DamageNumberLifetime = 0.72f;
	constexpr float DamageNumberRiseSpeed = 48.0f;
	constexpr float DamageNumberWorldSize = 62.0f;

	UWorld* World = GetWorld();
	UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	const UNaiwaP4Settings* Settings = GetDefault<UNaiwaP4Settings>();
	if (!Arena || !Settings)
	{
		return;
	}
	const int64 SimulationTick = Arena->GetState().CurrentTick;
	if (LastObservedSimulationTick >= 0 && SimulationTick < LastObservedSimulationTick)
	{
		// Run resets reuse entity IDs; discard old HP baselines so a fresh spawn is not mistaken for damage.
		PreviousEntityHP.Reset();
	}
	LastObservedSimulationTick = SimulationTick;

	TSet<int32> SeenEntityIds;
	for (const FNaiwaP4EntityState& Entity : Arena->GetState().Entities)
	{
		if (Entity.Kind != ENaiwaP4EntityKind::Player && !Entity.bTrainingEnemy
			&& !Entity.bWaveEnemy && !Entity.bWaveBoss)
		{
			continue;
		}

		SeenEntityIds.Add(Entity.EntityId);
		const int32* PreviousHP = PreviousEntityHP.Find(Entity.EntityId);
		if (PreviousHP && Entity.HP < *PreviousHP)
		{
			const int32 ActualDamage = *PreviousHP - Entity.HP;
			const FVector2D RenderPosition = Arena->GetSmoothedPresentationPosition(Entity.EntityId, Entity.Position);
			const float WorldX = Settings->LogicalArenaWidth - static_cast<float>(RenderPosition.X);
			float TextHeight = Entity.Kind == ENaiwaP4EntityKind::Player
				? SpriteHeightUU * CharacterPresentationScale + 30.0f * CharacterPresentationScale
				: Entity.CollisionRadius * (1.0f + CharacterPresentationScale) + 45.0f * CharacterPresentationScale;
			if (Entity.bWaveEnemy && Entity.EnemyTypeId == TEXT("naiding"))
			{
				TextHeight = Entity.CollisionRadius
					+ EnemyStaticPreviewVisibleHeightPx / EnemyStaticPreviewFrameSizePx
						* NaiDingStaticPreviewCanvasHeightUU * CharacterPresentationScale
					+ 28.0f * CharacterPresentationScale;
			}
			else if (Entity.bWaveEnemy && Entity.EnemyTypeId == TEXT("naidun"))
			{
				TextHeight = Entity.CollisionRadius
					+ EnemyStaticPreviewVisibleHeightPx / EnemyStaticPreviewFrameSizePx
						* NaiDunStaticPreviewCanvasHeightUU * CharacterPresentationScale
					+ 28.0f * CharacterPresentationScale;
			}
			else if (Entity.bWaveBoss)
			{
				TextHeight = Entity.CollisionRadius
					+ EnemyStaticPreviewVisibleHeightPx / EnemyStaticPreviewFrameSizePx
						* NaiTongBossPreviewCanvasHeightUU * CharacterPresentationScale
					+ 28.0f * CharacterPresentationScale;
			}
			const FVector NumberLocation(WorldX, RenderPosition.Y, Entity.HeightZ + TextHeight);

			UTextRenderComponent* Number = NewObject<UTextRenderComponent>(this,
				FName(*FString::Printf(TEXT("P4DamageNumber_%d_%d"), Entity.EntityId, GetUniqueID())));
			if (Number)
			{
				Number->SetupAttachment(SceneRoot);
				Number->SetAbsolute(true, true, true);
				Number->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Number->SetCastShadow(false);
				if (DamageNumberTextMaterial)
				{
					Number->SetTextMaterial(DamageNumberTextMaterial);
				}
				Number->SetHorizontalAlignment(EHTA_Center);
				Number->SetVerticalAlignment(EVRTA_TextCenter);
				Number->SetWorldSize(DamageNumberWorldSize);
				Number->SetText(FText::FromString(FString::Printf(TEXT("-%d"), ActualDamage)));
				Number->SetTextRenderColor(Entity.Kind == ENaiwaP4EntityKind::Player
					? FColor(255, 82, 82, 255)
					: FColor(255, 221, 74, 255));

				// TextRender glyphs lie in local YZ: their horizontal baseline runs along -Y,
				// while glyph tops are toward +Z. Map those directions to screen right/up.
				FRotator CameraRotation = FRotator(-50.0f, 90.0f, 0.0f);
				if (APlayerController* PlayerController = World->GetFirstPlayerController())
				{
					if (PlayerController->PlayerCameraManager)
					{
						CameraRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
					}
				}
				const FVector CameraForward = CameraRotation.RotateVector(FVector::ForwardVector);
				const FVector CameraUp = CameraRotation.RotateVector(FVector::UpVector);
				const FRotator TextRotation = FRotationMatrix::MakeFromXZ(-CameraForward, CameraUp).Rotator();
				Number->SetWorldLocation(NumberLocation);
				Number->SetWorldRotation(TextRotation);
				Number->RegisterComponent();

				FNaiwaP4ActiveDamageNumber& ActiveNumber = ActiveDamageNumbers.AddDefaulted_GetRef();
				ActiveNumber.Component = Number;
				ActiveNumber.InitialLocation = NumberLocation;
			}
		}
		PreviousEntityHP.Add(Entity.EntityId, Entity.HP);
	}

	for (auto It = PreviousEntityHP.CreateIterator(); It; ++It)
	{
		if (!SeenEntityIds.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	for (int32 Index = ActiveDamageNumbers.Num() - 1; Index >= 0; --Index)
	{
		FNaiwaP4ActiveDamageNumber& ActiveNumber = ActiveDamageNumbers[Index];
		ActiveNumber.AgeSeconds += DeltaSeconds;
		if (!ActiveNumber.Component || ActiveNumber.AgeSeconds >= DamageNumberLifetime)
		{
			if (ActiveNumber.Component)
			{
				ActiveNumber.Component->DestroyComponent();
			}
			ActiveDamageNumbers.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}

		const float LifeRatio = FMath::Clamp(ActiveNumber.AgeSeconds / DamageNumberLifetime, 0.0f, 1.0f);
		const float Alpha = 1.0f - LifeRatio;
		ActiveNumber.Component->SetWorldLocation(ActiveNumber.InitialLocation
			+ FVector(0.0f, 0.0f, DamageNumberRiseSpeed * ActiveNumber.AgeSeconds));
		FColor TextColor = ActiveNumber.Component->TextRenderColor;
		TextColor.A = FMath::RoundToInt(Alpha * 255.0f);
		ActiveNumber.Component->SetTextRenderColor(TextColor);
	}
}

void ANaiwaP4ArtIntegrationPawn::LoadFrameCatalog()
{
	FrameCatalog = LoadObject<UDataTable>(nullptr, FrameCatalogPath);
	if (!FrameCatalog || FrameCatalog->GetRowStruct() != FNaiwaP4A1FrameRow::StaticStruct())
	{
		UE_LOG(LogTemp, Error, TEXT("P4 A1 animation DataTable is missing or has the wrong row struct: %s"), FrameCatalogPath);
		return;
	}

	for (const FName RowName : FrameCatalog->GetRowNames())
	{
		const FNaiwaP4A1FrameRow* Row = FrameCatalog->FindRow<FNaiwaP4A1FrameRow>(RowName, TEXT("P4 A1 frame catalog load"), false);
		UTexture2D* Texture = Row ? Row->FrameTexture.LoadSynchronous() : nullptr;
		if (Row && Texture)
		{
			LoadedFrames.Add(RowName, Texture);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("P4 A1 frame '%s' is absent or failed to load."), *RowName.ToString());
		}
	}

	UE_LOG(LogTemp, Display, TEXT("P4 A1 art presenter loaded %d/%d approved frames."), LoadedFrames.Num(), FrameCatalog->GetRowNames().Num());
}

void ANaiwaP4ArtIntegrationPawn::LoadVfxCatalog()
{
	VfxFrameCatalog = LoadObject<UDataTable>(nullptr, VfxFrameCatalogPath);
	if (!VfxFrameCatalog || VfxFrameCatalog->GetRowStruct() != FNaiwaP4VfxFrameRow::StaticStruct())
	{
		UE_LOG(LogTemp, Warning, TEXT("P4 W4 VFX preview DataTable is missing or has the wrong row struct: %s"), VfxFrameCatalogPath);
		return;
	}

	for (const FName RowName : VfxFrameCatalog->GetRowNames())
	{
		const FNaiwaP4VfxFrameRow* Row = VfxFrameCatalog->FindRow<FNaiwaP4VfxFrameRow>(
			RowName, TEXT("P4 W4 VFX frame catalog load"), false);
		UTexture2D* Texture = Row ? Row->FrameTexture.LoadSynchronous() : nullptr;
		if (!Row || Row->EffectId.IsNone() || !Texture || Row->DurationSeconds <= 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("P4 W4 VFX row '%s' is incomplete or its texture failed to load."), *RowName.ToString());
			continue;
		}

		FNaiwaP4RuntimeVfxSequence& Sequence = VfxSequences.FindOrAdd(Row->EffectId);
		Sequence.bLoop = Row->bLoop;
		FNaiwaP4RuntimeVfxFrame& Frame = Sequence.Frames.AddDefaulted_GetRef();
		Frame.Texture = Texture;
		Frame.FrameIndex = Row->FrameIndex;
		Frame.DurationSeconds = Row->DurationSeconds;
	}

	for (TPair<FName, FNaiwaP4RuntimeVfxSequence>& Pair : VfxSequences)
	{
		Pair.Value.Frames.Sort([](const FNaiwaP4RuntimeVfxFrame& A, const FNaiwaP4RuntimeVfxFrame& B)
		{
			return A.FrameIndex < B.FrameIndex;
		});
	}
	UE_LOG(LogTemp, Display, TEXT("P4 W4 VFX preview presenter loaded %d sequences from %d rows."),
		VfxSequences.Num(), VfxFrameCatalog->GetRowNames().Num());
}

void ANaiwaP4ArtIntegrationPawn::LoadBossCatalog()
{
	BossFrameCatalog = LoadObject<UDataTable>(nullptr, BossFrameCatalogPath);
	if (!BossFrameCatalog || BossFrameCatalog->GetRowStruct() != FNaiwaP4BossFrameRow::StaticStruct())
	{
		UE_LOG(LogTemp, Display, TEXT("P4 W4 Boss animation preview table is unavailable; retaining the current static Boss preview."));
		return;
	}

	for (const FName RowName : BossFrameCatalog->GetRowNames())
	{
		const FNaiwaP4BossFrameRow* Row = BossFrameCatalog->FindRow<FNaiwaP4BossFrameRow>(
			RowName, TEXT("P4 W4 Boss frame catalog load"), false);
		UTexture2D* Texture = Row ? Row->FrameTexture.LoadSynchronous() : nullptr;
		if (!Row || Row->StateId.IsNone() || Row->ViewId.IsNone() || !Texture
			|| Row->DurationSeconds <= 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("P4 W4 Boss row '%s' is incomplete or its texture failed to load."), *RowName.ToString());
			continue;
		}

		const FName SequenceId(*FString::Printf(TEXT("%s_%s"), *Row->StateId.ToString(), *Row->ViewId.ToString()));
		FNaiwaP4RuntimeVfxSequence& Sequence = BossSequences.FindOrAdd(SequenceId);
		Sequence.bLoop = Row->bLoop;
		FNaiwaP4RuntimeVfxFrame& Frame = Sequence.Frames.AddDefaulted_GetRef();
		Frame.Texture = Texture;
		Frame.FrameIndex = Row->FrameIndex;
		Frame.DurationSeconds = Row->DurationSeconds;
	}

	for (TPair<FName, FNaiwaP4RuntimeVfxSequence>& Pair : BossSequences)
	{
		Pair.Value.Frames.Sort([](const FNaiwaP4RuntimeVfxFrame& A, const FNaiwaP4RuntimeVfxFrame& B)
		{
			return A.FrameIndex < B.FrameIndex;
		});
	}
	UE_LOG(LogTemp, Display, TEXT("P4 W4 Boss preview presenter loaded %d animation states from %d rows."),
		BossSequences.Num(), BossFrameCatalog->GetRowNames().Num());
}

FName ANaiwaP4ArtIntegrationPawn::ResolveBossSequenceId(const FNaiwaP4EntityState& Boss) const
{
	FName StateId = TEXT("idle");
	if (!Boss.bAlive && BossDeathPoseRemainingSeconds > 0.0f)
	{
		StateId = TEXT("death");
	}
	else if (BossImpactPoseRemainingSeconds > 0.0f)
	{
		StateId = TEXT("lid_slam_impact");
	}
	else if (Boss.BossPhaseTransitionTicksRemaining > 0)
	{
		StateId = TEXT("phase_two");
	}
	else if (Boss.HitCueTicksRemaining > 0)
	{
		StateId = TEXT("hit");
	}
	else if (Boss.EnemyTelegraphTicksRemaining > 0)
	{
		StateId = Boss.BossAttack == ENaiwaP4BossAttack::Charge
			? FName(TEXT("charge_telegraph")) : FName(TEXT("lid_slam_telegraph"));
	}
	else if (Boss.BossChargeTicksRemaining > 0 || Boss.Velocity.SizeSquared() > 1.0f)
	{
		// The current art batch has no accepted charge-attack pose; use the two-frame move preview.
		StateId = TEXT("move");
	}

	FName ViewId = Boss.Facing == ENaiwaP4Facing::Back ? FName(TEXT("back"))
		: Boss.Facing == ENaiwaP4Facing::Side ? FName(TEXT("side_right")) : FName(TEXT("front"));
	if (StateId == TEXT("charge_telegraph") || StateId == TEXT("move"))
	{
		ViewId = TEXT("side_right");
	}
	else if (StateId == TEXT("lid_slam_telegraph") || StateId == TEXT("lid_slam_impact")
		|| StateId == TEXT("hit") || StateId == TEXT("phase_two"))
	{
		ViewId = TEXT("front");
	}
	return FName(*FString::Printf(TEXT("%s_%s"), *StateId.ToString(), *ViewId.ToString()));
}

void ANaiwaP4ArtIntegrationPawn::ApplyBossArt(
	const FNaiwaP4EntityState& Boss, UStaticMeshComponent* Mesh, const float DeltaSeconds)
{
	if (!Mesh || !NaiTongBossPreviewMaterial)
	{
		return;
	}

	FName SequenceId = ResolveBossSequenceId(Boss);
	const FNaiwaP4RuntimeVfxSequence* Sequence = BossSequences.Find(SequenceId);
	if (!Sequence || Sequence->Frames.IsEmpty())
	{
		SequenceId = TEXT("idle_front");
		Sequence = BossSequences.Find(SequenceId);
	}
	if (!Sequence || Sequence->Frames.IsEmpty())
	{
		return;
	}

	if (CurrentBossSequenceId != SequenceId)
	{
		CurrentBossSequenceId = SequenceId;
		CurrentBossFrameIndex = 0;
		CurrentBossFrameElapsedSeconds = 0.0f;
		NaiTongBossPreviewMaterial->SetTextureParameterValue(FrameTextureParameter,
			Sequence->Frames[0].Texture);
	}
	else
	{
		CurrentBossFrameElapsedSeconds += FMath::Max(DeltaSeconds, 0.0f);
		while (CurrentBossFrameElapsedSeconds >= Sequence->Frames[CurrentBossFrameIndex].DurationSeconds)
		{
			CurrentBossFrameElapsedSeconds -= Sequence->Frames[CurrentBossFrameIndex].DurationSeconds;
			if (CurrentBossFrameIndex + 1 < Sequence->Frames.Num())
			{
				++CurrentBossFrameIndex;
			}
			else if (Sequence->bLoop)
			{
				CurrentBossFrameIndex = 0;
			}
			else
			{
				CurrentBossFrameElapsedSeconds = 0.0f;
				break;
			}
			NaiTongBossPreviewMaterial->SetTextureParameterValue(FrameTextureParameter,
				Sequence->Frames[CurrentBossFrameIndex].Texture);
		}
	}
	Mesh->SetMaterial(0, NaiTongBossPreviewMaterial);
}

const FNaiwaP4EntityState* ANaiwaP4ArtIntegrationPawn::FindEntity(const int32 EntityId) const
{
	if (EntityId == INDEX_NONE)
	{
		return nullptr;
	}
	const UWorld* World = GetWorld();
	const UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	return Arena ? Arena->GetState().Entities.FindByPredicate([EntityId](const FNaiwaP4EntityState& Entity)
	{
		return Entity.EntityId == EntityId;
	}) : nullptr;
}

FVector2D ANaiwaP4ArtIntegrationPawn::GetEntityWorldPresentationPosition(const FNaiwaP4EntityState& Entity) const
{
	const UNaiwaP4Settings* Settings = GetDefault<UNaiwaP4Settings>();
	const UWorld* World = GetWorld();
	const UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	const FVector2D RenderPosition = Arena
		? Arena->GetSmoothedPresentationPosition(Entity.EntityId, Entity.Position)
		: Entity.Position;
	const float WorldX = Settings
		? Settings->LogicalArenaWidth - static_cast<float>(RenderPosition.X)
		: static_cast<float>(RenderPosition.X);
	return FVector2D(WorldX, RenderPosition.Y);
}

FVector ANaiwaP4ArtIntegrationPawn::ResolveVfxWorldLocation(const FNaiwaP4ActiveVfx& Vfx) const
{
	if (Vfx.bFollowEntity)
	{
		if (const FNaiwaP4EntityState* Entity = FindEntity(Vfx.FollowEntityId))
		{
			const FVector2D Position = GetEntityWorldPresentationPosition(*Entity);
			float CenterZ = Entity->HeightZ + Entity->CollisionRadius;
			if (Entity->Kind == ENaiwaP4EntityKind::Player)
			{
				CenterZ = Entity->HeightZ + SpriteHeightUU * CharacterPresentationScale * 0.27f;
			}
			else if (Entity->Kind == ENaiwaP4EntityKind::Projectile)
			{
				CenterZ = Entity->HeightZ + 48.0f;
			}
			else if (Entity->bWaveBoss)
			{
				CenterZ += ((EnemyStaticPreviewFootAnchorYPx / EnemyStaticPreviewFrameSizePx) - 0.5f)
					* NaiTongBossPreviewCanvasHeightUU * CharacterPresentationScale;
			}
			return FVector(Position.X, Position.Y, CenterZ + Vfx.WorldZ);
		}
	}

	const UNaiwaP4Settings* Settings = GetDefault<UNaiwaP4Settings>();
	const float WorldX = Settings
		? Settings->LogicalArenaWidth - static_cast<float>(Vfx.LogicalPosition.X)
		: static_cast<float>(Vfx.LogicalPosition.X);
	return FVector(WorldX, Vfx.LogicalPosition.Y, Vfx.WorldZ);
}

void ANaiwaP4ArtIntegrationPawn::PlayVfx(const FName EffectId, const FVector2D LogicalPosition,
	const int32 FollowEntityId, const bool bFollowEntity, const bool bDestroyIfTargetMissing,
	const FVector WorldScale, const bool bFlipX, const float WorldZ)
{
	const FNaiwaP4RuntimeVfxSequence* Sequence = VfxSequences.Find(EffectId);
	if (!Sequence || Sequence->Frames.IsEmpty() || !EnemySpritePlaneMesh || !SpriteMaterial)
	{
		return;
	}
	const FNaiwaP4RuntimeVfxFrame& FirstFrame = Sequence->Frames[0];
	if (!FirstFrame.Texture)
	{
		return;
	}

	const FVector PresentationScale = WorldScale;
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this,
		FName(*FString::Printf(TEXT("P4VFX_%s_%u"), *EffectId.ToString(), GetUniqueID())));
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(SpriteMaterialBase, this);
	if (!Component || !Material)
	{
		return;
	}
	Material->SetTextureParameterValue(FrameTextureParameter, FirstFrame.Texture);
	Component->SetupAttachment(SceneRoot);
	Component->SetAbsolute(true, true, true);
	Component->SetStaticMesh(EnemySpritePlaneMesh);
	Component->SetMaterial(0, Material);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(false);
	Component->SetWorldRotation(SpritePlane ? SpritePlane->GetComponentRotation() : FRotator(0.0f, 0.0f, 90.0f));
	Component->SetWorldScale3D(FVector(bFlipX ? -PresentationScale.X : PresentationScale.X,
		PresentationScale.Y, PresentationScale.Z));
	Component->SetWorldLocation(FVector(0.0f, 0.0f, 0.0f));
	Component->RegisterComponent();

	FNaiwaP4ActiveVfx& Active = ActiveVfx.AddDefaulted_GetRef();
	Active.Component = Component;
	Active.Material = Material;
	Active.EffectId = EffectId;
	Active.LogicalPosition = LogicalPosition;
	Active.FollowEntityId = FollowEntityId;
	Active.bFollowEntity = bFollowEntity;
	Active.bDestroyIfTargetMissing = bDestroyIfTargetMissing;
	Active.bFlipX = bFlipX;
	Active.WorldScale = PresentationScale;
	Active.FrameIndex = 0;
	if (WorldZ >= 0.0f)
	{
		Active.WorldZ = WorldZ;
	}
	else if (const FNaiwaP4EntityState* Entity = FindEntity(FollowEntityId))
	{
		float CenterZ = Entity->HeightZ + Entity->CollisionRadius;
		if (Entity->Kind == ENaiwaP4EntityKind::Player)
		{
			CenterZ = Entity->HeightZ + SpriteHeightUU * CharacterPresentationScale * 0.27f;
		}
		else if (Entity->Kind == ENaiwaP4EntityKind::Projectile)
		{
			CenterZ = Entity->HeightZ + 48.0f;
		}
		else if (Entity->bWaveBoss)
		{
			CenterZ += ((EnemyStaticPreviewFootAnchorYPx / EnemyStaticPreviewFrameSizePx) - 0.5f)
				* NaiTongBossPreviewCanvasHeightUU * CharacterPresentationScale;
		}
		Active.WorldZ = bFollowEntity ? 0.0f : CenterZ;
	}
	else
	{
		Active.WorldZ = 6.0f;
	}
	Component->SetWorldLocation(ResolveVfxWorldLocation(Active));
}

void ANaiwaP4ArtIntegrationPawn::StopVfx(const FName EffectId)
{
	const FNaiwaP4RuntimeVfxSequence* Sequence = VfxSequences.Find(EffectId);
	if (!Sequence)
	{
		return;
	}
	for (int32 Index = ActiveVfx.Num() - 1; Index >= 0; --Index)
	{
		FNaiwaP4ActiveVfx& Active = ActiveVfx[Index];
		if (Active.EffectId == EffectId)
		{
			if (Active.Component)
			{
				Active.Component->DestroyComponent();
			}
			ActiveVfx.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}
}

void ANaiwaP4ArtIntegrationPawn::ProcessVfxEvents()
{
	UWorld* World = GetWorld();
	UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	if (!Arena)
	{
		return;
	}
	const TArray<FNaiwaP4Event>& Events = Arena->GetState().Events;
	if (Events.Num() < LastProcessedVfxEventCount)
	{
		for (FNaiwaP4ActiveVfx& Active : ActiveVfx)
		{
			if (Active.Component)
			{
				Active.Component->DestroyComponent();
			}
		}
		ActiveVfx.Reset();
		PreviousPlayerActionCue = MAX_uint8;
		LastProcessedVfxEventCount = 0;
	}

	for (int32 Index = LastProcessedVfxEventCount; Index < Events.Num(); ++Index)
	{
		const FNaiwaP4Event& Event = Events[Index];
		const FName EventId = Event.EventId;
		if (EventId == TEXT("player_melee_hit") || EventId == TEXT("player_dash_hit")
			|| EventId == TEXT("enemy_damaged") || EventId == TEXT("player_damaged")
			|| EventId == TEXT("boss_charge_hit"))
		{
			ImpactPresentationFreezeSecondsRemaining = FMath::Max(
				ImpactPresentationFreezeSecondsRemaining, 0.045f);
		}
		if (EventId == TEXT("player_projectile_fired"))
		{
			const FNaiwaP4EntityState* Projectile = nullptr;
			double NearestDistanceSquared = TNumericLimits<double>::Max();
			for (const FNaiwaP4EntityState& Entity : Arena->GetState().Entities)
			{
				if (Entity.Kind != ENaiwaP4EntityKind::Projectile || !Entity.bAlive
					|| Entity.ProjectileOwnerEntityId != Event.SourceEntityId)
				{
					continue;
				}
				const double DistanceSquared = (Entity.Position - Event.Position).SizeSquared();
				if (DistanceSquared < NearestDistanceSquared)
				{
					NearestDistanceSquared = DistanceSquared;
					Projectile = &Entity;
				}
			}
			if (Projectile)
			{
				PlayVfx(TEXT("fx_milk_shot"), Projectile->Position, Projectile->EntityId,
					true, true, FVector(3.4f, 2.3f, 1.0f), Projectile->Velocity.X > KINDA_SMALL_NUMBER);
			}
		}
		else if (EventId == TEXT("player_melee_started"))
		{
			const FNaiwaP4EntityState* Player = FindEntity(Event.SourceEntityId);
			FVector2D SwingPosition = Event.Position;
			if (Player)
			{
				SwingPosition.X += Player->bFacingRight ? 54.0 : -54.0;
			}
			PlayVfx(TEXT("fx_melee_swing"), SwingPosition, Event.SourceEntityId,
				false, false, FVector(4.0f, 3.6f, 1.0f), Player && Player->bFacingRight);
		}
		else if (EventId == TEXT("player_dash_started"))
		{
			const FNaiwaP4EntityState* Player = FindEntity(Event.SourceEntityId);
			PlayVfx(TEXT("fx_player_dash_trail"), Event.Position, Event.SourceEntityId,
				true, false, FVector(4.4f, 3.0f, 1.0f), Player && !Player->bFacingRight);
		}
		else if (EventId == TEXT("player_damaged"))
		{
			PlayVfx(TEXT("fx_player_hit"), Event.Position, Event.TargetEntityId,
				false, false, FVector(3.6f, 3.6f, 1.0f));
		}
		else if (EventId == TEXT("player_melee_hit"))
		{
			PlayVfx(TEXT("fx_melee_impact"), Event.Position, Event.TargetEntityId,
				false, false, FVector(3.8f, 3.8f, 1.0f));
		}
		else if (EventId == TEXT("player_dash_hit"))
		{
			PlayVfx(TEXT("fx_dash_impact"), Event.Position, Event.TargetEntityId,
				false, false, FVector(4.1f, 4.1f, 1.0f));
		}
		else if (EventId == TEXT("enemy_damaged"))
		{
			PlayVfx(TEXT("fx_enemy_hit"), Event.Position, Event.TargetEntityId,
				false, false, FVector(3.6f, 3.6f, 1.0f));
		}
		else if (EventId == TEXT("enemy_killed"))
		{
			PlayVfx(TEXT("fx_enemy_death"), Event.Position, Event.TargetEntityId,
				false, false, FVector(4.0f, 4.0f, 1.0f));
		}
		else if (EventId == TEXT("boss_lid_slam_hit") || EventId == TEXT("boss_lid_slam_missed"))
		{
			BossImpactPoseRemainingSeconds = 0.24f;
			const FNaiwaP4EntityState* Boss = FindEntity(Event.SourceEntityId);
			PlayVfx(TEXT("fx_boss_lid_slam"), Boss ? Boss->Position : Event.Position, INDEX_NONE,
				false, false, FVector(5.4f, 3.6f, 1.0f), false, 5.0f);
		}
		else if (EventId == TEXT("boss_killed"))
		{
			BossDeathPoseRemainingSeconds = 0.36f;
		}
		else if (EventId == TEXT("boss_charge_started"))
		{
			const FNaiwaP4EntityState* Boss = FindEntity(Event.SourceEntityId);
			PlayVfx(TEXT("fx_boss_charge_trail"), Boss ? Boss->Position : Event.Position, Event.SourceEntityId,
				true, false, FVector(4.5f, 2.7f, 1.0f), Boss && !Boss->bFacingRight);
		}
		else if (EventId == TEXT("boss_charge_hit") || EventId == TEXT("boss_charge_missed"))
		{
			StopVfx(TEXT("fx_boss_charge_trail"));
			if (EventId == TEXT("boss_charge_hit"))
			{
				PlayVfx(TEXT("fx_dash_impact"), Event.Position, Event.TargetEntityId,
					false, false, FVector(4.4f, 4.4f, 1.0f));
			}
		}
		else if (EventId == TEXT("boss_phase_two"))
		{
			StopVfx(TEXT("fx_boss_charge_trail"));
			PlayVfx(TEXT("fx_boss_phase_two"), Event.Position, Event.SourceEntityId,
				true, false, FVector(5.0f, 5.0f, 1.0f));
		}
	}
	LastProcessedVfxEventCount = Events.Num();
}

void ANaiwaP4ArtIntegrationPawn::SyncActionVfx()
{
	const FNaiwaP4EntityState* Player = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (const UNaiwaP4ArenaSubsystem* Arena = World->GetSubsystem<UNaiwaP4ArenaSubsystem>())
		{
			Player = Arena->GetState().Entities.FindByPredicate([](const FNaiwaP4EntityState& Entity)
			{
				return Entity.Kind == ENaiwaP4EntityKind::Player;
			});
		}
	}
	if (!Player || !Player->bAlive)
	{
		StopVfx(TEXT("fx_defend_guard"));
		PreviousPlayerActionCue = MAX_uint8;
		return;
	}

	const uint8 CurrentCue = static_cast<uint8>(Player->ActionCue);
	const bool bEnteredCue = PreviousPlayerActionCue != CurrentCue;
	if (bEnteredCue && Player->ActionCue == ENaiwaP4ActionCue::Laugh)
	{
		PlayVfx(TEXT("fx_laugh_burst"), Player->Position, Player->EntityId,
			false, false, FVector(4.0f, 4.0f, 1.0f));
	}
	if (Player->ActionCue == ENaiwaP4ActionCue::Defend)
	{
		const bool bGuardActive = ActiveVfx.ContainsByPredicate([](const FNaiwaP4ActiveVfx& Active)
		{
			return Active.EffectId == TEXT("fx_defend_guard");
		});
		if (!bGuardActive)
		{
			PlayVfx(TEXT("fx_defend_guard"), Player->Position, Player->EntityId,
				true, false, FVector(4.0f, 4.0f, 1.0f));
		}
	}
	else
	{
		StopVfx(TEXT("fx_defend_guard"));
	}
	PreviousPlayerActionCue = CurrentCue;
}

void ANaiwaP4ArtIntegrationPawn::SyncActiveVfx(const float DeltaSeconds)
{
	for (int32 Index = ActiveVfx.Num() - 1; Index >= 0; --Index)
	{
		FNaiwaP4ActiveVfx& Active = ActiveVfx[Index];
		const FNaiwaP4RuntimeVfxSequence* Sequence = VfxSequences.Find(Active.EffectId);
		if (!Active.Component || !Active.Material || !Sequence || Sequence->Frames.IsEmpty())
		{
			if (Active.Component)
			{
				Active.Component->DestroyComponent();
			}
			ActiveVfx.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}

		const FNaiwaP4EntityState* FollowTarget = Active.bFollowEntity
			? FindEntity(Active.FollowEntityId)
			: nullptr;
		if (Active.bDestroyIfTargetMissing && (!FollowTarget || !FollowTarget->bAlive))
		{
			Active.Component->DestroyComponent();
			ActiveVfx.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}
		if (Active.EffectId == TEXT("fx_milk_shot") && FollowTarget
			&& FMath::Abs(FollowTarget->Velocity.X) > KINDA_SMALL_NUMBER)
		{
			// The W4 source art's +X tip maps to screen-left on this yaw-90 presentation plane.
			// Flip it when logical X velocity moves the projectile toward screen-right.
			const bool bFlipForScreenDirection = FollowTarget->Velocity.X > 0.0;
			if (Active.bFlipX != bFlipForScreenDirection)
			{
				Active.bFlipX = bFlipForScreenDirection;
				Active.Component->SetWorldScale3D(FVector(
					Active.bFlipX ? -Active.WorldScale.X : Active.WorldScale.X,
					Active.WorldScale.Y, Active.WorldScale.Z));
			}
		}

		Active.FrameElapsedSeconds += FMath::Max(DeltaSeconds, 0.0f);
		bool bFinished = false;
		while (Active.FrameElapsedSeconds >= Sequence->Frames[Active.FrameIndex].DurationSeconds)
		{
			Active.FrameElapsedSeconds -= Sequence->Frames[Active.FrameIndex].DurationSeconds;
			if (Active.FrameIndex + 1 >= Sequence->Frames.Num())
			{
				if (!Sequence->bLoop)
				{
					bFinished = true;
					break;
				}
				Active.FrameIndex = 0;
			}
			else
			{
				++Active.FrameIndex;
			}
			Active.Material->SetTextureParameterValue(FrameTextureParameter,
				Sequence->Frames[Active.FrameIndex].Texture);
		}
		if (bFinished)
		{
			Active.Component->DestroyComponent();
			ActiveVfx.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}
		Active.Component->SetWorldLocation(ResolveVfxWorldLocation(Active));
	}
}

void ANaiwaP4ArtIntegrationPawn::SyncFromSimulation()
{
	UWorld* World = GetWorld();
	UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	const UNaiwaP4Settings* Settings = GetDefault<UNaiwaP4Settings>();
	if (!Arena || !FrameCatalog || !Settings)
	{
		return;
	}

	const FNaiwaP4EntityState* Player = Arena->GetState().Entities.FindByPredicate([](const FNaiwaP4EntityState& Entity)
	{
		return Entity.Kind == ENaiwaP4EntityKind::Player;
	});
	if (!Player || !Player->bAlive)
	{
		return;
	}

	// Logical X increases to screen-right; at the calibrated camera's yaw 90 degrees,
	// world X projects to screen-left, so mirror X only in this read-only presentation mapping.
	const FVector2D RenderPosition = Arena->GetSmoothedPresentationPosition(Player->EntityId, Player->Position);
	const float WorldX = Settings->LogicalArenaWidth - static_cast<float>(RenderPosition.X);
	SetActorLocation(FVector(WorldX, RenderPosition.Y, Player->HeightZ));
	ApplyFrame(ResolveFrameId(*Player), Player->bFacingRight);
}

FName ANaiwaP4ArtIntegrationPawn::ResolveFrameId(const FNaiwaP4EntityState& Player) const
{
	FString Action;
	FString Pose;
	switch (Player.ActionCue)
	{
	case ENaiwaP4ActionCue::Move:
		Action = TEXT("move");
		Pose = ((Player.ActionTick / MoveFrameTicks) % 2 == 0) ? TEXT("step_a") : TEXT("step_b");
		break;
	case ENaiwaP4ActionCue::Attack:
		if (Player.MeleeHitTicksRemaining > 0)
		{
			Action = TEXT("attack");
			Pose = TEXT("windup");
		}
		else if (Player.MeleeContactTicksRemaining > 0)
		{
			Action = TEXT("attack");
			Pose = TEXT("contact");
		}
		else
		{
			// A dedicated recovery pose will replace this neutral fallback when art lands.
			Action = TEXT("idle");
			Pose = TEXT("neutral");
		}
		break;
	case ENaiwaP4ActionCue::Dash:
		Action = TEXT("dash");
		Pose = (Player.ActionTick < TwoFrameActionTicks) ? TEXT("launch") : TEXT("stride");
		break;
	case ENaiwaP4ActionCue::Defend:
		Action = TEXT("defend");
		Pose = TEXT("guard");
		break;
	case ENaiwaP4ActionCue::Hit:
		Action = TEXT("hit");
		Pose = TEXT("recoil");
		break;
	case ENaiwaP4ActionCue::Laugh:
		Action = TEXT("laugh");
		Pose = ((Player.ActionTick / TwoFrameActionTicks) % 2 == 0) ? TEXT("open") : TEXT("recover");
		break;
	case ENaiwaP4ActionCue::Death:
	case ENaiwaP4ActionCue::Idle:
	default:
		Action = TEXT("idle");
		Pose = TEXT("neutral");
		break;
	}

	FString View = TEXT("front");
	if (Player.Facing == ENaiwaP4Facing::Back)
	{
		View = TEXT("back");
	}
	else if (Player.Facing == ENaiwaP4Facing::Side)
	{
		View = TEXT("side_right");
	}
	return FName(*FString::Printf(TEXT("%s_%s_%s"), *Action, *Pose, *View));
}

void ANaiwaP4ArtIntegrationPawn::ApplyFrame(const FName FrameId, const bool bFacingRight)
{
	if (!SpriteMaterial || !FrameCatalog)
	{
		return;
	}

	const FNaiwaP4A1FrameRow* Row = FrameCatalog->FindRow<FNaiwaP4A1FrameRow>(FrameId, TEXT("P4 A1 frame selection"), false);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("P4 A1 frame catalog has no entry for '%s'."), *FrameId.ToString());
		return;
	}

	UTexture2D* Texture = LoadedFrames.FindRef(FrameId);
	if (!Texture)
	{
		Texture = Row->FrameTexture.LoadSynchronous();
	}
	if (!Texture)
	{
		return;
	}
	if (CurrentFrameId != FrameId)
	{
		SpriteMaterial->SetTextureParameterValue(FrameTextureParameter, Texture);
		CurrentFrameId = FrameId;
	}

	const float AnchorY = static_cast<float>(Row->FootAnchorYPx);
	const float AnchorCenterZ = ((AnchorY / 724.0f) - 0.5f)
		* SpriteHeightUU * CharacterPresentationScale;
	SpritePlane->SetRelativeLocation(FVector(0.0f, 0.0f, AnchorCenterZ));
	// Screen-right maps to world -X at the calibrated yaw=90 camera, while this
	// front-facing sprite plane's local +X maps to world +X. Invert the mirror sign.
	SpritePlane->SetRelativeScale3D(FVector(
		bFacingRight ? -SpriteWidthScale * CharacterPresentationScale : SpriteWidthScale * CharacterPresentationScale,
		SpriteHeightUU * CharacterPresentationScale / 100.0f,
		1.0f));
}
