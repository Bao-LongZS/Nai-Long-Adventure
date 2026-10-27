#include "NaiwaP3M1Actors.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float CellSize = 100.0f;
	const FLinearColor WorldBackdrop = FLinearColor::FromSRGBColor(FColor(22, 30, 41));
	const FLinearColor Backplate = FLinearColor::FromSRGBColor(FColor(32, 38, 47));
	const FLinearColor FloorA = FLinearColor::FromSRGBColor(FColor(160, 185, 199));
	const FLinearColor FloorB = FLinearColor::FromSRGBColor(FColor(148, 174, 190));
	const FLinearColor FloorDot = FLinearColor::FromSRGBColor(FColor(190, 211, 220));
	const FLinearColor FloorArtTint(1.8f, 1.72f, 1.55f, 1.0f);
	const FLinearColor FrameColor = FLinearColor::FromSRGBColor(FColor(38, 54, 66));
	const FLinearColor HardStone = FLinearColor::FromSRGBColor(FColor(24, 36, 54));
	const FLinearColor HardArtTint(0.20f, 0.31f, 0.42f, 1.0f);
	const FLinearColor HardTop = FLinearColor::FromSRGBColor(FColor(36, 52, 73));
	const FLinearColor HardTopArtTint(0.32f, 0.44f, 0.58f, 1.0f);
	const FLinearColor HardCrack = FLinearColor::FromSRGBColor(FColor(17, 26, 40));
	const FLinearColor WoodCrate = FLinearColor::FromSRGBColor(FColor(150, 113, 79));
	const FLinearColor WoodBase = FLinearColor::FromSRGBColor(FColor(67, 56, 58));
	const FLinearColor WoodGrain = FLinearColor::FromSRGBColor(FColor(118, 86, 63));
	const FLinearColor WoodBrace = FLinearColor::FromSRGBColor(FColor(192, 154, 108));
	const FLinearColor WoodStud = FLinearColor::FromSRGBColor(FColor(208, 180, 119));
	const FLinearColor FireColor = FLinearColor::FromSRGBColor(FColor(255, 160, 40));
	const FLinearColor FireCore = FLinearColor::FromSRGBColor(FColor(255, 200, 90));
	const FLinearColor FogUnseen = FLinearColor::FromSRGBColor(FColor(8, 13, 19));
	const FLinearColor FogMemory = FLinearColor::FromSRGBColor(FColor(24, 36, 45));
	const FLinearColor PatrolColor = FLinearColor::FromSRGBColor(FColor(77, 139, 126));
	const FLinearColor ZoneColor = FLinearColor::FromSRGBColor(FColor(104, 169, 157));
	const FLinearColor SpawnColor = FLinearColor::FromSRGBColor(FColor(208, 180, 119));
	const FLinearColor EscapeColor = FLinearColor::FromSRGBColor(FColor(255, 160, 40));
	const FLinearColor EnemyDark = FLinearColor::FromSRGBColor(FColor(27, 54, 58));

	UMaterialInstanceDynamic* MakeColorMaterial(UObject* Outer, const FLinearColor& Color)
	{
		static UMaterialInterface* FlatColorParent = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Naiwa/P3/Materials/M_P3M1FlatColor.M_P3M1FlatColor"));
		UMaterialInterface* Parent = FlatColorParent ? FlatColorParent : UMaterial::GetDefaultMaterial(MD_Surface);
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(
			Parent, Outer);
		if (Material)
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
		}
		return Material;
	}

	bool SetTileArtMaterial(UStaticMeshComponent* Component, UObject* Outer, const TCHAR* TextureName,
		const FLinearColor& Tint = FLinearColor::White)
	{
		if (!Component || !Outer) return false;
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Naiwa/P3/Materials/M_P3TileArt.M_P3TileArt"));
		const FString TexturePath = FString::Printf(
			TEXT("/Game/Naiwa/P3/Tiles/%s.%s"), TextureName, TextureName);
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TexturePath);
		UMaterialInstanceDynamic* Material = Parent ? UMaterialInstanceDynamic::Create(Parent, Outer) : nullptr;
		if (!Material || !Texture) return false;
		Material->SetTextureParameterValue(TEXT("TileTexture"), Texture);
		Material->SetVectorParameterValue(TEXT("Color"), Tint);
		Component->SetMaterial(0, Material);
		return true;
	}
}

ANaiwaP3M1MapActor::ANaiwaP3M1MapActor()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		CubeMesh = CubeFinder.Object;
	}
}

void ANaiwaP3M1MapActor::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (DeltaTime <= 0.0f || !GetWorld()) return;
	const float Time = GetWorld()->GetTimeSeconds();
	for (int32 Index = 0; Index < FlameComponents.Num(); ++Index)
	{
		if (!FlameActiveCells.IsValidIndex(Index) || FlameActiveCells[Index] == 0 ||
			!FogVisibleCells.IsValidIndex(Index) || FogVisibleCells[Index] == 0)
		{
			continue;
		}
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 34.0f + Index * 0.37f);
		if (FlameComponents[Index])
		{
			const float Scale = 0.92f + 0.18f * Pulse;
			FlameComponents[Index]->SetRelativeScale3D(FVector(0.45f * Scale, 0.45f * Scale, 0.08f * Scale));
			if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(FlameComponents[Index]->GetMaterial(0)))
			{
				Material->SetVectorParameterValue(TEXT("Color"), FireColor * (0.90f + 0.25f * Pulse));
			}
		}
		if (FlameCoreComponents.IsValidIndex(Index) && FlameCoreComponents[Index])
		{
			const float Scale = 0.90f + 0.20f * Pulse;
			FlameCoreComponents[Index]->SetRelativeScale3D(FVector(0.25f * Scale, 0.25f * Scale, 0.06f * Scale));
			if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(FlameCoreComponents[Index]->GetMaterial(0)))
			{
				Material->SetVectorParameterValue(TEXT("Color"), FireCore * (0.92f + 0.22f * Pulse));
			}
		}
	}
}

UStaticMeshComponent* ANaiwaP3M1MapActor::AddCube(const TCHAR* Name,
	const FVector& RelativeLocation, const FVector& Scale, const FLinearColor& Color, const float Yaw)
{
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, FName(Name));
	if (!Component)
	{
		return nullptr;
	}
	AddInstanceComponent(Component);
	Component->SetupAttachment(SceneRoot);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(CubeMesh);
	Component->SetRelativeLocation(RelativeLocation);
	Component->SetRelativeRotation(FRotator(0.0f, Yaw, 0.0f));
	Component->SetRelativeScale3D(Scale);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCastShadow(false);
	Component->SetMaterial(0, MakeColorMaterial(this, Color));
	Component->RegisterComponent();
	return Component;
}

void ANaiwaP3M1MapActor::BuildMap(const TArray<FString>& InRows,
	const bool bBuildM1TestMarkers, const bool bBuildFogOverlay)
{
	RowCount = InRows.Num();
	Columns = RowCount > 0 ? InRows[0].Len() : 0;
	const int32 CellCount = Columns * RowCount;
	SoftBaseComponents.SetNumZeroed(CellCount);
	SoftGrainAComponents.SetNumZeroed(CellCount);
	SoftGrainBComponents.SetNumZeroed(CellCount);
	SoftStudComponents.SetNumZeroed(CellCount);
	ItemLockComponents.SetNumZeroed(CellCount);
	FloorComponents.SetNumZeroed(CellCount);
	FloorBaseColors.SetNumZeroed(CellCount);
	HardComponents.SetNumZeroed(CellCount);
	HardBaseColors.SetNumZeroed(CellCount);
	SoftBodyComponents.SetNumZeroed(CellCount);
	SoftBaseColors.SetNumZeroed(CellCount);
	SoftBraceAComponents.SetNumZeroed(CellCount);
	SoftBraceBComponents.SetNumZeroed(CellCount);
	FlameComponents.SetNumZeroed(CellCount);
	FlameCoreComponents.SetNumZeroed(CellCount);
	FlameActiveCells.Init(0, CellCount);
	FogVisibleCells.Init(bBuildFogOverlay ? 0 : 1, CellCount);
	if (bBuildFogOverlay)
	{
		FogUnseenComponents.SetNumZeroed(CellCount);
		FogMemoryNorthComponents.SetNumZeroed(CellCount);
		FogMemorySouthComponents.SetNumZeroed(CellCount);
		FogMemoryWestComponents.SetNumZeroed(CellCount);
		FogMemoryEastComponents.SetNumZeroed(CellCount);
	}

	if (bBuildM1TestMarkers)
	{
		AddCube(TEXT("ArenaBackplate"), FVector(0.0f, 0.0f, -15.0f),
			FVector(8.4f, 6.2f, 0.10f), Backplate);
		AddCube(TEXT("FrameNorth"), FVector(0.0f, -612.0f, -1.0f),
			FVector(8.2f, 0.045f, 0.035f), FrameColor);
		AddCube(TEXT("FrameSouth"), FVector(0.0f, 612.0f, -1.0f),
			FVector(8.2f, 0.045f, 0.035f), FrameColor);
		AddCube(TEXT("FrameWest"), FVector(-812.0f, 0.0f, -1.0f),
			FVector(0.045f, 6.2f, 0.035f), FrameColor);
		AddCube(TEXT("FrameEast"), FVector(812.0f, 0.0f, -1.0f),
			FVector(0.045f, 6.2f, 0.035f), FrameColor);
	}
	else if (Columns > 0 && RowCount > 0)
	{
		const float HalfWidth = Columns * CellSize * 0.5f + 50.0f;
		const float HalfHeight = RowCount * CellSize * 0.5f + 50.0f;
		const float NorthSouthY = HalfHeight + 50.0f;
		const float WestEastX = HalfWidth + 50.0f;
		AddCube(TEXT("M2WorldBackdrop"), FVector(0.0f, 0.0f, -35.0f),
			FVector((HalfWidth + 6000.0f) / 50.0f, (HalfHeight + 4000.0f) / 50.0f, 0.10f),
			WorldBackdrop);
		AddCube(TEXT("M2ArenaBackplate"), FVector(0.0f, 0.0f, -15.0f),
			FVector(HalfWidth / 50.0f, HalfHeight / 50.0f, 0.10f), Backplate);
		AddCube(TEXT("M2FrameNorth"), FVector(0.0f, -NorthSouthY, -1.0f),
			FVector(HalfWidth / 50.0f, 0.045f, 0.035f), FrameColor);
		AddCube(TEXT("M2FrameSouth"), FVector(0.0f, NorthSouthY, -1.0f),
			FVector(HalfWidth / 50.0f, 0.045f, 0.035f), FrameColor);
		AddCube(TEXT("M2FrameWest"), FVector(-WestEastX, 0.0f, -1.0f),
			FVector(0.045f, HalfHeight / 50.0f, 0.035f), FrameColor);
		AddCube(TEXT("M2FrameEast"), FVector(WestEastX, 0.0f, -1.0f),
			FVector(0.045f, HalfHeight / 50.0f, 0.035f), FrameColor);
	}

	for (int32 Y = 0; Y < RowCount; ++Y)
	{
		for (int32 X = 0; X < Columns; ++X)
		{
			const int32 Index = Y * Columns + X;
			const FVector Center = CellCenter(X, Y);
			const FLinearColor FloorColor = ((X + Y) % 2 == 0) ? FloorA : FloorB;
			FloorComponents[Index] = AddCube(*FString::Printf(TEXT("Floor_%d"), Index),
				Center + FVector(0.0f, 0.0f, -7.0f), FVector(0.48f, 0.48f, 0.06f), FloorColor);
			FloorBaseColors[Index] = FloorColor;
			if (SetTileArtMaterial(FloorComponents[Index], this, TEXT("T_P3Floor_A_v2"), FloorArtTint))
			{
				FloorBaseColors[Index] = FloorArtTint;
			}
			FloorComponents[Index]->SetRelativeRotation(FRotator(0.0f, ((X * 3 + Y * 7) % 4) * 90.0f, 0.0f));
			FlameComponents[Index] = AddCube(*FString::Printf(TEXT("Flame_%d"), Index),
				Center + FVector(0.0f, 0.0f, 5.0f), FVector(0.45f, 0.45f, 0.08f), FireColor);
			if (FlameComponents[Index])
			{
				FlameComponents[Index]->SetVisibility(false);
			}
			FlameCoreComponents[Index] = AddCube(*FString::Printf(TEXT("FlameCore_%d"), Index),
				Center + FVector(0.0f, 0.0f, 11.0f), FVector(0.25f, 0.25f, 0.06f), FireCore);
			if (FlameCoreComponents[Index])
			{
				FlameCoreComponents[Index]->SetVisibility(false);
			}
			if ((X + 2 * Y) % 4 == 0)
			{
				DetailComponents.Add(AddCube(*FString::Printf(TEXT("FloorFleck_%d"), Index),
					Center + FVector(-31.0f, 28.0f, -2.4f), FVector(0.025f, 0.018f, 0.012f), FloorDot));
			}
			ApplyTileVisual(Index, InRows[Y][X] == TCHAR('H') ? ENaiwaP3Tile::Hard :
				(InRows[Y][X] == TCHAR('I') ? ENaiwaP3Tile::Item :
				 (InRows[Y][X] == TCHAR('S') ? ENaiwaP3Tile::Soft : ENaiwaP3Tile::Floor)));
			if (bBuildFogOverlay)
			{
				FogUnseenComponents[Index] = AddCube(*FString::Printf(TEXT("FogUnseen_%d"), Index),
					Center + FVector(0.0f, 0.0f, 150.0f), FVector(0.50f, 0.50f, 0.14f), FogUnseen);
				FogMemoryNorthComponents[Index] = AddCube(*FString::Printf(TEXT("FogMemoryN_%d"), Index),
					Center + FVector(0.0f, -46.0f, 55.0f), FVector(0.46f, 0.012f, 0.008f), FogMemory);
				FogMemorySouthComponents[Index] = AddCube(*FString::Printf(TEXT("FogMemoryS_%d"), Index),
					Center + FVector(0.0f, 46.0f, 55.0f), FVector(0.46f, 0.012f, 0.008f), FogMemory);
				FogMemoryWestComponents[Index] = AddCube(*FString::Printf(TEXT("FogMemoryW_%d"), Index),
					Center + FVector(-46.0f, 0.0f, 55.0f), FVector(0.012f, 0.46f, 0.008f), FogMemory);
				FogMemoryEastComponents[Index] = AddCube(*FString::Printf(TEXT("FogMemoryE_%d"), Index),
					Center + FVector(46.0f, 0.0f, 55.0f), FVector(0.012f, 0.46f, 0.008f), FogMemory);
				FogMemoryNorthComponents[Index]->SetVisibility(false);
				FogMemorySouthComponents[Index]->SetVisibility(false);
				FogMemoryWestComponents[Index]->SetVisibility(false);
				FogMemoryEastComponents[Index]->SetVisibility(false);
			}
		}
	}

	if (bBuildM1TestMarkers)
	{
	const FVector Spawn = CellCenter(2, 9, -2.0f);
	DetailComponents.Add(AddCube(TEXT("SpawnMarkNorth"), Spawn + FVector(0.0f, -38.0f, 0.0f),
		FVector(0.27f, 0.020f, 0.015f), SpawnColor));
	DetailComponents.Add(AddCube(TEXT("SpawnMarkSouth"), Spawn + FVector(0.0f, 38.0f, 0.0f),
		FVector(0.27f, 0.020f, 0.015f), SpawnColor));
	DetailComponents.Add(AddCube(TEXT("SpawnMarkWest"), Spawn + FVector(-38.0f, 0.0f, 0.0f),
		FVector(0.020f, 0.27f, 0.015f), SpawnColor));
	DetailComponents.Add(AddCube(TEXT("SpawnMarkEast"), Spawn + FVector(38.0f, 0.0f, 0.0f),
		FVector(0.020f, 0.27f, 0.015f), SpawnColor));

	const FVector Escape = CellCenter(9, 6, -2.0f);
	DetailComponents.Add(AddCube(TEXT("EscapeMarkNorth"), Escape + FVector(0.0f, -34.0f, 0.0f),
		FVector(0.24f, 0.018f, 0.015f), EscapeColor));
	DetailComponents.Add(AddCube(TEXT("EscapeMarkSouth"), Escape + FVector(0.0f, 34.0f, 0.0f),
		FVector(0.24f, 0.018f, 0.015f), EscapeColor));
	DetailComponents.Add(AddCube(TEXT("EscapeMarkWest"), Escape + FVector(-34.0f, 0.0f, 0.0f),
		FVector(0.018f, 0.24f, 0.015f), EscapeColor));
	DetailComponents.Add(AddCube(TEXT("EscapeMarkEast"), Escape + FVector(34.0f, 0.0f, 0.0f),
		FVector(0.018f, 0.24f, 0.015f), EscapeColor));

	const FVector ChainEntry = CellCenter(5, 5, -2.0f);
	const FVector ChainExit = CellCenter(10, 5, -2.0f);
	DetailComponents.Add(AddCube(TEXT("ChainEntryMark"), ChainEntry, FVector(0.12f, 0.035f, 0.015f), EscapeColor));
	DetailComponents.Add(AddCube(TEXT("ChainExitMark"), ChainExit, FVector(0.12f, 0.035f, 0.015f), EscapeColor));

	const float ZoneLeft = CellCenter(5, 2).X - CellSize * 0.5f;
	const float ZoneRight = CellCenter(9, 2).X + CellSize * 0.5f;
	const float ZoneTop = CellCenter(5, 2).Y - CellSize * 0.5f;
	const float ZoneBottom = CellCenter(5, 8).Y + CellSize * 0.5f;
	const float BracketLength = 34.0f;
	for (const float XSign : {-1.0f, 1.0f})
	{
		for (const float YSign : {-1.0f, 1.0f})
		{
			const float X = XSign < 0.0f ? ZoneLeft : ZoneRight;
			const float Y = YSign < 0.0f ? ZoneTop : ZoneBottom;
			DetailComponents.Add(AddCube(TEXT("OpenZoneBracketX"),
				FVector(X + XSign * BracketLength * 0.5f, Y, -2.0f),
				FVector(BracketLength / CellSize * 0.5f, 0.018f, 0.012f), ZoneColor));
			DetailComponents.Add(AddCube(TEXT("OpenZoneBracketY"),
				FVector(X, Y + YSign * BracketLength * 0.5f, -2.0f),
				FVector(0.018f, BracketLength / CellSize * 0.5f, 0.012f), ZoneColor));
		}
	}

	TArray<FIntPoint> Patrol;
	for (int32 X = 4; X <= 10; ++X) Patrol.Add(FIntPoint(X, 1));
	for (int32 Y = 2; Y <= 9; ++Y) Patrol.Add(FIntPoint(10, Y));
	for (int32 X = 9; X >= 4; --X) Patrol.Add(FIntPoint(X, 9));
	for (int32 Y = 8; Y >= 2; --Y) Patrol.Add(FIntPoint(4, Y));
	for (int32 Index = 0; Index < Patrol.Num(); ++Index)
	{
		const FIntPoint Cell = Patrol[Index];
		const FIntPoint Next = Patrol[(Index + 1) % Patrol.Num()];
		const bool bVertical = Cell.X == Next.X;
		DetailComponents.Add(AddCube(*FString::Printf(TEXT("PatrolWaypoint_%02d"), Index),
			CellCenter(Cell.X, Cell.Y, -2.0f), bVertical ? FVector(0.035f, 0.15f, 0.014f) :
			FVector(0.15f, 0.035f, 0.014f), PatrolColor));
	}
	}
}

void ANaiwaP3M1MapActor::ApplyTileVisual(const int32 Index, const ENaiwaP3Tile Tile)
{
	if (!HardComponents.IsValidIndex(Index))
	{
		return;
	}
	if (Tile == ENaiwaP3Tile::Hard && !HardComponents[Index])
	{
		const int32 X = Index % Columns;
		const int32 Y = Index / Columns;
		const FVector Center = CellCenter(X, Y);
		HardComponents[Index] = AddCube(*FString::Printf(TEXT("Hard_%d"), Index),
			Center + FVector(0.0f, 0.0f, 26.0f), FVector(0.47f, 0.47f, 0.52f), HardStone);
		HardBaseColors[Index] = HardStone;
		if (SetTileArtMaterial(HardComponents[Index], this, TEXT("T_P3HardStone_v2"), HardArtTint))
		{
			HardBaseColors[Index] = HardArtTint;
		}
		const float StoneYaw = ((X + Y * 3) % 4) * 90.0f;
		HardComponents[Index]->SetRelativeRotation(FRotator(0.0f, StoneYaw, 0.0f));
		UStaticMeshComponent* HardCap = AddCube(*FString::Printf(TEXT("HardCap_%d"), Index),
			Center + FVector(0.0f, 0.0f, 52.5f), FVector(0.43f, 0.43f, 0.012f), HardTop);
		HardCap->SetRelativeRotation(FRotator(0.0f, StoneYaw, 0.0f));
		DetailComponents.Add(HardCap);
		SetTileArtMaterial(HardCap, this, TEXT("T_P3HardStone_v2"),
			HardTopArtTint);
		DetailComponents.Add(AddCube(*FString::Printf(TEXT("HardSeam_%d"), Index),
			Center + FVector(0.0f, 0.0f, 53.5f), FVector(0.38f, 0.015f, 0.008f), HardCrack,
			((X + Y) % 2 == 0) ? 0.0f : 90.0f));
		if ((X + 2 * Y) % 3 == 0)
		{
			DetailComponents.Add(AddCube(*FString::Printf(TEXT("HardFleck_%d"), Index),
				Center + FVector(23.0f, -19.0f, 53.5f), FVector(0.025f, 0.020f, 0.008f), HardTop));
		}
	}
	if ((Tile == ENaiwaP3Tile::Soft || Tile == ENaiwaP3Tile::Item) && !SoftBodyComponents[Index])
	{
		const int32 X = Index % Columns;
		const int32 Y = Index / Columns;
		const FVector Center = CellCenter(X, Y);
		SoftBaseComponents[Index] = AddCube(*FString::Printf(TEXT("CrateBase_%d"), Index),
			Center + FVector(0.0f, 0.0f, 3.0f), FVector(0.44f, 0.44f, 0.055f), WoodBase);
		const FLinearColor CrateColor = ((X + Y) % 2 == 0) ? WoodCrate :
			FLinearColor::FromSRGBColor(FColor(162, 123, 86));
		SoftBodyComponents[Index] = AddCube(*FString::Printf(TEXT("Crate_%d"), Index),
			Center + FVector(0.0f, 0.0f, 15.0f), FVector(0.43f, 0.43f, 0.30f), CrateColor);
		SoftBaseColors[Index] = CrateColor;
		if (SetTileArtMaterial(SoftBodyComponents[Index], this, TEXT("T_P3WoodCrate_v2")))
		{
			SoftBaseColors[Index] = FLinearColor::White;
		}
		SoftBodyComponents[Index]->SetRelativeRotation(FRotator(0.0f, ((X + Y) % 2) * 90.0f, 0.0f));
		SoftGrainAComponents[Index] = AddCube(*FString::Printf(TEXT("CrateGrainA_%d"), Index),
			Center + FVector(0.0f, -12.0f, 26.0f), FVector(0.32f, 0.012f, 0.012f), WoodGrain);
		SoftGrainBComponents[Index] = AddCube(*FString::Printf(TEXT("CrateGrainB_%d"), Index),
			Center + FVector(0.0f, 13.0f, 26.0f), FVector(0.32f, 0.012f, 0.012f), WoodGrain);
		SoftBraceAComponents[Index] = AddCube(*FString::Printf(TEXT("BraceA_%d"), Index),
			Center + FVector(0.0f, 0.0f, 31.0f), FVector(0.37f, 0.055f, 0.04f), WoodBrace, 45.0f);
		SoftBraceBComponents[Index] = AddCube(*FString::Printf(TEXT("BraceB_%d"), Index),
			Center + FVector(0.0f, 0.0f, 32.0f), FVector(0.37f, 0.055f, 0.04f), WoodBrace, -45.0f);
		SoftStudComponents[Index] = AddCube(*FString::Printf(TEXT("CrateStud_%d"), Index),
			Center + FVector(0.0f, 0.0f, 34.0f), FVector(0.035f, 0.035f, 0.018f), WoodStud);
	}
	if (Tile == ENaiwaP3Tile::Item && !ItemLockComponents[Index])
	{
		const int32 X = Index % Columns;
		const int32 Y = Index / Columns;
		const FVector Center = CellCenter(X, Y);
		ItemLockComponents[Index] = AddCube(*FString::Printf(TEXT("ItemCrateLock_%d"), Index),
			Center + FVector(0.0f, -2.0f, 19.0f), FVector(0.10f, 0.07f, 0.08f), WoodStud);
	}
	if (HardComponents[Index])
	{
		HardComponents[Index]->SetVisibility(Tile == ENaiwaP3Tile::Hard, true);
	}
	if (SoftBodyComponents[Index])
	{
		const bool bShowCrate = Tile == ENaiwaP3Tile::Soft || Tile == ENaiwaP3Tile::Item;
		SoftBaseComponents[Index]->SetVisibility(bShowCrate, true);
		SoftBodyComponents[Index]->SetVisibility(bShowCrate, true);
		SoftGrainAComponents[Index]->SetVisibility(bShowCrate, true);
		SoftGrainBComponents[Index]->SetVisibility(bShowCrate, true);
		SoftBraceAComponents[Index]->SetVisibility(bShowCrate, true);
		SoftBraceBComponents[Index]->SetVisibility(bShowCrate, true);
		SoftStudComponents[Index]->SetVisibility(bShowCrate, true);
	}
	if (ItemLockComponents[Index])
	{
		ItemLockComponents[Index]->SetVisibility(Tile == ENaiwaP3Tile::Item, true);
	}
}

void ANaiwaP3M1MapActor::SetTile(const int32 X, const int32 Y, const ENaiwaP3Tile Tile)
{
	if (X < 0 || Y < 0 || X >= Columns || Y >= RowCount)
	{
		return;
	}
	ApplyTileVisual(Y * Columns + X, Tile);
}

void ANaiwaP3M1MapActor::SetFlame(const int32 X, const int32 Y, const bool bVisible)
{
	if (X < 0 || Y < 0 || X >= Columns || Y >= RowCount)
	{
		return;
	}
	const int32 Index = Y * Columns + X;
	FlameActiveCells[Index] = bVisible ? 1 : 0;
	const bool bShouldShow = bVisible && FogVisibleCells.IsValidIndex(Index) && FogVisibleCells[Index] != 0;
	if (UStaticMeshComponent* Flame = FlameComponents[Index])
	{
		Flame->SetVisibility(bShouldShow, true);
	}
	if (UStaticMeshComponent* Core = FlameCoreComponents[Index])
	{
		Core->SetVisibility(bShouldShow, true);
	}
}

void ANaiwaP3M1MapActor::SetFogState(const int32 X, const int32 Y,
	const bool bVisible, const bool bRemembered)
{
	if (X < 0 || Y < 0 || X >= Columns || Y >= RowCount)
	{
		return;
	}
	const int32 Index = Y * Columns + X;
	if (FogVisibleCells.IsValidIndex(Index)) FogVisibleCells[Index] = bVisible ? 1 : 0;
	if (FogUnseenComponents.IsValidIndex(Index) && FogUnseenComponents[Index])
	{
		FogUnseenComponents[Index]->SetVisibility(!bVisible && !bRemembered, true);
	}
	const bool bShowMemoryEdge = !bVisible && bRemembered;
	if (FogMemoryNorthComponents.IsValidIndex(Index) && FogMemoryNorthComponents[Index])
	{
		FogMemoryNorthComponents[Index]->SetVisibility(bShowMemoryEdge, true);
		if (FogMemorySouthComponents.IsValidIndex(Index) && FogMemorySouthComponents[Index])
			FogMemorySouthComponents[Index]->SetVisibility(bShowMemoryEdge, true);
		if (FogMemoryWestComponents.IsValidIndex(Index) && FogMemoryWestComponents[Index])
			FogMemoryWestComponents[Index]->SetVisibility(bShowMemoryEdge, true);
		if (FogMemoryEastComponents.IsValidIndex(Index) && FogMemoryEastComponents[Index])
			FogMemoryEastComponents[Index]->SetVisibility(bShowMemoryEdge, true);
	}
	const float Brightness = bShowMemoryEdge ? 0.56f : 1.0f;
	const auto SetBrightness = [Brightness](UStaticMeshComponent* Component, const FLinearColor& BaseColor)
	{
		if (Component)
		{
			if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0)))
				Material->SetVectorParameterValue(TEXT("Color"), BaseColor * Brightness);
		}
	};
	if (FloorComponents.IsValidIndex(Index)) SetBrightness(FloorComponents[Index], FloorBaseColors[Index]);
	if (HardComponents.IsValidIndex(Index)) SetBrightness(HardComponents[Index], HardBaseColors[Index]);
	if (SoftBodyComponents.IsValidIndex(Index)) SetBrightness(SoftBodyComponents[Index], SoftBaseColors[Index]);
	const bool bShowFlame = FlameActiveCells.IsValidIndex(Index) && FlameActiveCells[Index] != 0 && bVisible;
	if (FlameComponents.IsValidIndex(Index) && FlameComponents[Index]) FlameComponents[Index]->SetVisibility(bShowFlame, true);
	if (FlameCoreComponents.IsValidIndex(Index) && FlameCoreComponents[Index]) FlameCoreComponents[Index]->SetVisibility(bShowFlame, true);
}

FVector ANaiwaP3M1MapActor::CellCenter(const int32 X, const int32 Y, const float Z) const
{
	return FVector((X - (Columns - 1) * 0.5f) * CellSize,
		(Y - (RowCount - 1) * 0.5f) * CellSize, Z);
}

ANaiwaP3M1EnemyActor::ANaiwaP3M1EnemyActor()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));

	ShadowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShadowMesh"));
	ShadowMesh->SetupAttachment(Root);
	if (CylinderFinder.Succeeded()) ShadowMesh->SetStaticMesh(CylinderFinder.Object);
	ShadowMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
	ShadowMesh->SetRelativeScale3D(FVector(0.31f, 0.20f, 0.012f));
	ShadowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShadowMesh->SetCastShadow(false);
	if (UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Naiwa/P3/Materials/M_P3M1FlatColor.M_P3M1FlatColor")))
	{
		if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, this))
		{
			Material->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(FColor(23, 31, 39)));
			ShadowMesh->SetMaterial(0, Material);
		}
	}
	const FName FrameNames[] = { TEXT("CellFrameNorth"), TEXT("CellFrameSouth"),
		TEXT("CellFrameWest"), TEXT("CellFrameEast") };
	const FVector FrameLocations[] = { FVector(0.0f, -44.0f, 4.0f), FVector(0.0f, 44.0f, 4.0f),
		FVector(-44.0f, 0.0f, 4.0f), FVector(44.0f, 0.0f, 4.0f) };
	const FVector FrameScales[] = { FVector(0.40f, 0.025f, 0.008f), FVector(0.40f, 0.025f, 0.008f),
		FVector(0.025f, 0.40f, 0.008f), FVector(0.025f, 0.40f, 0.008f) };
	const FLinearColor EnemyCellFrameColor = FLinearColor::FromSRGBColor(FColor(223, 151, 78));
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FrameNames); ++Index)
	{
		UStaticMeshComponent* Frame = CreateDefaultSubobject<UStaticMeshComponent>(FrameNames[Index]);
		Frame->SetupAttachment(Root);
		if (CubeFinder.Succeeded()) Frame->SetStaticMesh(CubeFinder.Object);
		Frame->SetRelativeLocation(FrameLocations[Index]);
		Frame->SetRelativeScale3D(FrameScales[Index]);
		Frame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Frame->SetCastShadow(false);
		Frame->SetMaterial(0, MakeColorMaterial(this, EnemyCellFrameColor));
		CellFrameComponents.Add(Frame);
	}

	PortraitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortraitMesh"));
	PortraitMesh->SetupAttachment(Root);
	if (PlaneFinder.Succeeded()) PortraitMesh->SetStaticMesh(PlaneFinder.Object);
	PortraitMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 14.0f));
	PortraitMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PortraitMesh->SetGenerateOverlapEvents(false);
	PortraitMesh->SetCastShadow(false);
	PortraitMesh->SetVisibility(false);

	MissingPortraitMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MissingPortraitMarker"));
	MissingPortraitMarker->SetupAttachment(Root);
	if (CylinderFinder.Succeeded()) MissingPortraitMarker->SetStaticMesh(CylinderFinder.Object);
	MissingPortraitMarker->SetRelativeLocation(FVector(0.0f, 0.0f, 4.0f));
	MissingPortraitMarker->SetRelativeScale3D(FVector(0.20f, 0.20f, 0.018f));
	MissingPortraitMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MissingPortraitMarker->SetCastShadow(false);
	MissingPortraitMarker->SetMaterial(0,
		MakeColorMaterial(this, FLinearColor::FromSRGBColor(FColor(90, 172, 205))));
}

void ANaiwaP3M1EnemyActor::SetGridPosition(const FVector& Position)
{
	SetActorLocation(Position);
}

void ANaiwaP3M1EnemyActor::SetPortrait(UTexture2D* Portrait)
{
	if (!Portrait || !PortraitMesh)
	{
		if (MissingPortraitMarker) MissingPortraitMarker->SetVisibility(true);
		return;
	}
	if (ActivePortrait == Portrait) return;
	if (!PortraitMaterial)
	{
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Naiwa/P3/Materials/M_P3Portrait.M_P3Portrait"));
		PortraitMaterial = Parent ? UMaterialInstanceDynamic::Create(Parent, this) : nullptr;
	}
	if (!PortraitMaterial) return;
	PortraitMaterial->SetTextureParameterValue(TEXT("Portrait"), Portrait);
	PortraitMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
	PortraitMesh->SetMaterial(0, PortraitMaterial);
	const float Height = 76.0f;
	const float Aspect = static_cast<float>(Portrait->GetSizeX()) /
		FMath::Max(static_cast<float>(Portrait->GetSizeY()), 1.0f);
	PortraitMesh->SetRelativeScale3D(FVector(FMath::Min(72.0f, Height * Aspect) / 100.0f,
		Height / 100.0f, 1.0f));
	PortraitMesh->SetVisibility(true);
	ActivePortrait = Portrait;
	if (MissingPortraitMarker) MissingPortraitMarker->SetVisibility(false);
}
