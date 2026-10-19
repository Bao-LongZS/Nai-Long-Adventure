#pragma once

#include "CoreMinimal.h"
#include "NaiwaGameMode.h"
#include "NaiwaP1GameMode.generated.h"

class ANaiwaCharacter;
class ANaiwaP1Trigger;
class AStaticMeshActor;
class UNiagaraSystem;
class UAudioComponent;
class USoundBase;
class FJsonValue;

UENUM(BlueprintType)
enum class ENaiwaP1State : uint8
{
    Playing,
    Dying,
    Respawning,
    Won
};

struct FNaiwaP1PendingTrigger
{
    TWeakObjectPtr<ANaiwaP1Trigger> Trigger;
    uint64 Frame = 0;
};

enum class ENaiwaP1SmokeStep : uint8
{
    WaitReady,
    WaitLockedGoal,
    WaitFirstPickup,
    WaitCheckpoint,
    WaitDeath,
    WaitRespawn,
    WaitSecondPickup,
    WaitWon,
    WaitRestart
};

enum class ENaiwaP1RouteStep : uint8
{
    ToCheckpoint,
    WaitRespawn,
    ReturnToPickup,
    ToGoal
};

enum class ENaiwaP1HazardAuditStep : uint8
{
    WaitReady,
    WaitDeath,
    WaitRespawn
};

enum class ENaiwaP1MatrixStep : uint8
{
    WaitReady,
    WaitPriorityDeath,
    WaitPriorityRespawn,
    WaitKillZDeath,
    WaitPausedDying,
    WaitKillZRespawn,
    WaitRestartDying,
    WaitDyingReset,
    WaitPausedSetup,
    WaitPausedReset,
    WaitPickupGoalSameFrame,
    WaitWon,
    WaitWonLock,
    WaitWonReset
};

enum class ENaiwaP1BounceProbeStep : uint8
{
    WaitReady,
    ToFirstBounce,
    WaitFirstLanding,
    ReturnForSecondBounce,
    WaitRReset,
    ToThirdBounce,
    RunToHazard,
    WaitDeathReset
};

enum class ENaiwaP1SwitchProbeStep : uint8
{
    WaitReady,
    ApproachBoard,
    WaitDepartureCue,
    RideRight,
    VerifyDockRight,
    RideLeft,
    WaitDockLeftAtRightSwitch,
    RetouchRightSwitch,
    WaitSecondRightTravel,
    WaitPausedTravel,
    WaitSecondRightDock,
    WaitDeath,
    WaitDeathReset,
    WaitRightTravelForR,
    WaitRReset
};

enum class ENaiwaP1SwitchFailProbeStep : uint8
{
    WaitReady,
    BoardFirst,
    RideFirst,
    ToRightShore,
    WaitDockLeft,
    FallFromRightEdge,
    WaitDying,
    WaitRespawn,
    BoardRetry,
    RideRetry,
    ToRightShoreAfterRetry
};

enum class ENaiwaP1TimedProbeStep : uint8
{
    WaitReady,
    FirstCrossing,
    WaitRReset,
    ApproachForExpiry,
    ObserveCountdown,
    WaitPausedCountdown,
    WaitDisable,
    WaitFall,
    HoldCapsuleInFloor,
    WaitRecovery,
    RetryCrossing,
    WaitDeath,
    WaitDeathReset
};

enum class ENaiwaP1TimedFailProbeStep : uint8
{
    WaitReady,
    ApproachCentre,
    WaitExpiry,
    WaitDying,
    WaitRespawn,
    RetryCrossing
};

enum class ENaiwaP1IntegratedProbeStep : uint8
{
    WaitReady,
    ToHazard,
    ToLeftSwitch,
    BoardShuttle,
    RideShuttle,
    ToRightBank,
    ToBounce,
    ToLanding,
    ToCheckpoint,
    SettleCheckpoint,
    ToFallEdge,
    WaitDeath,
    WaitRespawn,
    SettleRespawn
};

enum class ENaiwaP1IntegratedFullProbeStep : uint8
{
    WaitReady,
    HazardTeaching,
    SwitchTeachingTouch,
    SwitchTeachingBoard,
    SwitchTeachingRide,
    SwitchTeachingExit,
    BounceTeaching,
    LandingTeaching,
    CheckpointTeaching,
    TimedTeaching,
    SwitchCombinationTouch,
    SwitchCombinationBoard,
    SwitchCombinationRide,
    SwitchCombinationExit,
    HazardCombination,
    CheckpointCombination,
    BounceCombination,
    TimedCombinationLanding,
    TimedCombinationCross,
    CheckpointFinal,
    PickupRequired,
    Goal,
    WonSettle
};

enum class ENaiwaP1V7MixedStep : uint8
{
    WaitReady,
    TeachingHazard,
    TeachingSwitchTouch,
    TeachingSwitchBoard,
    TeachingSwitchRide,
    TeachingSwitchExit,
    TeachingBounce,
    TeachingBounceLanding,
    TeachingCheckpoint,
    TeachingTimedCross,
    SwitchA_Touch,
    SwitchA_Board,
    SwitchA_Ride,
    SwitchA_Exit,
    Checkpoint03,
    GapsA,
    Checkpoint04,
    SwitchB_Touch,
    SwitchB_Board,
    SwitchB_Ride,
    BounceB,
    BounceBLanding,
    Checkpoint05,
    GapsB,
    Checkpoint06,
    SwitchC_Touch,
    SwitchC_Board,
    SwitchC_Ride,
    TimedC_Jump,
    TimedC_Cross,
    Checkpoint07,
    GapsC,
    FinalBank,
    FinalBounce,
    FinalBounceLanding,
    FinalTimedCross,
    FinalExit,
    SecondBounce,
    SecondBounceLanding,
    SecondTimedCross,
    SecondExit,
    Pickup,
    Goal,
    WonSettle
};

/** P1-only rule owner; P0 maps retain ANaiwaGameMode and their original controls. */
UCLASS(Blueprintable)
class NAIWARUNTIME_API ANaiwaP1GameMode : public ANaiwaGameMode
{
    GENERATED_BODY()

public:
    static constexpr int32 DeathLaughFrameCount = 96;
    static constexpr float DeathLaughAnimationLoopSeconds = 4.0f;

    ANaiwaP1GameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Naiwa|P1", meta = (ClampMin = "1"))
    int32 RequiredPickupCount = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Naiwa|P1", meta = (ClampMin = "0.1"))
    float DeathDurationSeconds = 1.2f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Naiwa|P1")
    TObjectPtr<USoundBase> DeathSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Naiwa|P1")
    TObjectPtr<USoundBase> CollectionSound;

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1")
    ENaiwaP1State GetP1State() const { return State; }

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1")
    int32 GetCollectedPickupCount() const { return CollectedIds.Num(); }

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1")
    int32 GetTotalPickupCount() const { return TotalPickupCount; }

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1")
    int32 GetCollectedRequiredPickupCount() const;

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1")
    bool IsGoalUnlocked() const;

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1")
    bool HasCheckpoint() const { return ActiveCheckpoint.IsValid(); }

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1")
    bool IsExitLockedHintVisible() const { return bExitLockedHint; }

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1")
    int32 GetDeathEventCount() const { return DeathEventCount; }

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1|Feedback")
    bool IsDeathLaughSpriteActive() const;

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1|Feedback")
    int32 GetDeathLaughSpriteFrame() const;

    UFUNCTION(BlueprintPure, Category = "Naiwa|P1")
    bool IsGameplayInputAllowed() const;

    void RegisterCharacter(ANaiwaCharacter* Character);
    void QueueTrigger(ANaiwaP1Trigger* Trigger, ANaiwaCharacter* Character);
    void QueueFallDeath();
    void ToggleP1Pause();
    void RestartLevel();
    void PlayJumpFeedback();

private:
    void ProcessPendingTriggers();
    void EnterDying(FName Cause);
    void LoadFeedbackAssets();
    void StartBackgroundMusic();
    void PauseBackgroundMusicForEgg();
    void ResumeBackgroundMusicAfterEgg();
    void PlayDeathFollowup();
    void PlayPickupFeedback(const ANaiwaP1Trigger* Trigger);
    void FadePickupFeedback();
    void SetPickupMusicDucked(bool bDucked);
    void RestorePickupMusicAfterFade();
    void PlayCheckpointFeedback(const ANaiwaP1Trigger* Trigger);
    void SpawnCheckpointBurst(const ANaiwaP1Trigger* Trigger);
    void SpawnGoalBurst(const ANaiwaP1Trigger* Trigger);
    void StopDeathAudio();
    void BeginRespawn();
    void FinishRespawnIfReady();
    bool PlaceCharacterAt(const FVector& PreferredLocation);
    void ResetAttemptActors();
    void RevealAllHazards();
    void SetState(ENaiwaP1State NewState);
    void TickSmoke();
    bool SmokeTeleportTo(ANaiwaP1Trigger* Trigger);
    void SmokeAdvance(ENaiwaP1SmokeStep NextStep, const FString& Evidence);
    void FinishSmoke(const TCHAR* Result, const FString& Detail);
    void TickRoute();
    void TickFullRoute();
    void RouteJumpAt(float ThresholdX, int32 JumpIndex, float HoldSeconds = 0.18f);
    void CaptureRouteFrame(const TCHAR* Label);
    void FinishRoute(const TCHAR* Result, const FString& Detail);
    void TickHazardAudit();
    void FinishHazardAudit(const TCHAR* Result, const FString& Detail);
    void TickMatrix();
    void MatrixAdvance(ENaiwaP1MatrixStep NextStep, const FString& Evidence);
    void FinishMatrix(const TCHAR* Result, const FString& Detail);
    void TickBounceProbe();
    void FinishBounceProbe(const TCHAR* Result, const FString& Detail);
    void TickSwitchProbe();
    void SwitchProbeAdvance(ENaiwaP1SwitchProbeStep NextStep, const FString& Evidence);
    bool SwitchProbeTeleportToSwitch(bool bLeft);
    void CaptureSwitchProbeFrame(const TCHAR* Label);
    void FinishSwitchProbe(const TCHAR* Result, const FString& Detail);
    void TickSwitchFailProbe();
    void SwitchFailProbeAdvance(ENaiwaP1SwitchFailProbeStep NextStep, const FString& Evidence);
    void FinishSwitchFailProbe(const TCHAR* Result, const FString& Detail);
    void TickTimedProbe();
    void TimedProbeAdvance(ENaiwaP1TimedProbeStep NextStep, const FString& Evidence);
    void CaptureTimedProbeFrame(const TCHAR* Label);
    void FinishTimedProbe(const TCHAR* Result, const FString& Detail);
    void TickTimedFailProbe();
    void TimedFailProbeAdvance(ENaiwaP1TimedFailProbeStep NextStep, const FString& Evidence);
    void FinishTimedFailProbe(const TCHAR* Result, const FString& Detail);
    bool InitializeIntegratedProbe();
    void TickIntegratedProbe();
    void IntegratedProbeRecord(const TCHAR* Station);
    void IntegratedProbeAdvance(ENaiwaP1IntegratedProbeStep NextStep, const TCHAR* Station);
    void CaptureIntegratedVisualFrame(const TCHAR* Label);
    void FinishIntegratedProbe(const TCHAR* Result, const FString& Detail);

    bool InitializeIntegratedFullProbe();
    void TickIntegratedFullProbe();
    void IntegratedFullProbeRecord(const TCHAR* Station);
    void IntegratedFullProbeAdvance(ENaiwaP1IntegratedFullProbeStep NextStep, const TCHAR* Station);
    bool ObserveIntegratedFullTimed(int32 Index);
    bool CompleteIntegratedFullTimed(int32 Index);
    void FinishIntegratedFullProbe(const TCHAR* Result, const FString& Detail);

    // Gap-width probe: measures where a 160 cm block run stops carrying the capsule.
    // v5 claims a 65 cm gap drops Run300; the capsule diameter is 84 cm, so the claim
    // needs a physical reading rather than a design assumption.
    void TickGapProbe();
    void FinishGapProbe();
    void GapProbeEnterSegment(int32 Index);

    // Clearance probe: the real jump window over a 220 cm gap and a suspended spike
    // bar. v5 derives both on paper; this reads them from the character controller.
    void TickClearanceProbe();
    void FinishClearanceProbe();
    void ClearanceProbeEnterSegment(int32 Index);

    // Timed-window probe: measures (stable landing + active) end to end on a
    // dedicated map instead of trusting the state-machine reading.
    void TickTimedWindowProbe();
    void FinishTimedWindowProbe();

    // Chain-gap probe: v6 replaces the old 19-block narrow run with 11 gaps of
    // 240 cm separated by 400 cm islands. Whether the capsule can re-jump straight
    // off a 400 cm island is a controller question, not a geometry one.
    void TickChainProbe();
    void FinishChainProbe();
    void ChainProbeEnterStart();

    // Revealed-hazard probe: bRevealOnSpawn has to make the scare spike visible on
    // the first frame and survive a respawn without losing its kill volume.
    void TickHazardRevealProbe();
    void FinishHazardRevealProbe();

    // v7 isolated gate geometry. This measures one complete shuttle-to-target
    // transition or a separate natural-fall/checkpoint retry. Neither is
    // evidence for the full integrated route.
    void TickV7GateProbe();
    void TickV7GateRecovery();
    void FinishV7GateProbe(const TCHAR* Outcome);
    void RecordV7GateProbeEvent(const TCHAR* Event);

    bool InitializeV7MixedCoreProbe();
    void TickV7MixedCoreProbe();
    bool TickV7MixedCoreCheckpointRecovery();
    void TickV7HiddenFinalPitRecoveryProbe();
    void FinishV7HiddenFinalPitRecoveryProbe(const TCHAR* Result, const FString& Detail);
    void AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep NextStep, const TCHAR* Station);
    void RecordV7MixedCoreProbe(const TCHAR* Station);
    void FinishV7MixedCoreProbe(const TCHAR* Result, const FString& Detail);

    ENaiwaP1State State = ENaiwaP1State::Playing;
    TWeakObjectPtr<ANaiwaCharacter> PlayerCharacter;
    TWeakObjectPtr<ANaiwaP1Trigger> ActiveCheckpoint;
    FVector InitialSpawn = FVector::ZeroVector;
    bool bHasInitialSpawn = false;
    bool bExitLockedHint = false;
    bool bPendingFallDeath = false;
    bool bRespawnTriedInitial = false;
    uint64 PendingFallFrame = 0;
    float RespawnStartedAt = 0.0f;
    TArray<FNaiwaP1PendingTrigger> PendingTriggers;
    TSet<FName> CollectedIds;
    TSet<FName> RequiredPickupIds;
    TSet<FName> CollectedRequiredPickupIds;
    int32 TotalPickupCount = 0;
    FTimerHandle DeathTimer;
    FTimerHandle DeathFollowupTimer;
    FTimerHandle BackgroundMusicTimer;
    FTimerHandle EasterEggMusicResumeTimer;
    FTimerHandle PickupFadeTimer;
    FTimerHandle PickupMusicDuckRestoreTimer;
    int32 DeathEventCount = 0;
    int32 DeathsThisRun = 0;
    FName LastDeathCause = NAME_None;
    double DeathLaughStartedAt = -1.0;
    double BackgroundMusicScheduledEndAt = 0.0;
    float DeathLaughDuration = 0.0f;
    float BackgroundMusicRemainingSeconds = 0.0f;
    float BackgroundMusicBaseVolumeMultiplier = 0.32f;
    bool bDeathLaughPlaying = false;
    bool bBackgroundMusic1Next = true;
    bool bPickupMusicDucked = false;
    bool bBackgroundMusicPausedForDeathEgg = false;
    bool bBackgroundMusicComponentPaused = false;
    bool bUseDeathEggThisAttempt = false;
    UPROPERTY(Transient)
    TObjectPtr<USoundBase> FeedbackDeathLead;
    UPROPERTY(Transient)
    TObjectPtr<USoundBase> FeedbackDeathFollowup;
    UPROPERTY(Transient)
    TObjectPtr<USoundBase> FeedbackDeathEgg;
    UPROPERTY(Transient)
    TObjectPtr<USoundBase> FeedbackCollection;
    UPROPERTY(Transient)
    TObjectPtr<USoundBase> FeedbackCollection1;
    UPROPERTY(Transient)
    TObjectPtr<USoundBase> FeedbackCollection2;
    UPROPERTY(Transient)
    TObjectPtr<USoundBase> FeedbackCheckpointEarly;
    UPROPERTY(Transient)
    TObjectPtr<USoundBase> FeedbackCheckpointLate;
    UPROPERTY(Transient)
    TObjectPtr<USoundBase> FeedbackBackgroundMusic1;
    UPROPERTY(Transient)
    TObjectPtr<USoundBase> FeedbackBackgroundMusic2;
    UPROPERTY(Transient)
    TObjectPtr<USoundBase> FeedbackJump;
    UPROPERTY(Transient)
    TObjectPtr<UNiagaraSystem> CheckpointBurstSystem;
    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> DeathAudioComponent;
    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> BackgroundMusicComponent;
    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> PickupAudioComponent;
    bool bSmokeEnabled = false;
    ENaiwaP1SmokeStep SmokeStep = ENaiwaP1SmokeStep::WaitReady;
    double SmokeStartedWall = 0.0;
    double SmokeStepStartedWall = 0.0;
    TWeakObjectPtr<ANaiwaP1Trigger> SmokePickup;
    TWeakObjectPtr<ANaiwaP1Trigger> SmokeCheckpoint;
    TWeakObjectPtr<ANaiwaP1Trigger> SmokeHazard;
    TWeakObjectPtr<ANaiwaP1Trigger> SmokeGoal;
    TArray<FString> SmokeEvents;
    bool bRouteEnabled = false;
    bool bRouteWalk = false;
    bool bRouteFull = false;
    bool bRouteCapture = false;
    ENaiwaP1RouteStep RouteStep = ENaiwaP1RouteStep::ToCheckpoint;
    int32 RouteJumpIndex = 0;
    double RouteStartedWall = 0.0;
    float RouteJumpReleaseTime = 0.0f;
    TArray<FString> RouteEvents;
    TArray<FString> RouteCapturePaths;
    bool bHazardAuditEnabled = false;
    ENaiwaP1HazardAuditStep HazardAuditStep = ENaiwaP1HazardAuditStep::WaitReady;
    int32 HazardAuditIndex = 0;
    double HazardAuditStartedWall = 0.0;
    double HazardAuditStepWall = 0.0;
    TArray<TWeakObjectPtr<ANaiwaP1Trigger>> HazardAuditActors;
    TArray<FString> HazardAuditEvents;
    bool bMatrixEnabled = false;
    ENaiwaP1MatrixStep MatrixStep = ENaiwaP1MatrixStep::WaitReady;
    double MatrixStartedWall = 0.0;
    double MatrixStepWall = 0.0;
    float MatrixPausedTimerRemaining = 0.0f;
    FVector MatrixWonPosition = FVector::ZeroVector;
    bool bMatrixWonLockArmed = false;
    TWeakObjectPtr<ANaiwaP1Trigger> MatrixPickup;
    TWeakObjectPtr<ANaiwaP1Trigger> MatrixCheckpoint;
    TWeakObjectPtr<ANaiwaP1Trigger> MatrixHazard;
    TWeakObjectPtr<ANaiwaP1Trigger> MatrixGoal;
    TArray<FString> MatrixEvents;
    bool bBounceProbeEnabled = false;
    bool bBounceProbeWalk = false;
    bool bBounceProbeInitialized = false;
    ENaiwaP1BounceProbeStep BounceProbeStep = ENaiwaP1BounceProbeStep::WaitReady;
    double BounceProbeStartedWall = 0.0;
    double BounceProbeStepStartedWall = 0.0;
    int32 BounceProbeDeathCountBefore = 0;
    float BounceProbeMaxZ = 0.0f;
    FVector BounceProbeFirstLanding = FVector::ZeroVector;
    FVector BounceProbeFirstLaunch = FVector::ZeroVector;
    TWeakObjectPtr<class ANaiwaP1BouncePad> BounceProbePad;
    TWeakObjectPtr<ANaiwaP1Trigger> BounceProbeHazard;
    FVector BounceProbeLandingOrigin = FVector::ZeroVector;
    FVector BounceProbeLandingExtent = FVector::ZeroVector;
    TArray<FString> BounceProbeEvents;
    bool bSwitchProbeEnabled = false;
    bool bSwitchProbeWalk = false;
    bool bSwitchProbeCapture = false;
    bool bSwitchProbeInitialized = false;
    bool bSwitchProbeRideSupportedMidway = false;
    bool bSwitchProbeDeathResetPassed = false;
    bool bSwitchProbeRResetPassed = false;
    ENaiwaP1SwitchProbeStep SwitchProbeStep = ENaiwaP1SwitchProbeStep::WaitReady;
    double SwitchProbeStartedWall = 0.0;
    double SwitchProbeStepStartedWall = 0.0;
    float SwitchProbeBoardOffsetX = 0.0f;
    float SwitchProbePausedX = 0.0f;
    float SwitchProbePauseDeltaX = 0.0f;
    int32 SwitchProbeDeathCountBefore = 0;
    int32 SwitchProbeRightTouchAtLatch = 0;
    TWeakObjectPtr<class ANaiwaP1SwitchPlatform> SwitchProbePlatform;
    TArray<FString> SwitchProbeEvents;
    TArray<FString> SwitchProbeCapturePaths;
    bool bSwitchFailProbeEnabled = false;
    bool bSwitchFailProbeWalk = false;
    bool bSwitchFailProbeInitialized = false;
    bool bSwitchFailProbeFirstRideMidway = false;
    bool bSwitchFailProbeRetryRideMidway = false;
    bool bSwitchFailProbeFirstShoreReached = false;
    bool bSwitchFailProbeNaturalFallSeen = false;
    bool bSwitchFailProbeDyingSeen = false;
    bool bSwitchFailProbeRespawnReady = false;
    bool bSwitchFailProbeRetryShoreReached = false;
    ENaiwaP1SwitchFailProbeStep SwitchFailProbeStep = ENaiwaP1SwitchFailProbeStep::WaitReady;
    double SwitchFailProbeStartedWall = 0.0;
    double SwitchFailProbeStepStartedWall = 0.0;
    int32 SwitchFailProbeDeathCountBefore = 0;
    int32 SwitchFailProbeControlInjections = 0;
    float SwitchFailProbeFirstBoardOffsetX = 0.0f;
    float SwitchFailProbeRetryBoardOffsetX = 0.0f;
    FVector SwitchFailProbeFirstBoardLocation = FVector::ZeroVector;
    FVector SwitchFailProbeFirstShoreLocation = FVector::ZeroVector;
    FVector SwitchFailProbeFallLocation = FVector::ZeroVector;
    FVector SwitchFailProbeDeathLocation = FVector::ZeroVector;
    FVector SwitchFailProbeRespawnLocation = FVector::ZeroVector;
    FVector SwitchFailProbeRetryBoardLocation = FVector::ZeroVector;
    TWeakObjectPtr<class ANaiwaP1SwitchPlatform> SwitchFailProbePlatform;
    TArray<FString> SwitchFailProbeEvents;
    bool bTimedProbeEnabled = false;
    bool bTimedProbeWalk = false;
    bool bTimedProbeCapture = false;
    bool bTimedProbeInitialized = false;
    bool bTimedProbeFirstCrossed = false;
    bool bTimedProbeRetryCrossed = false;
    bool bTimedProbeCountdownDeclined = false;
    bool bTimedProbePausePassed = false;
    bool bTimedProbeFallPassed = false;
    bool bTimedProbeOccupancyGuardPassed = false;
    bool bTimedProbeRResetPassed = false;
    bool bTimedProbeDeathResetPassed = false;
    ENaiwaP1TimedProbeStep TimedProbeStep = ENaiwaP1TimedProbeStep::WaitReady;
    double TimedProbeStartedWall = 0.0;
    double TimedProbeStepStartedWall = 0.0;
    float TimedProbeFirstTriggerGameTime = -1.0f;
    float TimedProbeFirstCrossingSeconds = 0.0f;
    float TimedProbeRetryTriggerGameTime = -1.0f;
    float TimedProbeRetryCrossingSeconds = 0.0f;
    float TimedProbeObservedRemaining = 0.0f;
    float TimedProbeObserveStartedGameTime = 0.0f;
    float TimedProbePausedRemaining = 0.0f;
    float TimedProbePauseDeltaSeconds = 0.0f;
    float TimedProbeBottomAtDisable = 0.0f;
    double TimedProbeRecoveringStartedWall = 0.0;
    int32 TimedProbeDeathCountBefore = 0;
    int32 TimedProbeRecoveryCountBefore = 0;
    TWeakObjectPtr<class ANaiwaP1TimedPlatform> TimedProbePlatform;
    TArray<FString> TimedProbeEvents;
    TArray<FString> TimedProbeCapturePaths;
    bool bTimedFailProbeEnabled = false;
    bool bTimedFailProbeWalk = false;
    bool bTimedFailProbeInitialized = false;
    bool bTimedFailProbeSawFall = false;
    bool bTimedFailProbeSawDying = false;
    bool bTimedFailProbeRespawnReady = false;
    bool bTimedFailProbeRetryCrossed = false;
    ENaiwaP1TimedFailProbeStep TimedFailProbeStep = ENaiwaP1TimedFailProbeStep::WaitReady;
    double TimedFailProbeStartedWall = 0.0;
    double TimedFailProbeStepStartedWall = 0.0;
    int32 TimedFailProbeDeathCountBefore = 0;
    int32 TimedFailProbeTriggerCountBefore = 0;
    int32 TimedFailProbeDisableCountBefore = 0;
    FVector TimedFailProbeBoardLocation = FVector::ZeroVector;
    FVector TimedFailProbeFallLocation = FVector::ZeroVector;
    FVector TimedFailProbeRespawnLocation = FVector::ZeroVector;
    TWeakObjectPtr<class ANaiwaP1TimedPlatform> TimedFailProbePlatform;
    TArray<FString> TimedFailProbeEvents;

    // Partial integrated teaching route only. No character teleport, platform
    // command injection, screenshot, or direct trigger injection is used.
    bool bIntegratedProbeEnabled = false;
    bool bIntegratedProbeWalk = false;
    bool bIntegratedProbeCheckCheckpointFail = false;
    bool bIntegratedVisualCapture = false;
    bool bIntegratedVisualApexCaptured = false;
    bool bIntegratedVisualFinishing = false;
    bool bIntegratedProbeInitialized = false;
    bool bIntegratedProbeJumpIssued = false;
    bool bIntegratedProbeJumpHeld = false;
    bool bIntegratedProbeHazardCleared = false;
    bool bIntegratedProbeLeftSwitchTouched = false;
    bool bIntegratedProbeRightSwitchTouched = false;
    bool bIntegratedProbeBoarded = false;
    bool bIntegratedProbeDepartureCueSeen = false;
    bool bIntegratedProbeSupportedMidway = false;
    bool bIntegratedProbeDockRightReached = false;
    bool bIntegratedProbeLaunchedUpward = false;
    bool bIntegratedProbeLandedOnHighPlatform = false;
    bool bIntegratedProbeCheckpointReached = false;
    bool bIntegratedProbeNaturalFallSeen = false;
    bool bIntegratedProbeDyingSeen = false;
    bool bIntegratedProbeCheckpointRespawnSeen = false;
    ENaiwaP1IntegratedProbeStep IntegratedProbeStep = ENaiwaP1IntegratedProbeStep::WaitReady;
    double IntegratedProbeStartedWall = 0.0;
    double IntegratedProbeStepStartedWall = 0.0;
    double IntegratedVisualStartedWall = 0.0;
    double IntegratedVisualFinishStartedGame = 0.0;
    FString IntegratedVisualPendingResult;
    FString IntegratedVisualPendingDetail;
    float IntegratedProbeJumpReleaseTime = 0.0f;
    float IntegratedProbeMaxCharacterZ = 0.0f;
    float IntegratedProbeBoardOffsetX = 0.0f;
    int32 IntegratedProbeDeathCountBefore = 0;
    int32 IntegratedProbeActivationCountBefore = 0;
    int32 IntegratedProbeLeftTouchCountBefore = 0;
    int32 IntegratedProbeRightTouchCountBefore = 0;
    int32 IntegratedProbeProductionRespawnPlacements = 0;
    TWeakObjectPtr<class ANaiwaP1SwitchPlatform> IntegratedProbeSwitch;
    TWeakObjectPtr<class ANaiwaP1BouncePad> IntegratedProbePad;
    TWeakObjectPtr<ANaiwaP1Trigger> IntegratedProbeHazard;
    TWeakObjectPtr<ANaiwaP1Trigger> IntegratedProbeCheckpoint;
    TWeakObjectPtr<class AStaticMeshActor> IntegratedProbeLeftBank;
    TWeakObjectPtr<class AStaticMeshActor> IntegratedProbeRightBank;
    TWeakObjectPtr<class AStaticMeshActor> IntegratedProbeLanding;
    TWeakObjectPtr<class AStaticMeshActor> IntegratedProbeCue;
    TArray<TSharedPtr<FJsonValue>> IntegratedProbeStations;
    TArray<TSharedPtr<FJsonValue>> IntegratedVisualFrames;
    // Full candidate core route only: no failure recovery, screenshot or world mutation.
    bool bIntegratedFullProbeEnabled = false;
    bool bIntegratedFullProbeWalk = false;
    bool bIntegratedFullProbeInitialized = false;
    bool bIntegratedFullProbeJumpHeld = false;
    bool IntegratedFullProbeJumps[2] = {};
    bool IntegratedFullProbeHazardsCleared[2] = {};
    bool IntegratedFullProbeBoarded[2] = {};
    bool IntegratedFullProbeCuesSeen[2] = {};
    bool IntegratedFullProbeMidwaySupported[2] = {};
    bool IntegratedFullProbeDockedWithRider[2] = {};
    bool IntegratedFullProbeLaunched[2] = {};
    bool IntegratedFullProbeLanded[2] = {};
    bool IntegratedFullProbeTimedSupported[2] = {};
    bool IntegratedFullProbeTimedDeclined[2] = {};
    bool IntegratedFullProbeTimedCompleted[2] = {};
    bool IntegratedFullProbeCheckpoints[3] = {};
    bool bIntegratedFullProbePickupSeen = false;
    ENaiwaP1IntegratedFullProbeStep IntegratedFullProbeStep = ENaiwaP1IntegratedFullProbeStep::WaitReady;
    double IntegratedFullProbeStartedWall = 0.0;
    double IntegratedFullProbeStepStartedWall = 0.0;
    float IntegratedFullProbeJumpReleaseTime = 0.0f;
    float IntegratedFullProbeMaxZ = 0.0f;
    float IntegratedFullProbeBoardOffsets[2] = {};
    float IntegratedFullProbeTimedTriggerTimes[2] = { -1.0f, -1.0f };
    float IntegratedFullProbeTimedPreviousRemaining[2] = {};
    float IntegratedFullProbeTimedCrossingSeconds[2] = {};
    float IntegratedFullProbeTimedExitMargins[2] = {};
    TWeakObjectPtr<class ANaiwaP1SwitchPlatform> IntegratedFullProbeSwitches[2];
    TWeakObjectPtr<class ANaiwaP1BouncePad> IntegratedFullProbePads[2];
    TWeakObjectPtr<class ANaiwaP1TimedPlatform> IntegratedFullProbeTimed[2];
    TWeakObjectPtr<ANaiwaP1Trigger> IntegratedFullProbeHazards[2];
    TWeakObjectPtr<ANaiwaP1Trigger> IntegratedFullProbeCheckpointsActors[3];
    TWeakObjectPtr<ANaiwaP1Trigger> IntegratedFullProbePickup;
    TWeakObjectPtr<ANaiwaP1Trigger> IntegratedFullProbeGoal;
    // Left bank, right bank, bounce landing, timed exit, combination bank, final bank.
    TWeakObjectPtr<class AStaticMeshActor> IntegratedFullProbeGrounds[6];
    TArray<TSharedPtr<FJsonValue>> IntegratedFullProbeStations;
    // Gap probe: ladder run state. Settling teleports and waits, then runs with real input.
    bool bGapProbeEnabled = false;
    bool bGapProbeWalk = false;
    bool bGapProbeSettling = true;
    bool bGapProbeFinished = false;
    int32 GapProbeIndex = 0;
    double GapProbePhaseStartedWall = 0.0;
    float GapProbeMinZ = 0.0f;
    float GapProbeMaxX = 0.0f;
    double GapProbeNextSampleWall = 0.0;
    TArray<TSharedPtr<FJsonValue>> GapProbeTrace;
    TArray<TSharedPtr<FJsonValue>> GapProbeResults;

    bool bClearanceProbeEnabled = false;
    bool bClearanceProbeWalk = false;
    bool bClearanceProbeSettling = true;
    bool bClearanceProbeFinished = false;
    bool bClearanceProbeJumped = false;
    bool bClearanceProbeHit = false;
    int32 ClearanceProbeIndex = 0;
    double ClearanceProbePhaseStartedWall = 0.0;
    double ClearanceProbeNextSampleWall = 0.0;
    float ClearanceProbeJumpX = 0.0f;
    float ClearanceProbePeakZ = 0.0f;
    float ClearanceProbeMinClearance = 0.0f;
    float ClearanceProbeMinZ = 0.0f;
    float ClearanceProbeCapsuleRadius = 0.0f;
    float ClearanceProbeCapsuleHalfHeight = 0.0f;
    TArray<TSharedPtr<FJsonValue>> ClearanceProbeTrace;
    TArray<TSharedPtr<FJsonValue>> ClearanceProbeResults;

    bool bTimedWindowProbeEnabled = false;
    bool bTimedWindowProbeFinished = false;
    bool bTimedWindowProbeSettling = true;
    bool bTimedWindowProbeStopped = false;
    int32 bTimedWindowProbeStage = 0;
    double TimedWindowProbePhaseWall = 0.0;
    double TimedWindowProbeEnterWall = 0.0;
    double TimedWindowProbeCountdownWall = 0.0;
    double TimedWindowProbeDisabledWall = 0.0;
    double TimedWindowProbeFallWall = 0.0;
    int32 TimedWindowProbeLastState = 0;
    TWeakObjectPtr<class ANaiwaP1TimedPlatform> TimedWindowProbeTarget;

    bool bV7GateProbeEnabled = false;
    bool bV7GateProbeRun = false;
    bool bV7GateProbeRecovery = false;
    bool bV7GateProbeFinished = false;
    bool bV7GateProbeJumpIssued = false;
    bool bV7GateProbeJumpLaunched = false;
    bool bV7GateProbeBoarded = false;
    bool bV7GateProbeDockedRight = false;
    bool bV7GateProbePadSupported = false;
    bool bV7GateProbeTimedSupported = false;
    bool bV7GateProbeTimedTriggered = false;
    bool bV7GateProbeTimedDisabled = false;
    bool bV7GateProbeCheckpointActivated = false;
    bool bV7GateProbeNaturallyLeftShuttle = false;
    bool bV7GateProbeFellBelowKillZ = false;
    bool bV7GateProbeSawDying = false;
    bool bV7GateProbeRespawnedAtCheckpoint = false;
    bool bV7GateProbeMechanismReset = false;
    bool bV7GateProbePadResetReady = false;
    bool bV7GateProbeTimedResetReady = false;
    bool bV7GateProbeRetryBoarded = false;
    bool bV7GateProbeRetryDockedRight = false;
    TCHAR V7GateProbeGate = TEXT('A');
    // 0 fixture, 1 settle/activate CP, 2 board, 3 ride, 4 cross or fail;
    // recovery only: 5 natural death, 6 checkpoint respawn, 7 retry board, 8 retry ride.
    int32 V7GateProbeStage = 0;
    int32 V7GateProbeDeathBefore = 0;
    int32 V7GateProbeFixtureTeleportCount = 0;
    float V7GateProbeOffsetCm = 0.0f;
    float V7GateProbeJumpX = 0.0f;
    float V7GateProbePadSupportX = 0.0f;
    float V7GateProbePadActivationX = 0.0f;
    float V7GateProbeTimedSupportX = 0.0f;
    float V7GateProbeTimedTriggerX = 0.0f;
    float V7GateProbeTimedExitX = 0.0f;
    float V7GateProbeTimedRemainingAtExit = 0.0f;
    float V7GateProbeRespawnX = 0.0f;
    float V7GateProbeRespawnFeetZ = 0.0f;
    float V7GateProbePeakFeetZ = 0.0f;
    float V7GateProbeMinFeetZ = 100000.0f;
    double V7GateProbeStartedWall = 0.0;
    double V7GateProbeStageWall = 0.0;
    double V7GateProbeJumpWall = 0.0;
    double V7GateProbeTimedTriggerWall = 0.0;
    double V7GateProbeLastSampleWall = 0.0;
    TWeakObjectPtr<class ANaiwaP1SwitchPlatform> V7GateProbeSwitch;
    TWeakObjectPtr<ANaiwaP1Trigger> V7GateProbeCheckpoint;
    TWeakObjectPtr<class ANaiwaP1BouncePad> V7GateProbePad;
    TWeakObjectPtr<class ANaiwaP1TimedPlatform> V7GateProbeTimed;
    TArray<TSharedPtr<FJsonValue>> V7GateProbeEvents;
    TArray<TSharedPtr<FJsonValue>> V7GateProbeSamples;

    bool bV7MixedCoreProbeEnabled = false;
    bool bV7HiddenFinalPitRecoveryProbeEnabled = false;
    bool bV7DeathEggProbeEnabled = false;
    bool bV7DeathEggFollowupObserved = false;
    bool bV7DeathEggMusicPauseObserved = false;
    bool bV7DeathEggMusicResumeObserved = false;
    bool bV7DeathEggReturnToDie1Observed = false;
    bool bV7MixedCoreProbeInitialized = false;
    bool bV7MixedCoreProbeFinished = false;
    bool bV7HiddenFinalPitDeathObserved = false;
    bool bV7HiddenFinalPitVisibleDuringDeath = false;
    bool bV7HiddenFinalTimerExpiredBeforeDeath = false;
    bool bV7HiddenFinalPitDeathScreenshotRequested = false;
    bool bV7HiddenFinalRespawnObserved = false;
    bool bV7MixedCoreTeachingHazardJumped = false;
    bool bV7MixedCoreTeachingHazardCleared = false;
    bool bV7MixedCoreJumpHeld = false;
    bool bV7MixedCoreTimedTeachingSupported = false;
    bool bV7MixedCoreTimedTeachingTriggered = false;
    bool bV7MixedCoreTimedTeachingDeclined = false;
    bool bV7MixedCoreTimedTeachingCompleted = false;
    bool bV7MixedCoreTimedCSupported = false;
    bool bV7MixedCoreTimedCTriggered = false;
    bool bV7MixedCoreTimedCDeclined = false;
    bool bV7MixedCoreTimedCCompleted = false;
    bool bV7MixedCoreTeachingBounceLaunched = false;
    bool bV7MixedCorePadBSupported = false;
    bool bV7MixedCorePadBLaunched = false;
    bool bV7MixedCorePadBLanded = false;
    bool bV7MixedCoreSwitchBoarded[4] = {};
    bool bV7MixedCoreSwitchDocked[4] = {};
    bool bV7MixedCoreSwitchCueSeen[4] = {};
    bool bV7MixedCoreSwitchMidwaySupported[4] = {};
    bool bV7MixedCoreSwitchRiseObserved[4] = {};
    bool bV7MixedCoreCheckpointReached[8] = {};
    bool bV7MixedCoreGapJumpIssued = false;
    bool bV7MixedCoreGapJumpLaunched = false;
    bool bV7MixedCoreGapLanded = false;
    bool bV7MixedCoreAExitJumpIssued = false;
    bool bV7MixedCoreAExitJumpLaunched = false;
    bool bV7MixedCoreCExitJumpIssued = false;
    bool bV7MixedCoreCExitJumpLaunched = false;
    bool bV7MixedCoreHiddenFinalPitsInvisible = false;
    bool bV7MixedCoreFinalBounceSupported = false;
    bool bV7MixedCoreFinalBounceLaunched = false;
    bool bV7MixedCoreFinalTimedSupported = false;
    bool bV7MixedCoreFinalTimedTriggered = false;
    bool bV7MixedCoreFinalTimedDecreased = false;
    bool bV7MixedCoreFinalExitJumpIssued = false;
    bool bV7MixedCoreFinalExitJumpLaunched = false;
    bool bV7MixedCoreFinalCompoundCompleted = false;
    bool bV7MixedCoreSecondBounceSupported = false;
    bool bV7MixedCoreSecondBounceLaunched = false;
    bool bV7MixedCoreSecondTimedSupported = false;
    bool bV7MixedCoreSecondTimedTriggered = false;
    bool bV7MixedCoreSecondTimedDecreased = false;
    bool bV7MixedCoreSecondExitJumpIssued = false;
    bool bV7MixedCoreSecondExitJumpLaunched = false;
    bool bV7MixedCoreSecondCompoundCompleted = false;
    bool bV7MixedCorePickupSeen = false;
    int32 V7MixedCoreStage = 0;
    int32 V7MixedCoreGapIndex = 0;
    int32 V7MixedCoreJumpCalls = 0;
    int32 V7MixedCoreTransitTrapsJumped = 0;
    int32 V7MixedCoreDeathCountBefore = 0;
    int32 V7MixedCoreRecoveryCheckpointIndex = -1;
    int32 V7MixedCoreRecoveryPhase = 0;
    int32 V7MixedCoreRecoveryDeathCountBefore = 0;
    int32 V7MixedCoreTeachingTimedTriggerBefore = 0;
    int32 V7MixedCoreTeachingTimedDisableBefore = 0;
    int32 V7MixedCoreTimedCTriggerBefore = 0;
    int32 V7MixedCoreTimedCDisableBefore = 0;
    int32 V7MixedCorePadActivationBefore = 0;
    int32 V7MixedCoreFinalPadActivationBefore = 0;
    int32 V7MixedCoreTimedFinalTriggerBefore = 0;
    int32 V7MixedCoreTimedFinalDisableBefore = 0;
    int32 V7MixedCoreSecondPadActivationBefore = 0;
    int32 V7MixedCoreTimedSecondTriggerBefore = 0;
    int32 V7MixedCoreTimedSecondDisableBefore = 0;
    int32 V7MixedCoreSwitchSpeedSamples[4] = {};
    int32 V7MixedCoreHiddenPitCount = 0;
    float V7MixedCoreCapsuleRadius = 0.0f;
    float V7MixedCoreTeachingTimedRemainingPrevious = 0.0f;
    float V7MixedCoreTimedCRemainingPrevious = 0.0f;
    float V7MixedCoreTeachingTimedExitRemaining = 0.0f;
    float V7MixedCoreTimedCExitRemaining = 0.0f;
    float V7MixedCoreTimedFinalRemainingPrevious = 0.0f;
    float V7MixedCoreTimedFinalRemainingAtTakeoff = 0.0f;
    float V7MixedCoreSecondTimedRemainingPrevious = 0.0f;
    float V7MixedCoreSecondTimedRemainingAtTakeoff = 0.0f;
    float V7MixedCoreSecondBounceLaunchVz = 0.0f;
    float V7MixedCoreSecondBounceLandingFeetZ = 0.0f;
    float V7MixedCoreSecondExitJumpX = 0.0f;
    float V7MixedCoreSwitchSpeedMinimum[4] = {};
    float V7MixedCoreSwitchSpeedMaximum[4] = {};
    float V7MixedCoreCTopAtMidpoint = 0.0f;
    float V7MixedCoreCTopAtDock = 0.0f;
    float V7MixedCoreAExitJumpX = 0.0f;
    float V7MixedCoreCExitJumpX = 0.0f;
    float V7MixedCoreFinalBounceLaunchVz = 0.0f;
    float V7MixedCoreFinalBounceLandingFeetZ = 0.0f;
    float V7MixedCoreFinalExitJumpX = 0.0f;
    float V7MixedCoreGapJumpX = 0.0f;
    float V7MixedCoreGapLandX = 0.0f;
    float V7MixedCoreGapLaunchVz = 0.0f;
    float V7MixedCoreGapAirSeconds = 0.0f;
    double V7MixedCoreStartedWall = 0.0;
    double V7MixedCoreStageStartedWall = 0.0;
    double V7MixedCoreGapJumpWall = 0.0;
    double V7MixedCoreTimedTeachingStartedWall = 0.0;
    double V7MixedCoreTimedCStartedWall = 0.0;
    double V7MixedCoreWonSettleStartedWall = 0.0;
    double V7MixedCoreRecoveryStartedWall = 0.0;
    double V7MixedCoreJumpReleaseGameTime = 0.0;
    double V7HiddenFinalRespawnWall = 0.0;
    ENaiwaP1V7MixedStep V7MixedCoreStep = ENaiwaP1V7MixedStep::WaitReady;
    TWeakObjectPtr<ANaiwaP1Trigger> V7MixedCoreHazardTeaching;
    TWeakObjectPtr<ANaiwaP1Trigger> V7MixedCorePickup;
    TWeakObjectPtr<ANaiwaP1Trigger> V7MixedCoreGoal;
    TWeakObjectPtr<ANaiwaP1Trigger> V7MixedCoreCheckpoints[8];
    TWeakObjectPtr<class ANaiwaP1SwitchPlatform> V7MixedCoreSwitches[4];
    TWeakObjectPtr<class ANaiwaP1BouncePad> V7MixedCorePads[4];
    TWeakObjectPtr<class ANaiwaP1TimedPlatform> V7MixedCoreTimed[4];
    TWeakObjectPtr<AStaticMeshActor> V7MixedCoreGrounds[4];
    TMap<FName, TWeakObjectPtr<AStaticMeshActor>> V7MixedCoreV7GroundById;
    TArray<TSharedPtr<FJsonValue>> V7MixedCoreEvents;
    TArray<TSharedPtr<FJsonValue>> V7MixedCoreCheckpointRecords;
    TArray<TSharedPtr<FJsonValue>> V7MixedCoreGaps;
    TArray<TSharedPtr<FJsonValue>> V7MixedCoreGaitChanges;
    TSharedPtr<FJsonObject> V7MixedCoreRecoveryRecord;
    bool bV7MixedCoreRecoveryPassed = false;
    TSet<FName> V7MixedCoreTransitTrapIdsJumped;

    bool bChainProbeEnabled = false;
    bool bChainProbeWalk = false;
    bool bChainProbeFinished = false;
    bool bChainProbeSettling = true;
    bool bChainProbeDropped = false;
    int32 ChainProbeIndex = 0;
    bool bChainProbeJumped = false;
    bool ChainProbeJumpGrounded[3] = {};
    bool ChainProbeLanded[3] = {};
    float ChainProbeJumpVz[3] = {};
    float ChainProbeJumpX[3] = {};
    float ChainProbePeakZ[3] = {};
    float ChainProbeLandX[3] = {};
    float ChainProbeAirSeconds[3] = {};
    float ChainProbeGroundSeconds[3] = {};
    float ChainProbeCapsuleRadius = 0.0f;
    float ChainProbeCapsuleHalfHeight = 0.0f;
    float ChainProbeMinZ = 0.0f;
    double ChainProbePhaseWall = 0.0;
    double ChainProbeJumpWall = 0.0;
    double ChainProbeLandWall = 0.0;
    TArray<TSharedPtr<FJsonValue>> ChainProbeResults;

    bool bHazardRevealProbeEnabled = false;
    bool bHazardRevealProbeFinished = false;
    bool bHazardRevealProbeLaunched = false;
    int32 HazardRevealProbeStage = 0;
    int32 HazardRevealProbeDeathBefore = 0;
    bool HazardRevealProbeVisibleAtSpawn = false;
    bool HazardRevealProbeRevealOnSpawn = false;
    bool HazardRevealProbeKilled = false;
    bool HazardRevealProbeVisibleAfterRespawn = false;
    float HazardRevealProbeDeathX = 0.0f;
    float HazardRevealProbeRunSeconds = 0.0f;
    double HazardRevealProbePhaseWall = 0.0;
    double HazardRevealProbeLaunchWall = 0.0;
    TWeakObjectPtr<ANaiwaP1Trigger> HazardRevealProbeTarget;
};
