#include "NaiwaP5PrototypeTrackActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "../Core/NaiwaP5RunSubsystem.h"
#include "../Core/NaiwaP5TrackData.h"
#include "NaiwaP5A0ArtSampleUtils.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor GroundColor(0.22f, 0.34f, 0.43f, 1.0f);
	const FLinearColor TreeColor(0.13f, 0.27f, 0.22f, 1.0f);
	const FLinearColor CoinColor(0.95f, 0.69f, 0.16f, 1.0f);
	const FLinearColor AvalancheColor(0.72f, 0.88f, 0.96f, 1.0f);
	const FLinearColor AvalancheFoamColor(0.91f, 0.97f, 1.0f, 1.0f);
	constexpr double A0BackgroundTileWidthCm = 3000.0;
	constexpr double A0BackgroundParallax[] = { 0.12, 0.30, 0.65 };
	constexpr double A0BackgroundY[] = { -900.0, -600.0, -350.0 };
	constexpr double A0BackgroundZ[] = { 760.0, 320.0, -140.0 };
	// All A0 background exports are 2048x768. Keep the source aspect ratio at 30 m wide.
	constexpr double A0BackgroundHeight[] = { 1125.0, 1125.0, 1125.0 };
	constexpr double A0SnowThicknessCm = 150.0;
	constexpr double A0SnowTextureRepeatCm = 300.0;

	UMaterialInterface* LoadA0Material(const TCHAR* AssetName)
	{
		const FString Name(AssetName);
		const FString V05Path = FString::Printf(
			TEXT("/Game/Naiwa/P5/ArtSample_v05/Materials/%s.%s"), *Name, *Name);
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *V05Path))
		{
			return Material;
		}
		const FString V04Path = FString::Printf(
			TEXT("/Game/Naiwa/P5/ArtSample_v04/Materials/%s.%s"), *Name, *Name);
		return LoadObject<UMaterialInterface>(nullptr, *V04Path);
	}
}

ANaiwaP5PrototypeTrackActor::ANaiwaP5PrototypeTrackActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(
		TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (CubeFinder.Succeeded()) CubeMesh = CubeFinder.Object;
	if (ConeFinder.Succeeded()) ConeMesh = ConeFinder.Object;
	if (SphereFinder.Succeeded()) SphereMesh = SphereFinder.Object;
	if (CylinderFinder.Succeeded()) CylinderMesh = CylinderFinder.Object;
	if (PlaneFinder.Succeeded()) PlaneMesh = PlaneFinder.Object;

	GroundMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundMesh"));
	GroundMesh->SetupAttachment(SceneRoot);
	if (CubeMesh) GroundMesh->SetStaticMesh(CubeMesh);
	GroundMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GroundMesh->SetCastShadow(false);
	if (UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(ParentMaterial, this))
		{
			Material->SetVectorParameterValue(TEXT("Color"), GroundColor);
			GroundMesh->SetMaterial(0, Material);
		}
	}
}

UStaticMeshComponent* ANaiwaP5PrototypeTrackActor::AddA0Sprite(const FName Name,
	UMaterialInterface* Material, const FVector& Location, const FVector2D& SizeCm)
{
	if (!PlaneMesh || !Material)
	{
		return nullptr;
	}
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, Name);
	if (!Component)
	{
		return nullptr;
	}
	Component->SetupAttachment(SceneRoot);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(PlaneMesh);
	Component->SetRelativeLocation(Location);
	Component->SetRelativeRotation(FRotator(0.0, 0.0, 90.0));
	Component->SetRelativeScale3D(FVector(SizeCm.X / 100.0, SizeCm.Y / 100.0, 1.0));
	Component->SetMaterial(0, Material);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(false);
	Component->RegisterComponent();
	return Component;
}

void ANaiwaP5PrototypeTrackActor::BuildA0BackgroundLayers(const double RouteLengthCm)
{
	const double CoveredRouteCm = FMath::Max(0.0, RouteLengthCm) + 6000.0;
	A0SkyMesh = AddBlockoutMesh(TEXT("A0SkyBackdrop"), PlaneMesh,
		FVector(RouteLengthCm * 0.5, -1400.0, 200.0),
		FVector(CoveredRouteCm / 100.0, 24.0, 1.0), FLinearColor(0.50f, 0.70f, 0.82f, 1.0f));
	if (A0SkyMesh)
	{
		if (UMaterialInterface* SkyMaterial = LoadA0Material(TEXT("M_P5_A0_Sky")))
		{
			A0SkyMesh->SetMaterial(0, SkyMaterial);
		}
	}
	if (A0SkyMesh)
	{
		// The solid backdrop uses the front face toward the side camera; textured
		// artwork planes remain two-sided and preserve the established roll=90 UV up-axis.
		A0SkyMesh->SetRelativeRotation(FRotator(0.0, 0.0, -90.0));
	}

	const TCHAR* MaterialNames[] = {
		TEXT("M_P5_A0_MountainFar"),
		TEXT("M_P5_A0_ForestMid"),
		TEXT("M_P5_A0_RidgeNear")
	};
	A0BackgroundMeshes.Reset();
	A0BackgroundMeshes.Reserve(9);
	for (int32 LayerIndex = 0; LayerIndex < 3; ++LayerIndex)
	{
		UMaterialInterface* Material = LoadA0Material(MaterialNames[LayerIndex]);
		for (int32 TileIndex = 0; TileIndex < 3; ++TileIndex)
		{
			const double CenterX = (TileIndex - 1) * A0BackgroundTileWidthCm + A0BackgroundTileWidthCm * 0.5;
			UStaticMeshComponent* Tile = AddA0Sprite(
				FName(*FString::Printf(TEXT("A0Background_%d_%d"), LayerIndex, TileIndex)),
				Material,
				FVector(CenterX, A0BackgroundY[LayerIndex], A0BackgroundZ[LayerIndex]),
				FVector2D(A0BackgroundTileWidthCm, A0BackgroundHeight[LayerIndex]));
			A0BackgroundMeshes.Add(Tile);
		}
	}
}

void ANaiwaP5PrototypeTrackActor::HideLegacyA0SampleActors()
{
#if WITH_EDITOR
	if (!GetWorld())
	{
		return;
	}
	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!Actor || Actor == this)
		{
			continue;
		}
		const FString Label = Actor->GetActorLabel();
		if (!Label.StartsWith(TEXT("P5A0_")))
		{
			continue;
		}
		if (Label == TEXT("P5A0_PlayerStart") || Label == TEXT("P5A0_KeyLight"))
		{
			continue;
		}
		Actor->SetActorEnableCollision(false);
		Actor->SetActorHiddenInGame(true);
	}
#endif
}

void ANaiwaP5PrototypeTrackActor::BeginPlay()
{
	Super::BeginPlay();
	const UNaiwaP5RunSubsystem* RunSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<UNaiwaP5RunSubsystem>() : nullptr;
	if (!RunSubsystem)
	{
		return;
	}

	const FNaiwaP5RouteFixture& Fixture = RunSubsystem->GetActivePrototypeFixture();
	const bool bM2Route = RunSubsystem->GetState().bM2PrototypeRoute;
	const double RouteLengthMeters = static_cast<double>(Fixture.RouteLengthMm) / 1000.0;
	const double RouteLengthCm = RouteLengthMeters * 100.0;
	bA0ArtSample = IsNaiwaP5A0ArtSampleWorld(GetWorld());
	if (bA0ArtSample)
	{
		HideLegacyA0SampleActors();
		BuildA0BackgroundLayers(RouteLengthCm);
	}

	if (bM2Route)
	{
		GroundMesh->SetVisibility(false);
		GroundMesh->SetHiddenInGame(true);
		int32 SectionIndex = 0;
		for (const FNaiwaP5RouteSegment& Segment : Fixture.Segments)
		{
			TArray<TPair<int32, int32>> Spans;
			int32 SpanStartMm = Segment.StartDistanceMm;
			for (const FNaiwaP5RouteEvent& Event : Fixture.Events)
			{
				const bool bGap = Event.Kind == ENaiwaP5RouteEventKind::IceGap ||
					Event.Kind == ENaiwaP5RouteEventKind::CliffGap;
				if (!bGap || Event.WidthMm <= 0 ||
					Event.DistanceMm < Segment.StartDistanceMm || Event.DistanceMm > Segment.EndDistanceMm)
				{
					continue;
				}
				const int32 GapStartMm = FMath::Clamp(Event.DistanceMm - Event.WidthMm / 2,
					Segment.StartDistanceMm, Segment.EndDistanceMm);
				const int32 GapEndMm = FMath::Clamp(Event.DistanceMm + Event.WidthMm / 2,
					Segment.StartDistanceMm, Segment.EndDistanceMm);
				if (GapStartMm > SpanStartMm)
				{
					Spans.Emplace(SpanStartMm, GapStartMm);
				}
				SpanStartMm = FMath::Max(SpanStartMm, GapEndMm);
			}
			if (SpanStartMm < Segment.EndDistanceMm)
			{
				Spans.Emplace(SpanStartMm, Segment.EndDistanceMm);
			}

			const FLinearColor SurfaceColor = Segment.Surface == ENaiwaP5SurfaceKind::Ice
				? FLinearColor(0.24f, 0.58f, 0.68f, 1.0f)
				: (Segment.Surface == ENaiwaP5SurfaceKind::JumpRamp
					? FLinearColor(0.72f, 0.82f, 0.88f, 1.0f)
					: GroundColor);
			for (int32 SpanIndex = 0; SpanIndex < Spans.Num(); ++SpanIndex)
			{
				const TPair<int32, int32>& Span = Spans[SpanIndex];
				const int32 MaxVisualSpanMm = bA0ArtSample ? 30000 : Span.Value - Span.Key;
				UMaterialInterface* SnowMaterial = bA0ArtSample
					? LoadA0Material(TEXT("M_P5_A0_SnowGround")) : nullptr;
				int32 ChunkStartMm = Span.Key;
				int32 ChunkIndex = 0;
				while (ChunkStartMm < Span.Value)
				{
					const int32 ChunkEndMm = FMath::Min(Span.Value, ChunkStartMm + MaxVisualSpanMm);
					const double StartX = static_cast<double>(ChunkStartMm) / 10.0;
					const double EndX = static_cast<double>(ChunkEndMm) / 10.0;
					const double SpanLengthCm = EndX - StartX;
					if (SpanLengthCm > 0.0)
					{
						const double StartZ = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(Fixture, ChunkStartMm) * 100.0;
						const double EndZ = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(Fixture, ChunkEndMm) * 100.0;
						const double PitchDegrees = FMath::RadiansToDegrees(FMath::Atan2(EndZ - StartZ, SpanLengthCm));
						const bool bTexturedA0Surface = bA0ArtSample && SnowMaterial;
						const double ThicknessCm = bTexturedA0Surface ? A0SnowThicknessCm : 20.0;
						const double HalfThicknessCm = ThicknessCm * 0.5;
						const double PitchRadians = FMath::DegreesToRadians(PitchDegrees);
						const double MidX = (StartX + EndX) * 0.5;
						const double MidZ = (StartZ + EndZ) * 0.5;
						const FVector SnowCenter(
							MidX - FMath::Sin(PitchRadians) * HalfThicknessCm,
							0.0,
							MidZ - FMath::Cos(PitchRadians) * HalfThicknessCm);
						UStaticMeshComponent* GroundSection = AddBlockoutMesh(
							FName(*FString::Printf(TEXT("RouteSurface_%02d_%02d_%02d"), SectionIndex, SpanIndex, ChunkIndex)),
							CubeMesh, SnowCenter,
							FVector((SpanLengthCm + 20.0) / 100.0, bA0ArtSample ? 0.10 : 10.0,
								ThicknessCm / 100.0), SurfaceColor);
						if (GroundSection)
						{
							GroundSection->SetRelativeRotation(FRotator(PitchDegrees, 0.0, 0.0));
							if (bTexturedA0Surface)
							{
								if (UMaterialInstanceDynamic* SnowMID = UMaterialInstanceDynamic::Create(SnowMaterial, this))
								{
									SnowMID->SetVectorParameterValue(TEXT("A0UVScale"), FLinearColor(
										SpanLengthCm / A0SnowTextureRepeatCm,
										ThicknessCm / A0SnowTextureRepeatCm, 0.0f, 0.0f));
									SnowMID->SetVectorParameterValue(TEXT("A0UVOffset"), FLinearColor(
										StartX / A0SnowTextureRepeatCm, 0.0f, 0.0f, 0.0f));
									GroundSection->SetMaterial(0, SnowMID);
								}
							}
						}
					}
					ChunkStartMm = ChunkEndMm;
					++ChunkIndex;
				}
			}
			++SectionIndex;
		}
	}
	else
	{
		GroundMesh->SetVisibility(true);
		GroundMesh->SetHiddenInGame(false);
		GroundMesh->SetRelativeLocation(FVector(RouteLengthCm * 0.5, 0.0, -10.0));
		GroundMesh->SetRelativeScale3D(FVector(RouteLengthCm / 100.0, 10.0, 0.2));
	}

	const double FinishHeightCm = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(
		Fixture, Fixture.RouteLengthMm) * 100.0;
	AddBlockoutMesh(TEXT("FinishMarker"), CubeMesh,
		FVector(RouteLengthCm, 0.0, FinishHeightCm + 65.0),
		FVector(0.25, 0.15, 1.3), FLinearColor(0.88f, 0.93f, 0.98f));

	for (const FNaiwaP5RouteEvent& Event : Fixture.Events)
	{
		const double X = static_cast<double>(Event.DistanceMm) / 10.0;
		const double SurfaceZ = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(Fixture, Event.DistanceMm) * 100.0;
		if (Event.Kind == ENaiwaP5RouteEventKind::Tree)
		{
			if (bA0ArtSample && LoadA0Material(TEXT("M_P5_A0_TreeA")))
			{
				AddA0Sprite(FName(*FString::Printf(TEXT("TreeArt_%d"), Event.Id)),
					LoadA0Material(TEXT("M_P5_A0_TreeA")), FVector(X, -10.0, SurfaceZ + 220.0),
					FVector2D(220.0, 440.0));
			}
			else
			{
				AddBlockoutMesh(FName(*FString::Printf(TEXT("Tree_%d"), Event.Id)), ConeMesh,
					FVector(X, 0.0, SurfaceZ + 60.0), FVector(0.8, 0.8, 1.2), TreeColor);
			}
		}
		else if (Event.Kind == ENaiwaP5RouteEventKind::Rock)
		{
			if (bA0ArtSample && LoadA0Material(TEXT("M_P5_A0_RockA")))
			{
				AddA0Sprite(FName(*FString::Printf(TEXT("RockArt_%d"), Event.Id)),
					LoadA0Material(TEXT("M_P5_A0_RockA")), FVector(X, -10.0, SurfaceZ + 70.0),
					FVector2D(180.0, 180.0));
			}
			else
			{
				AddBlockoutMesh(FName(*FString::Printf(TEXT("Rock_%d"), Event.Id)), CubeMesh,
					FVector(X, 0.0, SurfaceZ + 35.0), FVector(0.8, 0.9, 0.7), FLinearColor(0.34f, 0.38f, 0.40f, 1.0f));
			}
		}
		else if (Event.Kind == ENaiwaP5RouteEventKind::Coin)
		{
			UStaticMeshComponent* Coin = AddBlockoutMesh(
				FName(*FString::Printf(TEXT("Coin_%d"), Event.Id)),
				CylinderMesh ? CylinderMesh : SphereMesh,
				FVector(X, 13.0, SurfaceZ + 100.0),
				CylinderMesh ? FVector(0.48, 0.08, 0.48) : FVector(0.28, 0.28, 0.28), CoinColor);
			if (Coin && CylinderMesh)
			{
				Coin->SetRelativeRotation(FRotator(0.0, 0.0, 90.0));
			}
		}
		else if (Event.Kind == ENaiwaP5RouteEventKind::Kangaroo)
		{
			UMaterialInterface* MountMaterial = bA0ArtSample
				? LoadA0Material(TEXT("M_P5_A0_KangarooRideAtlas")) : nullptr;
			UStaticMeshComponent* Mount = MountMaterial
				? AddA0Sprite(FName(*FString::Printf(TEXT("KangarooArt_%d"), Event.Id)), MountMaterial,
					FVector(X, 15.0, SurfaceZ + 110.0), FVector2D(200.0, 235.0))
				: nullptr;
			if (Mount)
			{
				if (UMaterialInstanceDynamic* MountMID = UMaterialInstanceDynamic::Create(MountMaterial, this))
				{
					// A5 idle pose (top-left cell) from the WIP atlas; the sample remains non-production.
					MountMID->SetVectorParameterValue(TEXT("A0UVScale"), FLinearColor(341.0 / 1024.0, 0.5, 0.0, 0.0));
					MountMID->SetVectorParameterValue(TEXT("A0UVOffset"), FLinearColor(0.0, 0.5, 0.0, 0.0));
					Mount->SetMaterial(0, MountMID);
				}
			}
			else
			{
				AddBlockoutMesh(FName(*FString::Printf(TEXT("Kangaroo_%d"), Event.Id)), SphereMesh,
					FVector(X, 0.0, SurfaceZ + 80.0), FVector(0.55, 0.45, 0.70),
					FLinearColor(0.42f, 0.72f, 0.46f, 1.0f));
			}
		}
		else if (Event.Kind == ENaiwaP5RouteEventKind::CliffLaunch)
		{
			AddBlockoutMesh(FName(*FString::Printf(TEXT("CliffLip_%d"), Event.Id)), CubeMesh,
				FVector(X, 0.0, SurfaceZ + 18.0), FVector(0.16, 10.5, 0.38),
				FLinearColor(0.96f, 0.78f, 0.36f, 1.0f));
		}
		else if (Event.Kind == ENaiwaP5RouteEventKind::CliffGap)
		{
			const double GapWidthCm = static_cast<double>(Event.WidthMm) / 10.0;
			AddBlockoutMesh(FName(*FString::Printf(TEXT("CliffVoid_%d"), Event.Id)), CubeMesh,
				FVector(X, 0.0, SurfaceZ - 340.0), FVector(GapWidthCm / 100.0, 10.0, 6.0),
				FLinearColor(0.035f, 0.055f, 0.085f, 1.0f));
		}
	}

	if (bM2Route)
	{
		if (bA0ArtSample)
		{
			AvalancheWallMesh = AddA0Sprite(TEXT("AvalancheCloudArt"),
				LoadA0Material(TEXT("M_P5_A0_AvalancheCloud")), FVector::ZeroVector, FVector2D(1200.0, 1200.0));
		AvalancheCrestMesh = AddA0Sprite(TEXT("AvalancheRimArt"),
				LoadA0Material(TEXT("M_P5_A0_AvalancheRim")), FVector::ZeroVector, FVector2D(1600.0, 800.0));
			AvalanchePlumeMesh = AddA0Sprite(TEXT("AvalancheMistArt"),
				LoadA0Material(TEXT("M_P5_A0_SnowMist")), FVector::ZeroVector, FVector2D(1300.0, 320.0));
			if (AvalanchePlumeMesh)
			{
				AvalanchePlumeMesh->SetVisibility(false, true);
			}
			A0PlayerShadowMesh = AddA0Sprite(TEXT("PlayerShadowArt"),
				LoadA0Material(TEXT("M_P5_A0_SoftShadow")), FVector::ZeroVector, FVector2D(260.0, 130.0));
		}
		else
		{
			AvalancheWallMesh = AddBlockoutMesh(TEXT("AvalancheCarrier_Wall"), CubeMesh,
				FVector::ZeroVector, FVector(18.0, 14.0, 13.0), AvalancheColor);
			AvalancheCrestMesh = AddBlockoutMesh(TEXT("AvalancheCarrier_Crest"), SphereMesh,
				FVector::ZeroVector, FVector(4.0, 11.0, 4.0), AvalancheFoamColor);
			AvalanchePlumeMesh = AddBlockoutMesh(TEXT("AvalancheCarrier_Plume"), SphereMesh,
				FVector::ZeroVector, FVector(6.0, 10.0, 6.0), AvalancheColor);
		}
		SetActorTickEnabled(true);
	}
}

void ANaiwaP5PrototypeTrackActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!GetWorld())
	{
		return;
	}

	const UNaiwaP5RunSubsystem* RunSubsystem = GetWorld()->GetSubsystem<UNaiwaP5RunSubsystem>();
	if (!RunSubsystem)
	{
		return;
	}

	const FNaiwaP5RunState& State = RunSubsystem->GetState();
	const FNaiwaP5RouteFixture& Fixture = RunSubsystem->GetActivePrototypeFixture();
	const double PlayerXcm = State.DistanceMeters * 100.0;
	if (bA0ArtSample)
	{
		const int32 PlayerDistanceMm = FMath::Clamp(FMath::RoundToInt(State.DistanceMeters * 1000.0),
			0, Fixture.RouteLengthMm);
		const double PlayerSurfaceZcm = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(
			Fixture, PlayerDistanceMm) * 100.0;
		if (A0SkyMesh)
		{
			A0SkyMesh->SetRelativeLocation(FVector(Fixture.RouteLengthMm / 10.0 * 0.5,
				-1400.0, PlayerSurfaceZcm + 200.0));
		}
		for (int32 LayerIndex = 0; LayerIndex < 3; ++LayerIndex)
		{
			const double LayerTravelCm = PlayerXcm * (1.0 - A0BackgroundParallax[LayerIndex]);
			const int32 CenterTile = FMath::FloorToInt(LayerTravelCm / A0BackgroundTileWidthCm);
			const double TileRemainderCm = LayerTravelCm - CenterTile * A0BackgroundTileWidthCm;
			for (int32 TileIndex = 0; TileIndex < 3; ++TileIndex)
			{
				const int32 ComponentIndex = LayerIndex * 3 + TileIndex;
				if (A0BackgroundMeshes.IsValidIndex(ComponentIndex) && A0BackgroundMeshes[ComponentIndex])
				{
					const double TileCenterX = (CenterTile + TileIndex - 1) * A0BackgroundTileWidthCm +
						A0BackgroundTileWidthCm * 0.5 + TileRemainderCm;
					A0BackgroundMeshes[ComponentIndex]->SetRelativeLocation(FVector(
						TileCenterX, A0BackgroundY[LayerIndex],
						PlayerSurfaceZcm + A0BackgroundZ[LayerIndex]));
				}
			}
		}

		if (A0PlayerShadowMesh)
		{
			const int32 PlayerDistanceMm = FMath::Clamp(FMath::RoundToInt(State.DistanceMeters * 1000.0),
				0, Fixture.RouteLengthMm);
			const double PlayerSurfaceZcm = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(
				Fixture, PlayerDistanceMm) * 100.0;
			A0PlayerShadowMesh->SetRelativeLocation(FVector(PlayerXcm, 7.0, PlayerSurfaceZcm + 4.0));
			const double ShadowScale = FMath::Clamp(1.0 / (1.0 + State.JumpHeightMeters * 0.12), 0.55, 1.0);
			A0PlayerShadowMesh->SetRelativeScale3D(FVector(2.6 * ShadowScale, 0.8 * ShadowScale, 1.0));
		}
	}

	if (!AvalancheWallMesh || !AvalancheCrestMesh || !AvalanchePlumeMesh)
	{
		return;
	}
	const double FrontDistanceMeters = State.DistanceMeters - State.AvalancheGapMeters;
	const int32 SurfaceDistanceMm = FMath::Clamp(FMath::RoundToInt(FrontDistanceMeters * 1000.0),
		0, Fixture.RouteLengthMm);
	const double SurfaceHeightCm = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(
		Fixture, SurfaceDistanceMm) * 100.0;
	const double FrontXcm = FrontDistanceMeters * 100.0;

	if (bA0ArtSample)
	{
		AvalancheWallMesh->SetRelativeLocation(FVector(FrontXcm - 800.0, -20.0, SurfaceHeightCm + 580.0));
		AvalancheCrestMesh->SetRelativeLocation(FVector(FrontXcm - 450.0, -18.0, SurfaceHeightCm + 140.0));
		const int32 PlayerDistanceMm = FMath::Clamp(FMath::RoundToInt(State.DistanceMeters * 1000.0),
			0, Fixture.RouteLengthMm);
		const double PlayerSurfaceZcm = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(
			Fixture, PlayerDistanceMm) * 100.0;
		AvalanchePlumeMesh->SetRelativeLocation(FVector(PlayerXcm - 280.0, -24.0, PlayerSurfaceZcm + 120.0));
		AvalanchePlumeMesh->SetVisibility(State.bAvalancheWarning, true);
	}
	else
	{
		// The graybox wall's leading edge is aligned to the simulated avalanche front.
		AvalancheWallMesh->SetRelativeLocation(FVector(FrontXcm - 900.0, 0.0, SurfaceHeightCm + 650.0));
		AvalancheCrestMesh->SetRelativeLocation(FVector(FrontXcm - 200.0, 180.0, SurfaceHeightCm + 850.0));
		AvalanchePlumeMesh->SetRelativeLocation(FVector(FrontXcm - 650.0, -220.0, SurfaceHeightCm + 900.0));
	}
}

UStaticMeshComponent* ANaiwaP5PrototypeTrackActor::AddBlockoutMesh(const FName Name, UStaticMesh* Mesh,
	const FVector& Location, const FVector& Scale, const FLinearColor& Color)
{
	if (!Mesh)
	{
		return nullptr;
	}

	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, Name);
	if (!Component)
	{
		return nullptr;
	}
	Component->SetupAttachment(SceneRoot);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh);
	Component->SetRelativeLocation(Location);
	Component->SetRelativeScale3D(Scale);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(false);
	if (UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(ParentMaterial, Component))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			Component->SetMaterial(0, Material);
		}
	}
	Component->RegisterComponent();
	return Component;
}
