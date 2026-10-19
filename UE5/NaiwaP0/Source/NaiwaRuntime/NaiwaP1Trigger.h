#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NaiwaP1Trigger.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class ENaiwaP1TriggerKind : uint8
{
    Pickup,
    Hazard,
    Checkpoint,
    Goal,
    KillZ
};

/** One overlap actor for the small P1 gameplay set. All decisions belong to the P1 GameMode. */
UCLASS(Blueprintable)
class NAIWARUNTIME_API ANaiwaP1Trigger : public AActor
{
    GENERATED_BODY()

public:
    ANaiwaP1Trigger();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1")
    TObjectPtr<UBoxComponent> TriggerBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1")
    TObjectPtr<UStaticMeshComponent> DisplayMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1")
    TObjectPtr<UStaticMeshComponent> CollectedDisplayMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1")
    TObjectPtr<UStaticMeshComponent> SideSpikeLeft;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1")
    TObjectPtr<UStaticMeshComponent> SideSpikeRight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1")
    TObjectPtr<UTextRenderComponent> MarkerText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1")
    ENaiwaP1TriggerKind TriggerKind = ENaiwaP1TriggerKind::Pickup;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1")
    FName TriggerId = NAME_None;

    /** Stable pickup ordering selects the specific collection sound. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1", meta = (ClampMin = "0"))
    int32 PickupOrder = 0;

    /** Only pickups with this flag set are required to unlock the goal. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1")
    bool bRequiredForGoal = false;

    /** Half extents in centimetres. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1", meta = (ClampMin = "1.0"))
    FVector BoxExtent = FVector(45.0f, 90.0f, 45.0f);

    /** Checkpoint centre to the safe capsule centre. Keep this clear of platforms and hazards. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1")
    FVector RespawnOffset = FVector::ZeroVector;

    /**
     * Hazards are normally invisible until the first death teaches the player where
     * they are. Set this when the bar is meant to read as a visible threat from the
     * first frame instead, which is what the v6 scare spike needs. Visibility is
     * restored on every attempt reset, so the spike never flickers back to hidden.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1")
    bool bRevealOnSpawn = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1")
    bool bUseSpikeCluster = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1")
    bool bShowCheckpointLabel = false;

    UFUNCTION(BlueprintCallable, Category = "Naiwa|P1")
    void ResetForAttempt();

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1")
    bool IsDisplayVisible() const;

    void SetCollected();
    void SetHazardRevealed(bool bRevealed);
    void SetCheckpointActive(bool bActive);
    FName GetStableId() const;
    void ConfigureAsHazard(FName StableId, const FVector& InExtent, UMaterialInterface* HazardMaterial);

private:
    bool ShouldDisplay() const;
    void ApplyDisplayGeometry();
    void SetDisplayVisible(bool bVisible);
    bool bCollected = false;
    bool bPickupFading = false;
    float PickupFadeElapsed = 0.0f;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> PickupClosedMaterial;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> PickupOpenedMaterial;
};
