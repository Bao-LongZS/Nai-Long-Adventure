#include "NaiwaP1Trigger.h"
#include "NaiwaCharacter.h"
#include "NaiwaP1GameMode.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ANaiwaP1Trigger::ANaiwaP1Trigger()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    TriggerBox->SetGenerateOverlapEvents(true);

    DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
    DisplayMesh->SetupAttachment(TriggerBox);
    DisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded()) DisplayMesh->SetStaticMesh(Cube.Object);

    CollectedDisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CollectedDisplayMesh"));
    CollectedDisplayMesh->SetupAttachment(TriggerBox);
    CollectedDisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CollectedDisplayMesh->SetVisibility(false);
    if (Cube.Succeeded()) CollectedDisplayMesh->SetStaticMesh(Cube.Object);

    SideSpikeLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SideSpikeLeft"));
    SideSpikeLeft->SetupAttachment(TriggerBox);
    SideSpikeLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SideSpikeRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SideSpikeRight"));
    SideSpikeRight->SetupAttachment(TriggerBox);
    SideSpikeRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    if (Cone.Succeeded())
    {
        SideSpikeLeft->SetStaticMesh(Cone.Object);
        SideSpikeRight->SetStaticMesh(Cone.Object);
    }

    MarkerText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("MarkerText"));
    MarkerText->SetupAttachment(TriggerBox);
    MarkerText->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    MarkerText->SetRelativeScale3D(FVector(1.0f, -1.0f, 1.0f));
    MarkerText->SetHorizontalAlignment(EHTA_Center);
    MarkerText->SetVerticalAlignment(EVRTA_TextCenter);
    MarkerText->SetWorldSize(42.0f);
    MarkerText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitText(
        TEXT("/Engine/EngineMaterials/UnlitText.UnlitText"));
    if (UnlitText.Succeeded()) MarkerText->SetTextMaterial(UnlitText.Object);
    MarkerText->SetTextRenderColor(FColor(255, 242, 170));
}

/**
 * One rule for every caller: KillZ is never drawn, a checkpoint/pickup/goal is always
 * drawn, and a hazard is drawn only when it opted into spawning visible. Keeping this
 * in one place stops OnConstruction, BeginPlay and ResetForAttempt from disagreeing.
 */
bool ANaiwaP1Trigger::ShouldDisplay() const
{
    if (TriggerKind == ENaiwaP1TriggerKind::KillZ) return false;
    if (TriggerKind == ENaiwaP1TriggerKind::Hazard) return bRevealOnSpawn;
    return true;
}

void ANaiwaP1Trigger::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyDisplayGeometry();
    SetDisplayVisible(ShouldDisplay());
}

void ANaiwaP1Trigger::BeginPlay()
{
    Super::BeginPlay();
    // Scripted editor property assignment may not rerun construction until the map is loaded.
    ApplyDisplayGeometry();
    ResetForAttempt();
    SetActorTickEnabled(TriggerKind == ENaiwaP1TriggerKind::Hazard || bPickupFading);
}

void ANaiwaP1Trigger::ApplyDisplayGeometry()
{
    if (!TriggerBox || !DisplayMesh) return;
    const FVector SafeExtent = BoxExtent.ComponentMax(FVector(1.0f));
    TriggerBox->SetBoxExtent(SafeExtent);
    if (TriggerKind == ENaiwaP1TriggerKind::Hazard && bUseSpikeCluster)
    {
        static UStaticMesh* ConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
        if (ConeMesh && DisplayMesh->GetStaticMesh() != ConeMesh) DisplayMesh->SetStaticMesh(ConeMesh);
        const float ClusterSpread = SafeExtent.X >= 80.0f ? SafeExtent.X * 0.58f : SafeExtent.X * 0.48f;
        const float SpikeWidth = SafeExtent.X >= 80.0f ? 0.82f : 0.70f;
        const FVector MainScale(SpikeWidth, 1.15f, 1.30f);
        DisplayMesh->SetRelativeLocation(FVector(0.0f, 0.0f, SafeExtent.Z));
        DisplayMesh->SetRelativeRotation(FRotator::ZeroRotator);
        DisplayMesh->SetRelativeScale3D(MainScale);
        SideSpikeLeft->SetRelativeLocation(FVector(-ClusterSpread, 0.0f, SafeExtent.Z));
        SideSpikeRight->SetRelativeLocation(FVector(ClusterSpread, 0.0f, SafeExtent.Z));
        SideSpikeLeft->SetRelativeRotation(FRotator::ZeroRotator);
        SideSpikeRight->SetRelativeRotation(FRotator::ZeroRotator);
        SideSpikeLeft->SetRelativeScale3D(MainScale * FVector(0.82f, 0.90f, 0.92f));
        SideSpikeRight->SetRelativeScale3D(MainScale * FVector(0.82f, 0.90f, 0.92f));
        SideSpikeLeft->SetVisibility(true);
        SideSpikeRight->SetVisibility(true);
        static UMaterialInterface* V7HazardMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Naiwa/P1/Materials/M_P1_V7_hazard.M_P1_V7_hazard"));
        if (V7HazardMaterial)
        {
            DisplayMesh->SetMaterial(0, V7HazardMaterial);
            SideSpikeLeft->SetMaterial(0, V7HazardMaterial);
            SideSpikeRight->SetMaterial(0, V7HazardMaterial);
        }
    }
    else if (TriggerKind == ENaiwaP1TriggerKind::Pickup)
    {
        static UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        static UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        static UMaterialInterface* SpriteMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Naiwa/P1/Feedback/Materials/M_P1_FeedbackPickup.M_P1_FeedbackPickup"));
        if (CubeMesh)
        {
            DisplayMesh->SetStaticMesh(CubeMesh);
            CollectedDisplayMesh->SetStaticMesh(SphereMesh ? SphereMesh : CubeMesh);
        }
        DisplayMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
        DisplayMesh->SetRelativeRotation(FRotator::ZeroRotator);
        DisplayMesh->SetRelativeScale3D(FVector(0.42f));
        CollectedDisplayMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
        CollectedDisplayMesh->SetRelativeRotation(FRotator::ZeroRotator);
        CollectedDisplayMesh->SetRelativeScale3D(FVector(0.42f));
        if (SpriteMaterial)
        {
            if (!PickupClosedMaterial) PickupClosedMaterial = UMaterialInstanceDynamic::Create(SpriteMaterial, this);
            if (!PickupOpenedMaterial) PickupOpenedMaterial = UMaterialInstanceDynamic::Create(SpriteMaterial, this);
            if (PickupClosedMaterial)
            {
                PickupClosedMaterial->SetVectorParameterValue(TEXT("PickupColor"), FLinearColor(1.0f, 0.72f, 0.08f, 1.0f));
                PickupClosedMaterial->SetScalarParameterValue(TEXT("SpriteOpacity"), 1.0f);
                DisplayMesh->SetMaterial(0, PickupClosedMaterial);
            }
            if (PickupOpenedMaterial)
            {
                PickupOpenedMaterial->SetVectorParameterValue(TEXT("PickupColor"), FLinearColor(0.18f, 0.94f, 0.82f, 1.0f));
                PickupOpenedMaterial->SetScalarParameterValue(TEXT("SpriteOpacity"), 1.0f);
                CollectedDisplayMesh->SetMaterial(0, PickupOpenedMaterial);
            }
        }
    }
    else
    {
        static UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (CubeMesh && DisplayMesh->GetStaticMesh() != CubeMesh) DisplayMesh->SetStaticMesh(CubeMesh);
        DisplayMesh->SetRelativeLocation(FVector::ZeroVector);
        DisplayMesh->SetRelativeScale3D(SafeExtent / 50.0f);
        SideSpikeLeft->SetVisibility(false);
        SideSpikeRight->SetVisibility(false);
    }
    if (TriggerKind == ENaiwaP1TriggerKind::Checkpoint)
    {
        FString Label = GetStableId().ToString();
        Label.ReplaceInline(TEXT("IntegratedCheckpoint"), TEXT("CP "));
        MarkerText->SetText(FText::FromString(Label));
        MarkerText->SetRelativeLocation(FVector(0.0f, FMath::Max(450.0f, SafeExtent.Y + 35.0f), SafeExtent.Z + 95.0f));
        MarkerText->SetVisibility(bShowCheckpointLabel);
    }
    else
    {
        MarkerText->SetVisibility(false);
    }
}

void ANaiwaP1Trigger::SetDisplayVisible(bool bVisible)
{
    if (DisplayMesh) DisplayMesh->SetVisibility(bVisible && !(TriggerKind == ENaiwaP1TriggerKind::Pickup && bCollected));
    if (CollectedDisplayMesh) CollectedDisplayMesh->SetVisibility(
        TriggerKind == ENaiwaP1TriggerKind::Pickup && bVisible && bPickupFading);
    if (SideSpikeLeft) SideSpikeLeft->SetVisibility(bVisible && TriggerKind == ENaiwaP1TriggerKind::Hazard && bUseSpikeCluster);
    if (SideSpikeRight) SideSpikeRight->SetVisibility(bVisible && TriggerKind == ENaiwaP1TriggerKind::Hazard && bUseSpikeCluster);
    if (MarkerText) MarkerText->SetVisibility(bVisible && TriggerKind == ENaiwaP1TriggerKind::Checkpoint
        && bShowCheckpointLabel);
}

void ANaiwaP1Trigger::ConfigureAsHazard(FName StableId, const FVector& InExtent, UMaterialInterface* HazardMaterial)
{
    TriggerKind = ENaiwaP1TriggerKind::Hazard;
    TriggerId = StableId;
    BoxExtent = InExtent.ComponentMax(FVector(1.0f));
    bRevealOnSpawn = true;
    bUseSpikeCluster = true;
    bCollected = false;
    ApplyDisplayGeometry();
    if (HazardMaterial)
    {
        DisplayMesh->SetMaterial(0, HazardMaterial);
        SideSpikeLeft->SetMaterial(0, HazardMaterial);
        SideSpikeRight->SetMaterial(0, HazardMaterial);
    }
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SetDisplayVisible(true);
    SetActorTickEnabled(true);
}

void ANaiwaP1Trigger::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (TriggerKind == ENaiwaP1TriggerKind::Pickup && bPickupFading)
    {
        PickupFadeElapsed += DeltaSeconds;
        const float Opacity = FMath::Clamp(1.0f - PickupFadeElapsed / 0.75f, 0.0f, 1.0f);
        if (PickupOpenedMaterial) PickupOpenedMaterial->SetScalarParameterValue(TEXT("SpriteOpacity"), Opacity);
        if (PickupFadeElapsed >= 0.75f)
        {
            bPickupFading = false;
            if (CollectedDisplayMesh) CollectedDisplayMesh->SetVisibility(false);
            SetActorTickEnabled(false);
        }
        return;
    }
    if (TriggerKind != ENaiwaP1TriggerKind::Hazard) return;
    ANaiwaCharacter* Character = Cast<ANaiwaCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!Character || !TriggerBox->IsOverlappingActor(Character)) return;
    // A tall capsule may brush a spike on an overhead cloud while the player's
    // feet are still below it. Only contact within the spike's height is lethal.
    const float FeetZ = Character->GetCapsuleBottom();
    const float Bottom = GetActorLocation().Z - BoxExtent.Z;
    const float Top = GetActorLocation().Z + BoxExtent.Z;
    if (FeetZ < Bottom - 4.0f || FeetZ > Top + 4.0f) return;
    // The capsule is wide for this character. A spike should meet the centre
    // of the landing feet, not kill when only the distant capsule edge grazes it.
    if (FMath::Abs(Character->GetActorLocation().X - GetActorLocation().X) > BoxExtent.X + 8.0f) return;
    if (ANaiwaP1GameMode* Mode = Cast<ANaiwaP1GameMode>(UGameplayStatics::GetGameMode(this)))
    {
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1 hazard_contact id=%s pawn_x=%.1f feet_z=%.1f layer=[%.1f,%.1f]"),
            *GetStableId().ToString(), Character->GetActorLocation().X, FeetZ, Bottom, Top);
        Mode->QueueTrigger(this, Character);
    }
}

void ANaiwaP1Trigger::NotifyActorBeginOverlap(AActor* OtherActor)
{
    Super::NotifyActorBeginOverlap(OtherActor);
    if (TriggerKind == ENaiwaP1TriggerKind::Hazard) return;
    if (bCollected || !Cast<ANaiwaCharacter>(OtherActor)) return;
    if (ANaiwaP1GameMode* Mode = Cast<ANaiwaP1GameMode>(UGameplayStatics::GetGameMode(this)))
    {
        Mode->QueueTrigger(this, CastChecked<ANaiwaCharacter>(OtherActor));
    }
}

void ANaiwaP1Trigger::ResetForAttempt()
{
    bCollected = false;
    bPickupFading = false;
    PickupFadeElapsed = 0.0f;
    if (PickupClosedMaterial) PickupClosedMaterial->SetScalarParameterValue(TEXT("SpriteOpacity"), 1.0f);
    if (PickupOpenedMaterial) PickupOpenedMaterial->SetScalarParameterValue(TEXT("SpriteOpacity"), 1.0f);
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SetDisplayVisible(ShouldDisplay());
    SetActorTickEnabled(TriggerKind == ENaiwaP1TriggerKind::Hazard);
}

void ANaiwaP1Trigger::SetCollected()
{
    bCollected = true;
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (TriggerKind == ENaiwaP1TriggerKind::Pickup)
    {
        bPickupFading = true;
        PickupFadeElapsed = 0.0f;
        if (PickupOpenedMaterial)
        {
            PickupOpenedMaterial->SetScalarParameterValue(TEXT("SpriteOpacity"), 1.0f);
        }
        if (DisplayMesh) DisplayMesh->SetVisibility(false);
        if (CollectedDisplayMesh) CollectedDisplayMesh->SetVisibility(true);
        SetActorTickEnabled(true);
    }
    else
    {
        SetDisplayVisible(false);
    }
}

void ANaiwaP1Trigger::SetHazardRevealed(bool bRevealed)
{
    if (TriggerKind == ENaiwaP1TriggerKind::Hazard) SetDisplayVisible(bRevealed);
}

bool ANaiwaP1Trigger::IsDisplayVisible() const
{
    return DisplayMesh && DisplayMesh->IsVisible();
}

void ANaiwaP1Trigger::SetCheckpointActive(bool bActive)
{
    // The HUD reports activation. The marker stays visible for route readability.
    (void)bActive;
    if (TriggerKind == ENaiwaP1TriggerKind::Checkpoint) SetDisplayVisible(true);
}

FName ANaiwaP1Trigger::GetStableId() const
{
    return TriggerId.IsNone() ? GetFName() : TriggerId;
}
