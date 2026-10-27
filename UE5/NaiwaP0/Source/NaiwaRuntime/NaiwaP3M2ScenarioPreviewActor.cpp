#include "NaiwaP3M2ScenarioPreviewActor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "NaiwaP3M1Actors.h"
#include "NaiwaP3M2LevelSession.h"
#include "NaiwaP3M2WorldPresentation.h"

ANaiwaP3M2ScenarioPreviewActor::ANaiwaP3M2ScenarioPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ANaiwaP3M2ScenarioPreviewActor::BuildEditorPreview()
{
	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld())
	{
		return;
	}
	if (MapPreview)
	{
		MapPreview->Destroy();
		MapPreview = nullptr;
	}
	if (WorldPreview)
	{
		WorldPreview->Destroy();
		WorldPreview = nullptr;
	}
	TArray<AActor*> AttachedPreviewActors;
	GetAttachedActors(AttachedPreviewActors, true, true);
	for (AActor* Attached : AttachedPreviewActors)
	{
		if (Cast<ANaiwaP3M1MapActor>(Attached) || Cast<ANaiwaP3M2WorldPresentation>(Attached))
		{
			Attached->Destroy();
		}
	}

	FNaiwaP3M2LevelSession PreviewSession;
	FString Error;
	if (!PreviewSession.Initialize(PreviewLevelId, 0, false, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("M2 编辑器场景预览生成失败：%s"), *Error);
		return;
	}

	const FNaiwaP3M2SeededLevel& Level = PreviewSession.GetLevel();
	TArray<FString> Rows;
	Rows.Reserve(Level.GridHeight);
	for (int32 Y = 0; Y < Level.GridHeight; ++Y)
	{
		FString Row;
		Row.Reserve(Level.GridWidth);
		for (int32 X = 0; X < Level.GridWidth; ++X)
		{
			switch (Level.GetCell(FIntPoint(X, Y)))
			{
			case ENaiwaP3M2Cell::HardBrick: Row.AppendChar(TCHAR('H')); break;
			case ENaiwaP3M2Cell::SoftBrick: Row.AppendChar(TCHAR('S')); break;
			case ENaiwaP3M2Cell::ItemBrick: Row.AppendChar(TCHAR('I')); break;
			case ENaiwaP3M2Cell::Floor:
			default: Row.AppendChar(TCHAR('.')); break;
			}
		}
		Rows.Add(MoveTemp(Row));
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MapPreview = World->SpawnActor<ANaiwaP3M1MapActor>(
		ANaiwaP3M1MapActor::StaticClass(), FTransform::Identity, SpawnParameters);
	if (!MapPreview)
	{
		return;
	}
	MapPreview->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	MapPreview->SetActorHiddenInGame(true);
	MapPreview->BuildMap(Rows, false, false);

	WorldPreview = World->SpawnActor<ANaiwaP3M2WorldPresentation>(
		ANaiwaP3M2WorldPresentation::StaticClass(), FTransform::Identity, SpawnParameters);
	if (WorldPreview)
	{
		WorldPreview->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		WorldPreview->SetActorHiddenInGame(true);
		WorldPreview->Build(Level);
		WorldPreview->BuildEditorCharacterPreview(Level.SpawnCell, Level.Enemies);
		TArray<FIntPoint> EnemyCells;
		TArray<uint8> EnemyAlive;
		EnemyCells.Reserve(Level.Enemies.Num());
		EnemyAlive.Init(1, Level.Enemies.Num());
		for (const FNaiwaP3M2EnemyPlacement& Enemy : Level.Enemies)
		{
			EnemyCells.Add(Enemy.Cell);
		}
		WorldPreview->Sync(PreviewSession, Level.SpawnCell, EnemyCells, EnemyAlive);
	}

	UE_LOG(LogTemp, Display, TEXT("M2 编辑器场景预览已生成：种子=%d，布局=%s"),
		Level.Seed, *Level.LayoutHash);
}
