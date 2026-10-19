#include "NaiwaP1BouncePad.h"

#include "NaiwaCharacter.h"
#include "NaiwaP1GameMode.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ANaiwaP1BouncePad::ANaiwaP1BouncePad()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;

    static ConstructorHelpers::FObjectFinder<USoundBase> FeedbackBounceCue(
        TEXT("/Game/Naiwa/P1/Feedback/Audio/SFX_P1_Feedback_Jump.SFX_P1_Feedback_Jump"));
    static ConstructorHelpers::FObjectFinder<USoundBase> LegacyBounceCue(
        TEXT("/Game/Naiwa/P1/Legacy/Audio/SFX_P1_Jump.SFX_P1_Jump"));
    if (FeedbackBounceCue.Succeeded()) BounceSound = FeedbackBounceCue.Object;
    else if (LegacyBounceCue.Succeeded()) BounceSound = LegacyBounceCue.Object;

    PadCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("PadCollision"));
    RootComponent = PadCollision;
    PadCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PadCollision->SetCollisionObjectType(ECC_WorldStatic);
    PadCollision->SetCollisionResponseToAllChannels(ECR_Block);
    PadCollision->SetGenerateOverlapEvents(false);
    PadCollision->SetCanEverAffectNavigation(false);

    DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
    DisplayMesh->SetupAttachment(PadCollision);
    DisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded()) DisplayMesh->SetStaticMesh(Cube.Object);

    StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
    StatusText->SetupAttachment(PadCollision);
    StatusText->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
    StatusText->SetHorizontalAlignment(EHTA_Center);
    StatusText->SetVerticalAlignment(EVRTA_TextCenter);
    StatusText->SetWorldSize(24.0f);
    StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ApplyGeometry();
    UpdateStatusText(true);
}

void ANaiwaP1BouncePad::ApplyGeometry()
{
    const FVector SafeExtent = BoxExtent.ComponentMax(FVector(1.0f));
    PadCollision->SetBoxExtent(SafeExtent);
    DisplayMesh->SetRelativeScale3D(SafeExtent / 50.0f);
    StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, SafeExtent.Z + 48.0f));
}

void ANaiwaP1BouncePad::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyGeometry();
    UpdateStatusText(true);
}

void ANaiwaP1BouncePad::BeginPlay()
{
    Super::BeginPlay();
    ApplyGeometry();
    ResetForAttempt();
}

bool ANaiwaP1BouncePad::IsReady() const
{
    const ANaiwaP1GameMode* Mode = Cast<ANaiwaP1GameMode>(UGameplayStatics::GetGameMode(this));
    return Mode && Mode->IsGameplayInputAllowed() && !bContactLatched && CooldownRemaining <= 0.0f;
}

bool ANaiwaP1BouncePad::IsCharacterInContactRegion(const ANaiwaCharacter* Character, float FeetZ, float TopZ) const
{
    const FVector LocalPosition = PadCollision->GetComponentTransform().InverseTransformPosition(Character->GetActorLocation());
    const FVector Extent = PadCollision->GetUnscaledBoxExtent();
    const FVector Scale = PadCollision->GetComponentScale().GetAbs().ComponentMax(FVector(0.01f));
    const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float CharacterTop = Character->GetActorLocation().Z
        + Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    return FMath::Abs(LocalPosition.X) <= Extent.X + Radius / Scale.X
        && FMath::Abs(LocalPosition.Y) <= Extent.Y + Radius / Scale.Y
        && FeetZ <= TopZ + FMath::Max(1.0f, RearmClearanceCm)
        && CharacterTop >= PadCollision->GetComponentLocation().Z - Extent.Z * Scale.Z;
}

void ANaiwaP1BouncePad::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const ANaiwaP1GameMode* Mode = Cast<ANaiwaP1GameMode>(UGameplayStatics::GetGameMode(this));
    const bool bGameplayAllowed = Mode && Mode->IsGameplayInputAllowed();
    if (!bGameplayAllowed)
    {
        bHasPreviousSample = false;
        bDescendingLandingArmed = false;
        UpdateStatusText(false);
        return;
    }

    CooldownRemaining = FMath::Max(0.0f, CooldownRemaining - DeltaSeconds);
    FeedbackRemaining = FMath::Max(0.0f, FeedbackRemaining - DeltaSeconds);
    ANaiwaCharacter* Character = Cast<ANaiwaCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!Character)
    {
        bHasPreviousSample = false;
        bContactLatched = false;
        bDescendingLandingArmed = false;
        UpdateStatusText(true);
        return;
    }

    if (PreviousCharacter.Get() != Character)
    {
        PreviousCharacter = Character;
        bHasPreviousSample = false;
        bContactLatched = false;
        bDescendingLandingArmed = false;
    }

    const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const float FeetZ = Character->GetCapsuleBottom();
    const float TopZ = PadCollision->GetComponentTransform().TransformPosition(
        FVector(0.0f, 0.0f, PadCollision->GetUnscaledBoxExtent().Z)).Z;
    const bool bInContactRegion = IsCharacterInContactRegion(Character, FeetZ, TopZ);
    if (!bInContactRegion)
    {
        bContactLatched = false;
        bDescendingLandingArmed = false;
    }
    // On a high-speed landing, CharacterMovement may stop the capsule at the
    // pad top one sample before it assigns PadCollision as the movement base.
    // Keep the measured descent until the first actual grounded pad contact.
    if (bInContactRegion && bHasPreviousSample
        && PreviousFeetZ > FeetZ + 0.1f && PreviousFeetZ > TopZ)
        bDescendingLandingArmed = true;
    if (bInContactRegion)
    {
        DiagnosticLogRemaining -= DeltaSeconds;
        if (DiagnosticLogRemaining <= 0.0f)
        {
            const UObject* MovementBase = Character->GetMovementBaseObject();
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P1_BOUNCE contact_sample actor=%s feet=%.1f top=%.1f grounded=%d falling=%d base=%s prev_valid=%d prev_falling=%d prev_feet=%.1f latched=%d armed=%d cooldown=%.2f"),
                *GetName(), FeetZ, TopZ, Movement->IsMovingOnGround(), Movement->IsFalling(),
                *GetNameSafe(MovementBase), bHasPreviousSample, bPreviouslyFalling,
                PreviousFeetZ, bContactLatched, bDescendingLandingArmed, CooldownRemaining);
            DiagnosticLogRemaining = 1.0f;
        }
    }

    // A blocking side hit or an upward head hit never becomes a pad landing.
    // Sampling after CharacterMovement also works when landing has already zeroed Velocity.Z.
    const bool bStandingOnPad = Movement->IsMovingOnGround() && Character->GetMovementBase() == PadCollision.Get();
    if (bStandingOnPad && !bContactLatched)
    {
        // The downward approach may precede movement-base assignment by a
        // frame; a stationary side-entry never arms this condition.
        const bool bDescendedFromAbove = bDescendingLandingArmed;
        const bool bHorizontalPad = PadCollision->GetUpVector().Z >= 0.99f;
        // Consume even a cooldown/side-entry contact: waiting on the surface cannot retrigger it.
        bContactLatched = true;
        bDescendingLandingArmed = false;
        if (bDescendedFromAbove && bHorizontalPad && CooldownRemaining <= 0.0f)
        {
            Character->StopJumping();
            // Add a vertical impulse while preserving the horizontal movement
            // the player used to reach the pad.
            Character->LaunchCharacter(FVector(0.0f, 0.0f, FMath::Max(1.0f, LaunchVelocityZ)), false, true);
            CooldownRemaining = FMath::Max(0.0f, CooldownSeconds);
            FeedbackRemaining = FMath::Max(0.05f, TriggerFeedbackSeconds);
            ++ActivationCount;
            if (BounceSound) UGameplayStatics::PlaySound2D(this, BounceSound);
            UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_FEEDBACK event=bounce sound=%s activation=%d"),
                *GetNameSafe(BounceSound.Get()), ActivationCount);
            UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_BOUNCE fired actor=%s count=%d launch_z=%.1f feet_z=%.1f"),
                *GetName(), ActivationCount, LaunchVelocityZ, FeetZ);
        }
    }

    PreviousFeetZ = FeetZ;
    bPreviouslyFalling = Movement->IsFalling();
    bHasPreviousSample = true;
    UpdateStatusText(true);
}

void ANaiwaP1BouncePad::ResetForAttempt()
{
    PreviousCharacter.Reset();
    bHasPreviousSample = false;
    bPreviouslyFalling = false;
    bContactLatched = false;
    bDescendingLandingArmed = false;
    CooldownRemaining = 0.0f;
    FeedbackRemaining = 0.0f;
    DiagnosticLogRemaining = 0.0f;
    const ANaiwaP1GameMode* Mode = Cast<ANaiwaP1GameMode>(UGameplayStatics::GetGameMode(this));
    UpdateStatusText(Mode && Mode->IsGameplayInputAllowed());
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_BOUNCE reset actor=%s lifetime_count=%d"), *GetName(), ActivationCount);
}

void ANaiwaP1BouncePad::UpdateStatusText(bool bGameplayAllowed)
{
    FString Text;
    FColor Color;
    if (!bGameplayAllowed)
    {
        Text = TEXT("BOUNCE - DISABLED");
        Color = FColor(160, 160, 160);
    }
    else if (FeedbackRemaining > 0.0f)
    {
        Text = TEXT("^ BOUNCE! ^");
        Color = FColor::White;
    }
    else if (CooldownRemaining > 0.0f)
    {
        Text = FString::Printf(TEXT("RECHARGING %.1f"), CooldownRemaining);
        Color = FColor(255, 185, 40);
    }
    else if (bContactLatched)
    {
        Text = TEXT("LEAVE AND LAND AGAIN");
        Color = FColor(255, 185, 40);
    }
    else
    {
        Text = TEXT("^ BOUNCE READY ^");
        Color = FColor(80, 255, 145);
    }
    StatusText->SetText(FText::FromString(Text));
    StatusText->SetTextRenderColor(Color);
}
