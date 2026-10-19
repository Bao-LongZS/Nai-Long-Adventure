#include "NaiwaP1SwitchPlatform.h"

#include "NaiwaCharacter.h"
#include "NaiwaP1GameMode.h"
#include "NaiwaP1Trigger.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* StateName(ENaiwaP1SwitchPlatformState State)
	{
		switch (State)
		{
		case ENaiwaP1SwitchPlatformState::DockedLeft: return TEXT("DockedLeft");
		case ENaiwaP1SwitchPlatformState::TravelingRight: return TEXT("TravelingRight");
		case ENaiwaP1SwitchPlatformState::DockedRight: return TEXT("DockedRight");
		case ENaiwaP1SwitchPlatformState::TravelingLeft: return TEXT("TravelingLeft");
		}
		return TEXT("Unknown");
	}

	void ConfigureSwitchTrigger(UBoxComponent* Trigger)
	{
		Trigger->SetBoxExtent(FVector(75.0f, 150.0f, 80.0f));
		Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Trigger->SetCollisionObjectType(ECC_WorldDynamic);
		Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
		Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Trigger->SetGenerateOverlapEvents(true);
		Trigger->SetCanEverAffectNavigation(false);
	}

	void ConfigureText(UTextRenderComponent* Text, float WorldSize)
	{
		Text->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		Text->SetHorizontalAlignment(EHTA_Center);
		Text->SetVerticalAlignment(EVRTA_TextCenter);
		Text->SetWorldSize(WorldSize);
		Text->SetRelativeScale3D(FVector(1.0f, -1.0f, 1.0f));
		Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitText(
			TEXT("/Engine/EngineMaterials/UnlitText.UnlitText"));
		if (UnlitText.Succeeded()) Text->SetTextMaterial(UnlitText.Object);
		Text->SetTextRenderColor(FColor::White);
	}
}

ANaiwaP1SwitchPlatform::ANaiwaP1SwitchPlatform()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	PlatformCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("PlatformCollision"));
	PlatformCollision->SetupAttachment(SceneRoot);
	PlatformCollision->SetMobility(EComponentMobility::Movable);
	PlatformCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PlatformCollision->SetCollisionObjectType(ECC_WorldDynamic);
	PlatformCollision->SetCollisionResponseToAllChannels(ECR_Block);
	PlatformCollision->SetGenerateOverlapEvents(false);
	PlatformCollision->SetCanEverAffectNavigation(false);

	PlatformDisplay = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformDisplay"));
	PlatformDisplay->SetupAttachment(PlatformCollision);
	PlatformDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) PlatformDisplay->SetStaticMesh(Cube.Object);

	PlatformStatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PlatformStatusText"));
	PlatformStatusText->SetupAttachment(PlatformCollision);
	ConfigureText(PlatformStatusText, 30.0f);

	LeftSwitchTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftSwitchTrigger"));
	LeftSwitchTrigger->SetupAttachment(SceneRoot);
	ConfigureSwitchTrigger(LeftSwitchTrigger);
	LeftSwitchTrigger->OnComponentBeginOverlap.AddDynamic(this, &ANaiwaP1SwitchPlatform::HandleLeftSwitchBeginOverlap);
	LeftSwitchTrigger->OnComponentEndOverlap.AddDynamic(this, &ANaiwaP1SwitchPlatform::HandleLeftSwitchEndOverlap);

	LeftSwitchDisplay = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftSwitchDisplay"));
	LeftSwitchDisplay->SetupAttachment(LeftSwitchTrigger);
	LeftSwitchDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cube.Succeeded()) LeftSwitchDisplay->SetStaticMesh(Cube.Object);

	LeftSwitchText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LeftSwitchText"));
	LeftSwitchText->SetupAttachment(SceneRoot);
	ConfigureText(LeftSwitchText, 24.0f);
	LeftSwitchText->SetText(FText::FromString(TEXT("CALL LEFT")));

	RightSwitchTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("RightSwitchTrigger"));
	RightSwitchTrigger->SetupAttachment(SceneRoot);
	ConfigureSwitchTrigger(RightSwitchTrigger);
	RightSwitchTrigger->OnComponentBeginOverlap.AddDynamic(this, &ANaiwaP1SwitchPlatform::HandleRightSwitchBeginOverlap);
	RightSwitchTrigger->OnComponentEndOverlap.AddDynamic(this, &ANaiwaP1SwitchPlatform::HandleRightSwitchEndOverlap);

	RightSwitchDisplay = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightSwitchDisplay"));
	RightSwitchDisplay->SetupAttachment(RightSwitchTrigger);
	RightSwitchDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cube.Succeeded()) RightSwitchDisplay->SetStaticMesh(Cube.Object);

	RightSwitchText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RightSwitchText"));
	RightSwitchText->SetupAttachment(SceneRoot);
	ConfigureText(RightSwitchText, 24.0f);
	RightSwitchText->SetText(FText::FromString(TEXT("CALL RIGHT")));

	ApplyGeometry();
}

void ANaiwaP1SwitchPlatform::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyGeometry();
	UpdateStatusText();
}

void ANaiwaP1SwitchPlatform::BeginPlay()
{
	Super::BeginPlay();
	TransitRunSeed = static_cast<uint32>(FPlatformTime::Cycles64())
		^ (static_cast<uint32>(GetUniqueID()) * 0x85ebca6bu);
	ApplyGeometry();
	ResetForAttempt();
	UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PLATFORM ready left=%.1f right=%.1f speed=%.1f variation=%.1f trap_run_seed=%u"),
		DockLeftX, DockRightX, TravelSpeedCmPerSecond, TravelSpeedVariationCmPerSecond, TransitRunSeed);
}

void ANaiwaP1SwitchPlatform::ApplyGeometry()
{
	const FVector SafeExtent = PlatformExtent.ComponentMax(FVector(1.0f));
	PlatformCollision->SetBoxExtent(SafeExtent);
	PlatformDisplay->SetRelativeLocation(FVector::ZeroVector);
	PlatformDisplay->SetRelativeScale3D(SafeExtent / 50.0f);
	if (PlatformStatusText->GetAttachParent() != PlatformCollision)
	{
		PlatformStatusText->AttachToComponent(PlatformCollision, FAttachmentTransformRules::KeepRelativeTransform);
	}
	PlatformStatusText->SetRelativeLocation(FVector(110.0f, 0.0f,
		FMath::Max(1.0f, PlatformExtent.Z) + 150.0f));
	LeftSwitchTrigger->SetRelativeLocation(FVector(LeftSwitchX, 0.0f, GetDockLeftTopZ() + 35.0f));
	LeftSwitchDisplay->SetRelativeLocation(FVector(0.0f, 0.0f, -18.0f));
	LeftSwitchDisplay->SetRelativeScale3D(FVector(1.1f, 2.2f, 0.45f));
	LeftSwitchText->SetRelativeLocation(FVector(LeftSwitchX, 0.0f, GetDockLeftTopZ() + 140.0f));
	RightSwitchTrigger->SetRelativeLocation(FVector(RightSwitchX, 0.0f, GetDockRightTopZ() + 35.0f));
	RightSwitchDisplay->SetRelativeLocation(FVector(0.0f, 0.0f, -18.0f));
	RightSwitchDisplay->SetRelativeScale3D(FVector(1.1f, 2.2f, 0.45f));
	RightSwitchText->SetRelativeLocation(FVector(RightSwitchX, 0.0f, GetDockRightTopZ() + 140.0f));
	if (GetWorld() && GetWorld()->WorldType != EWorldType::EditorPreview)
	{
		const float CurrentX = PlatformCollision->GetRelativeLocation().X;
		SetPlatformX(FMath::IsNearlyZero(CurrentX) ? DockLeftX : CurrentX);
	}
}

void ANaiwaP1SwitchPlatform::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ANaiwaP1GameMode* Mode = Cast<ANaiwaP1GameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode || !Mode->IsGameplayInputAllowed())
	{
		UpdateStatusText();
		return;
	}

	if (State == ENaiwaP1SwitchPlatformState::TravelingLeft
		|| State == ENaiwaP1SwitchPlatformState::TravelingRight)
	{
		const bool bMovingRight = State == ENaiwaP1SwitchPlatformState::TravelingRight;
		const float TargetX = bMovingRight ? DockRightX : DockLeftX;
		const float NewX = FMath::FInterpConstantTo(GetCurrentX(), TargetX, DeltaSeconds,
			GetCurrentTravelSpeedCmPerSecond());
		SetPlatformX(NewX);
		if (FMath::IsNearlyEqual(NewX, TargetX, 0.1f))
		{
			SetPlatformX(TargetX);
			State = bMovingRight ? ENaiwaP1SwitchPlatformState::DockedRight
				: ENaiwaP1SwitchPlatformState::DockedLeft;
			StableBoardingElapsed = 0.0f;
			bDepartureCueActive = false;
			DepartureCueRemaining = 0.0f;
			const ANaiwaCharacter* Character = Cast<ANaiwaCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
			// A moving-base update can lag the arrival frame. Keep the departure lock
			// until the rider actually leaves the platform's horizontal footprint.
			bRiderMustExitBeforeNextDeparture = bRiderMustExitBeforeNextDeparture
				|| IsCharacterWithinPlatformFootprint(Character);
			UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PLATFORM state=%s x=%.1f rider=%d"),
				StateName(State), GetCurrentX(), bRiderMustExitBeforeNextDeparture);
		}
		UpdateStatusText();
		return;
	}

	const ANaiwaCharacter* Character = Cast<ANaiwaCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	const bool bRiderSupported = IsCharacterSupported(Character);
	if (bRiderMustExitBeforeNextDeparture)
	{
		if (!bRiderSupported && !IsCharacterWithinPlatformFootprint(Character))
		{
			bRiderMustExitBeforeNextDeparture = false;
			StableBoardingElapsed = 0.0f;
			UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PLATFORM rider_left_dock x=%.1f"), GetCurrentX());
		}
		else
		{
			UpdateStatusText();
			return;
		}
	}

	if (!bRiderSupported)
	{
		StableBoardingElapsed = 0.0f;
			bDepartureCueActive = false;
		DepartureCueRemaining = 0.0f;
		UpdateStatusText();
		return;
	}

	if (!bDepartureCueActive)
	{
		StableBoardingElapsed += DeltaSeconds;
		if (StableBoardingElapsed >= FMath::Max(0.05f, StableBoardingSeconds))
		{
			bDepartureCueActive = true;
			DepartureCueRemaining = FMath::Max(0.05f, DepartureCueSeconds);
			UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PLATFORM departure_cue state=%s seconds=%.2f"),
				StateName(State), DepartureCueRemaining);
		}
	}
	else
	{
		DepartureCueRemaining = FMath::Max(0.0f, DepartureCueRemaining - DeltaSeconds);
		if (DepartureCueRemaining <= 0.0f)
		{
			const bool bDockedLeft = State == ENaiwaP1SwitchPlatformState::DockedLeft;
			StartTravel(bDockedLeft, TEXT("boarded"));
		}
	}
	UpdateStatusText();
}

void ANaiwaP1SwitchPlatform::ResetForAttempt()
{
	ClearTransitTraps();
	TransitTripIndex = 0;
	State = ENaiwaP1SwitchPlatformState::DockedLeft;
	StableBoardingElapsed = 0.0f;
	DepartureCueRemaining = 0.0f;
	bDepartureCueActive = false;
	bRiderMustExitBeforeNextDeparture = false;
	SetPlatformX(DockLeftX);
	const ANaiwaCharacter* Character = Cast<ANaiwaCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	bLeftSwitchLatched = Character && LeftSwitchTrigger->IsOverlappingActor(Character);
	bRightSwitchLatched = Character && RightSwitchTrigger->IsOverlappingActor(Character);
	UpdateStatusText();
	UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PLATFORM reset state=DockedLeft x=%.1f"), DockLeftX);
}

float ANaiwaP1SwitchPlatform::GetCurrentX() const
{
	return PlatformCollision ? PlatformCollision->GetRelativeLocation().X : DockLeftX;
}

float ANaiwaP1SwitchPlatform::GetCurrentTopZ() const
{
	if (!PlatformCollision) return GetDockLeftTopZ();
	return PlatformCollision->GetRelativeLocation().Z + FMath::Max(1.0f, PlatformExtent.Z);
}

float ANaiwaP1SwitchPlatform::GetCurrentTravelSpeedCmPerSecond() const
{
	const float Distance = DockRightX - DockLeftX;
	if (FMath::Abs(Distance) <= KINDA_SMALL_NUMBER)
	{
		return FMath::Max(1.0f, TravelSpeedCmPerSecond);
	}
	const float Alpha = FMath::Clamp((GetCurrentX() - DockLeftX) / Distance, 0.0f, 1.0f);
	const float SpeedPhase = Alpha * 4.0f * PI;
	const float Speed = TravelSpeedCmPerSecond + TravelSpeedVariationCmPerSecond * FMath::Sin(SpeedPhase);
	return FMath::Max(1.0f, Speed);
}

float ANaiwaP1SwitchPlatform::GetDockLeftTopZ() const
{
	return DockLeftTopZ >= 0.0f ? DockLeftTopZ : PlatformTopZ;
}

float ANaiwaP1SwitchPlatform::GetDockRightTopZ() const
{
	return DockRightTopZ >= 0.0f ? DockRightTopZ : PlatformTopZ;
}

bool ANaiwaP1SwitchPlatform::IsCharacterSupported(const ANaiwaCharacter* Character) const
{
	return Character && Character->GetCharacterMovement()->IsMovingOnGround()
		&& Character->GetMovementBaseObject() == PlatformCollision.Get();
}

bool ANaiwaP1SwitchPlatform::IsCharacterWithinPlatformFootprint(const ANaiwaCharacter* Character) const
{
	if (!Character || !PlatformCollision) return false;
	const FVector Offset = Character->GetActorLocation() - PlatformCollision->GetComponentLocation();
	const FVector Extent = PlatformCollision->GetScaledBoxExtent();
	return FMath::Abs(Offset.X) <= Extent.X + 10.0f
		&& FMath::Abs(Offset.Y) <= Extent.Y + 10.0f;
}

bool ANaiwaP1SwitchPlatform::RequestDockLeft()
{
	const ANaiwaP1GameMode* Mode = Cast<ANaiwaP1GameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode || !Mode->IsGameplayInputAllowed()) return false;
	if (State != ENaiwaP1SwitchPlatformState::DockedRight)
	{
		UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PLATFORM request_ignored target=Left state=%s"), StateName(State));
		return false;
	}
	StartTravel(false, TEXT("left_switch"));
	return true;
}

bool ANaiwaP1SwitchPlatform::RequestDockRight()
{
	const ANaiwaP1GameMode* Mode = Cast<ANaiwaP1GameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode || !Mode->IsGameplayInputAllowed()) return false;
	if (State != ENaiwaP1SwitchPlatformState::DockedLeft)
	{
		UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PLATFORM request_ignored target=Right state=%s"), StateName(State));
		return false;
	}
	StartTravel(true, TEXT("right_switch"));
	return true;
}

void ANaiwaP1SwitchPlatform::StartTravel(bool bToRight, const TCHAR* Cause)
{
	if (State == ENaiwaP1SwitchPlatformState::TravelingLeft
		|| State == ENaiwaP1SwitchPlatformState::TravelingRight)
	{
		UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PLATFORM request_ignored target=%s state=%s"),
			bToRight ? TEXT("Right") : TEXT("Left"), StateName(State));
		return;
	}
	bDepartureCueActive = false;
	DepartureCueRemaining = 0.0f;
	StableBoardingElapsed = 0.0f;
	State = bToRight ? ENaiwaP1SwitchPlatformState::TravelingRight
		: ENaiwaP1SwitchPlatformState::TravelingLeft;
	bRiderMustExitBeforeNextDeparture = true;
	SpawnTransitTraps(bToRight);
	UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PLATFORM state=%s cause=%s x=%.1f"),
		StateName(State), Cause, GetCurrentX());
	UpdateStatusText();
}

void ANaiwaP1SwitchPlatform::HandleLeftSwitchBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ANaiwaCharacter* Character = Cast<ANaiwaCharacter>(OtherActor);
	if (!Character || bLeftSwitchLatched) return;
	bLeftSwitchLatched = true;
	++LeftSwitchTouchCount;
	UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PLATFORM touch=Left count=%d"), LeftSwitchTouchCount);
	if (const ANaiwaP1GameMode* Mode = Cast<ANaiwaP1GameMode>(UGameplayStatics::GetGameMode(this));
		Mode && Mode->IsGameplayInputAllowed())
	{
		RequestDockLeft();
	}
}

void ANaiwaP1SwitchPlatform::HandleLeftSwitchEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	if (Cast<ANaiwaCharacter>(OtherActor)) bLeftSwitchLatched = false;
}

void ANaiwaP1SwitchPlatform::HandleRightSwitchBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ANaiwaCharacter* Character = Cast<ANaiwaCharacter>(OtherActor);
	if (!Character || bRightSwitchLatched) return;
	bRightSwitchLatched = true;
	++RightSwitchTouchCount;
	UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PLATFORM touch=Right count=%d"), RightSwitchTouchCount);
	if (const ANaiwaP1GameMode* Mode = Cast<ANaiwaP1GameMode>(UGameplayStatics::GetGameMode(this));
		Mode && Mode->IsGameplayInputAllowed())
	{
		RequestDockRight();
	}
}

void ANaiwaP1SwitchPlatform::HandleRightSwitchEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	if (Cast<ANaiwaCharacter>(OtherActor)) bRightSwitchLatched = false;
}

void ANaiwaP1SwitchPlatform::UpdateStatusText()
{
	if (!PlatformStatusText) return;
	FString Text;
	FColor Color = FColor::White;
	if (bDepartureCueActive)
	{
		const bool bWillGoRight = State == ENaiwaP1SwitchPlatformState::DockedLeft;
		Text = FString::Printf(TEXT("READY %s  %.1f"), bWillGoRight ? TEXT("RIGHT") : TEXT("LEFT"), DepartureCueRemaining);
		Color = FColor(255, 218, 70);
	}
	else
	{
		switch (State)
		{
		case ENaiwaP1SwitchPlatformState::DockedLeft:
			Text = TEXT("DOCK LEFT  BOARD TO DEPART");
			Color = FColor(90, 255, 150);
			break;
		case ENaiwaP1SwitchPlatformState::TravelingRight:
			Text = FString::Printf(TEXT("MOVING RIGHT  %s"),
				GetCurrentTravelSpeedCmPerSecond() >= TravelSpeedCmPerSecond ? TEXT("FAST") : TEXT("SLOW"));
			Color = FColor(255, 218, 70);
			break;
		case ENaiwaP1SwitchPlatformState::DockedRight:
			Text = TEXT("DOCK RIGHT  BOARD TO DEPART");
			Color = FColor(90, 255, 150);
			break;
		case ENaiwaP1SwitchPlatformState::TravelingLeft:
			Text = FString::Printf(TEXT("MOVING LEFT  %s"),
				GetCurrentTravelSpeedCmPerSecond() >= TravelSpeedCmPerSecond ? TEXT("FAST") : TEXT("SLOW"));
			Color = FColor(255, 218, 70);
			break;
		}
	}
	if (Text != LastStatusText)
	{
		LastStatusText = Text;
		PlatformStatusText->SetText(FText::FromString(Text));
		PlatformStatusText->SetTextRenderColor(Color);
	}
}

void ANaiwaP1SwitchPlatform::SetPlatformX(float NewX)
{
	if (!PlatformCollision) return;
	FVector Location = PlatformCollision->GetRelativeLocation();
	Location.X = NewX;
	const float TravelAlpha = FMath::Clamp((NewX - DockLeftX) / FMath::Max(1.0f, DockRightX - DockLeftX), 0.0f, 1.0f);
	const float TopZ = FMath::Lerp(GetDockLeftTopZ(), GetDockRightTopZ(), TravelAlpha);
	Location.Z = TopZ - FMath::Max(1.0f, PlatformExtent.Z);
	PlatformCollision->SetRelativeLocation(Location, false, nullptr, ETeleportType::None);
}

void ANaiwaP1SwitchPlatform::ClearTransitTraps()
{
	for (ANaiwaP1Trigger* Trap : TransitTrapActors)
	{
		if (IsValid(Trap)) Trap->Destroy();
	}
	TransitTrapActors.Reset();
}

void ANaiwaP1SwitchPlatform::SpawnTransitTraps(bool bToRight)
{
	ClearTransitTraps();
	const int32 Minimum = FMath::Clamp(TransitTrapMinCount, 0, 4);
	const int32 Maximum = FMath::Clamp(TransitTrapMaxCount, Minimum, 4);
	const float TravelLength = FMath::Abs(DockRightX - DockLeftX);
	const float Margin = FMath::Max(300.0f, PlatformExtent.X + 180.0f);
	const float AvailableLength = TravelLength - Margin * 2.0f;
	if (Minimum <= 0 || Maximum <= 0 || AvailableLength < 160.0f || !GetWorld()) return;

	const uint32 Seed = static_cast<uint32>(TransitTrapSeed)
		^ TransitRunSeed
		^ (static_cast<uint32>(++TransitTripIndex) * 0x9e3779b9u);
	FRandomStream Random(static_cast<int32>(Seed));
	const int32 CountRange = Maximum - Minimum + 1;
	const int32 Count = Minimum + static_cast<int32>(Seed % static_cast<uint32>(CountRange));
	const float MinSpacing = 520.0f;
	if (Count > 1 && AvailableLength < MinSpacing * (Count - 1) + 160.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("NAIWA_P1_SWITCH_PLATFORM traps_skipped reason=short_path length=%.1f count=%d"),
			TravelLength, Count);
		return;
	}

	TArray<float> Distances;
	Distances.Reserve(Count);
	const float Slack = AvailableLength - MinSpacing * (Count - 1);
	const float ClusterStart = Margin + Random.FRandRange(0.0f, Slack);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Distances.Add(ClusterStart + Index * MinSpacing);
	}

	const float Direction = bToRight ? 1.0f : -1.0f;
	const float StartX = bToRight ? DockLeftX : DockRightX;
	for (int32 Index = 0; Index < Distances.Num(); ++Index)
	{
		const float Distance = Distances[Index];
		const float LocalX = StartX + Direction * Distance;
		const float Alpha = FMath::Clamp((LocalX - DockLeftX) / FMath::Max(1.0f, DockRightX - DockLeftX), 0.0f, 1.0f);
		const float TopZ = GetActorLocation().Z + FMath::Lerp(GetDockLeftTopZ(), GetDockRightTopZ(), Alpha);
		const FVector Location(GetActorLocation().X + LocalX, GetActorLocation().Y, TopZ);
		const FName TrapId(*FString::Printf(TEXT("%s_TransitTrap_%02d_%02d"),
			*GetFName().ToString(), TransitTripIndex, Index + 1));
		ANaiwaP1Trigger* Trap = GetWorld()->SpawnActor<ANaiwaP1Trigger>(ANaiwaP1Trigger::StaticClass(), Location, FRotator::ZeroRotator);
		if (!Trap) continue;
		Trap->ConfigureAsHazard(TrapId, FVector(44.0f, 150.0f, 50.0f),
			LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Naiwa/P1/Materials/M_P1_V7_hazard.M_P1_V7_hazard")));
		Trap->Tags.AddUnique(FName("NaiwaP1V7TransitTrap"));
		Trap->Tags.AddUnique(FName(*FString::Printf(TEXT("NaiwaP1V7TransitSeed_%u"), Seed)));
		TransitTrapActors.Add(Trap);
	}
	UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PLATFORM transit_traps count=%d trip=%d direction=%s seed=%u rise=[%.1f,%.1f]"),
		TransitTrapActors.Num(), TransitTripIndex, bToRight ? TEXT("right") : TEXT("left"), Seed,
		GetDockLeftTopZ(), GetDockRightTopZ());
}
