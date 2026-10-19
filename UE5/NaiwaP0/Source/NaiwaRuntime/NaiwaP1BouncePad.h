#pragma once

#include "CoreMinimal.h"
#include "NaiwaP1Mechanism.h"
#include "NaiwaP1BouncePad.generated.h"

class ANaiwaCharacter;
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class USoundBase;

/** P1-only, horizontal solid pad. A new descending landing launches once. */
UCLASS(Blueprintable)
class NAIWARUNTIME_API ANaiwaP1BouncePad : public ANaiwaP1Mechanism
{
    GENERATED_BODY()

public:
    ANaiwaP1BouncePad();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Bounce")
    TObjectPtr<UBoxComponent> PadCollision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Bounce")
    TObjectPtr<UStaticMeshComponent> DisplayMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Bounce")
    TObjectPtr<UTextRenderComponent> StatusText;

    /** Half extents in centimetres. Keep the pad horizontal. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Bounce", meta = (ClampMin = "1.0", Units = "cm"))
    FVector BoxExtent = FVector(90.0f, 120.0f, 12.0f);

    /** Upward launch speed; existing horizontal movement is preserved. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Bounce", meta = (ClampMin = "1.0", Units = "cm/s"))
    float LaunchVelocityZ = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Bounce", meta = (ClampMin = "0.0", Units = "s"))
    float CooldownSeconds = 0.35f;

    /** Feet must clear this height, or leave the pad horizontally, to rearm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Bounce", meta = (ClampMin = "1.0", Units = "cm"))
    float RearmClearanceCm = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Bounce", meta = (ClampMin = "0.05", Units = "s"))
    float TriggerFeedbackSeconds = 0.25f;

    /** Clears cooldown/contact history. The lifetime activation counter is preserved. */
	virtual void ResetForAttempt() override;

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Bounce")
    bool IsReady() const;

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Bounce")
    int32 GetActivationCount() const { return ActivationCount; }

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Bounce")
    float GetCooldownRemaining() const { return CooldownRemaining; }

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Bounce")
    bool IsContactLatched() const { return bContactLatched; }

private:
    void ApplyGeometry();
    void UpdateStatusText(bool bGameplayAllowed);
    bool IsCharacterInContactRegion(const ANaiwaCharacter* Character, float FeetZ, float TopZ) const;

    UPROPERTY()
    TObjectPtr<USoundBase> BounceSound;

    TWeakObjectPtr<ANaiwaCharacter> PreviousCharacter;
    float PreviousFeetZ = 0.0f;
    float CooldownRemaining = 0.0f;
    float FeedbackRemaining = 0.0f;
    float DiagnosticLogRemaining = 0.0f;
    int32 ActivationCount = 0;
    bool bHasPreviousSample = false;
    bool bPreviouslyFalling = false;
    bool bContactLatched = false;
    bool bDescendingLandingArmed = false;
};
