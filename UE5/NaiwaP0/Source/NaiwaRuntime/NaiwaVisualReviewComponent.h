#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NaiwaVisualReviewComponent.generated.h"

class ANaiwaCharacter;
class UAnimSequence;
class FJsonValue;
class FJsonObject;

/** Opt-in saved-asset phase review. Does not run in ordinary gameplay. */
UCLASS()
class NAIWARUNTIME_API UNaiwaVisualReviewComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UNaiwaVisualReviewComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function) override;
private:
    void OnScreenshot(int32 Width, int32 Height, const TArray<FColor>& Pixels);
    void Finish(bool bSuccess);
    void TickPlayback(double Now);
    void CompletePlaybackClip();
    TSharedPtr<FJsonObject> PlaybackPose(bool bMeasureSurface) const;
    TWeakObjectPtr<ANaiwaCharacter> Character;
    UPROPERTY() TArray<TObjectPtr<UAnimSequence>> Clips;
    TArray<TSharedPtr<FJsonValue>> Entries;
    TArray<TSharedPtr<FJsonValue>> PlaybackFrames;
    TArray<TSharedPtr<FJsonValue>> PlaybackClips;
    FDelegateHandle ScreenshotHandle;
    FString Directory;
    FString PendingPath;
    int32 ClipIndex = 0;
    int32 PhaseIndex = 0;
    int32 SettledTicks = 0;
    double NextWallTime = 0.0;
    double PendingWallTime = 0.0;
    float SampleTime = 0.0f;
    bool bEnabled = false;
    bool bPrepared = false;
    bool bPending = false;
    bool bQuit = false;
    bool bPlayback = false;
    bool bPlaybackClipClosing = false;
    bool bFinished = false;
    double PlaybackStartGameTime = 0.0;
    double PlaybackElapsed = 0.0;
    double NextPlaybackScreenshotTime = 0.0;
    double PreviousPlaybackGameTime = 0.0;
    float PreviousPlaybackPosition = 0.0f;
    int32 PlaybackWraps = 0;
    int32 PlaybackFrameCount = 0;
    int32 PlaybackClipShotCount = 0;
    double MaximumPlaybackTimeError = 0.0;
};
