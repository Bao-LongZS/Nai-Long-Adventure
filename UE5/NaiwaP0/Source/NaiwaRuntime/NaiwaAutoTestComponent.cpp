#include "NaiwaAutoTestComponent.h"
#include "NaiwaCharacter.h"
#include "NaiwaAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/MorphTarget.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/InputSettings.h"
#include "Engine/SkeletalMesh.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "RenderTimer.h"
#include "DynamicRHI.h"
#include "SkeletalRenderPublic.h"
#include "ImageUtils.h"
#include "Async/Async.h"
#include "Modules/ModuleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
    TSharedPtr<FJsonObject> VectorJson(const FVector& Value)
    {
        TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("x"), Value.X);
        Object->SetNumberField(TEXT("y"), Value.Y);
        Object->SetNumberField(TEXT("z"), Value.Z);
        return Object;
    }

    FString Scalar(double Value)
    {
        return FString::Printf(TEXT("%.4f"), Value);
    }

    struct FNaiwaAutoTestInputMode final : FInputModeGameOnly
    {
        virtual bool ShouldFlushInputOnViewportFocus() const override { return false; }
    };
}

UNaiwaAutoTestComponent::UNaiwaAutoTestComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    PrimaryComponentTick.bTickEvenWhenPaused = true;
}

void UNaiwaAutoTestComponent::BeginPlay()
{
    Super::BeginPlay();
    bEnabled = FParse::Param(FCommandLine::Get(), TEXT("NaiwaAutoTest"));
    bQuitWhenFinished = FParse::Param(FCommandLine::Get(), TEXT("NaiwaTestQuit"));
    Character = Cast<ANaiwaCharacter>(GetOwner());
    PlayerController = Character.IsValid() ? Cast<APlayerController>(Character->GetController()) : nullptr;
    bRecordFrames = FParse::Param(FCommandLine::Get(), TEXT("NaiwaRecordFrames"));
    FParse::Value(FCommandLine::Get(), TEXT("NaiwaTestDelay="), StartupDelay);
    StartupDelay = FMath::Max(0.4, StartupDelay);
    if (!bEnabled || !Character.IsValid())
    {
        SetComponentTickEnabled(false);
        return;
    }

    AddTickPrerequisiteActor(Character.Get());
    AddTickPrerequisiteComponent(Character->GetCharacterMovement());
    AddTickPrerequisiteComponent(Character->GetMesh());
    TestViewport = GetWorld()->GetGameViewport();
    if (TestViewport.IsValid())
    {
        bViewportInputWasIgnored = TestViewport->IgnoreInput();
        TestViewport->SetIgnoreInput(true);
        bFocusFlushWasEnabled = GetDefault<UInputSettings>()->bShouldFlushPressedKeysOnViewportFocusLost;
        GetMutableDefault<UInputSettings>()->bShouldFlushPressedKeysOnViewportFocusLost = false;
        bInputIsolationActive = true;
    }
    ConfigureInputIsolation();
    if (bRecordFrames)
    {
        // Preload on the game thread; each worker creates its own PNG encoder.
        FModuleManager::Get().LoadModuleChecked<IModuleInterface>(TEXT("ImageWrapper"));
        ScreenshotDelegateHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(this,
            &UNaiwaAutoTestComponent::HandleScreenshotCaptured);
    }
    RunId = FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")),
        FPlatformProcess::GetCurrentProcessId());
    OutputDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("NaiwaTests"), RunId));
    bOutputWriteFailed = !IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    CsvPath = FPaths::Combine(OutputDirectory, TEXT("frames.csv"));
    const FString Header = TEXT("time_s,round,case,case_time_s,actor_x_cm,actor_y_cm,actor_z_cm,")
        TEXT("velocity_x_cm_s,velocity_y_cm_s,velocity_z_cm_s,is_falling,anim_state,")
        TEXT("mesh_bounds_bottom_cm,capsule_bottom_cm,mesh_yaw_deg,mesh_forward_y,frame_ms,wall_frame_ms\n");
    bOutputWriteFailed |= !FFileHelper::SaveStringToFile(Header, *CsvPath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    bOutputWriteFailed |= !FFileHelper::SaveStringToFile(
        TEXT("time_s,round,case,case_time_s,state,bone,component_x_cm,component_y_cm,component_z_cm,quat_x,quat_y,quat_z,quat_w,jump_sample_time_s,evaluate_count\n"),
        *FPaths::Combine(OutputDirectory, TEXT("bone_poses.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    PreviousWallSeconds = FPlatformTime::Seconds();
    bOutputWriteFailed |= !FFileHelper::SaveStringToFile(
        TEXT("time_s,round,case,case_time_s,state,move_sample_time_s,morph,anim_curve_value,manual_morph_value,final_component_weight\n"),
        *FPaths::Combine(OutputDirectory, TEXT("morph_weights.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    RecordWallStart = PreviousWallSeconds;
    UE_LOG(LogTemp, Display, TEXT("NAIWA_AUTOTEST_START output=%s; three rounds, eight cases per round"),
        *OutputDirectory);
}

void UNaiwaAutoTestComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bEnabled)
    {
        ReleaseKeys();
        CompleteScreenshotWrites();
        RestoreInputIsolation();
        FlushCsv();
        if (!bComplete)
        {
            bAllChecksPass = false;
            WriteSummary(false);
            UE_LOG(LogTemp, Error, TEXT("NAIWA_AUTOTEST_INTERRUPTED output=%s"), *OutputDirectory);
        }
    }
    Super::EndPlay(EndPlayReason);
}

void UNaiwaAutoTestComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bEnabled || !Character.IsValid())
    {
        return;
    }
    ConfigureInputIsolation();
    const double WallSeconds = FPlatformTime::Seconds();
    WallFrameMs = (WallSeconds - PreviousWallSeconds) * 1000.0;
    PreviousWallSeconds = WallSeconds;
    if (GEngine && bStarted && !bComplete)
    {
        GEngine->AddOnScreenDebugMessage(98201, 0.2f, FColor::White,
            FString::Printf(TEXT("Naiwa P0 | round %d/3 | %s | %s | Y %.3f cm | A/D move Space jump P pause R reset"),
                Round, bPerformanceStarted ? TEXT("performance") : *CaseName(),
                *Character->GetLocomotionState(), Character->GetActorLocation().Y));
    }
    if (bComplete)
    {
        QuitSeconds += DeltaTime;
        if (bQuitWhenFinished && QuitSeconds >= 0.5)
        {
            FPlatformMisc::RequestExit(false, TEXT("NaiwaAutoTest completed"));
            SetComponentTickEnabled(false);
        }
        return;
    }
    if (!bStarted)
    {
        StartupSeconds += WallFrameMs / 1000.0;
        if (StartupSeconds >= StartupDelay)
        {
            // The owner finishes BeginPlay before this first component tick.
            SpawnLocation = Character->GetActorLocation();
            Character->DisableAutomatedInput();
            PlayerController = Cast<APlayerController>(Character->GetController());
            if (!PlayerController.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("NAIWA_AUTOTEST_NO_PLAYER_CONTROLLER"));
                bAllChecksPass = false;
                FinishRun();
                return;
            }
            // Capture the requested spawn height separately from the settled character height.
            // ResetToSpawn is authoritative and returns the originally stored owner spawn.
            Character->ResetToSpawn();
            SpawnLocation = Character->GetActorLocation();
            bStarted = true;
            StartCase();
        }
        return;
    }

    RunSeconds += DeltaTime;
    if (bPerformanceStarted)
    {
        TickPerformance(WallFrameMs / 1000.0);
        return;
    }
    if (bBetweenCases)
    {
        GapSeconds += DeltaTime;
        if (GapSeconds >= 0.35)
        {
            if (CurrentCase == ECase::Reset)
            {
                if (Round == 3)
                {
                    ReleaseKeys();
                    Character->ResetToSpawn();
                    bPerformanceStarted = true;
                    PerformanceSeconds = 0.0;
                    PerformanceDirection = 1;
                    UE_LOG(LogTemp, Display, TEXT("NAIWA_PERFORMANCE_START visible and hidden mesh: 2 s warmup + 5 s sample each"));
                    return;
                }
                ++Round;
                CurrentCase = ECase::FlatStartStop;
            }
            else
            {
                CurrentCase = static_cast<ECase>(static_cast<uint8>(CurrentCase) + 1);
            }
            StartCase();
        }
        RecordFrame();
        return;
    }

    CaseSeconds += UGameplayStatics::IsGamePaused(this) ? WallFrameMs / 1000.0 : DeltaTime;
    DriveCase();
    ObserveFrame(DeltaTime);
    if (CaseSeconds >= CaseDuration())
    {
        CompleteCase();
    }
    else
    {
        RecordFrame();
    }
}

FString UNaiwaAutoTestComponent::CaseName() const
{
    switch (CurrentCase)
    {
    case ECase::FlatStartStop: return TEXT("flat_start_stop");
    case ECase::Reverse: return TEXT("reverse");
    case ECase::Wall: return TEXT("wall_collision");
    case ECase::PlatformJump: return TEXT("jump_onto_platform");
    case ECase::Drop: return TEXT("platform_drop");
    case ECase::LongAir: return TEXT("long_airborne");
    case ECase::EarlyLanding: return TEXT("early_landing");
    case ECase::Reset: return TEXT("reset");
    default: return TEXT("invalid");
    }
}

double UNaiwaAutoTestComponent::CaseDuration() const
{
    switch (CurrentCase)
    {
    case ECase::FlatStartStop: return 2.3;
    case ECase::Reverse: return 2.5;
    case ECase::Wall: return 4.8;
    case ECase::PlatformJump: return 4.0;
    case ECase::Drop: return 3.0;
    case ECase::LongAir: return 2.6;
    case ECase::EarlyLanding: return 4.0;
    case ECase::Reset: return 3.2;
    default: return 1.0;
    }
}

void UNaiwaAutoTestComponent::PlaceCharacter(const FVector& Location, const FVector& Velocity)
{
    Character->StopJumping();
    ReleaseKeys();
    Character->DisableAutomatedInput();
    Character->ConsumeMovementInputVector();
    Character->GetCharacterMovement()->StopMovementImmediately();
    Character->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
    Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    Character->GetCharacterMovement()->Velocity = Velocity;
}

void UNaiwaAutoTestComponent::StartCase()
{
    Observation = FObservation();
    CaseSeconds = 0.0;
    GapSeconds = 0.0;
    bBetweenCases = false;
    ReleaseKeys();
    Character->DisableAutomatedInput();
    switch (CurrentCase)
    {
    case ECase::FlatStartStop:
        Character->ResetToSpawn();
        break;
    case ECase::Reverse: PlaceCharacter(FVector(200.0, 0.0, 120.0)); break;
    case ECase::Wall: PlaceCharacter(FVector(680.0, 0.0, 120.0)); break;
    case ECase::PlatformJump: PlaceCharacter(FVector(1400.0, 0.0, 120.0)); break;
    case ECase::Drop: PlaceCharacter(FVector(2440.0, 0.0, 420.0)); break;
    case ECase::LongAir: PlaceCharacter(FVector(3000.0, 0.0, 900.0)); break;
    case ECase::EarlyLanding:
        PlaceCharacter(FVector(3450.0, 0.0, 120.0));
        break;
    case ECase::Reset:
        PlaceCharacter(FVector(4300.0, 0.0, 650.0), FVector(180.0, 0.0, -120.0));
        Observation.BeforeResetLocation = Character->GetActorLocation();
        break;
    default: break;
    }
    Observation.StartLocation = Character->GetActorLocation();
    Observation.bLastFalling = Character->GetCharacterMovement()->IsFalling();
    Observation.MoveEventsAtStart = Character->GetMoveInputEvents();
    Observation.JumpEventsAtStart = Character->GetJumpInputEvents();
    Observation.PauseEventsAtStart = Character->GetPauseInputEvents();
    Observation.ResetEventsAtStart = Character->GetResetInputEvents();
    if (const UNaiwaAnimInstance* Anim = Cast<UNaiwaAnimInstance>(Character->GetMesh()->GetAnimInstance()))
    {
        Observation.PoseEvaluationsAtStart = Anim->GetPoseEvaluationCount();
        Observation.AnimResetGenerationAtStart = Anim->GetResetGeneration();
    }
    UE_LOG(LogTemp, Display, TEXT("NAIWA_AUTOTEST_CASE_START round=%d case=%s"), Round, *CaseName());
}

void UNaiwaAutoTestComponent::DriveCase()
{
    float MoveInput = 0.0f;
    // The key is queued after movement. Measure its held duration from actual
    // Enhanced Input delivery, so recording hitches cannot shorten a 0.2 s hold.
    if (Observation.bJumpIssued && Observation.JumpInputAppliedCaseTime < 0.0 &&
        Character->GetJumpInputEvents() > Observation.JumpEventsAtStart)
        Observation.JumpInputAppliedCaseTime = CaseSeconds;
    if (Observation.bSecondJumpIssued && Observation.SecondJumpInputAppliedCaseTime < 0.0 &&
        Character->GetJumpInputEvents() > Observation.JumpEventsAtStart + 1)
        Observation.SecondJumpInputAppliedCaseTime = CaseSeconds;
    switch (CurrentCase)
    {
    case ECase::FlatStartStop:
        MoveInput = CaseSeconds >= 0.35 && CaseSeconds < 1.25 ? 1.0f : 0.0f;
        break;
    case ECase::Reverse:
        if (CaseSeconds >= 0.35 && CaseSeconds < 0.95) MoveInput = 1.0f;
        else if (CaseSeconds >= 0.95 && CaseSeconds < 1.80) MoveInput = -1.0f;
        break;
    case ECase::Wall:
        if (CaseSeconds >= 2.3 && !Observation.bLeftWallPlaced)
        {
            Observation.RightWallFinalX = Character->GetActorLocation().X;
            PlaceCharacter(FVector(-480.0, 0.0, 120.0));
            Observation.bLeftWallPlaced = true;
        }
        MoveInput = CaseSeconds >= 0.35 && CaseSeconds < 2.25 ? 1.0f :
            CaseSeconds >= 2.65 ? -1.0f : 0.0f;
        break;
    case ECase::PlatformJump:
        if (!Observation.bJumpIssued && CaseSeconds >= 0.35 && CaseSeconds < 1.0 &&
            !Character->GetCharacterMovement()->IsFalling())
        {
            SetKey(EKeys::SpaceBar, true);
            Observation.bJumpIssued = true;
            Observation.JumpIssuedCaseTime = CaseSeconds;
        }
        if (Observation.bJumpIssued)
        {
            if (Observation.JumpInputAppliedCaseTime >= 0.0 &&
                CaseSeconds - Observation.JumpInputAppliedCaseTime >= 0.2) SetKey(EKeys::SpaceBar, false);
            MoveInput = Character->GetActorLocation().X < 1700.0 && CaseSeconds < 1.9 ? 1.0f : 0.0f;
        }
        break;
    case ECase::Drop:
        MoveInput = CaseSeconds >= 0.4 && CaseSeconds < 1.3 ? 1.0f : 0.0f;
        break;
    case ECase::EarlyLanding:
        if (!Observation.bJumpIssued && CaseSeconds >= 0.35 && !Character->GetCharacterMovement()->IsFalling())
        {
            SetKey(EKeys::SpaceBar, true);
            Observation.bJumpIssued = true;
            Observation.JumpIssuedCaseTime = CaseSeconds;
        }
        if (Observation.bJumpIssued && !Observation.bSecondJumpIssued)
        {
            if (Observation.JumpInputAppliedCaseTime >= 0.0 &&
                CaseSeconds - Observation.JumpInputAppliedCaseTime >= 0.2) SetKey(EKeys::SpaceBar, false);
            MoveInput = Character->GetActorLocation().X < 3700.0 ? 1.0f : 0.0f;
            if (Observation.bSawLanding && CaseSeconds - Observation.FirstLandingTime >= 0.25 &&
                !Character->GetCharacterMovement()->IsFalling() &&
                Character->GetActorLocation().X >= 3700.0 && FMath::Abs(Character->GetVelocity().X) < 1.0)
            {
                SetKey(EKeys::SpaceBar, true);
                Observation.bSecondJumpIssued = true;
                Observation.SecondJumpTime = CaseSeconds;
            }
        }
        if (Observation.bSecondJumpIssued && Observation.SecondJumpInputAppliedCaseTime >= 0.0 &&
            CaseSeconds - Observation.SecondJumpInputAppliedCaseTime >= 0.2)
            SetKey(EKeys::SpaceBar, false);
        break;
    case ECase::Reset:
    {
        // Advance only after each mapped key callback is observed. Fixed 70 ms
        // windows can be skipped by screenshot/CPU-skin stalls and are not evidence.
        if ((Observation.PauseSequenceStep == 2 || Observation.PauseSequenceStep == 6) &&
            UGameplayStatics::IsGamePaused(this))
            Observation.MaxPausedDisplacement = FMath::Max(Observation.MaxPausedDisplacement,
                FVector::Distance(Observation.PauseLocation, Character->GetActorLocation()));
        switch (Observation.PauseSequenceStep)
        {
        case 0:
            if (CaseSeconds >= 0.2) { SetKey(EKeys::P, true); Observation.PauseSequenceStep = 1; }
            break;
        case 1:
            if (Character->GetPauseInputEvents() > Observation.PauseEventsAtStart) {
                SetKey(EKeys::P, false);
                Observation.bPauseObserved = UGameplayStatics::IsGamePaused(this);
                Observation.PauseLocation = Character->GetActorLocation();
                Observation.PauseSequenceStepTime = CaseSeconds; Observation.PauseSequenceStep = 2;
            }
            break;
        case 2:
            if (CaseSeconds - Observation.PauseSequenceStepTime >= 0.3) { SetKey(EKeys::P, true); Observation.PauseSequenceStep = 3; }
            break;
        case 3:
            if (Character->GetPauseInputEvents() > Observation.PauseEventsAtStart + 1) {
                SetKey(EKeys::P, false);
                Observation.bResumeObserved = !UGameplayStatics::IsGamePaused(this);
                Observation.PauseSequenceStepTime = CaseSeconds; Observation.PauseSequenceStep = 4;
            }
            break;
        case 4:
            if (CaseSeconds - Observation.PauseSequenceStepTime >= 0.25) { SetKey(EKeys::P, true); Observation.PauseSequenceStep = 5; }
            break;
        case 5:
            if (Character->GetPauseInputEvents() > Observation.PauseEventsAtStart + 2) {
                SetKey(EKeys::P, false);
                Observation.PauseLocation = Character->GetActorLocation();
                Observation.PauseSequenceStepTime = CaseSeconds; Observation.PauseSequenceStep = 6;
            }
            break;
        case 6:
            if (CaseSeconds - Observation.PauseSequenceStepTime >= 0.15) {
                Observation.bResetWhilePaused = UGameplayStatics::IsGamePaused(this);
                SetKey(EKeys::R, true); Observation.PauseSequenceStep = 7;
            }
            break;
        case 7:
            if (Character->GetResetInputEvents() > Observation.ResetEventsAtStart) {
                SetKey(EKeys::R, false); Observation.PauseSequenceStep = 8;
            }
            break;
        default: break;
        }
        if (Character->GetResetInputEvents() > Observation.ResetEventsAtStart)
        {
            Observation.ImmediateResetLocation = Character->GetLastResetLocation();
            Observation.ImmediateResetSpeed = Character->GetLastResetSpeed();
        }
        break;
    }
    default: break;
    }
    SetKey(EKeys::D, MoveInput > 0.0f);
    SetKey(EKeys::A, MoveInput < 0.0f);
}

void UNaiwaAutoTestComponent::ObserveFrame(float DeltaTime)
{
    const FVector Location = Character->GetActorLocation();
    const FVector Velocity = Character->GetVelocity();
    const bool bFalling = Character->GetCharacterMovement()->IsFalling();
    const FString State = Character->GetLocomotionState();
    const double Yaw = Character->GetMesh()->GetComponentRotation().Yaw;
    const double ForwardY = Character->GetMesh()->GetForwardVector().Y;
    const double ForwardX = Character->GetMesh()->GetForwardVector().X;
    const double BoundsBottom = Character->GetMeshBoundsBottom();
    ObservePose(DeltaTime);
    if (CurrentCase == ECase::Reset && !Observation.bResetClockApplied &&
        Character->GetResetInputEvents() > Observation.ResetEventsAtStart)
    {
        if (const UNaiwaAnimInstance* Anim = Cast<UNaiwaAnimInstance>(Character->GetMesh()->GetAnimInstance()))
        {
            if (Anim->GetProxyResetGeneration() > Observation.AnimResetGenerationAtStart)
            {
                Observation.bResetClockApplied = true;
                Observation.FirstResetClockMaximumTime = FMath::Max3(
                    Anim->GetAnimationSampleTime(ENaiwaLocomotionState::Idle),
                    Anim->GetAnimationSampleTime(ENaiwaLocomotionState::Walk),
                    Anim->GetAnimationSampleTime(ENaiwaLocomotionState::Jump));
            }
        }
    }
    Observation.MaxAbsY = FMath::Max(Observation.MaxAbsY, FMath::Abs(Location.Y));
    Observation.MinX = FMath::Min(Observation.MinX, Location.X);
    Observation.MaxX = FMath::Max(Observation.MaxX, Location.X);
    Observation.MaxSpeed = FMath::Max(Observation.MaxSpeed, FMath::Abs(Velocity.X));
    Observation.MinBoundsBottom = FMath::Min(Observation.MinBoundsBottom, BoundsBottom);
    Observation.MaxBoundsBottom = FMath::Max(Observation.MaxBoundsBottom, BoundsBottom);
    Observation.bSawWalkState |= State == TEXT("Walk");
    Observation.bSawIdleState |= State == TEXT("Idle");
    if (Velocity.X > 30.0)
    {
        Observation.bSawPositiveSpeed = true;
        Observation.PositiveYaw = Yaw;
        Observation.PositiveForwardY = ForwardY;
        Observation.PositiveForwardX = ForwardX;
    }
    if (Velocity.X < -30.0)
    {
        Observation.bSawNegativeSpeed = true;
        Observation.NegativeYaw = Yaw;
        Observation.NegativeForwardY = ForwardY;
        Observation.NegativeForwardX = ForwardX;
    }
    if (!bFalling && !Observation.bSawAirborne)
    {
        Observation.bGroundedBeforeAirborne = true;
    }
    // Ignore the initial placement fall for the jump and walk-off scenarios.
    const bool bObserveAir = CurrentCase == ECase::PlatformJump || CurrentCase == ECase::EarlyLanding ? Observation.bJumpIssued
        : (CurrentCase == ECase::Drop ? Observation.bGroundedBeforeAirborne && CaseSeconds >= 0.4
            : true);
    if (bObserveAir && bFalling)
    {
        Observation.bSawAirborne = true;
        Observation.CurrentAirSeconds += DeltaTime;
        Observation.MaxAirSeconds = FMath::Max(Observation.MaxAirSeconds, Observation.CurrentAirSeconds);
        Observation.bSawJumpState |= State == TEXT("Jump");
    }
    else
    {
        if (bObserveAir && Observation.bSawAirborne && Observation.bLastFalling && !bFalling)
        {
            Observation.bSawLanding = true;
            ++Observation.ObservedLandings;
            if (Observation.FirstLandingTime < 0.0) Observation.FirstLandingTime = CaseSeconds;
        }
        Observation.CurrentAirSeconds = 0.0;
    }
    if (Observation.bJumpIssued)
    {
        Observation.MaxZAfterJump = FMath::Max(Observation.MaxZAfterJump, Location.Z);
    }
    const FString ExpectedState = bFalling ? TEXT("Jump")
        : (FMath::Abs(Velocity.X) > 3.0 ? TEXT("Walk") : TEXT("Idle"));
    if (State != ExpectedState)
    {
        Observation.CurrentStateMismatchSeconds += DeltaTime;
        Observation.MaxStateMismatchSeconds = FMath::Max(Observation.MaxStateMismatchSeconds,
            Observation.CurrentStateMismatchSeconds);
    }
    else
    {
        Observation.CurrentStateMismatchSeconds = 0.0;
    }
    if (State != Observation.LastAnimState || bFalling != Observation.bLastFalling)
    {
        TSharedPtr<FJsonObject> Event = MakeShared<FJsonObject>();
        Event->SetNumberField(TEXT("case_time_s"), CaseSeconds);
        Event->SetBoolField(TEXT("is_falling"), bFalling);
        Event->SetStringField(TEXT("anim_state"), State);
        Event->SetObjectField(TEXT("position_cm"), VectorJson(Location));
        Observation.Events.Add(MakeShared<FJsonValueObject>(Event));
    }
    Observation.LastAnimState = State;
    Observation.bLastFalling = bFalling;
    CsvBuffer += FString::Printf(
        TEXT("%.6f,%d,%s,%.6f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%s,%.4f,%.4f,%.3f,%.6f,%.4f,%.4f\n"),
        RunSeconds, Round, *CaseName(), CaseSeconds, Location.X, Location.Y, Location.Z,
        Velocity.X, Velocity.Y, Velocity.Z, bFalling ? 1 : 0, *State, BoundsBottom,
        static_cast<double>(Character->GetCapsuleBottom()), Yaw, ForwardY,
        static_cast<double>(DeltaTime) * 1000.0, WallFrameMs);
    ++FrameCount;
    if (++CsvBufferedFrames >= 120) FlushCsv();
}

void UNaiwaAutoTestComponent::AddCheck(const TSharedPtr<FJsonObject>& Result,
    TArray<TSharedPtr<FJsonValue>>& Checks, const FString& Name, bool bPass,
    const FString& Expected, const FString& Actual)
{
    TSharedPtr<FJsonObject> Check = MakeShared<FJsonObject>();
    Check->SetStringField(TEXT("name"), Name);
    Check->SetBoolField(TEXT("pass"), bPass);
    Check->SetStringField(TEXT("expected"), Expected);
    Check->SetStringField(TEXT("actual"), Actual);
    Checks.Add(MakeShared<FJsonValueObject>(Check));
    if (!bPass)
    {
        Result->SetBoolField(TEXT("pass"), false);
        bAllChecksPass = false;
        UE_LOG(LogTemp, Error, TEXT("NAIWA_AUTOTEST_CHECK_FAIL round=%d case=%s check=%s expected=%s actual=%s"),
            Round, *CaseName(), *Name, *Expected, *Actual);
    }
}

void UNaiwaAutoTestComponent::CompleteCase()
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Checks;
    Result->SetBoolField(TEXT("pass"), true);
    Result->SetNumberField(TEXT("round"), Round);
    Result->SetStringField(TEXT("case"), CaseName());
    Result->SetNumberField(TEXT("duration_s"), CaseSeconds);
    const FVector FinalLocation = Character->GetActorLocation();
    const double FinalSpeed = Character->GetVelocity().Size();
    const bool bGrounded = !Character->GetCharacterMovement()->IsFalling();
    const FString FinalState = Character->GetLocomotionState();
    const double CapsuleBottom = Character->GetCapsuleBottom();
    Result->SetObjectField(TEXT("start_position_cm"), VectorJson(Observation.StartLocation));
    Result->SetObjectField(TEXT("final_position_cm"), VectorJson(FinalLocation));
    Result->SetObjectField(TEXT("final_velocity_cm_s"), VectorJson(Character->GetVelocity()));
    Result->SetNumberField(TEXT("maximum_abs_y_cm"), Observation.MaxAbsY);
    Result->SetNumberField(TEXT("minimum_x_cm"), Observation.MinX);
    Result->SetNumberField(TEXT("maximum_x_cm"), Observation.MaxX);
    Result->SetNumberField(TEXT("maximum_speed_cm_s"), Observation.MaxSpeed);
    Result->SetNumberField(TEXT("maximum_airborne_s"), Observation.MaxAirSeconds);
    Result->SetNumberField(TEXT("first_landing_case_time_s"), Observation.FirstLandingTime);
    Result->SetNumberField(TEXT("jump_key_queued_case_time_s"), Observation.JumpIssuedCaseTime);
    Result->SetNumberField(TEXT("jump_callback_observed_case_time_s"), Observation.JumpInputAppliedCaseTime);
    Result->SetNumberField(TEXT("maximum_consecutive_state_mismatch_s"), Observation.MaxStateMismatchSeconds);
    Result->SetNumberField(TEXT("positive_motion_mesh_yaw_deg"), Observation.PositiveYaw);
    Result->SetNumberField(TEXT("negative_motion_mesh_yaw_deg"), Observation.NegativeYaw);
    Result->SetNumberField(TEXT("positive_motion_mesh_forward_y"), Observation.PositiveForwardY);
    Result->SetNumberField(TEXT("negative_motion_mesh_forward_y"), Observation.NegativeForwardY);
    Result->SetNumberField(TEXT("positive_motion_mesh_forward_x"), Observation.PositiveForwardX);
    Result->SetNumberField(TEXT("negative_motion_mesh_forward_x"), Observation.NegativeForwardX);
    Result->SetNumberField(TEXT("minimum_mesh_bounds_bottom_cm"), Observation.MinBoundsBottom);
    Result->SetNumberField(TEXT("maximum_mesh_bounds_bottom_cm"), Observation.MaxBoundsBottom);
    Result->SetNumberField(TEXT("final_mesh_bounds_bottom_cm"), Character->GetMeshBoundsBottom());
    Result->SetNumberField(TEXT("final_capsule_bottom_cm"), CapsuleBottom);
    Result->SetStringField(TEXT("final_anim_state"), FinalState);
    Result->SetArrayField(TEXT("state_events"), Observation.Events);
    AddCheck(Result, Checks, TEXT("plane_constraint"), Observation.MaxAbsY <= 0.1,
        TEXT("maximum abs(Y) <= 0.1 cm"), Scalar(Observation.MaxAbsY));
    const bool bAssetsAssigned = Character->GetMesh()->GetSkeletalMeshAsset() &&
        Character->IdleSequence && Character->WalkSequence && Character->JumpSequence;
    AddCheck(Result, Checks, TEXT("animation_assets_assigned"), bAssetsAssigned,
        TEXT("skeletal mesh and Idle/Walk/Jump assigned"), bAssetsAssigned ? TEXT("assigned") : TEXT("incomplete"));
    AddCheck(Result, Checks, TEXT("movement_drives_animation_state"),
        Observation.MaxStateMismatchSeconds <= 0.2 && FinalState != TEXT("NoAnimInstance"),
        TEXT("Idle/Walk/Jump matches actual movement, continuous mismatch <= 0.2 s"),
        Scalar(Observation.MaxStateMismatchSeconds) + TEXT(" s; final=") + FinalState);
    const UNaiwaAnimInstance* Anim = Cast<UNaiwaAnimInstance>(Character->GetMesh()->GetAnimInstance());
    const uint64 EvaluationCount = Anim ? Anim->GetPoseEvaluationCount() : 0;
    const UAnimSequence* ActiveMovingSequence = Anim ? Anim->GetProxyMovingSequence() : nullptr;
    Result->SetStringField(TEXT("native_proxy_moving_sequence"), ActiveMovingSequence ? ActiveMovingSequence->GetPathName() : TEXT("unassigned"));
    if (Character->RunSequence)
    {
        AddCheck(Result, Checks, TEXT("assigned_run_clip_is_actually_sampled"),
            ActiveMovingSequence == Character->RunSequence,
            Character->RunSequence->GetPathName(), ActiveMovingSequence ? ActiveMovingSequence->GetPathName() : TEXT("unassigned"));
    }
    AddCheck(Result, Checks, TEXT("native_proxy_evaluates_actual_pose"),
        EvaluationCount > Observation.PoseEvaluationsAtStart + 5,
        TEXT("native proxy Evaluate executes more than five times during the case"),
        FString::Printf(TEXT("evaluate_count_delta=%llu"), EvaluationCount - Observation.PoseEvaluationsAtStart));
    TArray<TSharedPtr<FJsonValue>> PoseEvidence;
    for (int32 StateIndex = 0; StateIndex < 3; ++StateIndex)
    {
        TSharedPtr<FJsonObject> Pose = MakeShared<FJsonObject>();
        Pose->SetStringField(TEXT("state"), StateIndex == 0 ? TEXT("Idle") : StateIndex == 1 ? TEXT("Walk") : TEXT("Jump"));
        Pose->SetBoolField(TEXT("state_pose_sampled"), !Observation.FirstStatePose[StateIndex].IsEmpty());
        Pose->SetNumberField(TEXT("maximum_bone_rotation_change_degrees"), Observation.MaxPoseDeltaDegrees[StateIndex]);
        Pose->SetNumberField(TEXT("maximum_bone_rotation_vs_reference_degrees"), Observation.MaxPoseVsReferenceDegrees[StateIndex]);
        PoseEvidence.Add(MakeShared<FJsonValueObject>(Pose));
    }
    Result->SetArrayField(TEXT("component_space_pose_evidence"), PoseEvidence);
    Result->SetNumberField(TEXT("jump_sample_time_max_s"), Observation.MaximumJumpSampleTime);
    Result->SetNumberField(TEXT("jump_hold_observed_s"), Observation.JumpHoldSeconds);
    Result->SetNumberField(TEXT("held_pose_maximum_bone_rotation_change_degrees"), Observation.MaxHeldPoseDeltaDegrees);
    Result->SetNumberField(TEXT("move_mapping_callback_count"), Character->GetMoveInputEvents() - Observation.MoveEventsAtStart);
    Result->SetNumberField(TEXT("jump_mapping_callback_count"), Character->GetJumpInputEvents() - Observation.JumpEventsAtStart);
    Result->SetNumberField(TEXT("pause_mapping_callback_count"), Character->GetPauseInputEvents() - Observation.PauseEventsAtStart);
    Result->SetNumberField(TEXT("reset_mapping_callback_count"), Character->GetResetInputEvents() - Observation.ResetEventsAtStart);

    const auto CheckGround = [&](double SurfaceZ)
    {
        AddCheck(Result, Checks, TEXT("land_on_expected_surface"), bGrounded &&
            FMath::Abs(CapsuleBottom - SurfaceZ) <= 3.0,
            FString::Printf(TEXT("grounded and capsule bottom within 3 cm of Z=%.1f (includes UE floor clearance)"), SurfaceZ),
            FString::Printf(TEXT("grounded=%d bottom=%.4f"), bGrounded ? 1 : 0, CapsuleBottom));
    };
    const auto CheckStopped = [&]()
    {
        AddCheck(Result, Checks, TEXT("stop_and_idle"), FinalSpeed < 1.0 && FinalState == TEXT("Idle"),
            TEXT("final speed < 1 cm/s and Idle"), Scalar(FinalSpeed) + TEXT(" cm/s; ") + FinalState);
    };
    const auto CheckAirCycle = [&]()
    {
        AddCheck(Result, Checks, TEXT("actual_airborne_and_landing"), Observation.bSawAirborne &&
            Observation.bSawLanding && Observation.bSawJumpState,
            TEXT("real falling -> grounded transition with Jump observed while falling"),
            FString::Printf(TEXT("airborne=%d landed=%d jump=%d"), Observation.bSawAirborne,
                Observation.bSawLanding, Observation.bSawJumpState));
    };

    switch (CurrentCase)
    {
    case ECase::FlatStartStop:
        AddCheck(Result, Checks, TEXT("move_mapping_and_dynamic_poses"),
            Character->GetMoveInputEvents() > Observation.MoveEventsAtStart &&
            Observation.MaxPoseDeltaDegrees[0] > 0.05 && Observation.MaxPoseDeltaDegrees[1] > 1.0,
            TEXT("D invokes Enhanced Input callback; actual Idle and Walk bone poses change > 0.05 and 1 deg"),
            FString::Printf(TEXT("move_callbacks=%d idle_delta=%.4f walk_delta=%.4f"),
                Character->GetMoveInputEvents() - Observation.MoveEventsAtStart,
                Observation.MaxPoseDeltaDegrees[0], Observation.MaxPoseDeltaDegrees[1]));
        Result->SetObjectField(TEXT("actual_cpu_skinned_surface"), MeasureSkinnedVertices());
        AddCheck(Result, Checks, TEXT("move_more_than_100_cm"),
            Observation.MaxX - Observation.StartLocation.X > 100.0 && Observation.bSawWalkState,
            TEXT("positive displacement > 100 cm and Walk observed"),
            Scalar(Observation.MaxX - Observation.StartLocation.X));
        CheckGround(0.0); CheckStopped();
        break;
    case ECase::Reverse:
    {
        const double YawChange = FMath::Abs(FMath::FindDeltaAngleDegrees(
            Observation.PositiveYaw, Observation.NegativeYaw));
        const double FacingDot = Observation.PositiveForwardX * Observation.NegativeForwardX +
            Observation.PositiveForwardY * Observation.NegativeForwardY;
        AddCheck(Result, Checks, TEXT("reverse_motion_and_mesh_facing"), Observation.bSawPositiveSpeed &&
            Observation.bSawNegativeSpeed && YawChange >= 170.0 && FacingDot < -0.9,
            TEXT("both signed velocities observed; mesh yaw reverses >= 170 deg and facing XY dot < -0.9"),
            FString::Printf(TEXT("positive=%d negative=%d yaw_delta=%.3f facing_dot=%.4f forward_Y=(%.4f,%.4f)"),
                Observation.bSawPositiveSpeed, Observation.bSawNegativeSpeed, YawChange, FacingDot,
                Observation.PositiveForwardY, Observation.NegativeForwardY));
        CheckGround(0.0); CheckStopped();
        break;
    }
    case ECase::Wall:
    {
        const double ExpectedCenterX = 860.0 - Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
        AddCheck(Result, Checks, TEXT("wall_blocks_capsule"),
            Observation.MaxX <= ExpectedCenterX + 0.5 && Observation.MaxX <= 825.0 &&
            Observation.RightWallFinalX >= ExpectedCenterX - 4.0 && FMath::Abs(Character->GetVelocity().X) < 1.0,
            TEXT("reach right wall X=860 minus capsule radius, no penetration; actor X <= 825"),
            FString::Printf(TEXT("expected_center_x=%.3f max_x=%.3f right_wall_x=%.3f"),
                ExpectedCenterX, Observation.MaxX, Observation.RightWallFinalX));
        const double LeftCenterX = -660.0 + Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
        AddCheck(Result, Checks, TEXT("left_wall_blocks_capsule"),
            Observation.bLeftWallPlaced && Observation.MinX >= LeftCenterX - 0.5 &&
            FinalLocation.X <= LeftCenterX + 4.0 && FMath::Abs(Character->GetVelocity().X) < 1.0,
            TEXT("reach left wall X=-660 plus capsule radius, no penetration and abs(Vx) < 1"),
            FString::Printf(TEXT("expected_center_x=%.3f min_x=%.3f final_x=%.3f vx=%.3f"),
                LeftCenterX, Observation.MinX, FinalLocation.X, Character->GetVelocity().X));
        CheckGround(0.0);
        break;
    }
    case ECase::PlatformJump:
        AddCheck(Result, Checks, TEXT("jump_mapping_and_actual_jump_pose"),
            Character->GetJumpInputEvents() > Observation.JumpEventsAtStart && Observation.MaxPoseVsReferenceDegrees[2] > 1.0,
            TEXT("Space key invokes Enhanced Input Jump callback and actual airborne bone pose differs from reference > 1 deg"),
            FString::Printf(TEXT("jump_callbacks=%d jump_vs_reference=%.4f"),
                Character->GetJumpInputEvents() - Observation.JumpEventsAtStart, Observation.MaxPoseVsReferenceDegrees[2]));
        AddCheck(Result, Checks, TEXT("jump_issued_from_ground"), Observation.bJumpIssued &&
            Observation.bGroundedBeforeAirborne && Observation.MaxZAfterJump > 320.0,
            TEXT("Jump issued from ground X=1400; post-jump actor peak Z > 320 cm"),
            FString::Printf(TEXT("issued=%d post_jump_peak_z=%.3f"), Observation.bJumpIssued,
                Observation.bJumpIssued ? Observation.MaxZAfterJump : -1.0));
        AddCheck(Result, Checks, TEXT("jump_onto_platform_interior"),
            FinalLocation.X >= 1500.0 + Character->GetCapsuleComponent()->GetScaledCapsuleRadius() &&
            FinalLocation.X <= 1900.0 - Character->GetCapsuleComponent()->GetScaledCapsuleRadius(),
            TEXT("landed center lies fully inside platform X=1500..1900 allowing capsule radius"), Scalar(FinalLocation.X));
        CheckAirCycle(); CheckGround(180.0); CheckStopped();
        break;
    case ECase::Drop:
        AddCheck(Result, Checks, TEXT("walk_off_platform_edge"), Observation.bGroundedBeforeAirborne &&
            FinalLocation.X > 2542.0,
            TEXT("grounded on platform before walking off; final X > 2542 cm"), Scalar(FinalLocation.X));
        CheckAirCycle(); CheckGround(0.0); CheckStopped();
        break;
    case ECase::LongAir:
        AddCheck(Result, Checks, TEXT("long_air_holds_actual_jump_pose"),
            Observation.JumpHoldSeconds >= 0.2 && Observation.MaxHeldPoseDeltaDegrees <= 0.05,
            TEXT("Jump sample reaches hold time for >= 0.2 s; actual component-space bone pose drift <= 0.05 deg while held"),
            FString::Printf(TEXT("hold_s=%.4f drift_deg=%.5f sample_max_s=%.4f"),
                Observation.JumpHoldSeconds, Observation.MaxHeldPoseDeltaDegrees, Observation.MaximumJumpSampleTime));
        AddCheck(Result, Checks, TEXT("sustained_airborne"), Observation.MaxAirSeconds >= 0.8,
            TEXT("actual uninterrupted falling >= 0.8 s"), Scalar(Observation.MaxAirSeconds));
        CheckAirCycle(); CheckGround(0.0); CheckStopped();
        break;
    case ECase::EarlyLanding:
        AddCheck(Result, Checks, TEXT("early_landing_timing"), Observation.bJumpIssued &&
            Observation.JumpInputAppliedCaseTime >= 0.0 &&
            Observation.FirstLandingTime > Observation.JumpInputAppliedCaseTime && Character->JumpSequence &&
            Observation.FirstLandingTime - Observation.JumpInputAppliedCaseTime < Character->JumpSequence->GetPlayLength(),
            TEXT("jump from ground onto 260 cm platform; measured from actual jump callback delivery, first landing occurs before source Jump clip duration"),
            FString::Printf(TEXT("jump_to_first_landing_s=%.5f source_clip_s=%.5f"),
                Observation.FirstLandingTime - Observation.JumpInputAppliedCaseTime,
                Character->JumpSequence ? Character->JumpSequence->GetPlayLength() : 0.0));
        AddCheck(Result, Checks, TEXT("jump_again_after_early_landing"),
            Observation.bSecondJumpIssued && Observation.ObservedLandings >= 2 &&
            Character->GetJumpInputEvents() - Observation.JumpEventsAtStart == 2,
            TEXT("second Space callback after real landing causes a second complete airborne/landing cycle"),
            FString::Printf(TEXT("second_jump_issued=%d landing_count=%d jump_callbacks=%d"),
                Observation.bSecondJumpIssued, Observation.ObservedLandings,
                Character->GetJumpInputEvents() - Observation.JumpEventsAtStart));
        CheckAirCycle(); CheckGround(260.0); CheckStopped();
        break;
    case ECase::Reset:
        AddCheck(Result, Checks, TEXT("pause_resume_and_reset_keyboard_mappings"),
            Observation.bPauseObserved && Observation.bResumeObserved && Observation.bResetWhilePaused &&
            Observation.MaxPausedDisplacement <= 0.1 &&
            Character->GetPauseInputEvents() - Observation.PauseEventsAtStart == 3 &&
            Character->GetResetInputEvents() - Observation.ResetEventsAtStart == 1 &&
            !UGameplayStatics::IsGamePaused(this),
            TEXT("P pauses, second P resumes, third P pauses; paused actor moves <= 0.1 cm; R resets and resumes via Enhanced Input"),
            FString::Printf(TEXT("paused=%d resumed=%d reset_while_paused=%d pause_callbacks=%d reset_callbacks=%d paused_displacement=%.6f final_paused=%d"),
                Observation.bPauseObserved, Observation.bResumeObserved, Observation.bResetWhilePaused,
                Character->GetPauseInputEvents() - Observation.PauseEventsAtStart,
                Character->GetResetInputEvents() - Observation.ResetEventsAtStart,
                Observation.MaxPausedDisplacement, UGameplayStatics::IsGamePaused(this)));
        AddCheck(Result, Checks, TEXT("reset_restarts_native_animation_clock"),
            Anim && Anim->GetResetGeneration() > Observation.AnimResetGenerationAtStart &&
            Observation.bResetClockApplied && Observation.FirstResetClockMaximumTime <= 0.1,
            TEXT("worker proxy applies new reset generation and all three sample clocks are <= 0.1 s on first observed reset evaluation"),
            FString::Printf(TEXT("before=%u after=%u proxy_applied=%d first_max_sample_time_s=%.6f"),
                Observation.AnimResetGenerationAtStart, Anim ? Anim->GetResetGeneration() : 0,
                Observation.bResetClockApplied, Observation.FirstResetClockMaximumTime));
        Result->SetObjectField(TEXT("before_reset_position_cm"), VectorJson(Observation.BeforeResetLocation));
        Result->SetObjectField(TEXT("immediate_reset_position_cm"), VectorJson(Observation.ImmediateResetLocation));
        Result->SetObjectField(TEXT("expected_spawn_position_cm"), VectorJson(SpawnLocation));
        Result->SetNumberField(TEXT("immediate_reset_speed_cm_s"), Observation.ImmediateResetSpeed);
        AddCheck(Result, Checks, TEXT("reset_returns_to_spawn"),
            FVector::Distance(Observation.ImmediateResetLocation, SpawnLocation) <= 0.1 &&
            Observation.ImmediateResetSpeed < 1.0 && FMath::Abs(FinalLocation.X - SpawnLocation.X) <= 0.1,
            TEXT("immediate reset within 0.1 cm of spawn and speed < 1; no residual horizontal movement"),
            FString::Printf(TEXT("immediate_error=%.6f immediate_speed=%.6f final_x_error=%.6f"),
                FVector::Distance(Observation.ImmediateResetLocation, SpawnLocation),
                Observation.ImmediateResetSpeed, FMath::Abs(FinalLocation.X - SpawnLocation.X)));
        if (!bHaveResetReference)
        {
            ResetReference = FinalLocation;
            bHaveResetReference = true;
        }
        AddCheck(Result, Checks, TEXT("three_round_reset_consistency"),
            FVector::Distance(FinalLocation, ResetReference) <= 0.1,
            TEXT("settled reset position within 0.1 cm of round 1 reset"),
            Scalar(FVector::Distance(FinalLocation, ResetReference)));
        CheckGround(0.0); CheckStopped();
        break;
    default: break;
    }
    Result->SetArrayField(TEXT("checks"), Checks);
    const FString ScreenshotPath = FPaths::Combine(OutputDirectory,
        FString::Printf(TEXT("round_%02d_%02d_%s.png"), Round, static_cast<int32>(CurrentCase) + 1, *CaseName()));
    Result->SetStringField(TEXT("screenshot_path"), ScreenshotPath);
    Result->SetBoolField(TEXT("screenshot_requested"), true);
    FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
    Results.Add(MakeShared<FJsonValueObject>(Result));
    // Preserve the assertions above, then recover the fixture even if a pause/reset mapping failed.
    // Without this recovery a failed mapping would leave the next-case timer permanently paused.
    UGameplayStatics::SetGamePaused(this, false);
    ReleaseKeys();
    Character->StopJumping();
    FlushCsv();
    WriteSummary(false);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_AUTOTEST_CASE_COMPLETE round=%d case=%s result=%s"),
        Round, *CaseName(), Result->GetBoolField(TEXT("pass")) ? TEXT("PASS") : TEXT("FAIL"));
    bBetweenCases = true;
    GapSeconds = 0.0;
}

void UNaiwaAutoTestComponent::FlushCsv()
{
    if (!CsvBuffer.IsEmpty())
    {
        bOutputWriteFailed |= !FFileHelper::SaveStringToFile(CsvBuffer, *CsvPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
        CsvBuffer.Reset();
        CsvBufferedFrames = 0;
    }
    if (!BoneCsvBuffer.IsEmpty())
    {
        bOutputWriteFailed |= !FFileHelper::SaveStringToFile(BoneCsvBuffer,
            *FPaths::Combine(OutputDirectory, TEXT("bone_poses.csv")),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
        BoneCsvBuffer.Reset();
    }
    if (!MorphCsvBuffer.IsEmpty())
    {
        bOutputWriteFailed |= !FFileHelper::SaveStringToFile(MorphCsvBuffer,
            *FPaths::Combine(OutputDirectory, TEXT("morph_weights.csv")),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
        MorphCsvBuffer.Reset();
    }
}

void UNaiwaAutoTestComponent::WriteSummary(bool bRunComplete)
{
    bool bScreenshotsSaved = Results.Num() == 24;
    for (const TSharedPtr<FJsonValue>& Value : Results)
    {
        const TSharedPtr<FJsonObject> Result = Value->AsObject();
        const bool bSaved = IFileManager::Get().FileSize(*Result->GetStringField(TEXT("screenshot_path"))) > 0;
        Result->SetBoolField(TEXT("screenshot_file_exists"), bSaved);
        bScreenshotsSaved &= bSaved;
    }
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("run_id"), RunId);
    Root->SetStringField(TEXT("created_utc"), FDateTime::UtcNow().ToIso8601());
    Root->SetStringField(TEXT("status"), bRunComplete
        ? (bAllChecksPass && bScreenshotsSaved && !bOutputWriteFailed ? TEXT("PASS") : TEXT("FAIL"))
        : TEXT("INCOMPLETE"));
    Root->SetBoolField(TEXT("complete"), bRunComplete);
    Root->SetBoolField(TEXT("all_runtime_checks_pass"), bAllChecksPass);
    Root->SetBoolField(TEXT("all_24_screenshots_saved"), bScreenshotsSaved);
    Root->SetBoolField(TEXT("output_write_failed"), bOutputWriteFailed);
    Root->SetNumberField(TEXT("planned_rounds"), 3);
    Root->SetNumberField(TEXT("planned_cases"), 24);
    Root->SetNumberField(TEXT("completed_cases"), Results.Num());
    Root->SetNumberField(TEXT("sampled_frames"), static_cast<double>(FrameCount));
    Root->SetNumberField(TEXT("simulation_duration_s"), RunSeconds);
    Root->SetStringField(TEXT("csv_path"), CsvPath);
    Root->SetStringField(TEXT("output_directory"), OutputDirectory);
    Root->SetStringField(TEXT("bounds_measurement_limit"),
        TEXT("mesh_bounds_bottom_cm is component axis-aligned bounds origin minus Z extent. It is a coarse estimate, not the exact posed mesh foot sole or visual ground-contact measurement. Capsule floor clearance is allowed up to 3 cm."));
    Root->SetStringField(TEXT("timing_measurement_limit"),
        TEXT("frame_ms is component simulation DeltaTime; wall_frame_ms is elapsed wall time between component ticks. This scripted editor/game run is not a stable performance baseline; camera motion, screenshots, warm-up, caps and editor overhead affect timing."));
    Root->SetStringField(TEXT("animation_evidence_limit"),
        TEXT("State, native proxy evaluation count and actual component-space bone rotation traces prove sampled Idle/Walk/Jump poses. Screenshots and manual review are still required for likeness, transitions, normals, shadows and perceptual pose quality."));
    Root->SetStringField(TEXT("input_evidence_limit"),
        TEXT("A/D movement, Jump, Pause and Reset use simulated PlayerController InputKey events through the saved keyboard Enhanced Input mappings. During -NaiwaAutoTest only, viewport OS input is ignored and focus-loss key flushing is disabled to prevent contamination; settings are restored on completion/end. Fixture placement uses teleport/ResetToSpawn. Tests prove the mapping-to-callback-to-gameplay pipeline, not physical hardware or pause UI. Shift gait switching is validated separately."));
    Root->SetArrayField(TEXT("performance_samples"), PerformanceSamples);
    Root->SetStringField(TEXT("performance_limit"),
        TEXT("Preliminary 1080p editor-game baseline only. Each mesh-visible/mesh-hidden-and-animation-tick-disabled group discards 2 s warmup then samples 5 s wall frame/Game/Render/GPU time. Screenshots and CPU skin extraction are outside these intervals. Background applications, shader compilation, VSync, frame caps, quality and driver affect values. GPU zero is unavailable, not zero cost."));
    Root->SetStringField(TEXT("continuous_recording_limit"),
        TEXT("Optional frame sequence requests screenshots no faster than 8 fps in wall time. Viewport readback remains synchronous; PNG compression and disk writing use background workers and are drained before final manifest. Case screenshots take priority and skip the recording request for that tick. Actual wall timestamps and file existence are recorded. A resampled video is a review aid, not a timing measurement."));
    Root->SetArrayField(TEXT("recorded_frames"), RecordedFrames);
    for (const TSharedPtr<FJsonValue>& Value : RecordedFrames)
    {
        const TSharedPtr<FJsonObject> Frame = Value->AsObject();
        Frame->SetBoolField(TEXT("file_exists"), IFileManager::Get().FileSize(*Frame->GetStringField(TEXT("path"))) > 0);
    }
    Root->SetArrayField(TEXT("cases"), Results);
    FString Json;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
    {
        bOutputWriteFailed = true;
        return;
    }
    bOutputWriteFailed |= !FFileHelper::SaveStringToFile(Json,
        *FPaths::Combine(OutputDirectory, TEXT("summary.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (bRunComplete)
    {
        bOutputWriteFailed |= !FFileHelper::SaveStringToFile(Json,
            *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaTests"), TEXT("latest_summary.json")),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        if (!RecordedFrames.IsEmpty())
        {
            TSharedPtr<FJsonObject> Index = MakeShared<FJsonObject>();
            Index->SetStringField(TEXT("run_id"), RunId);
            Index->SetArrayField(TEXT("frames"), RecordedFrames);
            Index->SetStringField(TEXT("limit"), TEXT("Requested captures with actual wall-time stamps and saved-file existence. Missing files were dropped or superseded by case screenshots; fixed-fps video playback is not an actual timing measurement."));
            FString IndexJson;
            FJsonSerializer::Serialize(Index.ToSharedRef(), TJsonWriterFactory<>::Create(&IndexJson));
            bOutputWriteFailed |= !FFileHelper::SaveStringToFile(IndexJson,
                *FPaths::Combine(OutputDirectory, TEXT("Frames"), TEXT("frame_index.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        }
    }
}

void UNaiwaAutoTestComponent::FinishRun()
{
    ReleaseKeys();
    CompleteScreenshotWrites();
    RestoreInputIsolation();
    bComplete = true;
    Character->StopJumping();
    Character->DisableAutomatedInput();
    FlushCsv();
    WriteSummary(true);
    bool bScreenshotsSaved = Results.Num() == 24;
    for (const TSharedPtr<FJsonValue>& Value : Results)
    {
        bScreenshotsSaved &= Value->AsObject()->GetBoolField(TEXT("screenshot_file_exists"));
    }
    UE_LOG(LogTemp, Display, TEXT("NAIWA_AUTOTEST_COMPLETE result=%s cases=%d frames=%lld summary=%s"),
        bAllChecksPass && bScreenshotsSaved && !bOutputWriteFailed ? TEXT("PASS") : TEXT("FAIL"),
        Results.Num(), static_cast<long long>(FrameCount), *FPaths::Combine(OutputDirectory, TEXT("summary.json")));
}

void UNaiwaAutoTestComponent::SetKey(const FKey& Key, bool bDown)
{
    if (!PlayerController.IsValid() || HeldKeys.Contains(Key) == bDown) return;
    const FInputKeyEventArgs Event = FInputKeyEventArgs::CreateSimulated(Key,
        bDown ? IE_Pressed : IE_Released, bDown ? 1.0f : 0.0f);
    PlayerController->InputKey(Event);
    if (bDown) HeldKeys.Add(Key);
    else HeldKeys.Remove(Key);
}

void UNaiwaAutoTestComponent::ReleaseKeys()
{
    const TArray<FKey> Keys = HeldKeys.Array();
    for (const FKey& Key : Keys) SetKey(Key, false);
}

void UNaiwaAutoTestComponent::ObservePose(float DeltaTime)
{
    USkeletalMeshComponent* Mesh = Character->GetMesh();
    const UNaiwaAnimInstance* Anim = Cast<UNaiwaAnimInstance>(Mesh->GetAnimInstance());
    if (!Anim || !Mesh->GetSkeletalMeshAsset()) return;
    const int32 State = static_cast<int32>(Anim->LocomotionState);
    for (const UMorphTarget* Morph : Mesh->GetSkeletalMeshAsset()->GetMorphTargets())
    {
        if (!Morph) continue;
        const FName Name = Morph->GetFName();
        const int32* Index = Mesh->GetSkeletalMeshAsset()->GetMorphTargetIndexMap().Find(Name);
        const float FinalWeight = Index && Mesh->MorphTargetWeights.IsValidIndex(*Index) ? Mesh->MorphTargetWeights[*Index] : 0.0f;
        MorphCsvBuffer += FString::Printf(TEXT("%.6f,%d,%s,%.6f,%s,%.6f,%s,%.6f,%.6f,%.6f\n"),
            RunSeconds, Round, *CaseName(), CaseSeconds, *Character->GetLocomotionState(),
            Anim->GetAnimationSampleTime(ENaiwaLocomotionState::Walk), *Name.ToString(),
            Anim->GetCurveValue(Name), Mesh->GetMorphTarget(Name), FinalWeight);
    }
    const TArray<FTransform>& Pose = Mesh->GetComponentSpaceTransforms();
    const FReferenceSkeleton& Skeleton = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    TArray<FTransform> Reference = Skeleton.GetRefBonePose();
    for (int32 Bone = 0; Bone < Reference.Num(); ++Bone)
    {
        const int32 Parent = Skeleton.GetParentIndex(Bone);
        if (Parent >= 0) Reference[Bone] = Reference[Bone] * Reference[Parent];
    }
    if (Observation.FirstStatePose[State].IsEmpty()) Observation.FirstStatePose[State] = Pose;
    const float JumpTime = Anim->GetAnimationSampleTime(ENaiwaLocomotionState::Jump);
    Observation.MaximumJumpSampleTime = FMath::Max(Observation.MaximumJumpSampleTime, static_cast<double>(JumpTime));
    const bool bHeld = State == 2 && Character->JumpSequence &&
        JumpTime >= Character->JumpSequence->GetPlayLength() * Anim->JumpAirHoldFraction - 0.001f;
    if (bHeld)
    {
        Observation.JumpHoldSeconds += DeltaTime;
        if (Observation.FirstHeldPose.IsEmpty()) Observation.FirstHeldPose = Pose;
    }
    for (int32 Bone = 0; Bone < Pose.Num() && Bone < Reference.Num(); ++Bone)
    {
        const FQuat Rotation = Pose[Bone].GetRotation();
        const auto DegreesBetween = [&](const FTransform& Other)
        {
            return FMath::RadiansToDegrees(Rotation.AngularDistance(Other.GetRotation()));
        };
        if (Observation.FirstStatePose[State].IsValidIndex(Bone))
            Observation.MaxPoseDeltaDegrees[State] = FMath::Max(Observation.MaxPoseDeltaDegrees[State],
                DegreesBetween(Observation.FirstStatePose[State][Bone]));
        Observation.MaxPoseVsReferenceDegrees[State] = FMath::Max(Observation.MaxPoseVsReferenceDegrees[State], DegreesBetween(Reference[Bone]));
        if (bHeld && Observation.FirstHeldPose.IsValidIndex(Bone))
            Observation.MaxHeldPoseDeltaDegrees = FMath::Max(Observation.MaxHeldPoseDeltaDegrees,
                DegreesBetween(Observation.FirstHeldPose[Bone]));
        const FVector Position = Pose[Bone].GetLocation();
        BoneCsvBuffer += FString::Printf(TEXT("%.6f,%d,%s,%.6f,%s,%s,%.5f,%.5f,%.5f,%.7f,%.7f,%.7f,%.7f,%.6f,%llu\n"),
            RunSeconds, Round, *CaseName(), CaseSeconds, *Character->GetLocomotionState(),
            *Skeleton.GetBoneName(Bone).ToString(), Position.X, Position.Y, Position.Z,
            Rotation.X, Rotation.Y, Rotation.Z, Rotation.W, JumpTime, Anim->GetPoseEvaluationCount());
    }
}

TSharedPtr<FJsonObject> UNaiwaAutoTestComponent::MeasureSkinnedVertices() const
{
    TSharedPtr<FJsonObject> Evidence = MakeShared<FJsonObject>();
    Evidence->SetStringField(TEXT("method"), TEXT("USkinnedMeshComponent::GetCPUSkinnedVertices LOD0, each posed vertex transformed into world coordinates, minimum world Z across all mesh sections"));
    Evidence->SetStringField(TEXT("limit"), TEXT("This is the lowest actual posed mesh vertex, not component bounds or a foot-bone position. A visual check must confirm the lowest region belongs to the feet and judge contact/shadows. CPU skin extraction stalls rendering and is excluded from performance samples."));
    if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
    {
        Evidence->SetBoolField(TEXT("measured"), false);
        Evidence->SetStringField(TEXT("reason"), TEXT("CPU skin extraction requires a render mesh object; NullRHI run skipped"));
        return Evidence;
    }
    TArray<FFinalSkinVertex> Vertices;
    Character->GetMesh()->GetCPUSkinnedVertices(Vertices, 0);
    double MinimumZ = TNumericLimits<double>::Max();
    FVector MinimumVertex = FVector::ZeroVector;
    for (const FFinalSkinVertex& Vertex : Vertices)
    {
        const FVector World = Character->GetMesh()->GetComponentTransform().TransformPosition(FVector(Vertex.Position));
        if (World.Z < MinimumZ) { MinimumZ = World.Z; MinimumVertex = World; }
    }
    Evidence->SetBoolField(TEXT("measured"), !Vertices.IsEmpty());
    Evidence->SetNumberField(TEXT("posed_vertices"), Vertices.Num());
    if (!Vertices.IsEmpty())
    {
        Evidence->SetNumberField(TEXT("minimum_world_z_cm"), MinimumZ);
        Evidence->SetObjectField(TEXT("minimum_vertex_world_cm"), VectorJson(MinimumVertex));
        Evidence->SetNumberField(TEXT("graybox_ground_surface_z_cm"), 0.0);
        Evidence->SetNumberField(TEXT("minimum_vertex_ground_gap_cm"), MinimumZ);
        Evidence->SetBoolField(TEXT("within_3_cm_of_ground"), FMath::Abs(MinimumZ) <= 3.0);
    }
    return Evidence;
}

void UNaiwaAutoTestComponent::TickPerformance(double WallDeltaSeconds)
{
    // Use the same flat-ground motion in both phases; visible includes the Run and belly curves.
    const double X = Character->GetActorLocation().X;
    if (X >= 350.0) PerformanceDirection = -1;
    else if (X <= -450.0) PerformanceDirection = 1;
    SetKey(EKeys::D, PerformanceDirection > 0);
    SetKey(EKeys::A, PerformanceDirection < 0);
    PerformanceSeconds += WallDeltaSeconds;
    if (PerformanceSeconds >= PerformanceWarmup && PerformanceSeconds < PerformanceWarmup + PerformanceSampleDuration)
    {
        PerfWallMs.Add(WallFrameMs);
        PerfGameMs.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));
        PerfRenderMs.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime));
        PerfGpuMs.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
    }
    if (PerformanceSeconds < PerformanceWarmup + PerformanceSampleDuration) return;
    TSharedPtr<FJsonObject> Group = MakeShared<FJsonObject>();
    Group->SetStringField(TEXT("mode"), PerformancePhase == 0 ? TEXT("mesh_visible_animation_enabled") : TEXT("mesh_hidden_animation_tick_disabled"));
    Group->SetNumberField(TEXT("warmup_s"), PerformanceWarmup);
    Group->SetNumberField(TEXT("sample_window_s"), PerformanceSampleDuration);
    Group->SetNumberField(TEXT("sampled_frames"), PerfWallMs.Num());
    Group->SetStringField(TEXT("motion"), TEXT("Actual EnhancedInput A/D running on flat ground, turn at X=-450/350 cm; identical path policy in both phases"));
    const auto Mean = [](const TArray<double>& Values)
    {
        double Total = 0.0;
        for (double Value : Values) Total += Value;
        return Values.IsEmpty() ? 0.0 : Total / Values.Num();
    };
    Group->SetNumberField(TEXT("mean_wall_frame_ms"), Mean(PerfWallMs));
    Group->SetNumberField(TEXT("mean_game_ms"), Mean(PerfGameMs));
    Group->SetNumberField(TEXT("mean_render_ms"), Mean(PerfRenderMs));
    Group->SetNumberField(TEXT("mean_gpu_ms"), Mean(PerfGpuMs));
    Group->SetBoolField(TEXT("gpu_timer_available"), Mean(PerfGpuMs) > 0.0);
    PerfWallMs.Sort();
    if (!PerfWallMs.IsEmpty())
    {
        Group->SetNumberField(TEXT("p95_wall_frame_ms"), PerfWallMs[FMath::Min(PerfWallMs.Num() - 1, FMath::FloorToInt(PerfWallMs.Num() * 0.95))]);
        Group->SetNumberField(TEXT("median_wall_frame_ms"), PerfWallMs[PerfWallMs.Num() / 2]);
    }
    PerformanceSamples.Add(MakeShared<FJsonValueObject>(Group));
    UE_LOG(LogTemp, Display, TEXT("NAIWA_PERFORMANCE_GROUP mode=%s frames=%d mean_wall_ms=%.3f game_ms=%.3f render_ms=%.3f gpu_ms=%.3f"),
        *Group->GetStringField(TEXT("mode")), PerfWallMs.Num(), Mean(PerfWallMs), Mean(PerfGameMs), Mean(PerfRenderMs), Mean(PerfGpuMs));
    PerfWallMs.Reset(); PerfGameMs.Reset(); PerfRenderMs.Reset(); PerfGpuMs.Reset();
    PerformanceSeconds = 0.0;
    if (PerformancePhase == 0)
    {
        PerformancePhase = 1;
        ReleaseKeys();
        Character->ResetToSpawn();
        PerformanceDirection = 1;
        Character->GetMesh()->SetVisibility(false);
        Character->GetMesh()->SetComponentTickEnabled(false);
    }
    else
    {
        Character->GetMesh()->SetComponentTickEnabled(true);
        Character->GetMesh()->SetVisibility(true);
        FinishRun();
    }
}

void UNaiwaAutoTestComponent::RecordFrame()
{
    if (!bRecordFrames || FScreenshotRequest::IsScreenshotRequested()) return;
    const double Now = FPlatformTime::Seconds();
    if (Now - LastRecordWallTime < 0.125) return;
    int32 PendingWrites = 0;
    for (const TFuture<bool>& Write : ScreenshotWrites) PendingWrites += !Write.IsReady();
    if (PendingWrites >= 4) return; // Backpressure skips a request without allocating a frame number.
    const FString Directory = FPaths::Combine(OutputDirectory, TEXT("Frames"));
    if (RecordedFrames.IsEmpty())
    {
        IFileManager::Get().MakeDirectory(*Directory, true);
        FFileHelper::SaveStringToFile(TEXT("frame_name,wall_time_s\n"),
            *FPaths::Combine(Directory, TEXT("frame_times.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
    LastRecordWallTime = Now;
    const FString Name = FString::Printf(TEXT("frame_%06d.png"), RecordedFrames.Num());
    const FString Path = FPaths::Combine(Directory, Name);
    const double Time = Now - RecordWallStart;
    FScreenshotRequest::RequestScreenshot(Path, false, false);
    TSharedPtr<FJsonObject> Frame = MakeShared<FJsonObject>();
    Frame->SetStringField(TEXT("frame_name"), Name);
    Frame->SetStringField(TEXT("path"), Path);
    Frame->SetNumberField(TEXT("wall_time_s"), Time);
    Frame->SetNumberField(TEXT("round"), Round);
    Frame->SetStringField(TEXT("case"), CaseName());
    RecordedFrames.Add(MakeShared<FJsonValueObject>(Frame));
    FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s,%.6f\n"), *Name, Time),
        *FPaths::Combine(Directory, TEXT("frame_times.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

void UNaiwaAutoTestComponent::ConfigureInputIsolation()
{
    if (bComplete || bControllerInputIsolationActive || !Character.IsValid()) return;
    PlayerController = Cast<APlayerController>(Character->GetController());
    if (!PlayerController.IsValid()) return;
    bControllerFocusFlushWasEnabled = PlayerController->ShouldFlushKeysWhenViewportFocusChanges();
    PlayerController->SetInputMode(FNaiwaAutoTestInputMode());
    // FInputModeGameOnly::ApplyInputMode resets IgnoreInput(false), so isolate after applying it.
    if (TestViewport.IsValid()) TestViewport->SetIgnoreInput(true);
    PlayerController->FlushPressedKeys();
    bControllerInputIsolationActive = true;
    UE_LOG(LogTemp, Display, TEXT("NAIWA_AUTOTEST_INPUT_ISOLATED viewport OS input ignored; simulated PC InputKey retained"));
}

void UNaiwaAutoTestComponent::RestoreInputIsolation()
{
    if (bControllerInputIsolationActive && PlayerController.IsValid())
    {
        if (bControllerFocusFlushWasEnabled) PlayerController->SetInputMode(FInputModeGameOnly());
        else PlayerController->SetInputMode(FNaiwaAutoTestInputMode());
        bControllerInputIsolationActive = false;
    }
    if (bInputIsolationActive)
    {
        if (TestViewport.IsValid()) TestViewport->SetIgnoreInput(bViewportInputWasIgnored);
        GetMutableDefault<UInputSettings>()->bShouldFlushPressedKeysOnViewportFocusLost = bFocusFlushWasEnabled;
        bInputIsolationActive = false;
    }
}

void UNaiwaAutoTestComponent::HandleScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Bitmap)
{
    const FString Path = FScreenshotRequest::GetFilename();
    TArray<FColor> Pixels = Bitmap;
    ScreenshotWrites.Add(Async(EAsyncExecution::ThreadPool,
        [Width, Height, Path, Pixels = MoveTemp(Pixels)]()
        {
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Width, Height, MakeArrayView64(Pixels), Png);
            return !Png.IsEmpty() && FFileHelper::SaveArrayToFile(Png, *Path);
        }));
}

void UNaiwaAutoTestComponent::CompleteScreenshotWrites()
{
    if (ScreenshotDelegateHandle.IsValid())
    {
        UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotDelegateHandle);
        ScreenshotDelegateHandle.Reset();
    }
    for (TFuture<bool>& Write : ScreenshotWrites) bOutputWriteFailed |= !Write.Get();
    ScreenshotWrites.Reset();
}
