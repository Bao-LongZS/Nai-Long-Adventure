#include "NaiwaP5PrototypeTrackActor.h"

#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "../Core/NaiwaP5RunSubsystem.h"
#include "../Core/NaiwaP5TrackData.h"
#include "NaiwaP5A0ArtSampleUtils.h"
#include "NaiwaP5PrototypePawn.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor GroundColor(0.22f, 0.34f, 0.43f, 1.0f);
	const FLinearColor TreeColor(0.13f, 0.27f, 0.22f, 1.0f);
	const FLinearColor CoinColor(0.95f, 0.69f, 0.16f, 1.0f);
	const FLinearColor AvalancheColor(0.72f, 0.88f, 0.96f, 1.0f);
	const FLinearColor AvalancheFoamColor(0.91f, 0.97f, 1.0f, 1.0f);
	constexpr double A0BackgroundTileWidthCm = 3600.0;
	constexpr double A0BackgroundParallax[] = { 0.12, 0.30, 0.65 };
	constexpr double A0BackgroundY[] = { -1200.0, -900.0, -600.0 };
	constexpr double A0SnowBackdropHalfWidthCm = 1500.0;
	constexpr int32 A0SnowBackdropSampleIntervals = 32;
	constexpr double A0SnowBackdropSurfaceOffsetCm = 40.0;
	// Anchors place the actual artwork: far mountains and forest overlap, while the
	// near snow ridge is raised into the lower camera frame to connect the landscape.
	// Keep the landscape at the same screen height after moving the camera up to
	// lower the route in frame and expose more of the background layers.
	constexpr double A0BackgroundZ[] = { 635.0, 365.0, 440.0 };
	constexpr double A0BackgroundHeight[] = { 2700.0, 2700.0, 2700.0 };
	constexpr double A0SnowThicknessCm = 80.0;
	constexpr double A0SnowTextureRepeatCm = 450.0;
	constexpr double A0SnowBankDepthCm = 2600.0;
	constexpr double A0SnowBankTextureRepeatCm = 2400.0;
	constexpr double A0SnowBankYcm = -80.0;
	constexpr double A0ForegroundObstacleYcm = -35.0;
	constexpr double A0CliffArtYcm = -25.0;
	// Follow the local route profile, with a camera-relative lower-frame floor.
	// This avoids distant hills crossing the sky while closing blue gaps below the
	// near ridge when the current route is lower than the visible landscape.
	constexpr double A0SnowBackdropYcm = -1500.0;
	constexpr double A0SnowBackdropDepthCm = 5000.0;
	constexpr double A0CliffFaceYcm = -45.0;
	// The previous 4.5 m recess began below the normal camera's visible cutaway.
	// Raise the cavity under the lower lip while keeping its top below both ledges.
	constexpr double A0CliffFaceBelowLowerLipCm = 120.0;
	constexpr double A0CliffFaceDepthCm = 2200.0;

	void LogA0MaterialResolution(const FString& RequestedName, const TCHAR* Version,
		UMaterialInterface* Material)
	{
		if (!Material)
		{
			return;
		}
		static TSet<FName> LoggedMaterials;
		const FName Name(*RequestedName);
		if (LoggedMaterials.Contains(Name))
		{
			return;
		}
		LoggedMaterials.Add(Name);
		UE_LOG(LogTemp, Display,
			TEXT("NAIWA_P5_A0_PROBE_MATERIAL requested=%s source=%s resolved=%s blend=%d"),
			*RequestedName, Version, *Material->GetPathName(),
			static_cast<int32>(Material->GetBlendMode()));
	}

	void LogA0PrimitiveProbe(const TCHAR* Owner, UPrimitiveComponent* Component)
	{
		if (!Component)
		{
			UE_LOG(LogTemp, Display, TEXT("NAIWA_P5_A0_PROBE_COMPONENT owner=%s component=NULL"), Owner);
			return;
		}

		const UMaterialInterface* Material = Component->GetMaterial(0);
		const int32 BlendMode = Material ? static_cast<int32>(Material->GetBlendMode()) : -1;
		const FBoxSphereBounds Bounds = Component->Bounds;
		UE_LOG(LogTemp, Display,
			TEXT("NAIWA_P5_A0_PROBE_COMPONENT owner=%s component=%s visible=%d hidden=%d mobility=%d collision=%d loc=%s rot=%s scale=%s bounds_origin=%s bounds_extent=%s material=%s blend=%d"),
			Owner, *Component->GetName(), Component->IsVisible() ? 1 : 0,
			Component->bHiddenInGame ? 1 : 0, static_cast<int32>(Component->Mobility),
			static_cast<int32>(Component->GetCollisionEnabled()),
			*Component->GetComponentLocation().ToCompactString(),
			*Component->GetComponentRotation().ToCompactString(),
			*Component->GetComponentScale().ToCompactString(),
			*Bounds.Origin.ToCompactString(), *Bounds.BoxExtent.ToCompactString(),
			Material ? *Material->GetPathName() : TEXT("NULL"), BlendMode);
	}

	UMaterialInterface* LoadA0Material(const TCHAR* AssetName)
	{
		const FString Name(AssetName);
		const FString V06Path = FString::Printf(
			TEXT("/Game/Naiwa/P5/ArtSample_v06/Materials/%s.%s"), *Name, *Name);
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *V06Path))
		{
			LogA0MaterialResolution(Name, TEXT("v06"), Material);
			return Material;
		}
		const FString V05Path = FString::Printf(
			TEXT("/Game/Naiwa/P5/ArtSample_v05/Materials/%s.%s"), *Name, *Name);
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *V05Path))
		{
			LogA0MaterialResolution(Name, TEXT("v05"), Material);
			return Material;
		}
		const FString V04Path = FString::Printf(
			TEXT("/Game/Naiwa/P5/ArtSample_v04/Materials/%s.%s"), *Name, *Name);
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *V04Path);
		LogA0MaterialResolution(Name, TEXT("v04"), Material);
		return Material;
	}

	void AppendA0RouteSpan(const FNaiwaP5RouteFixture& Fixture, const int32 SpanStartMm,
		const int32 SpanEndMm, const int32 SampleStepMm, const double SurfaceYcm,
		const double ThicknessCm, const double TextureRepeatCm, TArray<FVector>& Vertices,
		TArray<int32>& Triangles, TArray<FVector2D>& UVs)
	{
		if (SpanEndMm <= SpanStartMm)
		{
			return;
		}

		const int32 IntervalCount = FMath::Max(1,
			FMath::DivideAndRoundUp(SpanEndMm - SpanStartMm, SampleStepMm));
		const int32 BaseVertex = Vertices.Num();
		for (int32 SampleIndex = 0; SampleIndex <= IntervalCount; ++SampleIndex)
		{
			const int32 DistanceMm = SampleIndex == IntervalCount
				? SpanEndMm
				: SpanStartMm + FMath::RoundToInt(
					static_cast<double>(SpanEndMm - SpanStartMm) * SampleIndex / IntervalCount);
			const double Xcm = static_cast<double>(DistanceMm) / 10.0;
			const double Zcm = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(Fixture, DistanceMm) * 100.0;
			const int32 BeforeMm = FMath::Max(SpanStartMm, DistanceMm - 500);
			const int32 AfterMm = FMath::Min(SpanEndMm, DistanceMm + 500);
			double Slope = 0.0;
			if (AfterMm > BeforeMm)
			{
				const double BeforeZcm = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(Fixture, BeforeMm) * 100.0;
				const double AfterZcm = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(Fixture, AfterMm) * 100.0;
				Slope = (AfterZcm - BeforeZcm) / (static_cast<double>(AfterMm - BeforeMm) / 10.0);
			}
			const double PitchRadians = FMath::Atan(Slope);
			const FVector SurfaceNormal(-FMath::Sin(PitchRadians), 0.0, FMath::Cos(PitchRadians));
			const FVector Top(Xcm, SurfaceYcm, Zcm);
			const FVector Bottom = Top - SurfaceNormal * ThicknessCm;
			const double U = Xcm / TextureRepeatCm;
			Vertices.Add(Top);
			Vertices.Add(Bottom);
			UVs.Add(FVector2D(U, 0.0));
			UVs.Add(FVector2D(U, ThicknessCm / TextureRepeatCm));

			if (SampleIndex > 0)
			{
				const int32 PreviousTop = BaseVertex + (SampleIndex - 1) * 2;
				const int32 PreviousBottom = PreviousTop + 1;
				const int32 CurrentTop = PreviousTop + 2;
				const int32 CurrentBottom = CurrentTop + 1;
				Triangles.Append({ PreviousTop, CurrentTop, PreviousBottom,
					CurrentTop, CurrentBottom, PreviousBottom });
			}
		}
	}

	void AppendA0SnowFillSpan(const FNaiwaP5RouteFixture& Fixture, const int32 SpanStartMm,
		const int32 SpanEndMm, const int32 SampleStepMm, const double FillYcm,
		const double TopOffsetZcm, const double DepthCm, const double TextureRepeatCm, TArray<FVector>& Vertices,
		TArray<int32>& Triangles, TArray<FVector2D>& UVs)
	{
		if (SpanEndMm <= SpanStartMm)
		{
			return;
		}

		const int32 IntervalCount = FMath::Max(1,
			FMath::DivideAndRoundUp(SpanEndMm - SpanStartMm, SampleStepMm));
		const int32 BaseVertex = Vertices.Num();
		for (int32 SampleIndex = 0; SampleIndex <= IntervalCount; ++SampleIndex)
		{
			const int32 DistanceMm = SampleIndex == IntervalCount
				? SpanEndMm
				: SpanStartMm + FMath::RoundToInt(
					static_cast<double>(SpanEndMm - SpanStartMm) * SampleIndex / IntervalCount);
			const double Xcm = static_cast<double>(DistanceMm) / 10.0;
			const double TopZcm = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(Fixture, DistanceMm) * 100.0 +
				TopOffsetZcm;
			const double SafeDepthCm = FMath::Max(100.0, DepthCm);
			const double BottomZcm = TopZcm - SafeDepthCm;
			Vertices.Add(FVector(Xcm, FillYcm, TopZcm));
			Vertices.Add(FVector(Xcm, FillYcm, BottomZcm));
			// The route surface keeps a fine 4.5 m repeat. Large snow side faces use
			// one vertically stretched tile and a broad horizontal repeat so the
			// same small texture does not form a stack of horizontal bands.
			UVs.Add(FVector2D(Xcm / TextureRepeatCm, 1.0));
			UVs.Add(FVector2D(Xcm / TextureRepeatCm, 0.0));

			if (SampleIndex > 0)
			{
				const int32 PreviousTop = BaseVertex + (SampleIndex - 1) * 2;
				const int32 PreviousBottom = PreviousTop + 1;
				const int32 CurrentTop = PreviousTop + 2;
				const int32 CurrentBottom = CurrentTop + 1;
				Triangles.Append({ PreviousTop, CurrentTop, PreviousBottom,
					CurrentTop, CurrentBottom, PreviousBottom });
			}
		}
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
		FVector(RouteLengthCm * 0.5, -2200.0, 380.0),
		FVector(CoveredRouteCm / 100.0, 80.0, 1.0), FLinearColor(0.50f, 0.70f, 0.82f, 1.0f));
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
	A0BackgroundMaterials.Reset();
	A0BackgroundMeshes.Reserve(9);
	A0BackgroundMaterials.Reserve(3);
	for (int32 LayerIndex = 0; LayerIndex < 3; ++LayerIndex)
	{
		UMaterialInterface* ParentMaterial = LoadA0Material(MaterialNames[LayerIndex]);
		UMaterialInstanceDynamic* Material = ParentMaterial
			? UMaterialInstanceDynamic::Create(ParentMaterial, this) : nullptr;
		A0BackgroundMaterials.Add(Material);
		for (int32 TileIndex = 0; TileIndex < 3; ++TileIndex)
		{
			const double CenterX = (TileIndex - 1) * A0BackgroundTileWidthCm;
			UStaticMeshComponent* Tile = AddA0Sprite(
				FName(*FString::Printf(TEXT("A0Background_%d_%d"), LayerIndex, TileIndex)),
				Material,
				FVector(CenterX, A0BackgroundY[LayerIndex], A0BackgroundZ[LayerIndex]),
				FVector2D(A0BackgroundTileWidthCm, A0BackgroundHeight[LayerIndex]));
			A0BackgroundMeshes.Add(Tile);
		}
	}

}

bool ANaiwaP5PrototypeTrackActor::BuildA0RouteSurface(const FNaiwaP5RouteFixture& Fixture)
{
	UMaterialInterface* SnowMaterial = LoadA0Material(TEXT("M_P5_A0_SnowGround"));
	if (!SnowMaterial)
	{
		return false;
	}

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	TArray<FVector> BankVertices;
	TArray<int32> BankTriangles;
	TArray<FVector2D> BankUVs;
	constexpr int32 RouteSampleStepMm = 1000;
	constexpr double SurfaceYcm = 5.0;
	constexpr double BankYcm = A0SnowBankYcm;

	for (const FNaiwaP5RouteSegment& Segment : Fixture.Segments)
	{
		int32 SpanStartMm = Segment.StartDistanceMm;
		for (const FNaiwaP5RouteEvent& Event : Fixture.Events)
		{
			const bool bGap = Event.Kind == ENaiwaP5RouteEventKind::IceGap ||
				Event.Kind == ENaiwaP5RouteEventKind::CliffGap;
			if (!bGap || Event.WidthMm <= 0 || Event.DistanceMm < Segment.StartDistanceMm ||
				Event.DistanceMm > Segment.EndDistanceMm)
			{
				continue;
			}

			const int32 GapStartMm = FMath::Clamp(Event.DistanceMm - Event.WidthMm / 2,
				Segment.StartDistanceMm, Segment.EndDistanceMm);
			const int32 GapEndMm = FMath::Clamp(Event.DistanceMm + Event.WidthMm / 2,
				Segment.StartDistanceMm, Segment.EndDistanceMm);
			if (GapStartMm > SpanStartMm)
			{
				AppendA0RouteSpan(Fixture, SpanStartMm, GapStartMm, RouteSampleStepMm,
					SurfaceYcm, A0SnowThicknessCm, A0SnowTextureRepeatCm, Vertices, Triangles, UVs);
				AppendA0SnowFillSpan(Fixture, SpanStartMm, GapStartMm, RouteSampleStepMm,
					BankYcm, 0.0, A0SnowBankDepthCm, A0SnowBankTextureRepeatCm,
					BankVertices, BankTriangles, BankUVs);
			}
			SpanStartMm = FMath::Max(SpanStartMm, GapEndMm);
		}
		if (SpanStartMm < Segment.EndDistanceMm)
		{
			AppendA0RouteSpan(Fixture, SpanStartMm, Segment.EndDistanceMm, RouteSampleStepMm,
				SurfaceYcm, A0SnowThicknessCm, A0SnowTextureRepeatCm, Vertices, Triangles, UVs);
			AppendA0SnowFillSpan(Fixture, SpanStartMm, Segment.EndDistanceMm, RouteSampleStepMm,
				BankYcm, 0.0, A0SnowBankDepthCm, A0SnowBankTextureRepeatCm,
				BankVertices, BankTriangles, BankUVs);
		}
	}

	if (Vertices.IsEmpty() || Triangles.IsEmpty())
	{
		return false;
	}
	{
		TArray<FVector> BackdropVertices;
		TArray<int32> BackdropTriangles;
		TArray<FVector2D> BackdropUVs;
		BackdropVertices.Reserve((A0SnowBackdropSampleIntervals + 1) * 2);
		BackdropTriangles.Reserve(A0SnowBackdropSampleIntervals * 6);
		BackdropUVs.Reserve((A0SnowBackdropSampleIntervals + 1) * 2);
		for (int32 SampleIndex = 0; SampleIndex <= A0SnowBackdropSampleIntervals; ++SampleIndex)
		{
			const double Alpha = static_cast<double>(SampleIndex) / A0SnowBackdropSampleIntervals;
			const double LocalXcm = FMath::Lerp(-A0SnowBackdropHalfWidthCm,
				A0SnowBackdropHalfWidthCm, Alpha);
			BackdropVertices.Add(FVector(LocalXcm, 0.0, 0.0));
			BackdropVertices.Add(FVector(LocalXcm, 0.0, -A0SnowBackdropDepthCm));
			BackdropUVs.Add(FVector2D(LocalXcm / A0SnowBankTextureRepeatCm, 1.0));
			BackdropUVs.Add(FVector2D(LocalXcm / A0SnowBankTextureRepeatCm, 0.0));
			if (SampleIndex > 0)
			{
				const int32 PreviousTop = (SampleIndex - 1) * 2;
				const int32 PreviousBottom = PreviousTop + 1;
				const int32 CurrentTop = PreviousTop + 2;
				const int32 CurrentBottom = CurrentTop + 1;
				BackdropTriangles.Append({ PreviousTop, CurrentTop, PreviousBottom,
					CurrentTop, CurrentBottom, PreviousBottom });
			}
		}
		A0SnowBackdropMesh = NewObject<UProceduralMeshComponent>(this,
			TEXT("A0RouteProfileSnowBackdrop"));
		if (A0SnowBackdropMesh)
		{
			A0SnowBackdropMesh->SetupAttachment(SceneRoot);
			A0SnowBackdropMesh->SetMobility(EComponentMobility::Movable);
			A0SnowBackdropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			A0SnowBackdropMesh->SetCastShadow(false);
			A0SnowBackdropMesh->RegisterComponent();
			A0SnowBackdropMesh->CreateMeshSection(0, BackdropVertices, BackdropTriangles,
				TArray<FVector>(), BackdropUVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
			A0SnowBackdropMesh->SetMaterial(0, SnowMaterial);
		}
	}
	if (!BankVertices.IsEmpty() && !BankTriangles.IsEmpty())
	{
		UProceduralMeshComponent* SnowBankFillMesh = NewObject<UProceduralMeshComponent>(this,
			TEXT("A0SnowBankFill"));
		if (SnowBankFillMesh)
		{
			SnowBankFillMesh->SetupAttachment(SceneRoot);
			SnowBankFillMesh->SetMobility(EComponentMobility::Movable);
			SnowBankFillMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SnowBankFillMesh->SetCastShadow(false);
			SnowBankFillMesh->RegisterComponent();
			SnowBankFillMesh->CreateMeshSection(0, BankVertices, BankTriangles,
				TArray<FVector>(), BankUVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
			SnowBankFillMesh->SetMaterial(0, SnowMaterial);
		}
	}

	A0RouteSurfaceMesh = NewObject<UProceduralMeshComponent>(this, TEXT("A0ContinuousRouteSurface"));
	if (!A0RouteSurfaceMesh)
	{
		return false;
	}
	A0RouteSurfaceMesh->SetupAttachment(SceneRoot);
	A0RouteSurfaceMesh->SetMobility(EComponentMobility::Movable);
	A0RouteSurfaceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	A0RouteSurfaceMesh->SetCastShadow(false);
	A0RouteSurfaceMesh->RegisterComponent();
	A0RouteSurfaceMesh->CreateMeshSection(0, Vertices, Triangles, TArray<FVector>(), UVs,
		TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	A0RouteSurfaceMesh->SetMaterial(0, SnowMaterial);
	return true;
}

void ANaiwaP5PrototypeTrackActor::HideLegacyA0SampleActors()
{
	if (!GetWorld())
	{
		return;
	}
	const FName GeneratedA0Tag(TEXT("NaiwaP5A0ArtSampleV05Generated"));
	int32 LegacyActorCount = 0;
	int32 HiddenActorCount = 0;
	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!Actor || Actor == this)
		{
			continue;
		}
		bool bIsLegacyA0Art = Actor->ActorHasTag(GeneratedA0Tag);
		bool bLabelMatch = false;
#if WITH_EDITOR
		// Older saved sample maps may predate the generated Actor Tag. Their
		// stable P5A0 label still identifies legacy visuals in editor/PIE builds.
		bLabelMatch = Actor->GetActorLabel().StartsWith(TEXT("P5A0_"));
		bIsLegacyA0Art |= bLabelMatch;
#endif
		if (!bIsLegacyA0Art)
		{
			continue;
		}
		++LegacyActorCount;
		const bool bPreserved = Actor->IsA<APlayerStart>() || Actor->IsA<ADirectionalLight>();
		UE_LOG(LogTemp, Display,
			TEXT("NAIWA_P5_A0_PROBE_LEGACY actor=%s class=%s tag_match=%d label_match=%d preserved=%d action=%s"),
			*Actor->GetName(), *Actor->GetClass()->GetName(),
			Actor->ActorHasTag(GeneratedA0Tag) ? 1 : 0, bLabelMatch ? 1 : 0,
			bPreserved ? 1 : 0, bPreserved ? TEXT("keep") : TEXT("hide"));
		// The authored map assets coexist with rebuilt runtime layers. Hide those
		// generated legacy visuals in every target, while preserving spawn and light.
		if (bPreserved)
		{
			continue;
		}
		Actor->SetActorEnableCollision(false);
		Actor->SetActorHiddenInGame(true);
		++HiddenActorCount;
	}
	UE_LOG(LogTemp, Display,
		TEXT("NAIWA_P5_A0_LEGACY_CLEANUP map=%s legacy=%d hidden=%d"),
		*GetWorld()->GetMapName(), LegacyActorCount, HiddenActorCount);
}

void ANaiwaP5PrototypeTrackActor::EmitA0VisualProbe(const FString& Trigger)
{
	if (!bA0ArtSample || !GetWorld())
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
	const AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	const UCameraComponent* PlayerCamera = PlayerPawn
		? PlayerPawn->FindComponentByClass<UCameraComponent>() : nullptr;
	int32 ViewWidthPx = 0;
	int32 ViewHeightPx = 0;
	if (PlayerController)
	{
		PlayerController->GetViewportSize(ViewWidthPx, ViewHeightPx);
	}
	UE_LOG(LogTemp, Display,
		TEXT("NAIWA_P5_A0_PROBE trigger=%s map=%s game_mode=%s route=%s route_mm=%d route_version=%d distance_m=%.3f speed_mps=%.3f gap_m=%.3f boost_level=%d boost_s=%.2f riding=%d airborne=%d fallen=%d charging=%d jump_height_m=%.3f avalanche_warning=%d viewport=%dx%d camera=%s projection=%d ortho_width=%.2f camera_loc=%s"),
		*Trigger, *GetWorld()->GetMapName(),
		GameMode ? *GameMode->GetClass()->GetName() : TEXT("NULL"),
		*Fixture.RouteId.ToString(), Fixture.RouteLengthMm, Fixture.Version, State.DistanceMeters,
		State.PlayerSpeedMetersPerSecond, State.AvalancheGapMeters,
		State.TrickBoostLevel, State.TrickBoostSeconds,
		State.bRiding ? 1 : 0, State.bAirborne ? 1 : 0, State.bFallen ? 1 : 0,
		State.bJumpCharging ? 1 : 0, State.JumpHeightMeters,
		State.bAvalancheWarning ? 1 : 0, ViewWidthPx, ViewHeightPx,
		PlayerCamera ? *PlayerCamera->GetName() : TEXT("NULL"),
		PlayerCamera ? static_cast<int32>(PlayerCamera->ProjectionMode) : -1,
		PlayerCamera ? PlayerCamera->OrthoWidth : 0.0f,
		PlayerCamera ? *PlayerCamera->GetComponentLocation().ToCompactString() : TEXT("NULL"));

	TArray<UPrimitiveComponent*> TrackComponents;
	GetComponents<UPrimitiveComponent>(TrackComponents);
	for (UPrimitiveComponent* Component : TrackComponents)
	{
		if (!Component)
		{
			continue;
		}
		const FString Name = Component->GetName();
		const bool bRelevant = Name.StartsWith(TEXT("A0")) ||
			Name.Contains(TEXT("Avalanche")) || Name.Contains(TEXT("CliffFace")) ||
			Name.Contains(TEXT("CliffShoulder")) || Name.Contains(TEXT("CliffLipRock")) ||
			Name.Contains(TEXT("PlayerShadow")) || Name.Contains(TEXT("Kangaroo"));
		if (bRelevant)
		{
			LogA0PrimitiveProbe(TEXT("track"), Component);
		}
	}

	if (PlayerPawn)
	{
		TArray<UPrimitiveComponent*> PawnComponents;
		PlayerPawn->GetComponents<UPrimitiveComponent>(PawnComponents);
		for (UPrimitiveComponent* Component : PawnComponents)
		{
			LogA0PrimitiveProbe(TEXT("pawn"), Component);
		}
	}

	const ANaiwaP5PrototypePawn* P5Pawn = Cast<ANaiwaP5PrototypePawn>(PlayerPawn);
	FVector RiderSeatWorld = FVector::ZeroVector;
	FVector MountSaddleWorld = FVector::ZeroVector;
	const bool bAnchorPointsValid = P5Pawn && P5Pawn->GetA0RideAnchorProbeWorldPoints(
		RiderSeatWorld, MountSaddleWorld);
	FVector2D RiderSeatScreen = FVector2D::ZeroVector;
	FVector2D MountSaddleScreen = FVector2D::ZeroVector;
	const bool bProjected = bAnchorPointsValid && PlayerController &&
		PlayerController->ProjectWorldLocationToScreen(RiderSeatWorld, RiderSeatScreen) &&
		PlayerController->ProjectWorldLocationToScreen(MountSaddleWorld, MountSaddleScreen);
	const FVector WorldDelta = RiderSeatWorld - MountSaddleWorld;
	const FVector2D ScreenDelta = RiderSeatScreen - MountSaddleScreen;
	UE_LOG(LogTemp, Display,
		TEXT("NAIWA_P5_A0_PROBE_RIDE_LINK trigger=%s riding=%d valid=%d projected=%d seat_world=%s saddle_world=%s world_delta_cm=%s delta_px=(%.2f,%.2f) delta_px_len=%.2f"),
		*Trigger, State.bRiding ? 1 : 0, bAnchorPointsValid ? 1 : 0, bProjected ? 1 : 0,
		*RiderSeatWorld.ToCompactString(), *MountSaddleWorld.ToCompactString(),
		*WorldDelta.ToCompactString(), ScreenDelta.X, ScreenDelta.Y, ScreenDelta.Size());
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
		const bool bUseContinuousA0Route = bA0ArtSample && BuildA0RouteSurface(Fixture);
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
			if (!bUseContinuousA0Route)
			{
				for (int32 SpanIndex = 0; SpanIndex < Spans.Num(); ++SpanIndex)
				{
					const TPair<int32, int32>& Span = Spans[SpanIndex];
					const int32 MaxVisualSpanMm = Span.Value - Span.Key;
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
							UStaticMeshComponent* GroundSection = AddBlockoutMesh(
								FName(*FString::Printf(TEXT("RouteSurface_%02d_%02d_%02d"), SectionIndex, SpanIndex, ChunkIndex)),
								CubeMesh, FVector((StartX + EndX) * 0.5, 0.0, (StartZ + EndZ) * 0.5 - 10.0),
								FVector((SpanLengthCm + 20.0) / 100.0, 10.0, 0.20), SurfaceColor);
							if (GroundSection)
							{
								GroundSection->SetRelativeRotation(FRotator(PitchDegrees, 0.0, 0.0));
							}
						}
						ChunkStartMm = ChunkEndMm;
						++ChunkIndex;
					}
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
			const int32 TreeVariant = FMath::Abs(Event.Id) % 3;
			const TCHAR* TreeMaterialName = TreeVariant == 1 ? TEXT("M_P5_A0_TreeB") :
				TreeVariant == 2 ? TEXT("M_P5_A0_TreeC") : TEXT("M_P5_A0_TreeA");
			UMaterialInterface* TreeMaterial = bA0ArtSample ? LoadA0Material(TreeMaterialName) : nullptr;
			if (!TreeMaterial && bA0ArtSample && TreeVariant != 0)
			{
				TreeMaterial = LoadA0Material(TEXT("M_P5_A0_TreeA"));
			}
			if (TreeMaterial)
			{
				AddA0Sprite(FName(*FString::Printf(TEXT("TreeArt_%d"), Event.Id)),
					TreeMaterial, FVector(X, A0ForegroundObstacleYcm, SurfaceZ + 220.0),
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
			const int32 RockVariant = FMath::Abs(Event.Id) % 2;
			const TCHAR* RockMaterialName = RockVariant == 1 ? TEXT("M_P5_A0_RockB") :
				TEXT("M_P5_A0_RockA");
			UMaterialInterface* RockMaterial = bA0ArtSample ? LoadA0Material(RockMaterialName) : nullptr;
			if (!RockMaterial && bA0ArtSample && RockVariant != 0)
			{
				RockMaterial = LoadA0Material(TEXT("M_P5_A0_RockA"));
			}
			if (RockMaterial)
			{
				AddA0Sprite(FName(*FString::Printf(TEXT("RockArt_%d"), Event.Id)),
					RockMaterial, FVector(X, A0ForegroundObstacleYcm, SurfaceZ + 70.0),
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
				CylinderMesh ? FVector(0.48, 0.48, 0.08) : FVector(0.28, 0.28, 0.28), CoinColor);
			if (Coin && CylinderMesh)
			{
				Coin->SetRelativeRotation(FRotator(0.0, 0.0, 90.0));
			}
		}
		else if (Event.Kind == ENaiwaP5RouteEventKind::Kangaroo)
		{
			bool bUseCandidateMount = false;
			UMaterialInterface* MountMaterial = bA0ArtSample
				? LoadA0Material(TEXT("M_P5_A5_KangarooMount_v02_WIP")) : nullptr;
			if (MountMaterial)
			{
				bUseCandidateMount = true;
			}
			else if (bA0ArtSample)
			{
				MountMaterial = LoadA0Material(TEXT("M_P5_A0_KangarooRideAtlas"));
			}
			UStaticMeshComponent* Mount = MountMaterial
				? AddA0Sprite(FName(*FString::Printf(TEXT("KangarooArt_%d"), Event.Id)), MountMaterial,
					FVector(X, 15.0, SurfaceZ + (bUseCandidateMount ? 130.0 : 85.0)),
					bUseCandidateMount ? FVector2D(220.0, 258.0) : FVector2D(200.0, 300.0))
				: nullptr;
			if (Mount)
			{
				A0KangarooEventMeshes.Add(Mount);
				if (UMaterialInstanceDynamic* MountMID = UMaterialInstanceDynamic::Create(MountMaterial, this))
				{
					if (bUseCandidateMount)
					{
						MountMID->SetVectorParameterValue(TEXT("A0UVScale"),
							FLinearColor(285.0 / 1024.0, -334.0 / 1024.0, 0.0, 0.0));
						MountMID->SetVectorParameterValue(TEXT("A0UVOffset"),
							FLinearColor(28.0 / 1024.0, 1.0 - 150.0 / 1024.0, 0.0, 0.0));
					}
					else
					{
						// Preserve the v05 mount fallback's horizontal correction.
						MountMID->SetVectorParameterValue(TEXT("A0UVScale"), FLinearColor(-341.0 / 1024.0, -0.5, 0.0, 0.0));
						MountMID->SetVectorParameterValue(TEXT("A0UVOffset"), FLinearColor(341.0 / 1024.0, 1.0, 0.0, 0.0));
					}
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
			if (!bA0ArtSample)
			{
				AddBlockoutMesh(FName(*FString::Printf(TEXT("CliffLip_%d"), Event.Id)), CubeMesh,
					FVector(X, 0.0, SurfaceZ + 18.0), FVector(0.16, 10.5, 0.38),
					FLinearColor(0.96f, 0.78f, 0.36f, 1.0f));
			}
		}
		else if (Event.Kind == ENaiwaP5RouteEventKind::CliffGap)
		{
			if (bA0ArtSample)
			{
				// Keep the high gap open. A dark lower cavity provides depth without
				// drawing a ramp-shaped wall from the takeoff lip to the landing lip.
				const int32 GapStartMm = FMath::Clamp(Event.DistanceMm - Event.WidthMm / 2,
					0, Fixture.RouteLengthMm);
				const int32 GapEndMm = FMath::Clamp(Event.DistanceMm + Event.WidthMm / 2,
					0, Fixture.RouteLengthMm);
				const double GapStartXcm = static_cast<double>(GapStartMm) / 10.0;
				const double GapEndXcm = static_cast<double>(GapEndMm) / 10.0;
				const double GapWidthCm = FMath::Max(0.0, GapEndXcm - GapStartXcm);
				const double TakeoffZcm = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(
					Fixture, GapStartMm) * 100.0;
				const double LandingZcm = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(
					Fixture, GapEndMm) * 100.0;
				const double FaceTopZcm = FMath::Min(TakeoffZcm, LandingZcm) -
					A0CliffFaceBelowLowerLipCm;
				const double BottomZcm = FaceTopZcm - A0CliffFaceDepthCm;
				const int32 FaceIntervals = FMath::Max(1,
					FMath::DivideAndRoundUp<int32>(FMath::RoundToInt(GapWidthCm), 300));
				TArray<FVector> FaceVertices;
				TArray<int32> FaceTriangles;
				TArray<FVector2D> FaceUVs;
				FaceVertices.Reserve((FaceIntervals + 1) * 2);
				FaceUVs.Reserve((FaceIntervals + 1) * 2);
				for (int32 SampleIndex = 0; SampleIndex <= FaceIntervals; ++SampleIndex)
				{
					const double Alpha = static_cast<double>(SampleIndex) / FaceIntervals;
					const double FaceXcm = FMath::Lerp(GapStartXcm, GapEndXcm, Alpha);
					FaceVertices.Add(FVector(FaceXcm, A0CliffFaceYcm, FaceTopZcm));
					FaceVertices.Add(FVector(FaceXcm, A0CliffFaceYcm, BottomZcm));
					FaceUVs.Add(FVector2D(FaceXcm / 300.0, FaceTopZcm / 300.0));
					FaceUVs.Add(FVector2D(FaceXcm / 300.0, BottomZcm / 300.0));
					if (SampleIndex > 0)
					{
						const int32 PreviousTop = (SampleIndex - 1) * 2;
						const int32 PreviousBottom = PreviousTop + 1;
						const int32 CurrentTop = PreviousTop + 2;
						const int32 CurrentBottom = CurrentTop + 1;
						FaceTriangles.Append({ PreviousTop, SampleIndex * 2, PreviousBottom,
							SampleIndex * 2, CurrentBottom, PreviousBottom });
					}
				}

				UProceduralMeshComponent* CliffFace = NewObject<UProceduralMeshComponent>(this,
					FName(*FString::Printf(TEXT("CliffFace_%d"), Event.Id)));
				if (CliffFace && FaceTriangles.Num() > 0)
				{
					CliffFace->SetupAttachment(SceneRoot);
					CliffFace->SetMobility(EComponentMobility::Movable);
					CliffFace->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					CliffFace->SetCastShadow(false);
					CliffFace->RegisterComponent();
					CliffFace->CreateMeshSection(0, FaceVertices, FaceTriangles,
						TArray<FVector>(), FaceUVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
					if (UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(nullptr,
						TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
					{
						if (UMaterialInstanceDynamic* FaceMaterial =
							UMaterialInstanceDynamic::Create(ParentMaterial, this))
						{
							FaceMaterial->SetVectorParameterValue(TEXT("Color"),
								FLinearColor(0.075f, 0.095f, 0.13f, 1.0f));
							CliffFace->SetMaterial(0, FaceMaterial);
						}
					}
				}

				// The textured rock accents define both lips. Tall blockout shoulders
				// read as black pillars in the side camera, so the cavity stays open.
				if (UMaterialInterface* CliffMaterial = LoadA0Material(TEXT("M_P5_A0_RockA")))
				{
					AddA0Sprite(FName(*FString::Printf(TEXT("CliffLipRockArt_%d_Start"), Event.Id)),
						CliffMaterial, FVector(GapStartXcm + 150.0, A0CliffArtYcm,
							TakeoffZcm - 140.0), FVector2D(300.0, 280.0));
					AddA0Sprite(FName(*FString::Printf(TEXT("CliffLipRockArt_%d_End"), Event.Id)),
						CliffMaterial, FVector(GapEndXcm - 150.0, A0CliffArtYcm,
							LandingZcm - 140.0), FVector2D(300.0, 280.0));
				}
			}
			else
			{
				const double GapWidthCm = static_cast<double>(Event.WidthMm) / 10.0;
				AddBlockoutMesh(FName(*FString::Printf(TEXT("CliffVoid_%d"), Event.Id)), CubeMesh,
					FVector(X, 0.0, SurfaceZ - 340.0), FVector(GapWidthCm / 100.0, 10.0, 6.0),
					FLinearColor(0.035f, 0.055f, 0.085f, 1.0f));
			}
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
		for (UStaticMeshComponent* KangarooEventMesh : A0KangarooEventMeshes)
		{
			if (KangarooEventMesh)
			{
				// Once riding starts, only the attached player/mount layers remain visible.
				KangarooEventMesh->SetVisibility(!State.bRiding, true);
				KangarooEventMesh->SetHiddenInGame(State.bRiding);
			}
		}
		const int32 PlayerDistanceMm = FMath::Clamp(FMath::RoundToInt(State.DistanceMeters * 1000.0),
			0, Fixture.RouteLengthMm);
		const double PlayerSurfaceZcm = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(
			Fixture, PlayerDistanceMm) * 100.0;
		const double PlayerViewZcm = PlayerSurfaceZcm + State.JumpHeightMeters * 100.0;
		if (A0SkyMesh)
		{
			A0SkyMesh->SetRelativeLocation(FVector(Fixture.RouteLengthMm / 10.0 * 0.5,
				-2200.0, PlayerViewZcm + 380.0));
		}
		for (int32 LayerIndex = 0; LayerIndex < 3; ++LayerIndex)
		{
			// Keep the quads centered on the camera so they always cover its view.
			// Apply parallax to the sampled image instead of moving the geometry away
			// from the camera (the old approach left the sky exposed late in the run).
			const double ParallaxU = FMath::Fmod(
				PlayerXcm * A0BackgroundParallax[LayerIndex] / A0BackgroundTileWidthCm, 1.0);
			if (A0BackgroundMaterials.IsValidIndex(LayerIndex) && A0BackgroundMaterials[LayerIndex])
			{
				A0BackgroundMaterials[LayerIndex]->SetVectorParameterValue(TEXT("A0UVOffset"),
					FLinearColor(ParallaxU, 0.0f, 0.0f, 0.0f));
			}
			for (int32 TileIndex = 0; TileIndex < 3; ++TileIndex)
			{
				const int32 ComponentIndex = LayerIndex * 3 + TileIndex;
				if (A0BackgroundMeshes.IsValidIndex(ComponentIndex) && A0BackgroundMeshes[ComponentIndex])
				{
					const double TileCenterX = PlayerXcm + (TileIndex - 1) * A0BackgroundTileWidthCm;
					A0BackgroundMeshes[ComponentIndex]->SetRelativeLocation(FVector(
						TileCenterX, A0BackgroundY[LayerIndex],
						PlayerSurfaceZcm + A0BackgroundZ[LayerIndex]));
				}
			}
		}
		if (A0SnowBackdropMesh)
		{
			double CameraHalfWidthCm = A0SnowBackdropHalfWidthCm;
			if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
			{
				if (const APawn* PlayerPawn = PlayerController->GetPawn())
				{
					if (const UCameraComponent* PlayerCamera =
						PlayerPawn->FindComponentByClass<UCameraComponent>())
					{
						if (PlayerCamera->ProjectionMode == ECameraProjectionMode::Orthographic)
						{
							CameraHalfWidthCm = PlayerCamera->OrthoWidth * 0.5;
						}
					}
				}
			}
			const double BackdropHalfWidthCm = CameraHalfWidthCm + 350.0;
			TArray<FVector> BackdropVertices;
			TArray<FVector2D> BackdropUVs;
			BackdropVertices.Reserve((A0SnowBackdropSampleIntervals + 1) * 2);
			BackdropUVs.Reserve((A0SnowBackdropSampleIntervals + 1) * 2);
			for (int32 SampleIndex = 0; SampleIndex <= A0SnowBackdropSampleIntervals; ++SampleIndex)
			{
				const double Alpha = static_cast<double>(SampleIndex) / A0SnowBackdropSampleIntervals;
				const double LocalXcm = FMath::Lerp(-BackdropHalfWidthCm, BackdropHalfWidthCm, Alpha);
				const double SampleXcm = PlayerXcm + LocalXcm;
				const int32 SampleDistanceMm = FMath::Clamp(
					FMath::RoundToInt(SampleXcm * 10.0), 0, Fixture.RouteLengthMm);
				const double RouteProfileTopZcm = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(
					Fixture, SampleDistanceMm) * 100.0 - A0SnowBackdropSurfaceOffsetCm;
				const double TopZcm = RouteProfileTopZcm;
				const double BottomZcm = TopZcm - A0SnowBackdropDepthCm;
				BackdropVertices.Add(FVector(LocalXcm, 0.0, TopZcm));
				BackdropVertices.Add(FVector(LocalXcm, 0.0, BottomZcm));
				BackdropUVs.Add(FVector2D(SampleXcm / A0SnowBankTextureRepeatCm, 1.0));
				BackdropUVs.Add(FVector2D(SampleXcm / A0SnowBankTextureRepeatCm, 0.0));
			}
			A0SnowBackdropMesh->SetRelativeScale3D(FVector::OneVector);
			A0SnowBackdropMesh->SetRelativeLocation(FVector(PlayerXcm, A0SnowBackdropYcm, 0.0));
			A0SnowBackdropMesh->UpdateMeshSection(0, BackdropVertices,
				TArray<FVector>(), BackdropUVs, TArray<FColor>(), TArray<FProcMeshTangent>());
		}

		if (A0PlayerShadowMesh)
		{
			A0PlayerShadowMesh->SetRelativeLocation(FVector(PlayerXcm, 7.0, PlayerSurfaceZcm + 4.0));
			const double ShadowScale = FMath::Clamp(1.0 / (1.0 + State.JumpHeightMeters * 0.12), 0.55, 1.0);
			A0PlayerShadowMesh->SetRelativeScale3D(FVector(2.6 * ShadowScale, 0.8 * ShadowScale, 1.0));
		}
	}
	static constexpr double A0ProbeDistancesMeters[] = {
		0.0, 150.0, 153.0, 165.0, 336.0, 385.0, 604.0, 617.0, 803.0, 805.0, 1500.0, 1740.0
	};
	while (NextA0VisualProbeIndex < UE_ARRAY_COUNT(A0ProbeDistancesMeters) &&
		State.DistanceMeters >= A0ProbeDistancesMeters[NextA0VisualProbeIndex])
	{
		const FString Trigger = FString::Printf(TEXT("checkpoint_%dm"),
			FMath::RoundToInt(A0ProbeDistancesMeters[NextA0VisualProbeIndex]));
		EmitA0VisualProbe(Trigger);
		++NextA0VisualProbeIndex;
	}

	if (!AvalancheWallMesh || !AvalancheCrestMesh || !AvalanchePlumeMesh)
	{
		return;
	}
	const double RenderGapMeters = bA0ArtSample
		? State.AvalancheGapMeters * 0.18
		: State.AvalancheGapMeters;
	const double FrontDistanceMeters = State.DistanceMeters - RenderGapMeters;
	const int32 SurfaceDistanceMm = FMath::Clamp(FMath::RoundToInt(FrontDistanceMeters * 1000.0),
		0, Fixture.RouteLengthMm);
	const double SurfaceHeightCm = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(
		Fixture, SurfaceDistanceMm) * 100.0;
	const double FrontXcm = FrontDistanceMeters * 100.0;

	if (bA0ArtSample)
	{
		AvalancheWallMesh->SetRelativeLocation(FVector(FrontXcm - 450.0, -20.0, SurfaceHeightCm + 580.0));
		AvalancheCrestMesh->SetRelativeLocation(FVector(FrontXcm - 350.0, -18.0, SurfaceHeightCm + 140.0));
		const int32 PlayerDistanceMm = FMath::Clamp(FMath::RoundToInt(State.DistanceMeters * 1000.0),
			0, Fixture.RouteLengthMm);
		const double PlayerSurfaceZcm = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(
			Fixture, PlayerDistanceMm) * 100.0;
		AvalanchePlumeMesh->SetRelativeLocation(FVector(FrontXcm - 100.0, -24.0, PlayerSurfaceZcm + 120.0));
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
