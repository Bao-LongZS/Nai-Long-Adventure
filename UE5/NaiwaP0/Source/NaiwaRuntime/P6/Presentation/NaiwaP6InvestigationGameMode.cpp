#include "NaiwaP6InvestigationGameMode.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "P5/Data/NaiwaP5SkinCatalog.h"
#include "P6/Data/NaiwaP6CaseDefinition.h"
#include "P6/Core/NaiwaP6InvestigationSubsystem.h"
#include "P6/Presentation/NaiwaP6HUD.h"
#include "P6/Presentation/NaiwaP6InteractableActor.h"
#include "P6/Presentation/NaiwaP6PlayerController.h"
#include "P6/Presentation/NaiwaP6PlayerPawn.h"
#include "NaiwaMenuProgress.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void SpawnGrayboxProp(UWorld* World, UStaticMesh* Mesh, const FVector& Location,
		const FVector& Scale, const FLinearColor& Color)
	{
		if (!World || !Mesh) return;
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Prop = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
			Location, FRotator::ZeroRotator, SpawnParameters);
		if (!Prop) return;
		UStaticMeshComponent* Component = Prop->GetStaticMeshComponent();
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Prop->SetActorScale3D(Scale);
		if (UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, Prop))
			{
				Material->SetVectorParameterValue(TEXT("Color"), Color);
				Component->SetMaterial(0, Material);
			}
		}
	}
}

ANaiwaP6InvestigationGameMode::ANaiwaP6InvestigationGameMode()
{
	DefaultPawnClass = ANaiwaP6PlayerPawn::StaticClass();
	PlayerControllerClass = ANaiwaP6PlayerController::StaticClass();
	HUDClass = ANaiwaP6HUD::StaticClass();
}

void ANaiwaP6InvestigationGameMode::BeginPlay()
{
	Super::BeginPlay();
	BuildM2Graybox();
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* Controller = World->GetFirstPlayerController())
		{
			EnsureCamera(Controller);
		}
	}
	RunM1ProbeIfRequested();
	RunM2ProbeIfRequested();
	RunM3ProbeIfRequested();
	RunM4ProbeIfRequested();
}

void ANaiwaP6InvestigationGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	EnsureCamera(NewPlayer);
}

void ANaiwaP6InvestigationGameMode::EnsureCamera(APlayerController* PlayerController)
{
	if (!PlayerController || !GetWorld()) return;
	if (!InvestigationCamera)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		InvestigationCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),
			FVector(0.0f, 2078.0f, 3600.0f), FRotator(-60.0f, -90.0f, 0.0f), SpawnParameters);
		if (InvestigationCamera)
		{
			UCameraComponent* Camera = InvestigationCamera->GetCameraComponent();
			Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
			Camera->OrthoWidth = 7800.0f;
			Camera->bConstrainAspectRatio = false;
		}
	}
	if (InvestigationCamera) PlayerController->SetViewTarget(InvestigationCamera);
}

void ANaiwaP6InvestigationGameMode::BuildM2Graybox()
{
	UWorld* World = GetWorld();
	if (!World) return;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube) return;

	SpawnGrayboxProp(World, Cube, FVector(0.0f, 0.0f, -55.0f), FVector(80.0f, 65.0f, 0.8f),
		FLinearColor(0.31f, 0.36f, 0.34f));
	SpawnGrayboxProp(World, Cube, FVector(0.0f, 2220.0f, 340.0f), FVector(19.0f, 4.0f, 8.0f),
		FLinearColor(0.55f, 0.43f, 0.34f));
	SpawnGrayboxProp(World, Cube, FVector(250.0f, 500.0f, -4.0f), FVector(16.0f, 3.1f, 0.38f),
		FLinearColor(0.38f, 0.34f, 0.31f));
	SpawnGrayboxProp(World, Cube, FVector(2290.0f, 610.0f, 80.0f), FVector(2.7f, 2.7f, 1.7f),
		FLinearColor(0.37f, 0.54f, 0.57f));
	SpawnGrayboxProp(World, Cube, FVector(1050.0f, 650.0f, 20.0f), FVector(0.65f, 0.65f, 0.65f),
		FLinearColor(0.47f, 0.57f, 0.38f));
	SpawnGrayboxProp(World, Cube, FVector(1770.0f, -1080.0f, 35.0f), FVector(5.4f, 0.20f, 0.65f),
		FLinearColor(0.42f, 0.31f, 0.29f));

	for (const FNaiwaP6InteractableDefinition& Definition : FNaiwaP6CaseDefinition::GetM2Interactables())
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ANaiwaP6InteractableActor* Point = World->SpawnActor<ANaiwaP6InteractableActor>(
			ANaiwaP6InteractableActor::StaticClass(), Definition.Location, FRotator::ZeroRotator, SpawnParameters))
		{
			Point->InitializePoint(Definition.PointId);
		}
	}
}

void ANaiwaP6InvestigationGameMode::RunM1ProbeIfRequested()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("NaiwaP6M1Probe"))) return;

	bool bPassed = true;
	TArray<FString> Checks;
	const auto Check = [&bPassed, &Checks](const bool bCondition, const TCHAR* Name)
	{
		Checks.Add(FString::Printf(TEXT("%s=%s"), Name, bCondition ? TEXT("ok") : TEXT("FAIL")));
		bPassed = bPassed && bCondition;
	};

	UNaiwaP6InvestigationSubsystem* Investigation = GetWorld()
		? GetWorld()->GetSubsystem<UNaiwaP6InvestigationSubsystem>() : nullptr;
	int32 PointCount = 0;
	int32 M1PointCount = 0;
	for (TActorIterator<ANaiwaP6InteractableActor> It(GetWorld()); It; ++It)
	{
		++PointCount;
		bool bIsM1Point = false;
		for (const FNaiwaP6InteractableDefinition& Definition : FNaiwaP6CaseDefinition::GetM1Interactables())
		{
			if (Definition.PointId == It->GetPointId())
			{
				bIsM1Point = true;
				break;
			}
		}
		if (bIsM1Point) ++M1PointCount;
	}
	Check(FNaiwaP6CaseDefinition::GetM1Clues().Num() == 2, TEXT("two_clue_fixture"));
	Check(FNaiwaP6CaseDefinition::GetM1Interactables().Num() == 3 && M1PointCount == 3,
		TEXT("three_m1_baseline_points_preserved"));
	Check(FNaiwaP6CaseDefinition::GetM2Interactables().Num() == 14 && PointCount == 14,
		TEXT("fourteen_point_world_spawn"));
	Check(FNaiwaP6CaseDefinition::GetFirstQuestionOptions().Num() == 4, TEXT("four_answer_options"));
	Check(Investigation != nullptr, TEXT("world_authority_ready"));
	if (Investigation)
	{
		FString Feedback;
		Check(!Investigation->SubmitFirstQuestion(FName(TEXT("NaiDuck")), Feedback),
			TEXT("question_requires_footprint_evidence"));
		Check(Investigation->CollectClue(FName(TEXT("G1"))), TEXT("collect_g1"));
		Check(Investigation->CollectClue(FName(TEXT("G5"))), TEXT("collect_g5"));
		Check(!Investigation->CollectClue(FName(TEXT("G5"))), TEXT("duplicate_collect_rejected"));
		Check(Investigation->HasClue(FName(TEXT("G1"))) && Investigation->HasClue(FName(TEXT("G5"))),
			TEXT("evidence_retained"));
		Check(!Investigation->SubmitFirstQuestion(FName(TEXT("NaiPai")), Feedback),
			TEXT("wrong_answer_not_accepted"));
		Check(Investigation->HasClue(FName(TEXT("G1"))) && Investigation->HasClue(FName(TEXT("G5"))),
			TEXT("wrong_answer_keeps_evidence"));
		Check(Investigation->SubmitFirstQuestion(FName(TEXT("NaiDuck")), Feedback),
			TEXT("correct_answer_accepted"));
		Check(Investigation->IsFirstQuestionSolved(), TEXT("question_state_recorded"));
	}
	ANaiwaP6InteractableActor* TrophyPoint = nullptr;
	for (TActorIterator<ANaiwaP6InteractableActor> It(GetWorld()); It; ++It)
	{
		if (It->GetPointId() == FName(TEXT("Trophy")))
		{
			TrophyPoint = *It;
			break;
		}
	}
	Check(TrophyPoint && TrophyPoint->HasInfoLayer(), TEXT("trophy_layer_available"));
	if (TrophyPoint)
	{
		Check(TrophyPoint->ToggleInfoLayer() && TrophyPoint->IsInfoLayerVisible(), TEXT("info_layer_toggles_on"));
		Check(TrophyPoint->ToggleInfoLayer() && !TrophyPoint->IsInfoLayerVisible(), TEXT("info_layer_toggles_off"));
	}
	UNaiwaMenuSaveGame* InMemorySave = NewObject<UNaiwaMenuSaveGame>();
	if (InMemorySave)
	{
		InMemorySave->P6CollectedClueIds.Add(FName(TEXT("G1")));
		InMemorySave->P6CollectedClueIds.Add(FName(TEXT("G5")));
		TArray<uint8> Bytes;
		const bool bSavedInMemory = UGameplayStatics::SaveGameToMemory(InMemorySave, Bytes);
		const UNaiwaMenuSaveGame* LoadedSave = bSavedInMemory
			? Cast<UNaiwaMenuSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)) : nullptr;
		Check(LoadedSave && LoadedSave->P6CollectedClueIds.Num() == 2 &&
			LoadedSave->P6CollectedClueIds.Contains(FName(TEXT("G5"))), TEXT("save_data_memory_roundtrip"));
	}
	else
	{
		Check(false, TEXT("save_data_memory_roundtrip"));
	}

	const TArray<FString> DetailLines = Checks;
	UE_LOG(LogTemp, Display, TEXT("NAIWA_P6_M1_%s %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"),
		*FString::Join(DetailLines, TEXT("; ")));
	FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1, TEXT("P6 M1 probe complete"));
}

void ANaiwaP6InvestigationGameMode::RunM2ProbeIfRequested()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("NaiwaP6M2Probe"))) return;

	bool bPassed = true;
	TArray<FString> Checks;
	const auto Check = [&bPassed, &Checks](const bool bCondition, const TCHAR* Name)
	{
		Checks.Add(FString::Printf(TEXT("%s=%s"), Name, bCondition ? TEXT("ok") : TEXT("FAIL")));
		bPassed = bPassed && bCondition;
	};

	UNaiwaP6InvestigationSubsystem* Investigation = GetWorld()
		? GetWorld()->GetSubsystem<UNaiwaP6InvestigationSubsystem>() : nullptr;
	TMap<FName, ANaiwaP6InteractableActor*> SpawnedPoints;
	for (TActorIterator<ANaiwaP6InteractableActor> It(GetWorld()); It; ++It)
	{
		SpawnedPoints.Add(It->GetPointId(), *It);
	}

	const TArray<FNaiwaP6ClueDefinition>& Clues = FNaiwaP6CaseDefinition::GetM2Clues();
	const TArray<FNaiwaP6InteractableDefinition>& Points = FNaiwaP6CaseDefinition::GetM2Interactables();
	Check(Clues.Num() == 6, TEXT("six_general_clue_cards"));
	Check(Points.Num() == 14 && SpawnedPoints.Num() == 14, TEXT("fourteen_interaction_actors"));
	Check(Investigation != nullptr, TEXT("world_authority_ready"));

	int32 RequiredCount = 0;
	int32 OptionalCount = 0;
	TSet<FName> UniqueClueIds;
	for (const FNaiwaP6ClueDefinition& Clue : Clues)
	{
		UniqueClueIds.Add(Clue.ClueId);
		Check(FNaiwaP6CaseDefinition::FindClue(Clue.ClueId) != nullptr,
			*FString::Printf(TEXT("clue_%s_resolves"), *Clue.ClueId.ToString()));
	}
	Check(UniqueClueIds.Num() == 6, TEXT("clue_ids_are_unique"));

	for (const FNaiwaP6InteractableDefinition& Definition : Points)
	{
		Definition.bM2Required ? ++RequiredCount : ++OptionalCount;
		const FVector Location = Definition.Location;
		const bool bWithinPlayableBounds = FMath::Abs(Location.X) <= 3550.0f &&
			FMath::Abs(Location.Y) <= 2750.0f;
		Check(SpawnedPoints.Contains(Definition.PointId),
			*FString::Printf(TEXT("point_%s_spawns"), *Definition.PointId.ToString()));
		Check(bWithinPlayableBounds,
			*FString::Printf(TEXT("point_%s_within_walk_bounds"), *Definition.PointId.ToString()));
	}
	Check(RequiredCount == 9 && OptionalCount == 5, TEXT("nine_required_five_optional_points"));

	if (Investigation)
	{
		FString Feedback;
		Check(!Investigation->SubmitFirstQuestion(FName(TEXT("NaiDuck")), Feedback),
			TEXT("first_question_still_requires_g5"));
		const FString InitialHint = Investigation->GetNextRequiredHintText();
		Check(InitialHint.Contains(TEXT("感应奖杯")), TEXT("guided_route_starts_at_trophy"));

		for (const FNaiwaP6InteractableDefinition& Definition : Points)
		{
			ANaiwaP6InteractableActor* const* Point = SpawnedPoints.Find(Definition.PointId);
			if (!Point || !*Point)
			{
				Check(false, TEXT("all_points_are_interactable"));
				continue;
			}
			if (Definition.bM2Required)
			{
				Check(Investigation->GetNextRequiredHintText().Contains(Definition.HintText),
					*FString::Printf(TEXT("point_%s_is_next_guided_hint"), *Definition.PointId.ToString()));
			}
			const int32 CluesBefore = Investigation->GetCollectedClueIds().Num();
			const FString InteractionFeedback = (*Point)->Interact();
			Check(!InteractionFeedback.IsEmpty(),
				*FString::Printf(TEXT("point_%s_returns_interaction_feedback"), *Definition.PointId.ToString()));
			Check(Investigation->HasVisitedPoint(Definition.PointId),
				*FString::Printf(TEXT("point_%s_records_visit"), *Definition.PointId.ToString()));
			if (Definition.bM2Required && !Definition.ClueId.IsNone())
			{
				Check(Investigation->HasClue(Definition.ClueId),
					*FString::Printf(TEXT("point_%s_collects_%s"),
						*Definition.PointId.ToString(), *Definition.ClueId.ToString()));
			}
			if (!Definition.bM2Required)
			{
				Check(Investigation->GetCollectedClueIds().Num() == CluesBefore,
					*FString::Printf(TEXT("optional_%s_adds_no_clue"), *Definition.PointId.ToString()));
			}
		}

		Check(Investigation->GetCollectedClueIds().Num() == 6, TEXT("all_general_evidence_collects"));
		Check(Investigation->GetVisitedRequiredPointCount() == 9, TEXT("all_nine_required_points_visit"));
		Check(Investigation->GetNextRequiredHintText().Contains(TEXT("必看点已检查")),
			TEXT("guided_route_completes"));
		Check(!Investigation->SubmitFirstQuestion(FName(TEXT("NaiPai")), Feedback),
			TEXT("wrong_first_answer_not_accepted"));
		Check(Investigation->HasClue(FName(TEXT("G1"))) && Investigation->HasClue(FName(TEXT("G6"))),
			TEXT("wrong_answer_keeps_all_evidence"));
		Check(Investigation->SubmitFirstQuestion(FName(TEXT("NaiDuck")), Feedback),
			TEXT("correct_first_answer_still_accepted"));
	}

	UNaiwaMenuSaveGame* InMemorySave = NewObject<UNaiwaMenuSaveGame>();
	if (InMemorySave && Investigation)
	{
		InMemorySave->P6CollectedClueIds = Investigation->GetCollectedClueIds();
		TArray<uint8> Bytes;
		const bool bSavedInMemory = UGameplayStatics::SaveGameToMemory(InMemorySave, Bytes);
		const UNaiwaMenuSaveGame* LoadedSave = bSavedInMemory
			? Cast<UNaiwaMenuSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)) : nullptr;
		Check(LoadedSave && LoadedSave->P6CollectedClueIds.Num() == 6 &&
			LoadedSave->P6CollectedClueIds.Contains(FName(TEXT("G6"))),
			TEXT("six_clue_save_data_memory_roundtrip"));
	}
	else
	{
		Check(false, TEXT("six_clue_save_data_memory_roundtrip"));
	}

	const TArray<FString> DetailLines = Checks;
	UE_LOG(LogTemp, Display, TEXT("NAIWA_P6_M2_%s %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"),
		*FString::Join(DetailLines, TEXT("; ")));
	FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1, TEXT("P6 M2 probe complete"));
}

void ANaiwaP6InvestigationGameMode::RunM3ProbeIfRequested()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("NaiwaP6M3Probe"))) return;

	bool bPassed = true;
	TArray<FString> Checks;
	const auto Check = [&bPassed, &Checks](const bool bCondition, const TCHAR* Name)
	{
		Checks.Add(FString::Printf(TEXT("%s=%s"), Name, bCondition ? TEXT("ok") : TEXT("FAIL")));
		bPassed = bPassed && bCondition;
	};

	UWorld* World = GetWorld();
	UNaiwaP6InvestigationSubsystem* Investigation = World
		? World->GetSubsystem<UNaiwaP6InvestigationSubsystem>() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UNaiwaProgressSubsystem* Progress = GameInstance
		? GameInstance->GetSubsystem<UNaiwaProgressSubsystem>() : nullptr;
	const TArray<FName> SavedCluesBefore = Progress ? Progress->GetP6CollectedClueIds() : TArray<FName>();
	const int32 MilkTicketsBefore = Progress ? Progress->GetMilkTickets() : 0;
	const int32 SavedFailuresBefore = Progress ? Progress->GetP6WrongSubmissionCount() : 0;
	const int32 SavedStreakBefore = Progress ? Progress->GetP6FailureStreakCount() : 0;
	const int32 SavedElapsedBefore = Progress ? Progress->GetP6CaseElapsedMinutes() : 20;
	const int32 SavedBestRatingBefore = Progress ? Progress->GetP6BestCaseRating() : 0;

	TMap<FName, ANaiwaP6InteractableActor*> SpawnedPoints;
	for (TActorIterator<ANaiwaP6InteractableActor> It(World); It; ++It)
	{
		SpawnedPoints.Add(It->GetPointId(), *It);
	}
	const TArray<FNaiwaP6InfoLayerDefinition>& InfoLayers = FNaiwaP6CaseDefinition::GetM3InfoLayers();
	const TArray<FNaiwaP5SkinRow>& SkinRows = FNaiwaP5SkinCatalog::GetRows();
	Check(Investigation != nullptr, TEXT("world_authority_ready"));
	Check(Progress != nullptr, TEXT("shared_save_subsystem_ready"));
	Check(FNaiwaP6CaseDefinition::GetM2Interactables().Num() == 14 && SpawnedPoints.Num() == 14,
		TEXT("m2_world_remains_fourteen_points"));
	Check(FNaiwaP6CaseDefinition::GetM2Clues().Num() == 6 &&
		FNaiwaP6CaseDefinition::GetM3Clues().Num() == 12 && InfoLayers.Num() == 4,
		TEXT("six_general_four_skin_and_two_cross_clues"));
	Check(SkinRows.Num() == 5, TEXT("five_shared_skin_configurations"));
	Check(FNaiwaP6CaseDefinition::GetM3Clues().Num() == 12 &&
		FNaiwaP6CaseDefinition::GetM3CrossClues().Num() == 2, TEXT("twelve_card_board_capacity"));

	const TArray<FName>& Question0Clues = FNaiwaP6CaseDefinition::GetQuestionRequiredClueIds(0);
	const TArray<FName>& Question1Clues = FNaiwaP6CaseDefinition::GetQuestionRequiredClueIds(1);
	const TArray<FName>& Question2Clues = FNaiwaP6CaseDefinition::GetQuestionRequiredClueIds(2);
	Check(Question0Clues.Num() == 1 && Question0Clues.Contains(FName(TEXT("G5"))) &&
		Question1Clues.Num() == 2 && Question1Clues.Contains(FName(TEXT("G1"))) &&
		Question1Clues.Contains(FName(TEXT("G3"))) && Question2Clues.Num() == 2 &&
		Question2Clues.Contains(FName(TEXT("G4"))) && Question2Clues.Contains(FName(TEXT("G6"))),
		TEXT("all_skin_configs_share_general_evidence_gate"));

	const FName TrophyId(TEXT("Trophy"));
	const FName BaseSkinId(TEXT("NaiwaSkin_Base"));
	ANaiwaP6InteractableActor* const* TrophyEntry = SpawnedPoints.Find(TrophyId);
	ANaiwaP6InteractableActor* Trophy = TrophyEntry ? *TrophyEntry : nullptr;
	Check(Trophy && Trophy->HasInfoLayer(), TEXT("trophy_owns_all_four_info_layers"));
	Check(Trophy && !Trophy->HasInfoLayer(BaseSkinId), TEXT("base_skin_keeps_general_evidence_path"));
	if (Investigation && Trophy)
	{
		Check(Investigation->CollectClue(FName(TEXT("G1")), TrophyId), TEXT("collect_general_clue_for_x2"));
	}

	TSet<FName> LayerClueIds;
	TSet<FName> LayerSkinIds;
	for (const FNaiwaP6InfoLayerDefinition& Layer : InfoLayers)
	{
		const FString LayerCheckName = FString::Printf(TEXT("layer_%s"), *Layer.ClueId.ToString());
		const bool bValidLayer = Layer.ClueId.ToString().StartsWith(TEXT("S")) &&
			Layer.PointId == TrophyId && !Layer.SkinId.IsNone() && !Layer.Marks.IsEmpty() &&
			FNaiwaP6CaseDefinition::FindClue(Layer.ClueId) != nullptr &&
			SkinRows.ContainsByPredicate([&Layer](const FNaiwaP5SkinRow& Row) { return Row.SkinId == Layer.SkinId; });
		Check(bValidLayer && !LayerClueIds.Contains(Layer.ClueId) && !LayerSkinIds.Contains(Layer.SkinId),
			*FString::Printf(TEXT("%s_has_unique_skin_and_clue_fixture"), *LayerCheckName));
		LayerClueIds.Add(Layer.ClueId);
		LayerSkinIds.Add(Layer.SkinId);
		Check(FNaiwaP6CaseDefinition::FindInfoLayer(Layer.PointId, Layer.SkinId) != nullptr,
			*FString::Printf(TEXT("%s_resolves_from_point_and_skin"), *LayerCheckName));
		if (!Trophy || !Investigation) continue;

		Check(Trophy->HasInfoLayer(Layer.SkinId),
			*FString::Printf(TEXT("%s_is_available_on_trophy"), *LayerCheckName));
		const FString ShowFeedback = Trophy->ToggleInfoLayer(Layer.SkinId);
		Check(!ShowFeedback.IsEmpty() && Trophy->IsInfoLayerVisible() && Investigation->HasClue(Layer.ClueId),
			*FString::Printf(TEXT("%s_displays_and_collects"), *LayerCheckName));
		const FString HideFeedback = Trophy->ToggleInfoLayer(Layer.SkinId);
		Check(!HideFeedback.IsEmpty() && !Trophy->IsInfoLayerVisible(),
			*FString::Printf(TEXT("%s_toggles_off"), *LayerCheckName));
	}
	Check(LayerClueIds.Num() == 4 && LayerClueIds.Contains(FName(TEXT("S1"))) &&
		LayerClueIds.Contains(FName(TEXT("S2"))) && LayerClueIds.Contains(FName(TEXT("S3"))) &&
		LayerClueIds.Contains(FName(TEXT("S4"))) && LayerSkinIds.Num() == 4,
		TEXT("four_distinct_skin_layers"));

	for (const FNaiwaP6CrossClueDefinition& Recipe : FNaiwaP6CaseDefinition::GetM3CrossClues())
	{
		Check(Recipe.PointId == TrophyId && Recipe.RequiredClueIds.Num() == 2 &&
			FNaiwaP6CaseDefinition::FindClue(Recipe.ClueId) != nullptr,
			*FString::Printf(TEXT("cross_clue_%s_has_two_valid_inputs"), *Recipe.ClueId.ToString()));
	}
	const auto HasCrossRecipe = [TrophyId](const FName ClueId, const FName InputA, const FName InputB)
	{
		const FNaiwaP6CrossClueDefinition* Recipe = FNaiwaP6CaseDefinition::GetM3CrossClues()
			.FindByPredicate([ClueId](const FNaiwaP6CrossClueDefinition& Candidate)
				{ return Candidate.ClueId == ClueId; });
		return Recipe && Recipe->PointId == TrophyId && Recipe->RequiredClueIds.Num() == 2 &&
			Recipe->RequiredClueIds.Contains(InputA) && Recipe->RequiredClueIds.Contains(InputB);
	};
	Check(HasCrossRecipe(FName(TEXT("X1")), FName(TEXT("S1")), FName(TEXT("S2"))) &&
		HasCrossRecipe(FName(TEXT("X2")), FName(TEXT("S3")), FName(TEXT("G1"))),
		TEXT("x1_and_x2_match_planned_inputs"));
	Check(FNaiwaP6CaseDefinition::GetM3CrossClues().Num() == 2 &&
		Investigation && Investigation->HasClue(FName(TEXT("X1"))) &&
		Investigation->HasClue(FName(TEXT("X2"))), TEXT("x1_and_x2_collect_from_required_inputs"));

	if (Investigation)
	{
		FString Feedback;
		const bool bFirstSwitch = Investigation->CycleSkin(Feedback);
		const bool bSecondSwitch = Investigation->CycleSkin(Feedback);
		const bool bThirdSwitch = Investigation->CycleSkin(Feedback);
		Check(bFirstSwitch && bSecondSwitch && !bThirdSwitch && Investigation->GetSkinSwitchesRemaining() == 0,
			TEXT("two_switches_per_entry_then_locked"));
	}
	if (Progress)
	{
		Check(Progress->GetP6CollectedClueIds() == SavedCluesBefore &&
			Progress->GetMilkTickets() == MilkTicketsBefore &&
			Progress->GetP6WrongSubmissionCount() == SavedFailuresBefore &&
			Progress->GetP6FailureStreakCount() == SavedStreakBefore &&
			Progress->GetP6CaseElapsedMinutes() == SavedElapsedBefore &&
			Progress->GetP6BestCaseRating() == SavedBestRatingBefore,
			TEXT("probe_does_not_mutate_player_save_state"));
	}

	UE_LOG(LogTemp, Display, TEXT("NAIWA_P6_M3_%s %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"),
		*FString::Join(Checks, TEXT("; ")));
	FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1, TEXT("P6 M3 probe complete"));
}

void ANaiwaP6InvestigationGameMode::RunM4ProbeIfRequested()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("NaiwaP6M4Probe"))) return;

	bool bPassed = true;
	TArray<FString> Checks;
	const auto Check = [&bPassed, &Checks](const bool bCondition, const TCHAR* Name)
	{
		Checks.Add(FString::Printf(TEXT("%s=%s"), Name, bCondition ? TEXT("ok") : TEXT("FAIL")));
		bPassed = bPassed && bCondition;
	};

	FString Scenario;
	const bool bHasScenario = FParse::Value(FCommandLine::Get(), TEXT("NaiwaP6M4Scenario="), Scenario);
	Check(bHasScenario, TEXT("scenario_argument_present"));
	const bool bMorningScenario = Scenario.Equals(TEXT("Morning"), ESearchCase::IgnoreCase);
	const bool bFailureScenario = Scenario.Equals(TEXT("Failure"), ESearchCase::IgnoreCase);
	const bool bPerfectScenario = Scenario.Equals(TEXT("Perfect"), ESearchCase::IgnoreCase);
	const bool bClearScenario = Scenario.Equals(TEXT("Clear"), ESearchCase::IgnoreCase);
	Check(bMorningScenario || bFailureScenario || bPerfectScenario || bClearScenario,
		TEXT("scenario_argument_supported"));

	UNaiwaP6InvestigationSubsystem* Investigation = GetWorld()
		? GetWorld()->GetSubsystem<UNaiwaP6InvestigationSubsystem>() : nullptr;
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNaiwaProgressSubsystem* Progress = GameInstance
		? GameInstance->GetSubsystem<UNaiwaProgressSubsystem>() : nullptr;
	const TArray<FName> SavedCluesBefore = Progress ? Progress->GetP6CollectedClueIds() : TArray<FName>();
	const int32 MilkTicketsBefore = Progress ? Progress->GetMilkTickets() : 0;
	const int32 SavedFailuresBefore = Progress ? Progress->GetP6WrongSubmissionCount() : 0;
	const int32 SavedStreakBefore = Progress ? Progress->GetP6FailureStreakCount() : 0;
	const int32 SavedElapsedBefore = Progress ? Progress->GetP6CaseElapsedMinutes() : 20;
	const int32 SavedBestRatingBefore = Progress ? Progress->GetP6BestCaseRating() : 0;

	const TArray<FName> CorrectAnswers = {
		FName(TEXT("NaiDuck")), FName(TEXT("Chip")), FName(TEXT("NoOne")) };
	TArray<FName> WrongAnswers;
	WrongAnswers.Reserve(FNaiwaP6CaseDefinition::GetQuestionCount());
	for (int32 QuestionIndex = 0; QuestionIndex < FNaiwaP6CaseDefinition::GetQuestionCount(); ++QuestionIndex)
	{
		const TArray<FNaiwaP6AnswerOption>& Options = FNaiwaP6CaseDefinition::GetQuestionOptions(QuestionIndex);
		const FNaiwaP6AnswerOption* WrongOption = Options.FindByPredicate([&CorrectAnswers, QuestionIndex](
			const FNaiwaP6AnswerOption& Option) { return Option.AnswerId != CorrectAnswers[QuestionIndex]; });
		WrongAnswers.Add(WrongOption ? WrongOption->AnswerId : NAME_None);
	}
	const TArray<FName> OneWrongAnswer = { CorrectAnswers[0], CorrectAnswers[1], WrongAnswers[2] };
	const TArray<FName> OneCorrectAnswer = { CorrectAnswers[0], WrongAnswers[1], WrongAnswers[2] };
	const TArray<FName> TwoWrongAnswers = { WrongAnswers[0], WrongAnswers[1], CorrectAnswers[2] };
	Check(FNaiwaP6CaseDefinition::GetQuestionCount() == 3 && WrongAnswers.Num() == 3,
		TEXT("three_question_fixture"));
	Check(FNaiwaP6CaseDefinition::ScoreAnswers(CorrectAnswers) == 3 &&
		FNaiwaP6CaseDefinition::ScoreAnswers(OneWrongAnswer) == 2 &&
		FNaiwaP6CaseDefinition::ScoreAnswers(TwoWrongAnswers) == 1 &&
		FNaiwaP6CaseDefinition::ScoreAnswers(WrongAnswers) == 0,
		TEXT("three_two_one_zero_answer_score_bands"));
	TArray<FName> InvalidAnswers = CorrectAnswers;
	InvalidAnswers[0] = FName(TEXT("not_a_question_option"));
	Check(FNaiwaP6CaseDefinition::ScoreAnswers(InvalidAnswers) == INDEX_NONE,
		TEXT("invalid_answer_id_rejected"));

	Check(Investigation != nullptr, TEXT("world_authority_ready"));
	Check(Progress != nullptr, TEXT("shared_save_subsystem_ready"));
	if (Investigation)
	{
		Check(!Investigation->IsCaseConcluded(), TEXT("case_reveal_stays_hidden_before_submission"));
	}
	if (Investigation)
	{
		FString Feedback;
		for (int32 QuestionIndex = 0; QuestionIndex < CorrectAnswers.Num(); ++QuestionIndex)
		{
			Check(Investigation->SetSelectedAnswer(QuestionIndex, CorrectAnswers[QuestionIndex], Feedback),
				*FString::Printf(TEXT("initial_answer_%d_selects"), QuestionIndex));
		}
		const int32 InitialElapsed = Investigation->GetCaseElapsedMinutes();
		const int32 InitialFailures = Investigation->GetWrongSubmissionCount();
		Check(!Investigation->SubmitCase(Feedback) && Investigation->GetWrongSubmissionCount() == InitialFailures &&
			Investigation->GetCaseElapsedMinutes() == InitialElapsed &&
			Investigation->GetSelectedQuestionAnswerCount() == 3,
			TEXT("core_clue_gate_does_not_count_as_failure_or_clear_answers"));

		for (const FName CoreClueId : { FName(TEXT("G1")), FName(TEXT("G3")), FName(TEXT("G4")),
			FName(TEXT("G5")), FName(TEXT("G6")) })
		{
			Check(Investigation->CollectClue(CoreClueId),
				*FString::Printf(TEXT("collect_core_%s"), *CoreClueId.ToString()));
		}

		const auto SetAnswers = [&Investigation, &Check, &Feedback](const TArray<FName>& Answers, const TCHAR* Label)
		{
			bool bAllSet = true;
			for (int32 QuestionIndex = 0; QuestionIndex < Answers.Num(); ++QuestionIndex)
			{
				const bool bSet = Investigation->SetSelectedAnswer(QuestionIndex, Answers[QuestionIndex], Feedback);
				Check(bSet, *FString::Printf(TEXT("%s_question_%d_selects"), Label, QuestionIndex));
				bAllSet = bAllSet && bSet;
			}
			return bAllSet;
		};

		if (bPerfectScenario)
		{
			SetAnswers(CorrectAnswers, TEXT("perfect"));
			Check(Investigation->SubmitCase(Feedback) && Investigation->IsCaseConcluded() &&
				Investigation->GetCurrentCaseRating() == 2 && Investigation->GetBestCaseRating() == 2,
				TEXT("three_of_three_perfect_clear"));
			Check(Investigation->GetFailureStreakCount() == 0 &&
				Investigation->GetCaseElapsedMinutes() == InitialElapsed,
				TEXT("perfect_clear_preserves_clock_and_clears_streak"));
		}
		else if (bClearScenario)
		{
			SetAnswers(OneWrongAnswer, TEXT("two_of_three"));
			Check(Investigation->SubmitCase(Feedback) && Investigation->IsCaseConcluded() &&
				Investigation->GetCurrentCaseRating() == 1 && Investigation->GetBestCaseRating() == 1,
				TEXT("two_of_three_case_clear"));
		}
		else if (bFailureScenario)
		{
			SetAnswers(WrongAnswers, TEXT("zero_of_three"));
			Check(!Investigation->SubmitCase(Feedback) && Investigation->GetWrongSubmissionCount() == InitialFailures + 1 &&
				Investigation->GetFailureStreakCount() == 1 && Investigation->GetCaseElapsedMinutes() == InitialElapsed + 40,
				TEXT("zero_of_three_preserves_evidence_and_advances_forty_minutes"));
			SetAnswers(OneCorrectAnswer, TEXT("one_of_three"));
			Check(!Investigation->SubmitCase(Feedback) && Investigation->GetFailureStreakCount() == 2,
				TEXT("one_of_three_still_fails"));
			SetAnswers(WrongAnswers, TEXT("third_failure"));
			Check(!Investigation->SubmitCase(Feedback) && Investigation->GetFailureStreakCount() == 3 &&
				Feedback.Contains(TEXT("建议")), TEXT("three_failures_offer_clue_review_hint"));
			for (int32 FailureNumber = 4; FailureNumber <= 9; ++FailureNumber)
			{
				Check(!Investigation->SubmitCase(Feedback),
					*FString::Printf(TEXT("failure_%d_keeps_case_open"), FailureNumber));
			}
		Check(Investigation->GetWrongSubmissionCount() == InitialFailures + 9 &&
			Investigation->GetCaseElapsedMinutes() == 360 && Investigation->IsCaseFailedAtDawn(),
			TEXT("ninth_failure_reaches_dawn_and_locks_night_submission"));
		const int32 DawnFailures = Investigation->GetWrongSubmissionCount();
		const int32 DawnMinutes = Investigation->GetCaseElapsedMinutes();
		Check(!Investigation->SubmitCase(Feedback) && Investigation->GetWrongSubmissionCount() == DawnFailures &&
			Investigation->GetCaseElapsedMinutes() == DawnMinutes,
			TEXT("dawn_lock_blocks_more_night_failures"));
		Check(!Investigation->SetSelectedAnswer(0, CorrectAnswers[0], Feedback),
			TEXT("dawn_lock_blocks_answer_changes"));
		Check(Investigation->GetCollectedClueIds().Contains(FName(TEXT("G5"))) &&
			!Investigation->IsCaseConcluded(), TEXT("dawn_keeps_evidence_for_reentry"));
		}
		else if (bMorningScenario)
		{
			Check(Investigation->IsMorningReentry() && Investigation->GetCaseElapsedMinutes() == 360 &&
				Investigation->GetWrongSubmissionCount() == 8 && Investigation->GetFailureStreakCount() == 2 &&
				Investigation->GetBestCaseRating() == 1,
				TEXT("morning_reentry_loads_persisted_counters"));
			SetAnswers(OneCorrectAnswer, TEXT("morning_one_of_three"));
			Check(!Investigation->SubmitCase(Feedback) && Investigation->GetWrongSubmissionCount() == 9 &&
				Investigation->GetFailureStreakCount() == 3 && Investigation->GetCaseElapsedMinutes() == 360 &&
				Investigation->IsMorningReentry() && !Investigation->IsCaseFailedAtDawn(),
				TEXT("morning_failure_does_not_advance_or_relock_clock"));
			SetAnswers(OneWrongAnswer, TEXT("morning_two_of_three"));
			Check(Investigation->SubmitCase(Feedback) && Investigation->IsCaseConcluded() &&
				Investigation->GetCurrentCaseRating() == 1 && Investigation->GetFailureStreakCount() == 0 &&
				Investigation->GetCaseElapsedMinutes() == 360,
				TEXT("morning_case_can_clear_and_reset_failure_streak"));
		}
	}

	if (Progress)
	{
		Check(Progress->GetP6CollectedClueIds() == SavedCluesBefore &&
			Progress->GetMilkTickets() == MilkTicketsBefore &&
			Progress->GetP6WrongSubmissionCount() == SavedFailuresBefore &&
			Progress->GetP6FailureStreakCount() == SavedStreakBefore &&
			Progress->GetP6CaseElapsedMinutes() == SavedElapsedBefore &&
			Progress->GetP6BestCaseRating() == SavedBestRatingBefore,
			TEXT("probe_does_not_mutate_player_save_state"));
	}

	UNaiwaMenuSaveGame* RewardFixture = NewObject<UNaiwaMenuSaveGame>();
	int32 RewardTicketsGranted = INDEX_NONE;
	const FGuid InvalidRunId;
	Check(RewardFixture && !UNaiwaProgressSubsystem::ApplyP6CaseClearReward(
		*RewardFixture, 1, InvalidRunId, RewardTicketsGranted) && RewardTicketsGranted == 0,
		TEXT("reward_rejects_invalid_run_id"));
	const FGuid InvalidRatingRunId = FGuid::NewGuid();
	Check(RewardFixture && !UNaiwaProgressSubsystem::ApplyP6CaseClearReward(
		*RewardFixture, 0, InvalidRatingRunId, RewardTicketsGranted) && RewardTicketsGranted == 0,
		TEXT("reward_rejects_invalid_rating"));
	if (RewardFixture)
	{
		RewardFixture->P6FailureStreakCount = 3;
		const FGuid FirstClearRunId = FGuid::NewGuid();
		const bool bFirstClearApplied = UNaiwaProgressSubsystem::ApplyP6CaseClearReward(
			*RewardFixture, 1, FirstClearRunId, RewardTicketsGranted);
		Check(bFirstClearApplied && RewardTicketsGranted == 60 && RewardFixture->MilkTickets == 60 &&
			RewardFixture->P6BestCaseRating == 1 && RewardFixture->P6FailureStreakCount == 0,
			TEXT("first_standard_clear_grants_sixty_and_resets_streak"));

		const int32 TicketsBeforeDuplicate = RewardFixture->MilkTickets;
		const int32 ClaimedKeysBeforeDuplicate = RewardFixture->P6ClaimedRewardKeys.Num();
		RewardFixture->P6FailureStreakCount = 2;
		const bool bDuplicateClearApplied = UNaiwaProgressSubsystem::ApplyP6CaseClearReward(
			*RewardFixture, 2, FirstClearRunId, RewardTicketsGranted);
		Check(!bDuplicateClearApplied && RewardTicketsGranted == 0 &&
			RewardFixture->MilkTickets == TicketsBeforeDuplicate &&
			RewardFixture->P6BestCaseRating == 1 &&
			RewardFixture->P6FailureStreakCount == 2 &&
			RewardFixture->P6ClaimedRewardKeys.Num() == ClaimedKeysBeforeDuplicate,
			TEXT("same_run_guid_cannot_duplicate_or_upgrade_reward"));

		const FGuid PerfectRunId = FGuid::NewGuid();
		const bool bPerfectClearApplied = UNaiwaProgressSubsystem::ApplyP6CaseClearReward(
			*RewardFixture, 2, PerfectRunId, RewardTicketsGranted);
		Check(bPerfectClearApplied && RewardTicketsGranted == 80 && RewardFixture->MilkTickets == 140 &&
			RewardFixture->P6BestCaseRating == 2,
			TEXT("first_perfect_clear_pays_sixty_tier_and_twenty_replay"));

		const FGuid ReplayRunId = FGuid::NewGuid();
		const bool bReplayApplied = UNaiwaProgressSubsystem::ApplyP6CaseClearReward(
			*RewardFixture, 1, ReplayRunId, RewardTicketsGranted);
		Check(bReplayApplied && RewardTicketsGranted == 20 && RewardFixture->MilkTickets == 160 &&
			RewardFixture->P6BestCaseRating == 2,
			TEXT("later_standard_clear_pays_twenty_without_lowering_best"));

		RewardFixture->MilkTickets = MAX_int32 - 5;
		const FGuid CappedRunId = FGuid::NewGuid();
		const bool bCappedClearApplied = UNaiwaProgressSubsystem::ApplyP6CaseClearReward(
			*RewardFixture, 2, CappedRunId, RewardTicketsGranted);
		Check(bCappedClearApplied && RewardTicketsGranted == 5 && RewardFixture->MilkTickets == MAX_int32,
			TEXT("wallet_cap_reports_only_credited_tickets"));
	}
	else
	{
		Check(false, TEXT("in_memory_reward_fixture_created"));
	}
	UNaiwaMenuSaveGame* FirstPerfectFixture = NewObject<UNaiwaMenuSaveGame>();
	const FGuid FirstPerfectRunId = FGuid::NewGuid();
	const bool bFirstPerfectApplied = FirstPerfectFixture &&
		UNaiwaProgressSubsystem::ApplyP6CaseClearReward(
			*FirstPerfectFixture, 2, FirstPerfectRunId, RewardTicketsGranted);
	Check(bFirstPerfectApplied && RewardTicketsGranted == 120 &&
		FirstPerfectFixture->MilkTickets == 120 && FirstPerfectFixture->P6BestCaseRating == 2,
		TEXT("first_perfect_clear_grants_one_hundred_twenty"));

	const FString MarkerScenario = Scenario.ToUpper();
	UE_LOG(LogTemp, Display, TEXT("NAIWA_P6_M4_%s_%s %s"), *MarkerScenario,
		bPassed ? TEXT("PASS") : TEXT("FAIL"), *FString::Join(Checks, TEXT("; ")));
	FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1, TEXT("P6 M4 probe complete"));
}
