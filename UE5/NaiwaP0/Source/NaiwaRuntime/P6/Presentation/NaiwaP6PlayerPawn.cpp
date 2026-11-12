#include "NaiwaP6PlayerPawn.h"

#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "P6/Core/NaiwaP6InvestigationSubsystem.h"
#include "P6/Data/NaiwaP6CaseDefinition.h"
#include "P6/Presentation/NaiwaP6InteractableActor.h"
#include "NaiwaMenuProgress.h"
#include "UObject/ConstructorHelpers.h"

ANaiwaP6PlayerPawn::ANaiwaP6PlayerPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded()) CubeMesh = CubeFinder.Object;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlayerGraybox"));
	SetRootComponent(BodyMesh);
	if (CubeMesh) BodyMesh->SetStaticMesh(CubeMesh);
	BodyMesh->SetRelativeScale3D(FVector(0.52f, 0.42f, 0.82f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCastShadow(false);
	if (UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, this))
		{
			BodyMaterial = Material;
			BodyMesh->SetMaterial(0, Material);
		}
	}

	FloatingMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingMovement"));
	FloatingMovement->SetUpdatedComponent(BodyMesh);
	FloatingMovement->MaxSpeed = 520.0f;
	FloatingMovement->Acceleration = 2400.0f;
	FloatingMovement->Deceleration = 3200.0f;
}

void ANaiwaP6PlayerPawn::BeginPlay()
{
	Super::BeginPlay();
	ApplyCurrentSkinAppearance();
	const UNaiwaP6InvestigationSubsystem* Investigation = GetWorld()
		? GetWorld()->GetSubsystem<UNaiwaP6InvestigationSubsystem>() : nullptr;
	UpdateFeedback(Investigation
		? FString::Printf(TEXT("走近物件按空格检查；Tab 切换皮肤（还可切换 %d 次），F 查看对应信息层。"),
			Investigation->GetSkinSwitchesRemaining())
		: TEXT("走近院内物件后按空格检查；左键也可点选附近物件。"));
}

void ANaiwaP6PlayerPawn::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FVector Direction((bMoveRight ? 1.0f : 0.0f) - (bMoveLeft ? 1.0f : 0.0f),
		(bMoveUp ? 1.0f : 0.0f) - (bMoveDown ? 1.0f : 0.0f), 0.0f);
	if (!bCaseBoardOpen && !Direction.IsNearlyZero())
	{
		Direction.Normalize();
		AddMovementInput(Direction, 1.0f);
	}

	const FVector Position = GetActorLocation();
	const FVector BoundedPosition(FMath::Clamp(Position.X, -3550.0f, 3550.0f),
		FMath::Clamp(Position.Y, -2750.0f, 2750.0f), 27.0f);
	if (FVector::DistSquared2D(Position, BoundedPosition) > 1.0f)
	{
		SetActorLocation(BoundedPosition, false);
	}
}

void ANaiwaP6PlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!PlayerInputComponent) return;
	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &ANaiwaP6PlayerPawn::MoveUpPressed);
	PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &ANaiwaP6PlayerPawn::MoveUpReleased);
	PlayerInputComponent->BindKey(EKeys::Up, IE_Pressed, this, &ANaiwaP6PlayerPawn::MoveUpPressed);
	PlayerInputComponent->BindKey(EKeys::Up, IE_Released, this, &ANaiwaP6PlayerPawn::MoveUpReleased);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &ANaiwaP6PlayerPawn::MoveDownPressed);
	PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &ANaiwaP6PlayerPawn::MoveDownReleased);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Pressed, this, &ANaiwaP6PlayerPawn::MoveDownPressed);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Released, this, &ANaiwaP6PlayerPawn::MoveDownReleased);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed, this, &ANaiwaP6PlayerPawn::MoveLeftPressed);
	PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &ANaiwaP6PlayerPawn::MoveLeftReleased);
	PlayerInputComponent->BindKey(EKeys::Left, IE_Pressed, this, &ANaiwaP6PlayerPawn::MoveLeftPressed);
	PlayerInputComponent->BindKey(EKeys::Left, IE_Released, this, &ANaiwaP6PlayerPawn::MoveLeftReleased);
	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed, this, &ANaiwaP6PlayerPawn::MoveRightPressed);
	PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &ANaiwaP6PlayerPawn::MoveRightReleased);
	PlayerInputComponent->BindKey(EKeys::Right, IE_Pressed, this, &ANaiwaP6PlayerPawn::MoveRightPressed);
	PlayerInputComponent->BindKey(EKeys::Right, IE_Released, this, &ANaiwaP6PlayerPawn::MoveRightReleased);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ANaiwaP6PlayerPawn::InteractPressed);
	PlayerInputComponent->BindKey(EKeys::J, IE_Pressed, this, &ANaiwaP6PlayerPawn::InteractPressed);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ANaiwaP6PlayerPawn::InteractAtCursor);
	PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &ANaiwaP6PlayerPawn::ToggleClueBoard);
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &ANaiwaP6PlayerPawn::ToggleInfoLayer);
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ANaiwaP6PlayerPawn::SwitchSkin);
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ANaiwaP6PlayerPawn::PreviousQuestion);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &ANaiwaP6PlayerPawn::NextQuestion);
	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &ANaiwaP6PlayerPawn::SelectAnswer1);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ANaiwaP6PlayerPawn::SelectAnswer2);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ANaiwaP6PlayerPawn::SelectAnswer3);
	PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ANaiwaP6PlayerPawn::SelectAnswer4);
	PlayerInputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ANaiwaP6PlayerPawn::SubmitAnswer);
	PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ANaiwaP6PlayerPawn::ReturnToLevelSelect);
}

void ANaiwaP6PlayerPawn::MoveUpPressed() { SetMovementKey(bMoveUp, true); }
void ANaiwaP6PlayerPawn::MoveUpReleased() { SetMovementKey(bMoveUp, false); }
void ANaiwaP6PlayerPawn::MoveDownPressed() { SetMovementKey(bMoveDown, true); }
void ANaiwaP6PlayerPawn::MoveDownReleased() { SetMovementKey(bMoveDown, false); }
void ANaiwaP6PlayerPawn::MoveLeftPressed() { SetMovementKey(bMoveLeft, true); }
void ANaiwaP6PlayerPawn::MoveLeftReleased() { SetMovementKey(bMoveLeft, false); }
void ANaiwaP6PlayerPawn::MoveRightPressed() { SetMovementKey(bMoveRight, true); }
void ANaiwaP6PlayerPawn::MoveRightReleased() { SetMovementKey(bMoveRight, false); }

void ANaiwaP6PlayerPawn::SetMovementKey(bool& bKeyState, const bool bPressed)
{
	bKeyState = bPressed;
}

ANaiwaP6InteractableActor* ANaiwaP6PlayerPawn::GetNearbyInteractable() const
{
	if (!GetWorld()) return nullptr;
	ANaiwaP6InteractableActor* Nearest = nullptr;
	float BestDistanceSquared = FMath::Square(300.0f);
	for (TActorIterator<ANaiwaP6InteractableActor> It(GetWorld()); It; ++It)
	{
		const float DistanceSquared = FVector::DistSquared2D(GetActorLocation(), It->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Nearest = *It;
		}
	}
	return Nearest;
}

void ANaiwaP6PlayerPawn::InteractPressed()
{
	if (bCaseBoardOpen) return;
	if (ANaiwaP6InteractableActor* Nearby = GetNearbyInteractable())
	{
		UpdateFeedback(Nearby->Interact());
	}
	else
	{
		UpdateFeedback(TEXT("再走近一点，靠近场景中的调查点后可检查。"));
	}
}

void ANaiwaP6PlayerPawn::InteractAtCursor()
{
	if (bCaseBoardOpen) return;
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController || !GetWorld()) return;
	FVector Origin;
	FVector Direction;
	if (!PlayerController->DeprojectMousePositionToWorld(Origin, Direction)) return;

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(P6Interact), true, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 12000.0f,
		ECC_Visibility, QueryParams))
	{
		if (ANaiwaP6InteractableActor* Point = Cast<ANaiwaP6InteractableActor>(Hit.GetActor()))
		{
			if (FVector::DistSquared2D(GetActorLocation(), Point->GetActorLocation()) <= FMath::Square(300.0f))
			{
				UpdateFeedback(Point->Interact());
			}
			else
			{
				UpdateFeedback(TEXT("请先走近这个调查点，再点击检查。"));
			}
		}
	}
}

void ANaiwaP6PlayerPawn::ToggleClueBoard()
{
	bCaseBoardOpen = !bCaseBoardOpen;
	if (bCaseBoardOpen)
	{
		const UNaiwaP6InvestigationSubsystem* Investigation = GetWorld()
			? GetWorld()->GetSubsystem<UNaiwaP6InvestigationSubsystem>() : nullptr;
		if (Investigation && Investigation->IsCaseConcluded())
		{
			UpdateFeedback(TEXT("本次结论已记录；证据卡仍可查看。重新进入 5-1 可再次复核。"));
		}
		else if (Investigation && Investigation->IsCaseFailedAtDawn())
		{
			UpdateFeedback(TEXT("已到 06:00，夜间结案流程暂停；仍可查看证据。离开并重新进入 5-1 后可晨间复核。"));
		}
		else
		{
			UpdateFeedback(TEXT("三段答案会一起提交；提交前不会显示单题判断。"));
		}
	}
}

void ANaiwaP6PlayerPawn::PreviousQuestion()
{
	if (!bCaseBoardOpen) return;
	const int32 QuestionCount = FNaiwaP6CaseDefinition::GetQuestionCount();
	if (QuestionCount <= 0) return;
	SelectedQuestionIndex = (SelectedQuestionIndex + QuestionCount - 1) % QuestionCount;
	UpdateFeedback(FString::Printf(TEXT("当前第 %d / %d 段；已选内容保留。"),
		SelectedQuestionIndex + 1, QuestionCount));
}

void ANaiwaP6PlayerPawn::NextQuestion()
{
	if (!bCaseBoardOpen) return;
	const int32 QuestionCount = FNaiwaP6CaseDefinition::GetQuestionCount();
	if (QuestionCount <= 0) return;
	SelectedQuestionIndex = (SelectedQuestionIndex + 1) % QuestionCount;
	UpdateFeedback(FString::Printf(TEXT("当前第 %d / %d 段；已选内容保留。"),
		SelectedQuestionIndex + 1, QuestionCount));
}

void ANaiwaP6PlayerPawn::ToggleInfoLayer()
{
	if (bCaseBoardOpen) return;
	if (ANaiwaP6InteractableActor* Nearby = GetNearbyInteractable())
	{
		const UNaiwaP6InvestigationSubsystem* Investigation = GetWorld()
			? GetWorld()->GetSubsystem<UNaiwaP6InvestigationSubsystem>() : nullptr;
		if (Investigation)
		{
			UpdateFeedback(Nearby->ToggleInfoLayer(Investigation->GetCurrentSkinId()));
			return;
		}
	}
	UpdateFeedback(TEXT("当前附近没有调查点；走近戏台边的奖杯调查点后按 F 查看皮肤信息层。"));
}

void ANaiwaP6PlayerPawn::SwitchSkin()
{
	if (bCaseBoardOpen) return;
	UNaiwaP6InvestigationSubsystem* Investigation = GetWorld()
		? GetWorld()->GetSubsystem<UNaiwaP6InvestigationSubsystem>() : nullptr;
	if (!Investigation)
	{
		UpdateFeedback(TEXT("当前案件状态暂不可用。"));
		return;
	}
	FString Feedback;
	if (!Investigation->CycleSkin(Feedback))
	{
		UpdateFeedback(Feedback);
		return;
	}
	for (TActorIterator<ANaiwaP6InteractableActor> It(GetWorld()); It; ++It)
	{
		It->HideInfoLayer();
	}
	ApplyCurrentSkinAppearance();
	UpdateFeedback(Feedback);
}

void ANaiwaP6PlayerPawn::ApplyCurrentSkinAppearance()
{
	if (!BodyMaterial) return;
	const UNaiwaP6InvestigationSubsystem* Investigation = GetWorld()
		? GetWorld()->GetSubsystem<UNaiwaP6InvestigationSubsystem>() : nullptr;
	const FName SkinId = Investigation ? Investigation->GetCurrentSkinId() : FName(TEXT("NaiwaSkin_Base"));
	FLinearColor Color(0.30f, 0.72f, 0.66f);
	if (SkinId == FName(TEXT("NaiwaSkin_NaiYu"))) Color = FLinearColor(0.28f, 0.68f, 0.82f);
	else if (SkinId == FName(TEXT("NaiwaSkin_NaiCha"))) Color = FLinearColor(0.62f, 0.78f, 0.48f);
	else if (SkinId == FName(TEXT("NaiwaSkin_NaiHao"))) Color = FLinearColor(0.88f, 0.70f, 0.35f);
	else if (SkinId == FName(TEXT("NaiwaSkin_NaiSha"))) Color = FLinearColor(0.83f, 0.43f, 0.39f);
	BodyMaterial->SetVectorParameterValue(TEXT("Color"), Color);
}

void ANaiwaP6PlayerPawn::SelectAnswer1() { SelectAnswer(0); }
void ANaiwaP6PlayerPawn::SelectAnswer2() { SelectAnswer(1); }
void ANaiwaP6PlayerPawn::SelectAnswer3() { SelectAnswer(2); }
void ANaiwaP6PlayerPawn::SelectAnswer4() { SelectAnswer(3); }

void ANaiwaP6PlayerPawn::SelectAnswer(const int32 OptionIndex)
{
	if (!bCaseBoardOpen) return;
	const TArray<FNaiwaP6AnswerOption>& Options = FNaiwaP6CaseDefinition::GetQuestionOptions(SelectedQuestionIndex);
	if (!Options.IsValidIndex(OptionIndex))
	{
		UpdateFeedback(TEXT("当前题目没有这个选项。"));
		return;
	}
	UNaiwaP6InvestigationSubsystem* Investigation = GetWorld()
		? GetWorld()->GetSubsystem<UNaiwaP6InvestigationSubsystem>() : nullptr;
	if (!Investigation)
	{
		UpdateFeedback(TEXT("案件状态暂不可用。"));
		return;
	}
	FString Feedback;
	Investigation->SetSelectedAnswer(SelectedQuestionIndex, Options[OptionIndex].AnswerId, Feedback);
	UpdateFeedback(Feedback);
}

void ANaiwaP6PlayerPawn::SubmitAnswer()
{
	if (!bCaseBoardOpen) return;
	UNaiwaP6InvestigationSubsystem* Investigation = GetWorld()
		? GetWorld()->GetSubsystem<UNaiwaP6InvestigationSubsystem>() : nullptr;
	if (!Investigation)
	{
		UpdateFeedback(TEXT("案件状态暂不可用。"));
		return;
	}
	FString Feedback;
	Investigation->SubmitCase(Feedback);
	UpdateFeedback(Feedback);
}

void ANaiwaP6PlayerPawn::ReturnToLevelSelect()
{
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Naiwa/UI/L_NaiwaMainMenu")),
		true, TEXT("NaiwaMenuPage=levels"));
}

void ANaiwaP6PlayerPawn::UpdateFeedback(const FString& Text)
{
	LastFeedback = Text;
}
