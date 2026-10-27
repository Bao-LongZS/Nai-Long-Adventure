#include "NaiwaP3M2WorldPresentation.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor EggGold = FLinearColor::FromSRGBColor(FColor(255, 211, 77));
	const FLinearColor PortalViolet = FLinearColor::FromSRGBColor(FColor(153, 145, 198));
	const FLinearColor PortalCyan = FLinearColor::FromSRGBColor(FColor(87, 221, 225));
const FLinearColor SplashWarning = FLinearColor::FromSRGBColor(FColor(236, 157, 83));
const FLinearColor PatrolOuter = FLinearColor::FromSRGBColor(FColor(70, 119, 114));
const FLinearColor PatrolInner = FLinearColor::FromSRGBColor(FColor(112, 170, 157));
const FLinearColor PortraitShadow = FLinearColor::FromSRGBColor(FColor(23, 31, 39));

UTexture2D* LoadPortraitAsset(const TCHAR* AssetName)
{
	const FString AssetPath = FString::Printf(
		TEXT("/Game/Naiwa/P3/Portraits/Processed/%s.%s"), AssetName, AssetName);
	return LoadObject<UTexture2D>(nullptr, *AssetPath);
}

UTexture2D* LoadSpriteAsset(const TCHAR* AssetName)
{
	const FString AssetPath = FString::Printf(
		TEXT("/Game/Naiwa/P3/Sprites/%s.%s"), AssetName, AssetName);
	return LoadObject<UTexture2D>(nullptr, *AssetPath);
}

UMaterialInterface* LoadPortraitPreviewMaterial(const UTexture2D* Texture)
{
	if (!Texture) return nullptr;
	const FString PortraitName = Texture->GetName();
	const FString MaterialPath = FString::Printf(
		TEXT("/Game/Naiwa/P3/Materials/M_P3Sprite_%s.M_P3Sprite_%s"), *PortraitName, *PortraitName);
	return LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
}
}

ANaiwaP3M2WorldPresentation::ANaiwaP3M2WorldPresentation()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (CubeFinder.Succeeded()) CubeMesh = CubeFinder.Object;
	if (SphereFinder.Succeeded()) SphereMesh = SphereFinder.Object;
	if (CylinderFinder.Succeeded()) CylinderMesh = CylinderFinder.Object;
	if (ConeFinder.Succeeded()) ConeMesh = ConeFinder.Object;
	if (PlaneFinder.Succeeded()) PlaneMesh = PlaneFinder.Object;
}

void ANaiwaP3M2WorldPresentation::Build(const FNaiwaP3M2SeededLevel& Level)
{
	GridWidth = Level.GridWidth;
	GridHeight = Level.GridHeight;
	EggVisuals.Reset();
	MechanicVisuals.Reset();
	ExitComponents.Reset();
	ExitDoorComponent = INDEX_NONE;
	AddPatrolOutline(Level.OuterPatrolMin, Level.OuterPatrolMax, PatrolOuter);
	AddPatrolOutline(Level.InnerPatrolMin, Level.InnerPatrolMax, PatrolInner);

	for (const FIntPoint& EggCell : Level.GoldenEggCells)
	{
		TArray<int32>& EggParts = EggVisuals.FindOrAdd(EggCell);
		EggParts.Add(AddSprite(EggCell, TEXT("SPR_GoldenEgg_A"), 10.0f, 74.0f, FLinearColor::White));
		for (const int32 Part : EggParts) SetComponentVisible(Part, false);
	}

	const FIntPoint ExitCell = Level.ExitCell;
	ExitComponents.Add(AddSprite(ExitCell, TEXT("SPR_Tile_Exit_Locked_A"), 5.0f, 88.0f,
		FLinearColor::White));
	ExitDoorComponent = AddSprite(ExitCell, TEXT("SPR_Tile_Exit_Open_A"), 6.0f, 88.0f,
		FLinearColor::White, false);
	for (const int32 Part : ExitComponents) SetComponentVisible(Part, false);
	SetComponentVisible(ExitDoorComponent, false);

	for (const FNaiwaP3M2MechanicPlacement& Placement : Level.Mechanics)
	{
		AddMechanicVisual(Placement);
	}
}

void ANaiwaP3M2WorldPresentation::BuildEditorCharacterPreview(const FIntPoint& PlayerCell,
	const TArray<FNaiwaP3M2EnemyPlacement>& Enemies)
{
	AddPortrait(PlayerCell, LoadPortraitAsset(TEXT("SPR_Player_Neutral_A")),
		FLinearColor::FromSRGBColor(FColor(78, 208, 182)));
	for (const FNaiwaP3M2EnemyPlacement& Enemy : Enemies)
	{
		UTexture2D* Texture = nullptr;
		switch (Enemy.Kind)
		{
		case ENaiwaP3M2EnemyKind::NaiCha:
			Texture = LoadPortraitAsset(TEXT("SPR_Naicha_Chase_A"));
			break;
		case ENaiwaP3M2EnemyKind::NaiSha:
			Texture = LoadPortraitAsset(TEXT("SPR_Naisha_Rage_A"));
			break;
		case ENaiwaP3M2EnemyKind::NaiHao:
			Texture = LoadPortraitAsset(TEXT("SPR_Naihao_Neutral_A"));
			break;
		case ENaiwaP3M2EnemyKind::NaiYu:
			Texture = LoadPortraitAsset(TEXT("SPR_Naiyu_Neutral_A"));
			break;
		default:
			break;
		}
		if (Texture)
		{
			AddPortrait(Enemy.Cell, Texture, FLinearColor::FromSRGBColor(FColor(223, 151, 78)));
		}
		else AddCellFrame(Enemy.Cell, FLinearColor::FromSRGBColor(FColor(90, 172, 205)));
	}
}

void ANaiwaP3M2WorldPresentation::AddCellFrame(const FIntPoint& Cell, const FLinearColor& Color)
{
	const FVector Center = GridToWorld(Cell, 4.0f);
	AddCube(Center + FVector(0.0f, -44.0f, 0.0f), FVector(0.40f, 0.025f, 0.008f), Color);
	AddCube(Center + FVector(0.0f, 44.0f, 0.0f), FVector(0.40f, 0.025f, 0.008f), Color);
	AddCube(Center + FVector(-44.0f, 0.0f, 0.0f), FVector(0.025f, 0.40f, 0.008f), Color);
	AddCube(Center + FVector(44.0f, 0.0f, 0.0f), FVector(0.025f, 0.40f, 0.008f), Color);
}

int32 ANaiwaP3M2WorldPresentation::AddPortrait(const FIntPoint& Cell, UTexture2D* Texture,
	const FLinearColor& FrameColor)
{
	const FVector Center = GridToWorld(Cell);
	AddCellFrame(Cell, FrameColor);
	AddCylinder(Center + FVector(0.0f, 0.0f, 2.0f), FVector(0.31f, 0.19f, 0.012f), PortraitShadow);
	if (!Texture || !PlaneMesh)
	{
		return INDEX_NONE;
	}
	const float Height = 76.0f;
	const float Aspect = static_cast<float>(Texture->GetSizeX()) /
		FMath::Max(static_cast<float>(Texture->GetSizeY()), 1.0f);
	const float Width = FMath::Min(72.0f, Height * Aspect);
	const int32 ComponentIndex = AddMesh(PlaneMesh,
		Center + FVector(0.0f, 0.0f, 14.0f), FVector(Width / 100.0f, Height / 100.0f, 1.0f),
		FLinearColor::White);
	if (!Components.IsValidIndex(ComponentIndex)) return INDEX_NONE;

	UMaterialInterface* PreviewMaterial = LoadPortraitPreviewMaterial(Texture);
	if (PreviewMaterial && Components.IsValidIndex(ComponentIndex))
	{
		Components[ComponentIndex]->SetMaterial(0, PreviewMaterial);
		Materials[ComponentIndex] = nullptr;
	}
	else
	{
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Naiwa/P3/Materials/M_P3Portrait.M_P3Portrait"));
		UMaterialInstanceDynamic* Material = Parent ? UMaterialInstanceDynamic::Create(Parent, this) : nullptr;
		if (Material)
		{
			Material->SetTextureParameterValue(TEXT("Portrait"), Texture);
			Components[ComponentIndex]->SetMaterial(0, Material);
			Materials[ComponentIndex] = Material;
		}
	}
	return ComponentIndex;
}

int32 ANaiwaP3M2WorldPresentation::AddSprite(const FIntPoint& Cell, const TCHAR* AssetName,
	const float Z, const float MaxSize, const FLinearColor& Tint, const bool bVisible)
{
	UTexture2D* Texture = LoadSpriteAsset(AssetName);
	if (!Texture || !PlaneMesh) return INDEX_NONE;
	const float Aspect = static_cast<float>(Texture->GetSizeX()) /
		FMath::Max(static_cast<float>(Texture->GetSizeY()), 1.0f);
	const float Width = MaxSize * FMath::Min(Aspect, 1.0f);
	const float Height = MaxSize * FMath::Min(1.0f / FMath::Max(Aspect, KINDA_SMALL_NUMBER), 1.0f);
	const int32 ComponentIndex = AddMesh(PlaneMesh, GridToWorld(Cell, Z),
		FVector(Width / 100.0f, Height / 100.0f, 1.0f), Tint);
	if (!Components.IsValidIndex(ComponentIndex) || !Components[ComponentIndex]) return INDEX_NONE;

	const bool bRuntimeWorld = GetWorld() && GetWorld()->IsGameWorld();
	UMaterialInterface* PreviewMaterial = bRuntimeWorld ? nullptr : LoadPortraitPreviewMaterial(Texture);
	if (!bRuntimeWorld && PreviewMaterial)
	{
		Components[ComponentIndex]->SetMaterial(0, PreviewMaterial);
		Materials[ComponentIndex] = nullptr;
	}
	else
	{
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Naiwa/P3/Materials/M_P3Portrait.M_P3Portrait"));
		UMaterialInstanceDynamic* Material = Parent ? UMaterialInstanceDynamic::Create(Parent, this) : nullptr;
		if (Material)
		{
			Material->SetTextureParameterValue(TEXT("Portrait"), Texture);
			Material->SetVectorParameterValue(TEXT("Color"), Tint);
			Components[ComponentIndex]->SetMaterial(0, Material);
			Materials[ComponentIndex] = Material;
		}
	}
	SetComponentVisible(ComponentIndex, bVisible);
	return ComponentIndex;
}

void ANaiwaP3M2WorldPresentation::AddPatrolOutline(const FIntPoint& MinCell,
	const FIntPoint& MaxCell, const FLinearColor& Color)
{
	const FVector MinCenter = GridToWorld(MinCell, 2.5f);
	const FVector MaxCenter = GridToWorld(MaxCell, 2.5f);
	const FVector NorthWest = MinCenter + FVector(-50.0f, -50.0f, 0.0f);
	const FVector NorthEast = FVector(MaxCenter.X + 50.0f, MinCenter.Y - 50.0f, 2.5f);
	const FVector SouthEast = MaxCenter + FVector(50.0f, 50.0f, 0.0f);
	const FVector SouthWest = FVector(MinCenter.X - 50.0f, MaxCenter.Y + 50.0f, 2.5f);
	const auto AddDashedEdge = [this, &Color](const FVector& Start, const FVector& End)
	{
		const FVector Delta = End - Start;
		const float Length = Delta.Size2D();
		const bool bHorizontal = FMath::Abs(Delta.X) >= FMath::Abs(Delta.Y);
		constexpr float DashLength = 34.0f;
		constexpr float DashGap = 30.0f;
		for (float Distance = 0.0f; Distance < Length; Distance += DashLength + DashGap)
		{
			const float SegmentLength = FMath::Min(DashLength, Length - Distance);
			const FVector Center = Start + Delta * ((Distance + SegmentLength * 0.5f) / Length);
			const FVector Scale = bHorizontal ? FVector(SegmentLength / 100.0f, 0.040f, 0.007f) :
				FVector(0.040f, SegmentLength / 100.0f, 0.007f);
			AddCube(Center, Scale, Color);
		}
	};
	AddDashedEdge(NorthWest, NorthEast);
	AddDashedEdge(NorthEast, SouthEast);
	AddDashedEdge(SouthEast, SouthWest);
	AddDashedEdge(SouthWest, NorthWest);
}

void ANaiwaP3M2WorldPresentation::Sync(const FNaiwaP3M2LevelSession& Session,
	const FIntPoint& PlayerCell, const TArray<FIntPoint>& EnemyCells,
	const TArray<uint8>& EnemyAlive)
{
	const FNaiwaP3M2SeededLevel& Level = Session.GetLevel();
	const FNaiwaP3FogOfWar& Fog = Session.GetFog();
	for (const FIntPoint& EggCell : Level.GoldenEggCells)
	{
		if (!EggVisuals.Contains(EggCell))
		{
			TArray<int32>& Parts = EggVisuals.FindOrAdd(EggCell);
			Parts.Add(AddSprite(EggCell, TEXT("SPR_GoldenEgg_A"), 10.0f, 74.0f, FLinearColor::White));
		}
	}
	for (const TPair<FIntPoint, TArray<int32>>& Egg : EggVisuals)
	{
		const bool bStillInWorld = Level.GoldenEggCells.Contains(Egg.Key);
		const bool bVisible = bStillInWorld && Fog.IsVisible(Egg.Key);
		for (const int32 Part : Egg.Value)
		{
			SetComponentBrightness(Part, Fog.IsVisible(Egg.Key) ? 1.0f : 0.56f);
			SetComponentVisible(Part, bVisible);
		}
	}

	for (int32 Y = 0; Y < Level.GridHeight; ++Y)
	{
		for (int32 X = 0; X < Level.GridWidth; ++X)
		{
			const FIntPoint Cell(X, Y);
			ENaiwaP3LootType Loot;
			if (Session.TryGetItemAt(Cell, Loot))
			{
				if (!ItemVisuals.Contains(Cell)) SetItemAt(Cell, Loot);
				const TArray<int32>& Parts = ItemVisuals.FindChecked(Cell);
				for (const int32 Part : Parts)
				{
					SetComponentBrightness(Part, Fog.IsVisible(Cell) ? 1.0f : 0.56f);
					SetComponentVisible(Part, Fog.IsVisible(Cell));
				}
			}
		}
	}
	TArray<FIntPoint> ExpiredItemCells;
	for (const TPair<FIntPoint, TArray<int32>>& Item : ItemVisuals)
	{
		ENaiwaP3LootType Loot;
		if (!Session.TryGetItemAt(Item.Key, Loot)) ExpiredItemCells.Add(Item.Key);
	}
	for (const FIntPoint& Cell : ExpiredItemCells) RemoveItemAt(Cell);

	for (FMechanicVisual& Visual : MechanicVisuals)
	{
		const ENaiwaP3FogCellState FogState = Fog.GetCellState(Visual.Cell);
		const bool bDiscovered = FogState != ENaiwaP3FogCellState::Unseen;
		const float Brightness = FogState == ENaiwaP3FogCellState::Visible ? 1.0f : 0.56f;
		for (const int32 Part : Visual.Components)
		{
			SetComponentBrightness(Part, Brightness);
			SetComponentVisible(Part, bDiscovered);
		}
		for (const int32 Part : Visual.ActiveComponents)
		{
			SetComponentBrightness(Part, Brightness);
			SetComponentVisible(Part, bDiscovered);
		}
		if (Visual.StateComponent != INDEX_NONE)
		{
			SetComponentBrightness(Visual.StateComponent, Brightness);
			SetComponentVisible(Visual.StateComponent, bDiscovered);
		}
		if (!bDiscovered) continue;

		if (Visual.Kind == ENaiwaP3M2MechanicKind::PressurePlate)
		{
			bool bOccupied = PlayerCell == Visual.Cell;
			for (int32 EnemyIndex = 0; EnemyIndex < EnemyCells.Num(); ++EnemyIndex)
			{
				bOccupied |= EnemyAlive.IsValidIndex(EnemyIndex) && EnemyAlive[EnemyIndex] != 0 &&
					EnemyCells[EnemyIndex] == Visual.Cell;
			}
			for (const int32 Part : Visual.Components) SetComponentVisible(Part, !bOccupied);
			for (const int32 Part : Visual.ActiveComponents) SetComponentVisible(Part, bOccupied);
		}
		else if (Visual.Kind == ENaiwaP3M2MechanicKind::VaultGate)
		{
			const bool bGateOpen = Session.GetMechanics().IsGateOpen(Visual.Cell, Session.GetCurrentTick());
			for (const int32 Part : Visual.Components) SetComponentVisible(Part, bDiscovered && !bGateOpen);
		}
		else if (Visual.Kind == ENaiwaP3M2MechanicKind::Spike)
		{
			const bool bActive = Session.GetMechanics().IsSpikeActive(Visual.Cell, Session.GetCurrentTick());
			for (const int32 Part : Visual.Components) SetComponentVisible(Part, bDiscovered && !bActive);
			for (const int32 Part : Visual.ActiveComponents) SetComponentVisible(Part, bDiscovered && bActive);
		}
		else if (Visual.Kind == ENaiwaP3M2MechanicKind::CrackedFloor)
		{
			const bool bHole = Session.GetMechanics().IsHole(Visual.Cell, Session.GetCurrentTick());
			for (const int32 Part : Visual.Components) SetComponentVisible(Part, bDiscovered && !bHole);
			SetComponentVisible(Visual.StateComponent, bDiscovered && bHole);
		}
	}

	const bool bExitVisible = Fog.IsVisible(Level.ExitCell) || Fog.IsRemembered(Level.ExitCell);
	const float ExitBrightness = Fog.IsVisible(Level.ExitCell) ? 1.0f : 0.56f;
	const bool bExitUnlocked = Session.GetRemainingEggCount() == 0;
	for (const int32 Part : ExitComponents)
	{
		SetComponentBrightness(Part, ExitBrightness);
		SetComponentVisible(Part, bExitVisible && !bExitUnlocked);
	}
	SetComponentBrightness(ExitDoorComponent, ExitBrightness);
	SetComponentVisible(ExitDoorComponent, bExitVisible && bExitUnlocked);
}

void ANaiwaP3M2WorldPresentation::SetSlowSplash(const FIntPoint& CenterCell,
	const bool bActive, const bool bImpacted, const FNaiwaP3FogOfWar& Fog)
{
	if (SlowSplashComponents.Num() != 5)
	{
		SlowSplashComponents.Reset();
		for (int32 Index = 0; Index < 5; ++Index)
		{
			SlowSplashComponents.Add(AddCylinder(FVector::ZeroVector,
				FVector(0.47f, 0.47f, 0.035f), SplashWarning));
		}
	}
	if (!bActive)
	{
		for (const int32 Component : SlowSplashComponents) SetComponentVisible(Component, false);
		return;
	}
	static const FIntPoint Offsets[] = {
		FIntPoint::ZeroValue, FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0)
	};
	const FLinearColor Color = bImpacted ? FLinearColor::FromSRGBColor(FColor(255, 82, 54)) : SplashWarning;
	for (int32 Index = 0; Index < SlowSplashComponents.Num(); ++Index)
	{
		const FIntPoint Cell = CenterCell + Offsets[Index];
		const int32 ComponentIndex = SlowSplashComponents[Index];
		if (!Components.IsValidIndex(ComponentIndex) || !Components[ComponentIndex]) continue;
		Components[ComponentIndex]->SetRelativeLocation(GridToWorld(Cell, 6.0f));
		SetComponentColor(ComponentIndex, Color);
		SetComponentVisible(ComponentIndex, Fog.IsVisible(Cell));
	}
}

void ANaiwaP3M2WorldPresentation::MarkEggCollected(const FIntPoint& Cell)
{
	if (const TArray<int32>* Parts = EggVisuals.Find(Cell))
	{
		for (const int32 Part : *Parts) SetComponentVisible(Part, false);
	}
}

void ANaiwaP3M2WorldPresentation::SetItemAt(const FIntPoint& Cell, const ENaiwaP3LootType Loot)
{
	if (ItemVisuals.Contains(Cell)) return;
	TArray<int32>& Parts = ItemVisuals.FindOrAdd(Cell);
	const TCHAR* SpriteName = nullptr;
	switch (Loot)
	{
	case ENaiwaP3LootType::BombCapacity: SpriteName = TEXT("SPR_Loot_BombCapacity_A"); break;
	case ENaiwaP3LootType::BombRange: SpriteName = TEXT("SPR_Loot_BombRange_A"); break;
	case ENaiwaP3LootType::ChickenJump: SpriteName = TEXT("SPR_Loot_ChickenJump_A"); break;
	case ENaiwaP3LootType::SpeedBoost: SpriteName = TEXT("SPR_Loot_SpeedBoost_A"); break;
	case ENaiwaP3LootType::Shield: SpriteName = TEXT("SPR_Loot_Shield_A"); break;
	case ENaiwaP3LootType::Xray: SpriteName = TEXT("SPR_Loot_Xray_A"); break;
	case ENaiwaP3LootType::ChaosReverse: SpriteName = TEXT("SPR_Loot_ChaosReverse_A"); break;
	default: break;
	}
	Parts.Add(AddCylinder(GridToWorld(Cell, 6.0f), FVector(0.34f, 0.34f, 0.035f),
		FLinearColor::FromSRGBColor(FColor(55, 69, 79))));
	Parts.Add(AddSprite(Cell, TEXT("SPR_Tile_BrokenCrate_A"), 7.0f, 84.0f, FLinearColor::White));
	const int32 Icon = SpriteName ? AddSprite(Cell, SpriteName, 26.0f, 74.0f, FLinearColor::White) : INDEX_NONE;
	if (Icon != INDEX_NONE) Parts.Add(Icon);
	else Parts.Add(AddSphere(GridToWorld(Cell, 30.0f), FVector(0.20f, 0.20f, 0.24f), LootColor(Loot)));
	for (const int32 Part : Parts) SetComponentVisible(Part, true);
}

void ANaiwaP3M2WorldPresentation::RemoveItemAt(const FIntPoint& Cell)
{
	if (TArray<int32>* Parts = ItemVisuals.Find(Cell))
	{
		for (const int32 Part : *Parts) SetComponentVisible(Part, false);
		ItemVisuals.Remove(Cell);
	}
}

void ANaiwaP3M2WorldPresentation::AddMechanicVisual(const FNaiwaP3M2MechanicPlacement& Placement)
{
	FMechanicVisual& Visual = MechanicVisuals.AddDefaulted_GetRef();
	Visual.Cell = Placement.Cell;
	Visual.Kind = Placement.Kind;
	Visual.Variant = Placement.Variant;

	switch (Placement.Kind)
	{
	case ENaiwaP3M2MechanicKind::PressurePlate:
		Visual.Components.Add(AddSprite(Placement.Cell, TEXT("SPR_Tile_PressurePlate_Idle_A"),
			5.0f, 88.0f, FLinearColor::White));
		Visual.ActiveComponents.Add(AddSprite(Placement.Cell, TEXT("SPR_Tile_PressurePlate_Active_A"),
			6.0f, 88.0f, FLinearColor::White, false));
		break;
	case ENaiwaP3M2MechanicKind::VaultGate:
		Visual.Components.Add(AddSprite(Placement.Cell, TEXT("SPR_Tile_VaultGate_A"),
			5.0f, 88.0f, FLinearColor::White));
		break;
	case ENaiwaP3M2MechanicKind::Spike:
		Visual.Components.Add(AddSprite(Placement.Cell, TEXT("SPR_Tile_Spike_Inactive_A"),
			5.0f, 88.0f, FLinearColor::White));
		Visual.ActiveComponents.Add(AddSprite(Placement.Cell, TEXT("SPR_Tile_Spike_Active_A"),
			6.0f, 88.0f, FLinearColor::White, false));
		break;
	case ENaiwaP3M2MechanicKind::Conveyor:
		Visual.Components.Add(AddSprite(Placement.Cell, Placement.Variant < 0 ?
			TEXT("SPR_Tile_Conveyor_Left_A") : TEXT("SPR_Tile_Conveyor_Right_A"),
			5.0f, 88.0f, FLinearColor::White));
		break;
	case ENaiwaP3M2MechanicKind::CrackedFloor:
		Visual.Components.Add(AddSprite(Placement.Cell, TEXT("SPR_Tile_CrackedFloor_A"),
			5.0f, 88.0f, FLinearColor::White));
		Visual.StateComponent = AddSprite(Placement.Cell, TEXT("SPR_Tile_CollapsedFloor_A"),
			6.0f, 88.0f, FLinearColor::White, false);
		SetComponentVisible(Visual.StateComponent, false);
		break;
	case ENaiwaP3M2MechanicKind::Portal:
		{
			const FLinearColor PairColor = Placement.Variant == 0 ? PortalCyan : PortalViolet;
			Visual.Components.Add(AddCylinder(GridToWorld(Placement.Cell, 2.0f),
				FVector(0.48f, 0.48f, 0.045f), PairColor));
			Visual.Components.Add(AddSprite(Placement.Cell, TEXT("SPR_Tile_Portal_A"),
				5.0f, 88.0f, FLinearColor::White));
		}
		break;
	default:
		break;
	}
}

int32 ANaiwaP3M2WorldPresentation::AddMesh(UStaticMesh* Mesh, const FVector& Position,
	const FVector& Scale, const FLinearColor& Color, const float Yaw)
{
	if (!Mesh) return INDEX_NONE;
	const FName ComponentName = MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), TEXT("M2WorldVisual"));
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, ComponentName);
	if (!Component) return INDEX_NONE;
	AddInstanceComponent(Component);
	Component->SetupAttachment(SceneRoot);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh);
	Component->SetRelativeLocation(Position);
	Component->SetRelativeRotation(FRotator(0.0f, Yaw, 0.0f));
	Component->SetRelativeScale3D(Scale);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCastShadow(false);
	UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Naiwa/P3/Materials/M_P3M1FlatColor.M_P3M1FlatColor"));
	UMaterialInstanceDynamic* Material = ParentMaterial ? UMaterialInstanceDynamic::Create(ParentMaterial, this) : nullptr;
	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Component->SetMaterial(0, Material);
	}
	Component->RegisterComponent();
	const int32 ComponentIndex = Components.Add(Component);
	Materials.Add(Material);
	ComponentColors.Add(Color);
	ComponentBrightness.Add(1.0f);
	return ComponentIndex;
}

int32 ANaiwaP3M2WorldPresentation::AddCube(const FVector& Position, const FVector& Scale,
	const FLinearColor& Color, const float Yaw)
{
	return AddMesh(CubeMesh, Position, Scale, Color, Yaw);
}

int32 ANaiwaP3M2WorldPresentation::AddSphere(const FVector& Position, const FVector& Scale,
	const FLinearColor& Color)
{
	return AddMesh(SphereMesh, Position, Scale, Color);
}

int32 ANaiwaP3M2WorldPresentation::AddCylinder(const FVector& Position, const FVector& Scale,
	const FLinearColor& Color)
{
	return AddMesh(CylinderMesh, Position, Scale, Color);
}

int32 ANaiwaP3M2WorldPresentation::AddCone(const FVector& Position, const FVector& Scale,
	const FLinearColor& Color)
{
	return AddMesh(ConeMesh, Position, Scale, Color);
}

void ANaiwaP3M2WorldPresentation::SetComponentVisible(const int32 ComponentIndex, const bool bVisible)
{
	if (Components.IsValidIndex(ComponentIndex) && Components[ComponentIndex])
	{
		Components[ComponentIndex]->SetVisibility(bVisible, true);
	}
}

void ANaiwaP3M2WorldPresentation::SetComponentColor(const int32 ComponentIndex, const FLinearColor& Color)
{
	if (Materials.IsValidIndex(ComponentIndex) && Materials[ComponentIndex])
	{
		if (ComponentColors.IsValidIndex(ComponentIndex)) ComponentColors[ComponentIndex] = Color;
		const float Brightness = ComponentBrightness.IsValidIndex(ComponentIndex) ? ComponentBrightness[ComponentIndex] : 1.0f;
		Materials[ComponentIndex]->SetVectorParameterValue(TEXT("Color"), Color * Brightness);
	}
}

void ANaiwaP3M2WorldPresentation::SetComponentBrightness(const int32 ComponentIndex,
	const float Brightness)
{
	if (!Materials.IsValidIndex(ComponentIndex) || !Materials[ComponentIndex] ||
		!ComponentColors.IsValidIndex(ComponentIndex) || !ComponentBrightness.IsValidIndex(ComponentIndex))
	{
		return;
	}
	ComponentBrightness[ComponentIndex] = FMath::Clamp(Brightness, 0.0f, 1.0f);
	Materials[ComponentIndex]->SetVectorParameterValue(TEXT("Color"),
		ComponentColors[ComponentIndex] * ComponentBrightness[ComponentIndex]);
}

FVector ANaiwaP3M2WorldPresentation::GridToWorld(const FIntPoint& Cell, const float Z) const
{
	return FVector((Cell.X - (GridWidth - 1) * 0.5f) * 100.0f,
		(Cell.Y - (GridHeight - 1) * 0.5f) * 100.0f, Z);
}

FLinearColor ANaiwaP3M2WorldPresentation::LootColor(const ENaiwaP3LootType Loot)
{
	switch (Loot)
	{
	case ENaiwaP3LootType::BombCapacity: return FLinearColor::FromSRGBColor(FColor(104, 169, 157));
	case ENaiwaP3LootType::BombRange: return FLinearColor::FromSRGBColor(FColor(255, 160, 40));
	case ENaiwaP3LootType::ChickenJump: return EggGold;
	case ENaiwaP3LootType::SpeedBoost: return FLinearColor::FromSRGBColor(FColor(108, 193, 223));
	case ENaiwaP3LootType::Shield: return FLinearColor::FromSRGBColor(FColor(140, 207, 207));
	case ENaiwaP3LootType::Xray: return PortalViolet;
	case ENaiwaP3LootType::ChaosReverse: return FLinearColor::FromSRGBColor(FColor(225, 104, 164));
	default: return FLinearColor::White;
	}
}
