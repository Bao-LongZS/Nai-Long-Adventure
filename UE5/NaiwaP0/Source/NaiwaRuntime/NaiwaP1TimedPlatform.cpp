#include "NaiwaP1TimedPlatform.h"

#include "NaiwaCharacter.h"
#include "NaiwaP1GameMode.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* StateName(ENaiwaP1TimedPlatformState State)
	{
		switch (State)
		{
		case ENaiwaP1TimedPlatformState::Ready: return TEXT("Ready");
		case ENaiwaP1TimedPlatformState::Countdown: return TEXT("Countdown");
		case ENaiwaP1TimedPlatformState::Disabled: return TEXT("Disabled");
		case ENaiwaP1TimedPlatformState::Recovering: return TEXT("Recovering");
		}
		return TEXT("Unknown");
	}
}

ANaiwaP1TimedPlatform::ANaiwaP1TimedPlatform()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	PlatformCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("PlatformCollision"));
	PlatformCollision->SetupAttachment(SceneRoot);
	PlatformCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PlatformCollision->SetCollisionObjectType(ECC_WorldStatic);
	PlatformCollision->SetCollisionResponseToAllChannels(ECR_Block);
	PlatformCollision->SetGenerateOverlapEvents(false);
	PlatformCollision->SetCanEverAffectNavigation(false);

	PlatformDisplay = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformDisplay"));
	PlatformDisplay->SetupAttachment(PlatformCollision);
	PlatformDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) PlatformDisplay->SetStaticMesh(Cube.Object);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusText->SetupAttachment(PlatformCollision);
	StatusText->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	StatusText->SetRelativeScale3D(FVector(1.0f, -1.0f, 1.0f));
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextCenter);
	StatusText->SetWorldSize(30.0f);
	StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitText(
		TEXT("/Engine/EngineMaterials/UnlitText.UnlitText"));
	if (UnlitText.Succeeded()) StatusText->SetTextMaterial(UnlitText.Object);

	ApplyGeometry();
	UpdatePresentation();
}

void ANaiwaP1TimedPlatform::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyGeometry();
	UpdatePresentation();
}

void ANaiwaP1TimedPlatform::BeginPlay()
{
	Super::BeginPlay();
	ResetForAttempt();
	UE_LOG(LogTemp, Display,
		TEXT("NAIWA_P1_TIMED_PLATFORM ready top=%.1f extent_x=%.1f active=%.2f disabled=%.2f"),
		PlatformTopZ, PlatformExtent.X, ActiveSeconds, DisabledSeconds);
}

void ANaiwaP1TimedPlatform::ApplyGeometry()
{
	const FVector SafeExtent = PlatformExtent.ComponentMax(FVector(1.0f));
	PlatformCollision->SetBoxExtent(SafeExtent);
	PlatformDisplay->SetRelativeLocation(FVector::ZeroVector);
	PlatformDisplay->SetRelativeScale3D(SafeExtent / 50.0f);
	StatusText->SetRelativeLocation(FVector(125.0f, 0.0f, SafeExtent.Z + 145.0f));
	// The actor origin is the map's reference floor; the component centre sits below its top.
	PlatformCollision->SetRelativeLocation(FVector(0.0f, 0.0f, PlatformTopZ - SafeExtent.Z));
}

bool ANaiwaP1TimedPlatform::IsCharacterSupported(const ANaiwaCharacter* Character) const
{
	return Character && Character->GetCharacterMovement()->IsMovingOnGround()
		&& Character->GetMovementBaseObject() == PlatformCollision.Get();
}

bool ANaiwaP1TimedPlatform::IsCarryingCollisionEnabled() const
{
	return PlatformCollision && PlatformCollision->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics;
}

bool ANaiwaP1TimedPlatform::IsCapsuleOccupyingPlatform() const
{
	ANaiwaCharacter* Character = Cast<ANaiwaCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!Character || !PlatformCollision || !Character->GetCapsuleComponent()) return false;
	// Bounds are conservative for rotated boxes, which is intentional: never restore
	// blocking collision while the player's capsule might be inside the floor.
	const FBox PlatformBounds = PlatformCollision->Bounds.GetBox().ExpandBy(1.0f);
	const FBox CapsuleBounds = Character->GetCapsuleComponent()->Bounds.GetBox();
	return PlatformBounds.Intersect(CapsuleBounds);
}

void ANaiwaP1TimedPlatform::SetCarryingEnabled(bool bEnabled)
{
	const ECollisionEnabled::Type Desired = bEnabled
		? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
	if (PlatformCollision->GetCollisionEnabled() != Desired)
	{
		PlatformCollision->SetCollisionEnabled(Desired);
	}
	PlatformDisplay->SetVisibility(bEnabled);
}

void ANaiwaP1TimedPlatform::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ANaiwaP1GameMode* Mode = Cast<ANaiwaP1GameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode || !Mode->IsGameplayInputAllowed()) return;

	ANaiwaCharacter* Character = Cast<ANaiwaCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	switch (State)
	{
	case ENaiwaP1TimedPlatformState::Ready:
		StableLandingElapsed = IsCharacterSupported(Character)
			? StableLandingElapsed + DeltaSeconds : 0.0f;
		if (StableLandingElapsed >= FMath::Max(0.05f, StableLandingSeconds))
		{
			State = ENaiwaP1TimedPlatformState::Countdown;
			CountdownRemaining = FMath::Max(0.1f, ActiveSeconds);
			StableLandingElapsed = 0.0f;
			++TriggerCount;
			UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_PLATFORM state=%s trigger_count=%d remaining=%.2f"),
				StateName(State), TriggerCount, CountdownRemaining);
		}
		break;
	case ENaiwaP1TimedPlatformState::Countdown:
		// Once armed, leaving/re-entering or repeated overlap cannot extend the timer.
		CountdownRemaining = FMath::Max(0.0f, CountdownRemaining - DeltaSeconds);
		if (CountdownRemaining <= 0.0f)
		{
			const bool bWasRiderSupported = IsCharacterSupported(Character);
			State = ENaiwaP1TimedPlatformState::Disabled;
			DisabledRemaining = FMath::Max(0.1f, DisabledSeconds);
			SetCarryingEnabled(false);
			if (bWasRiderSupported && Character)
			{
				Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			}
			++DisableCount;
			UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_PLATFORM state=%s disable_count=%d seconds=%.2f"),
				StateName(State), DisableCount, DisabledRemaining);
		}
		break;
	case ENaiwaP1TimedPlatformState::Disabled:
		DisabledRemaining = FMath::Max(0.0f, DisabledRemaining - DeltaSeconds);
		if (DisabledRemaining <= 0.0f)
		{
			State = ENaiwaP1TimedPlatformState::Recovering;
			UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_PLATFORM state=%s occupied=%d"),
				StateName(State), IsCapsuleOccupyingPlatform());
		}
		break;
	case ENaiwaP1TimedPlatformState::Recovering:
		if (!IsCapsuleOccupyingPlatform())
		{
			SetCarryingEnabled(true);
			State = ENaiwaP1TimedPlatformState::Ready;
			StableLandingElapsed = 0.0f;
			++RecoveryCount;
			UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_PLATFORM state=%s recovery_count=%d"),
				StateName(State), RecoveryCount);
		}
		break;
	}
	UpdatePresentation();
}

void ANaiwaP1TimedPlatform::ResetForAttempt()
{
	const bool bWasDisabled = !IsCarryingCollisionEnabled();
	StableLandingElapsed = 0.0f;
	CountdownRemaining = 0.0f;
	DisabledRemaining = 0.0f;
	if (bWasDisabled && IsCapsuleOccupyingPlatform())
	{
		// GameMode resets mechanisms before teleporting the player on death/R.
		// Defer collision until a later gameplay tick sees the new clear position.
		State = ENaiwaP1TimedPlatformState::Recovering;
		SetCarryingEnabled(false);
	}
	else
	{
		State = ENaiwaP1TimedPlatformState::Ready;
		SetCarryingEnabled(true);
	}
	UpdatePresentation();
	UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_PLATFORM reset state=%s occupied=%d lifetime_triggers=%d"),
		StateName(State), IsCapsuleOccupyingPlatform(), TriggerCount);
}

void ANaiwaP1TimedPlatform::UpdatePresentation()
{
	if (!StatusText) return;
	FString Text;
	FColor Color = FColor::White;
	switch (State)
	{
	case ENaiwaP1TimedPlatformState::Ready:
		Text = TEXT("TIMED FLOOR READY");
		Color = FColor(90, 255, 150);
		break;
	case ENaiwaP1TimedPlatformState::Countdown:
		Text = FString::Printf(TEXT("FALLS IN %.1f"), CountdownRemaining);
		Color = CountdownRemaining <= 1.0f ? FColor(255, 80, 75) : FColor(255, 218, 70);
		break;
	case ENaiwaP1TimedPlatformState::Disabled:
		Text = FString::Printf(TEXT("GONE %.1f"), DisabledRemaining);
		Color = FColor(255, 80, 75);
		break;
	case ENaiwaP1TimedPlatformState::Recovering:
		Text = TEXT("CLEAR TO RESTORE");
		Color = FColor(120, 200, 255);
		break;
	}
	if (Text != LastStatusText)
	{
		LastStatusText = Text;
		StatusText->SetText(FText::FromString(Text));
		StatusText->SetTextRenderColor(Color);
	}
}
