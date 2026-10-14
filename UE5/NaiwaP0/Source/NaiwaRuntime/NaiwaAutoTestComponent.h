#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "Async/Future.h"
#include "NaiwaAutoTestComponent.generated.h"

class ANaiwaCharacter;
class FJsonObject;
class FJsonValue;
class APlayerController;
class UGameViewportClient;

/** Runs the P0 movement scenarios in the actual world, only with -NaiwaAutoTest. */
UCLASS(ClassGroup = (Naiwa), meta = (BlueprintSpawnableComponent))
class NAIWARUNTIME_API UNaiwaAutoTestComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UNaiwaAutoTestComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    enum class ECase : uint8
    {
        FlatStartStop, Reverse, Wall, PlatformJump, Drop, LongAir, EarlyLanding, Reset, Count
    };

    struct FObservation
    {
        FVector StartLocation = FVector::ZeroVector;
        FVector ImmediateResetLocation = FVector::ZeroVector;
        FVector BeforeResetLocation = FVector::ZeroVector;
        double ImmediateResetSpeed = 0.0;
        double MaxAbsY = 0.0;
        double MinX = TNumericLimits<double>::Max();
        double MaxX = TNumericLimits<double>::Lowest();
        double MaxZAfterJump = TNumericLimits<double>::Lowest();
        double MaxSpeed = 0.0;
        double MaxAirSeconds = 0.0;
        double CurrentAirSeconds = 0.0;
        double MaxStateMismatchSeconds = 0.0;
        double CurrentStateMismatchSeconds = 0.0;
        double FirstLandingTime = -1.0;
        double JumpIssuedCaseTime = -1.0;
        double PositiveYaw = 0.0;
        double NegativeYaw = 0.0;
        double PositiveForwardY = 0.0;
        double NegativeForwardY = 0.0;
        double PositiveForwardX = 0.0;
        double NegativeForwardX = 0.0;
        double MinBoundsBottom = TNumericLimits<double>::Max();
        double MaxBoundsBottom = TNumericLimits<double>::Lowest();
        bool bGroundedBeforeAirborne = false;
        bool bSawAirborne = false;
        bool bSawLanding = false;
        bool bSawJumpState = false;
        bool bSawWalkState = false;
        bool bSawIdleState = false;
        bool bSawPositiveSpeed = false;
        bool bSawNegativeSpeed = false;
        bool bJumpIssued = false;
        bool bLastFalling = false;
        bool bPauseObserved = false;
        bool bResumeObserved = false;
        bool bResetWhilePaused = false;
        FVector PauseLocation = FVector::ZeroVector;
        double MaxPausedDisplacement = 0.0;
        int32 MoveEventsAtStart = 0;
        int32 JumpEventsAtStart = 0;
        int32 PauseEventsAtStart = 0;
        int32 ResetEventsAtStart = 0;
        uint64 PoseEvaluationsAtStart = 0;
        uint32 AnimResetGenerationAtStart = 0;
        double MaxPoseDeltaDegrees[3] = {0.0, 0.0, 0.0};
        double MaxPoseVsReferenceDegrees[3] = {0.0, 0.0, 0.0};
        TArray<FTransform> FirstStatePose[3];
        double JumpHoldSeconds = 0.0;
        double MaxHeldPoseDeltaDegrees = 0.0;
        TArray<FTransform> FirstHeldPose;
        double MaximumJumpSampleTime = 0.0;
        double RightWallFinalX = 0.0;
        bool bLeftWallPlaced = false;
        bool bSecondJumpIssued = false;
        double SecondJumpTime = -1.0;
        double JumpInputAppliedCaseTime = -1.0;
        double SecondJumpInputAppliedCaseTime = -1.0;
        int32 PauseSequenceStep = 0;
        double PauseSequenceStepTime = 0.0;
        int32 ObservedLandings = 0;
        bool bResetClockApplied = false;
        double FirstResetClockMaximumTime = -1.0;
        FString LastAnimState;
        TArray<TSharedPtr<FJsonValue>> Events;
    };

    void StartCase();
    void DriveCase();
    void ObserveFrame(float DeltaTime);
    void CompleteCase();
    void FinishRun();
    void FlushCsv();
    void WriteSummary(bool bComplete);
    void PlaceCharacter(const FVector& Location, const FVector& Velocity = FVector::ZeroVector);
    void SetKey(const FKey& Key, bool bDown);
    void ReleaseKeys();
    void ObservePose(float DeltaTime);
    TSharedPtr<FJsonObject> MeasureSkinnedVertices() const;
    void TickPerformance(double WallDeltaSeconds);
    void RecordFrame();
    void ConfigureInputIsolation();
    void RestoreInputIsolation();
    void HandleScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Bitmap);
    void CompleteScreenshotWrites();
    FString CaseName() const;
    double CaseDuration() const;
    void AddCheck(const TSharedPtr<FJsonObject>& Result, TArray<TSharedPtr<FJsonValue>>& Checks,
        const FString& Name, bool bPass, const FString& Expected, const FString& Actual);

    TWeakObjectPtr<ANaiwaCharacter> Character;
    TWeakObjectPtr<APlayerController> PlayerController;
    TSet<FKey> HeldKeys;
    TWeakObjectPtr<UGameViewportClient> TestViewport;
    bool bViewportInputWasIgnored = false;
    bool bFocusFlushWasEnabled = true;
    bool bControllerFocusFlushWasEnabled = true;
    bool bInputIsolationActive = false;
    bool bControllerInputIsolationActive = false;
    FDelegateHandle ScreenshotDelegateHandle;
    TArray<TFuture<bool>> ScreenshotWrites;
    ECase CurrentCase = ECase::FlatStartStop;
    int32 Round = 1;
    int32 CsvBufferedFrames = 0;
    int64 FrameCount = 0;
    double StartupSeconds = 0.0;
    double RunSeconds = 0.0;
    double CaseSeconds = 0.0;
    double GapSeconds = 0.0;
    double QuitSeconds = 0.0;
    double PreviousWallSeconds = 0.0;
    double WallFrameMs = 0.0;
    double StartupDelay = 0.4;
    double RecordWallStart = 0.0;
    double LastRecordWallTime = 0.0;
    bool bRecordFrames = false;
    bool bPerformanceStarted = false;
    int32 PerformancePhase = 0;
    int32 PerformanceDirection = 1;
    double PerformanceSeconds = 0.0;
    double PerformanceWarmup = 2.0;
    double PerformanceSampleDuration = 5.0;
    TArray<TSharedPtr<FJsonValue>> PerformanceSamples;
    TArray<TSharedPtr<FJsonValue>> RecordedFrames;
    bool bEnabled = false;
    bool bStarted = false;
    bool bBetweenCases = false;
    bool bComplete = false;
    bool bQuitWhenFinished = false;
    bool bAllChecksPass = true;
    bool bOutputWriteFailed = false;
    bool bHaveResetReference = false;
    FVector SpawnLocation = FVector::ZeroVector;
    FVector ResetReference = FVector::ZeroVector;
    FString RunId;
    FString OutputDirectory;
    FString CsvPath;
    FString CsvBuffer;
    FString BoneCsvBuffer;
    FString MorphCsvBuffer;
    TArray<double> PerfWallMs;
    TArray<double> PerfGameMs;
    TArray<double> PerfRenderMs;
    TArray<double> PerfGpuMs;
    FObservation Observation;
    TArray<TSharedPtr<FJsonValue>> Results;
};
