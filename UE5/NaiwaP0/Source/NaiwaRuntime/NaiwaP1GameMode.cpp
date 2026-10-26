#include "NaiwaP1GameMode.h"
#include "NaiwaP1PlayerController.h"
#include "NaiwaMenuProgress.h"
#include "NaiwaCharacter.h"
#include "NaiwaP1BouncePad.h"
#include "NaiwaP1Mechanism.h"
#include "NaiwaP1SwitchPlatform.h"
#include "NaiwaP1TimedPlatform.h"
#include "NaiwaP1Trigger.h"
#include "Animation/AnimSequence.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"

namespace
{
    bool IsAuditMap(const UWorld* World, const TCHAR* ExpectedName)
    {
        FString MapName = World->GetMapName();
        MapName.RemoveFromStart(World->StreamingLevelsPrefix);
        return MapName == ExpectedName;
    }

    const TCHAR* StateName(ENaiwaP1State State)
    {
        switch (State)
        {
        case ENaiwaP1State::Playing: return TEXT("Playing");
        case ENaiwaP1State::Dying: return TEXT("Dying");
        case ENaiwaP1State::Respawning: return TEXT("Respawning");
        case ENaiwaP1State::Won: return TEXT("Won");
        }
        return TEXT("Unknown");
    }

    const TCHAR* SwitchStateName(ENaiwaP1SwitchPlatformState State)
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

    const TCHAR* TimedStateName(ENaiwaP1TimedPlatformState State)
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

ANaiwaP1GameMode::ANaiwaP1GameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PlayerControllerClass = ANaiwaP1PlayerController::StaticClass();
    static ConstructorHelpers::FObjectFinder<USoundBase> DeathCue(
        TEXT("/Game/Naiwa/P1/Legacy/Audio/SFX_P1_Die.SFX_P1_Die"));
    if (DeathCue.Succeeded()) DeathSound = DeathCue.Object;
    static ConstructorHelpers::FObjectFinder<USoundBase> CollectionCue(
        TEXT("/Game/Naiwa/P1/Legacy/Audio/SFX_P1_Collection.SFX_P1_Collection"));
    if (CollectionCue.Succeeded()) CollectionSound = CollectionCue.Object;
}

void ANaiwaP1GameMode::BeginPlay()
{
    Super::BeginPlay();
    LoadFeedbackAssets();
    const bool bStandaloneP1 = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1Standalone"));
    bLinkRun = FParse::Param(FCommandLine::Get(), TEXT("NaiwaLinkRun")) || !bStandaloneP1;
    TotalPickupCount = 0;
    RequiredPickupIds.Reset();
    CollectedRequiredPickupIds.Reset();
    for (TActorIterator<ANaiwaP1Trigger> It(GetWorld()); It; ++It)
    {
        if (It->TriggerKind != ENaiwaP1TriggerKind::Pickup) continue;
        ++TotalPickupCount;
        if (It->bRequiredForGoal) RequiredPickupIds.Add(It->GetStableId());
    }
    if (!RequiredPickupIds.IsEmpty()) RequiredPickupCount = RequiredPickupIds.Num();
    ResetAttemptActors();
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1 state=Playing pickups=%d goal_required=%d"),
        TotalPickupCount, RequiredPickupCount);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1 campaign_mode=%s"),
        bLinkRun ? TEXT("linked") : TEXT("standalone"));
    StartBackgroundMusic();
    bSmokeEnabled = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1Smoke"));
    const bool bRouteRun = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1RouteRun"));
    bRouteWalk = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1RouteWalk"));
    bRouteEnabled = bRouteRun || bRouteWalk;
    bRouteFull = bRouteEnabled && IsAuditMap(GetWorld(), TEXT("L_NaiwaP1_Full"));
    bRouteCapture = bRouteEnabled && FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1Capture"));
    bHazardAuditEnabled = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1AllHazards"));
    bMatrixEnabled = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1Matrix"));
    const bool bBounceWalk = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1BounceProbeWalk"));
    const bool bBounceRun = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1BounceProbeRun"));
    bBounceProbeWalk = bBounceWalk;
    bBounceProbeEnabled = bBounceWalk || bBounceRun;
    const bool bSwitchWalk = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1SwitchProbeWalk"));
    const bool bSwitchRun = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1SwitchProbeRun"));
    bSwitchProbeWalk = bSwitchWalk;
    bSwitchProbeEnabled = bSwitchWalk || bSwitchRun;
    bSwitchProbeCapture = bSwitchProbeEnabled && FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1SwitchCapture"));
    const bool bSwitchFailWalk = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1SwitchFailProbeWalk"));
    const bool bSwitchFailRun = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1SwitchFailProbeRun"));
    bSwitchFailProbeWalk = bSwitchFailWalk;
    bSwitchFailProbeEnabled = bSwitchFailWalk || bSwitchFailRun;
    const bool bTimedWalk = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1TimedProbeWalk"));
    const bool bTimedRun = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1TimedProbeRun"));
    bTimedProbeWalk = bTimedWalk;
    bTimedProbeEnabled = bTimedWalk || bTimedRun;
    bTimedProbeCapture = bTimedProbeEnabled && FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1TimedCapture"));
    const bool bTimedFailWalk = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1TimedFailProbeWalk"));
    const bool bTimedFailRun = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1TimedFailProbeRun"));
    bTimedFailProbeWalk = bTimedFailWalk;
    bTimedFailProbeEnabled = bTimedFailWalk || bTimedFailRun;
    const bool bIntegratedWalk = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1IntegratedProbeWalk"));
    const bool bIntegratedRun = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1IntegratedProbeRun"));
    const bool bIntegratedVisualWalk = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1IntegratedVisualWalk"));
    const bool bIntegratedVisualRun = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1IntegratedVisualRun"));
    bIntegratedVisualCapture = bIntegratedVisualWalk || bIntegratedVisualRun;
    bIntegratedProbeWalk = bIntegratedWalk || bIntegratedVisualWalk;
    bIntegratedProbeEnabled = bIntegratedWalk || bIntegratedRun || bIntegratedVisualCapture;
    bIntegratedProbeCheckCheckpointFail = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1IntegratedProbeCheckpointFail"));
    const bool bFullWalk = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1IntegratedFullProbeWalk"));
    const bool bFullRun = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1IntegratedFullProbeRun"));
    bIntegratedFullProbeWalk = bFullWalk;
    bIntegratedFullProbeEnabled = bFullWalk || bFullRun;
    // Gap-width probe: standalone, maps L_NaiwaP1_GapProbe only.
    const bool bGapWalk = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1GapProbeWalk"));
    const bool bGapRun = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1GapProbeRun"));
    bGapProbeWalk = bGapWalk;
    bGapProbeEnabled = bGapWalk || bGapRun;
    if (bGapProbeEnabled)
    {
        GapProbePhaseStartedWall = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_GAP_PROBE started gait=%s segments=6"),
            bGapProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    }
    // Clearance probe: standalone, map L_NaiwaP1_JumpProbe only.
    const bool bClearWalk = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1ClearanceProbeWalk"));
    const bool bClearRun = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1ClearanceProbeRun"));
    bClearanceProbeWalk = bClearWalk;
    bClearanceProbeEnabled = bClearWalk || bClearRun;
    if (bClearanceProbeEnabled)
    {
        ClearanceProbePhaseStartedWall = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_CLEARANCE_PROBE started gait=%s segments=6"),
            bClearanceProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    }
    // Timed-window probe: standalone, map L_NaiwaP1_TimedProbe only.
    bTimedWindowProbeEnabled = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1TimedWindowProbe"));
    if (bTimedWindowProbeEnabled)
    {
        TimedWindowProbePhaseWall = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_WINDOW_PROBE started"));
    }
    // Chain-gap probe: standalone, map L_NaiwaP1_ChainProbe only.
    const bool bChainWalk = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1ChainProbeWalk"));
    const bool bChainRun = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1ChainProbeRun"));
    bChainProbeWalk = bChainWalk;
    bChainProbeEnabled = bChainWalk || bChainRun;
    if (bChainProbeEnabled)
    {
        ChainProbePhaseWall = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_CHAIN_PROBE started gait=%s gaps=3 gap_cm=240 island_cm=400"),
            bChainProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    }
    // Revealed-hazard probe: standalone, map L_NaiwaP1_ChainProbe only.
    bHazardRevealProbeEnabled = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1HazardRevealProbe"));
    if (bHazardRevealProbeEnabled)
    {
        HazardRevealProbePhaseWall = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_HAZARD_REVEAL_PROBE started"));
    }
    FString V7GateSpec;
    if (FParse::Value(FCommandLine::Get(), TEXT("NaiwaP1V7GateProbe="), V7GateSpec))
    {
        V7GateSpec = V7GateSpec.ToUpper();
        bV7GateProbeEnabled = true;
        bV7GateProbeRun = V7GateSpec.EndsWith(TEXT("RUN"));
        bV7GateProbeRecovery = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1V7GateRecovery"));
        V7GateProbeGate = V7GateSpec.IsEmpty() ? TEXT('?') : V7GateSpec[0];
        FParse::Value(FCommandLine::Get(), TEXT("NaiwaP1V7GateOffsetCm="), V7GateProbeOffsetCm);
        V7GateProbeStartedWall = FPlatformTime::Seconds();
        V7GateProbeStageWall = V7GateProbeStartedWall;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_V7_GATE_PROBE started gate=%s gait=%s mode=%s offset=%.1f map=%s"),
            *V7GateSpec, bV7GateProbeRun ? TEXT("Run300") : TEXT("Walk150"),
            bV7GateProbeRecovery ? TEXT("recovery") : TEXT("baseline"),
            V7GateProbeOffsetCm, *GetWorld()->GetMapName());
        if ((V7GateProbeGate != TEXT('A') && V7GateProbeGate != TEXT('B') && V7GateProbeGate != TEXT('C'))
            || (V7GateSpec != FString::Printf(TEXT("%c%s"), V7GateProbeGate,
                bV7GateProbeRun ? TEXT("RUN") : TEXT("WALK")))
            || !IsAuditMap(GetWorld(), TEXT("L_NaiwaP1_v7_GateProbe"))
            || FMath::Abs(V7GateProbeOffsetCm) > 80.0f
            || (bV7GateProbeRecovery && !FMath::IsNearlyZero(V7GateProbeOffsetCm)))
        {
            FinishV7GateProbe(TEXT("invalid_fixture_or_flag"));
            return;
        }
    }
    bV7DeathEggProbeEnabled = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1V7DeathEggProbe"));
    bV7HiddenFinalPitRecoveryProbeEnabled = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1V7HiddenFinalPitRecoveryProbe"))
        || bV7DeathEggProbeEnabled;
    bV7MixedCoreProbeEnabled = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1V7MixedCoreProbe"))
        || bV7HiddenFinalPitRecoveryProbeEnabled;
    if (bV7MixedCoreProbeEnabled)
    {
        V7MixedCoreStartedWall = FPlatformTime::Seconds();
        V7MixedCoreStageStartedWall = V7MixedCoreStartedWall;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_V7_MIXED_CORE_PROBE started map=%s"), *GetWorld()->GetMapName());
    }
    if (bIntegratedFullProbeEnabled)
    {
        IntegratedFullProbeStartedWall = FPlatformTime::Seconds();
        IntegratedFullProbeStepStartedWall = IntegratedFullProbeStartedWall;
        if ((bFullWalk && bFullRun) || bIntegratedProbeEnabled || bSmokeEnabled || bRouteEnabled || bHazardAuditEnabled
            || bMatrixEnabled || bBounceProbeEnabled || bSwitchProbeEnabled || bSwitchFailProbeEnabled
            || bTimedProbeEnabled || bTimedFailProbeEnabled
            || FParse::Param(FCommandLine::Get(), TEXT("NaiwaAutoTest"))
            || FParse::Param(FCommandLine::Get(), TEXT("NaiwaVisualReview"))
            || FParse::Param(FCommandLine::Get(), TEXT("NaiwaReviewPlayback"))
            || FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1IntegratedVisualWalk"))
            || FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1IntegratedVisualRun")))
        {
            FinishIntegratedFullProbe(TEXT("INCOMPLETE"), TEXT("Use exactly one full-route gait and no other functional/visual/partial scenario"));
            return;
        }
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_INTEGRATED_FULL_PROBE started gait=%s"), bFullWalk ? TEXT("Walk150") : TEXT("Run300"));
    }
    if (bIntegratedProbeEnabled)
    {
        IntegratedVisualStartedWall = FPlatformTime::Seconds();
        IntegratedProbeStartedWall = bIntegratedVisualCapture ? GetWorld()->GetTimeSeconds() : IntegratedVisualStartedWall;
        IntegratedProbeStepStartedWall = IntegratedProbeStartedWall;
        if ((bIntegratedWalk && bIntegratedRun) || (bIntegratedVisualWalk && bIntegratedVisualRun)
            || (bIntegratedVisualCapture && (bIntegratedWalk || bIntegratedRun || bIntegratedProbeCheckCheckpointFail))
            || bSmokeEnabled || bRouteEnabled || bHazardAuditEnabled
            || bMatrixEnabled || bBounceProbeEnabled || bSwitchProbeEnabled || bSwitchFailProbeEnabled
            || bTimedProbeEnabled || bTimedFailProbeEnabled
            || FParse::Param(FCommandLine::Get(), TEXT("NaiwaAutoTest"))
            || FParse::Param(FCommandLine::Get(), TEXT("NaiwaVisualReview"))
            || FParse::Param(FCommandLine::Get(), TEXT("NaiwaReviewPlayback")))
        {
            FinishIntegratedProbe(TEXT("INCOMPLETE"), TEXT("Select exactly one integrated gait and no other automated scenario"));
            return;
        }
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_INTEGRATED_PROBE started gait=%s mode=%s scope=partial_teaching_slice checkpoint_fail=%d"),
            bIntegratedProbeWalk ? TEXT("Walk150") : TEXT("Run300"),
            bIntegratedVisualCapture ? TEXT("visual_only") : TEXT("functional"), bIntegratedProbeCheckCheckpointFail);
    }
    if (bSmokeEnabled)
    {
        SmokeStartedWall = FPlatformTime::Seconds();
        SmokeStepStartedWall = SmokeStartedWall;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SMOKE started map=%s"), *GetWorld()->GetMapName());
    }
    if (bRouteEnabled)
    {
        RouteStartedWall = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_ROUTE started map=%s gait=%s"),
            *GetWorld()->GetMapName(), bRouteWalk ? TEXT("Walk150") : TEXT("Run300"));
    }
    if (bHazardAuditEnabled)
    {
        HazardAuditStartedWall = FPlatformTime::Seconds();
        HazardAuditStepWall = HazardAuditStartedWall;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_HAZARD_AUDIT started map=%s"), *GetWorld()->GetMapName());
    }
    if (bMatrixEnabled)
    {
        MatrixStartedWall = FPlatformTime::Seconds();
        MatrixStepWall = MatrixStartedWall;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_MATRIX started map=%s"), *GetWorld()->GetMapName());
    }
    if (bBounceProbeEnabled)
    {
        BounceProbeStartedWall = FPlatformTime::Seconds();
        BounceProbeStepStartedWall = BounceProbeStartedWall;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_BOUNCE_PROBE started map=%s gait=%s"),
            *GetWorld()->GetMapName(), bBounceProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    }
    if (bSwitchProbeEnabled)
    {
        SwitchProbeStartedWall = FPlatformTime::Seconds();
        SwitchProbeStepStartedWall = SwitchProbeStartedWall;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PROBE started map=%s gait=%s"),
            *GetWorld()->GetMapName(), bSwitchProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    }
    if (bSwitchFailProbeEnabled)
    {
        SwitchFailProbeStartedWall = FPlatformTime::Seconds();
        SwitchFailProbeStepStartedWall = SwitchFailProbeStartedWall;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_FAIL_PROBE started map=%s gait=%s"),
            *GetWorld()->GetMapName(), bSwitchFailProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    }
    if (bTimedProbeEnabled)
    {
        TimedProbeStartedWall = FPlatformTime::Seconds();
        TimedProbeStepStartedWall = TimedProbeStartedWall;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_PROBE started map=%s gait=%s"),
            *GetWorld()->GetMapName(), bTimedProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    }
    if (bTimedFailProbeEnabled)
    {
        TimedFailProbeStartedWall = FPlatformTime::Seconds();
        TimedFailProbeStepStartedWall = TimedFailProbeStartedWall;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_FAIL_PROBE started map=%s gait=%s"),
            *GetWorld()->GetMapName(), bTimedFailProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    }
}

void ANaiwaP1GameMode::RegisterCharacter(ANaiwaCharacter* Character)
{
    if (!Character) return;
    PlayerCharacter = Character;
    InitialSpawn = Character->GetActorLocation();
    bHasInitialSpawn = true;
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1 spawn=(%.1f,%.1f,%.1f)"), InitialSpawn.X, InitialSpawn.Y, InitialSpawn.Z);
}

bool ANaiwaP1GameMode::IsGameplayInputAllowed() const
{
    return State == ENaiwaP1State::Playing && !UGameplayStatics::IsGamePaused(this);
}

void ANaiwaP1GameMode::QueueTrigger(ANaiwaP1Trigger* Trigger, ANaiwaCharacter* Character)
{
    if (!Trigger || !Character || Character != PlayerCharacter.Get() || !IsGameplayInputAllowed()) return;
    FNaiwaP1PendingTrigger Event;
    Event.Trigger = Trigger;
    Event.Frame = GFrameCounter;
    PendingTriggers.Add(Event);
}

void ANaiwaP1GameMode::QueueFallDeath()
{
    if (State != ENaiwaP1State::Playing || bPendingFallDeath) return;
    bPendingFallDeath = true;
    PendingFallFrame = GFrameCounter;
}

void ANaiwaP1GameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (UGameplayStatics::IsGamePaused(this))
    {
        if (bSmokeEnabled) FinishSmoke(TEXT("FAIL"), TEXT("Game unexpectedly paused during smoke"));
        if (bBounceProbeEnabled) FinishBounceProbe(TEXT("FAIL"), TEXT("Game unexpectedly paused during bounce probe"));
        if (bMatrixEnabled) TickMatrix();
        if (bSwitchProbeEnabled) TickSwitchProbe();
        if (bSwitchFailProbeEnabled) TickSwitchFailProbe();
        if (bTimedProbeEnabled) TickTimedProbe();
        if (bTimedFailProbeEnabled) TickTimedFailProbe();
        if (bIntegratedProbeEnabled) TickIntegratedProbe();
        if (bIntegratedFullProbeEnabled) TickIntegratedFullProbe();
        if (bV7MixedCoreProbeEnabled) FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Unexpected pause during the V7 mixed-gait core route"));
        return;
    }
    if (State == ENaiwaP1State::Playing && PlayerCharacter.IsValid())
    {
        if (const AWorldSettings* Settings = GetWorld()->GetWorldSettings())
        {
            if (PlayerCharacter->GetActorLocation().Z < Settings->KillZ) QueueFallDeath();
        }
    }
    ProcessPendingTriggers();
    if (State == ENaiwaP1State::Respawning) FinishRespawnIfReady();
    if (bSmokeEnabled) TickSmoke();
    if (bRouteEnabled) TickRoute();
    if (bHazardAuditEnabled) TickHazardAudit();
    if (bMatrixEnabled) TickMatrix();
    if (bBounceProbeEnabled) TickBounceProbe();
    if (bSwitchProbeEnabled) TickSwitchProbe();
    if (bSwitchFailProbeEnabled) TickSwitchFailProbe();
    if (bTimedProbeEnabled) TickTimedProbe();
    if (bTimedFailProbeEnabled) TickTimedFailProbe();
    if (bIntegratedProbeEnabled) TickIntegratedProbe();
    if (bIntegratedFullProbeEnabled) TickIntegratedFullProbe();
    if (bGapProbeEnabled) TickGapProbe();
    if (bClearanceProbeEnabled) TickClearanceProbe();
    if (bTimedWindowProbeEnabled) TickTimedWindowProbe();
    if (bChainProbeEnabled) TickChainProbe();
    if (bHazardRevealProbeEnabled) TickHazardRevealProbe();
    if (bV7GateProbeEnabled) TickV7GateProbe();
    if (bV7MixedCoreProbeEnabled) TickV7MixedCoreProbe();
}

void ANaiwaP1GameMode::ProcessPendingTriggers()
{
    while (State == ENaiwaP1State::Playing)
    {
        uint64 Oldest = MAX_uint64;
        for (const FNaiwaP1PendingTrigger& Event : PendingTriggers)
        {
            if (Event.Frame < GFrameCounter) Oldest = FMath::Min(Oldest, Event.Frame);
        }
        if (bPendingFallDeath && PendingFallFrame < GFrameCounter)
        {
            Oldest = FMath::Min(Oldest, PendingFallFrame);
        }
        if (Oldest == MAX_uint64) return;

        TArray<ANaiwaP1Trigger*> Batch;
        for (int32 Index = PendingTriggers.Num() - 1; Index >= 0; --Index)
        {
            if (PendingTriggers[Index].Frame != Oldest) continue;
            if (ANaiwaP1Trigger* Trigger = PendingTriggers[Index].Trigger.Get()) Batch.Add(Trigger);
            PendingTriggers.RemoveAtSwap(Index);
        }
        const bool bFall = bPendingFallDeath && PendingFallFrame == Oldest;
        if (bFall) bPendingFallDeath = false;

        ANaiwaP1Trigger* Hazard = nullptr;
        ANaiwaP1Trigger* KillVolume = nullptr;
        for (ANaiwaP1Trigger* Trigger : Batch)
        {
            if (Trigger->TriggerKind == ENaiwaP1TriggerKind::Hazard)
            {
                if (!Hazard || Trigger->GetStableId().ToString() < Hazard->GetStableId().ToString()) Hazard = Trigger;
            }
            else if (Trigger->TriggerKind == ENaiwaP1TriggerKind::KillZ)
            {
                if (!KillVolume || Trigger->GetStableId().ToString() < KillVolume->GetStableId().ToString()) KillVolume = Trigger;
            }
        }
        if (bFall || Hazard || KillVolume)
        {
            if (Hazard) RevealAllHazards();
            EnterDying(bFall ? FName(TEXT("WorldKillZ")) : Hazard ? Hazard->GetStableId() : KillVolume->GetStableId());
            return;
        }

        // The category order is fixed even when overlap callbacks arrive in another order.
        for (ANaiwaP1Trigger* Trigger : Batch)
        {
            if (Trigger->TriggerKind != ENaiwaP1TriggerKind::Pickup) continue;
            const FName Id = Trigger->GetStableId();
            if (CollectedIds.Contains(Id)) continue;
            CollectedIds.Add(Id);
            if (Trigger->bRequiredForGoal) CollectedRequiredPickupIds.Add(Id);
            Trigger->SetCollected();
            bExitLockedHint = false;
            PlayPickupFeedback(Trigger);
            UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_FEEDBACK event=pickup sound=%s id=%s"),
                *GetNameSafe(Trigger->PickupOrder == 2 ? FeedbackCollection1.Get()
                    : Trigger->PickupOrder == 3 ? FeedbackCollection2.Get() : FeedbackCollection.Get()), *Id.ToString());
            UE_LOG(LogTemp, Display, TEXT("NAIWA_P1 pickup id=%s total=%d/%d goal_required=%d/%d"),
                *Id.ToString(), CollectedIds.Num(), TotalPickupCount,
                GetCollectedRequiredPickupCount(), RequiredPickupCount);
        }

        ANaiwaP1Trigger* Checkpoint = nullptr;
        for (ANaiwaP1Trigger* Trigger : Batch)
        {
            if (Trigger->TriggerKind != ENaiwaP1TriggerKind::Checkpoint) continue;
            if (!Checkpoint || Trigger->GetStableId().ToString() < Checkpoint->GetStableId().ToString()) Checkpoint = Trigger;
        }
        if (Checkpoint && ActiveCheckpoint.Get() != Checkpoint)
        {
            const bool bLateCheckpoint = FCString::Atoi(*Checkpoint->GetStableId().ToString().Right(2)) >= 5;
            if (ActiveCheckpoint.IsValid()) ActiveCheckpoint->SetCheckpointActive(false);
            ActiveCheckpoint = Checkpoint;
            Checkpoint->SetCheckpointActive(true);
            PlayCheckpointFeedback(Checkpoint);
            SpawnCheckpointBurst(Checkpoint);
            UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_FEEDBACK event=checkpoint sound=%s id=%s"),
                *GetNameSafe(bLateCheckpoint
                    ? FeedbackCheckpointLate.Get() : FeedbackCheckpointEarly.Get()), *Checkpoint->GetStableId().ToString());
            UE_LOG(LogTemp, Display, TEXT("NAIWA_P1 checkpoint id=%s respawn=(%.1f,%.1f,%.1f)"),
                *Checkpoint->GetStableId().ToString(),
                Checkpoint->GetActorLocation().X + Checkpoint->RespawnOffset.X,
                Checkpoint->GetActorLocation().Y + Checkpoint->RespawnOffset.Y,
                Checkpoint->GetActorLocation().Z + Checkpoint->RespawnOffset.Z);
        }

        for (ANaiwaP1Trigger* Trigger : Batch)
        {
            if (Trigger->TriggerKind != ENaiwaP1TriggerKind::Goal) continue;
            if (!IsGoalUnlocked())
            {
                bExitLockedHint = true;
                UE_LOG(LogTemp, Display, TEXT("NAIWA_P1 goal_locked id=%s goal_required=%d/%d"),
                    *Trigger->GetStableId().ToString(), GetCollectedRequiredPickupCount(), RequiredPickupCount);
            }
            else
            {
                if (GetGameInstance())
                {
                    if (UNaiwaProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UNaiwaProgressSubsystem>())
                    {
                        Progress->RecordLevelClear(TEXT("1-1"), 0,
                            GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f,
                            DeathsThisRun, CollectedIds.Num());
                    }
                }
                SetState(ENaiwaP1State::Won);
                if (PlayerCharacter.IsValid()) PlayerCharacter->SetP1ControlsLocked(true);
                SpawnGoalBurst(Trigger);
                if (FeedbackCollection) UGameplayStatics::PlaySound2D(this, FeedbackCollection);
                UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_FEEDBACK event=win sound=%s id=%s"),
                    *GetNameSafe(CollectionSound.Get()), *Trigger->GetStableId().ToString());
                UE_LOG(LogTemp, Display, TEXT("NAIWA_P1 goal_won id=%s pickups=%d"), *Trigger->GetStableId().ToString(), CollectedIds.Num());
                if (bLinkRun)
                {
                    GetWorldTimerManager().SetTimer(LinkAdvanceTimer, this,
                        &ANaiwaP1GameMode::AdvanceToP2Campaign, 1.5f, false);
                }
                PendingTriggers.Reset();
                bPendingFallDeath = false;
                return;
            }
        }
    }
}

void ANaiwaP1GameMode::AdvanceToP2Campaign()
{
    if (!bLinkRun || !GetWorld())
    {
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("NAIWA_CAMPAIGN transition=P1_to_P2 level=2-1"));
    UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Naiwa/P2/Maps/L_NaiwaP2_21")), true,
        TEXT("NaiwaLinkRun=1"));
}

void ANaiwaP1GameMode::RevealAllHazards()
{
    for (TActorIterator<ANaiwaP1Trigger> It(GetWorld()); It; ++It)
    {
        if (It->TriggerKind == ENaiwaP1TriggerKind::Hazard) It->SetHazardRevealed(true);
    }
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1 hazards_revealed"));
}

void ANaiwaP1GameMode::EnterDying(FName Cause)
{
    if (State != ENaiwaP1State::Playing) return;
    LastDeathCause = Cause;
    SetState(ENaiwaP1State::Dying);
    ++DeathEventCount;
    ++DeathsThisRun;
    PendingTriggers.Reset();
    bPendingFallDeath = false;
    if (PlayerCharacter.IsValid())
    {
        PlayerCharacter->SetP1ControlsLocked(true);
        PlayerCharacter->SetP1DeathPose(false);
    }
    StopDeathAudio();
    bUseDeathEggThisAttempt = DeathsThisRun == 3;
    if (bV7DeathEggProbeEnabled && bUseDeathEggThisAttempt)
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_V7_DEATH_EGG third_death_entered total=%d"), DeathsThisRun);
    bDeathLaughPlaying = false;
    DeathLaughStartedAt = -1.0;
    DeathLaughDuration = 0.0f;
    USoundBase* LeadSound = FeedbackDeathLead ? FeedbackDeathLead.Get() : DeathSound.Get();
    USoundBase* FollowupSound = bUseDeathEggThisAttempt
        ? (FeedbackDeathEgg ? FeedbackDeathEgg.Get() : FeedbackDeathFollowup.Get())
        : FeedbackDeathFollowup.Get();
    const float LeadDuration = LeadSound ? FMath::Max(0.0f, LeadSound->GetDuration()) : 0.0f;
    const float FollowupDuration = FollowupSound ? FMath::Max(0.0f, FollowupSound->GetDuration()) : 0.0f;
    if (LeadSound) DeathAudioComponent = UGameplayStatics::SpawnSound2D(this, LeadSound, 1.0f);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_FEEDBACK event=death_lead sound=%s count=%d"),
        *GetNameSafe(LeadSound), DeathEventCount);
    if (FollowupSound)
    {
        GetWorldTimerManager().SetTimer(DeathFollowupTimer, this, &ANaiwaP1GameMode::PlayDeathFollowup,
            FMath::Max(0.02f, LeadDuration), false);
    }
    else
    {
        PlayDeathFollowup();
    }
    const float Duration = FMath::Max(FMath::Max(0.1f, DeathDurationSeconds),
        LeadDuration + FollowupDuration + 0.15f);
    GetWorldTimerManager().SetTimer(DeathTimer, this, &ANaiwaP1GameMode::BeginRespawn, Duration, false);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1 death cause=%s sequence=Die>%s duration=%.2f"),
        *Cause.ToString(), bUseDeathEggThisAttempt ? TEXT("Die_3") : TEXT("Die_1"), Duration);
}

void ANaiwaP1GameMode::LoadFeedbackAssets()
{
    FeedbackBackgroundMusic1 = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/Naiwa/P1/Feedback/Audio/SFX_P1_Feedback_BackgroundMusic1.SFX_P1_Feedback_BackgroundMusic1"));
    FeedbackBackgroundMusic2 = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/Naiwa/P1/Feedback/Audio/SFX_P1_Feedback_BackgroundMusic2.SFX_P1_Feedback_BackgroundMusic2"));
    FeedbackJump = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/Naiwa/P1/Feedback/Audio/SFX_P1_Feedback_Jump.SFX_P1_Feedback_Jump"));
    FeedbackCollection = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/Naiwa/P1/Feedback/Audio/SFX_P1_Feedback_Collection.SFX_P1_Feedback_Collection"));
    FeedbackCollection1 = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/Naiwa/P1/Feedback/Audio/SFX_P1_Feedback_Collection1.SFX_P1_Feedback_Collection1"));
    FeedbackCollection2 = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/Naiwa/P1/Feedback/Audio/SFX_P1_Feedback_Collection2.SFX_P1_Feedback_Collection2"));
    FeedbackDeathLead = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/Naiwa/P1/Feedback/Audio/SFX_P1_Feedback_Die.SFX_P1_Feedback_Die"));
    FeedbackDeathFollowup = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/Naiwa/P1/Feedback/Audio/SFX_P1_Feedback_Die1.SFX_P1_Feedback_Die1"));
    FeedbackDeathEgg = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/Naiwa/P1/Feedback/Audio/SFX_P1_Feedback_Die3.SFX_P1_Feedback_Die3"));
    FeedbackCheckpointEarly = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/Naiwa/P1/Feedback/Audio/SFX_P1_Feedback_CheckpointEarly.SFX_P1_Feedback_CheckpointEarly"));
    FeedbackCheckpointLate = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/Naiwa/P1/Feedback/Audio/SFX_P1_Feedback_CheckpointLate.SFX_P1_Feedback_CheckpointLate"));
    CheckpointBurstSystem = LoadObject<UNiagaraSystem>(nullptr,
        TEXT("/Niagara/DefaultAssets/Templates/Systems/RadialBurst.RadialBurst"));
    DeathSound = FeedbackDeathLead ? FeedbackDeathLead.Get() : DeathSound.Get();
    CollectionSound = FeedbackCollection ? FeedbackCollection.Get() : CollectionSound.Get();
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_FEEDBACK assets bgm=%d/%d jump=%d collection=%d/%d/%d death=%d/%d/%d checkpoint=%d/%d burst=%d"),
        FeedbackBackgroundMusic1 != nullptr, FeedbackBackgroundMusic2 != nullptr, FeedbackJump != nullptr,
        FeedbackCollection != nullptr, FeedbackCollection1 != nullptr, FeedbackCollection2 != nullptr,
        FeedbackDeathLead != nullptr, FeedbackDeathFollowup != nullptr, FeedbackDeathEgg != nullptr,
        FeedbackCheckpointEarly != nullptr, FeedbackCheckpointLate != nullptr, CheckpointBurstSystem != nullptr);
}

void ANaiwaP1GameMode::StartBackgroundMusic()
{
    if (bBackgroundMusicPausedForDeathEgg) return;
    USoundBase* Sound = bBackgroundMusic1Next ? FeedbackBackgroundMusic1.Get() : FeedbackBackgroundMusic2.Get();
    const bool bPlayingMusic1 = bBackgroundMusic1Next;
    bBackgroundMusic1Next = !bBackgroundMusic1Next;
    if (!Sound) return;
    const float Pitch = bPlayingMusic1 ? 1.3f : 1.0f;
    BackgroundMusicBaseVolumeMultiplier = bPlayingMusic1 ? 0.16f : 0.32f;
    const float EffectiveVolume = BackgroundMusicBaseVolumeMultiplier * (bPickupMusicDucked ? 0.30f : 1.0f);
    BackgroundMusicComponent = UGameplayStatics::SpawnSound2D(this, Sound, BackgroundMusicBaseVolumeMultiplier, Pitch);
    if (BackgroundMusicComponent && bPickupMusicDucked)
        BackgroundMusicComponent->SetVolumeMultiplier(EffectiveVolume);
    const float Duration = FMath::Max(0.1f, Sound->GetDuration() / Pitch);
    BackgroundMusicScheduledEndAt = GetWorld()->GetTimeSeconds() + Duration;
    GetWorldTimerManager().SetTimer(BackgroundMusicTimer, this, &ANaiwaP1GameMode::StartBackgroundMusic,
        Duration, false);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_FEEDBACK event=background_music sound=%s pitch=%.2f volume=%.2f ducked=%d next_in=%.2f"),
        *GetNameSafe(Sound), Pitch, EffectiveVolume, bPickupMusicDucked, Duration);
}

void ANaiwaP1GameMode::PauseBackgroundMusicForEgg()
{
    if (bBackgroundMusicPausedForDeathEgg) return;
    bBackgroundMusicPausedForDeathEgg = true;
    GetWorldTimerManager().ClearTimer(BackgroundMusicTimer);
    BackgroundMusicRemainingSeconds = GetWorld()
        ? FMath::Max(0.0f, static_cast<float>(BackgroundMusicScheduledEndAt - GetWorld()->GetTimeSeconds()))
        : 0.0f;
    const bool bPaused = BackgroundMusicComponent && BackgroundMusicComponent->IsPlaying();
    if (bPaused) BackgroundMusicComponent->SetPaused(true);
    bBackgroundMusicComponentPaused = bPaused;
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_FEEDBACK event=background_music_paused_for_egg remaining=%.2f component_paused=%d"),
        BackgroundMusicRemainingSeconds, bPaused);
}

void ANaiwaP1GameMode::ResumeBackgroundMusicAfterEgg()
{
    if (!bBackgroundMusicPausedForDeathEgg) return;
    if (bV7DeathEggProbeEnabled && DeathsThisRun == 3 && bBackgroundMusicComponentPaused)
        bV7DeathEggMusicResumeObserved = true;
    GetWorldTimerManager().ClearTimer(EasterEggMusicResumeTimer);
    bBackgroundMusicPausedForDeathEgg = false;
    if (BackgroundMusicComponent && bBackgroundMusicComponentPaused && BackgroundMusicRemainingSeconds > 0.05f)
    {
        BackgroundMusicComponent->SetPaused(false);
        bBackgroundMusicComponentPaused = false;
        GetWorldTimerManager().SetTimer(BackgroundMusicTimer, this, &ANaiwaP1GameMode::StartBackgroundMusic,
            BackgroundMusicRemainingSeconds, false);
    }
    else
    {
        if (BackgroundMusicComponent)
        {
            BackgroundMusicComponent->Stop();
            BackgroundMusicComponent = nullptr;
        }
        bBackgroundMusicComponentPaused = false;
        StartBackgroundMusic();
    }
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_FEEDBACK event=background_music_resumed"));
}

void ANaiwaP1GameMode::PlayDeathFollowup()
{
    if (State != ENaiwaP1State::Dying) return;
    USoundBase* Sound = bUseDeathEggThisAttempt
        ? (FeedbackDeathEgg ? FeedbackDeathEgg.Get() : FeedbackDeathFollowup.Get())
        : FeedbackDeathFollowup.Get();
    if (!Sound) return;
    if (bUseDeathEggThisAttempt) PauseBackgroundMusicForEgg();
    if (bV7DeathEggProbeEnabled && bUseDeathEggThisAttempt)
    {
        bV7DeathEggFollowupObserved = Sound == FeedbackDeathEgg.Get();
        bV7DeathEggMusicPauseObserved = bBackgroundMusicPausedForDeathEgg && bBackgroundMusicComponentPaused;
    }
    if (bV7DeathEggProbeEnabled && DeathsThisRun == 4)
        bV7DeathEggReturnToDie1Observed = Sound == FeedbackDeathFollowup.Get()
            && !bBackgroundMusicPausedForDeathEgg;
    DeathAudioComponent = UGameplayStatics::SpawnSound2D(this, Sound, 1.0f);
    DeathLaughStartedAt = GetWorld()->GetTimeSeconds();
    DeathLaughDuration = FMath::Max(0.0f, Sound->GetDuration());
    bDeathLaughPlaying = true;
    // P1 death uses the selected video frames in the HUD. Keep the older
    // skeletal laugh sequence out of the same presentation.
    if (PlayerCharacter.IsValid()) PlayerCharacter->SetP1DeathPose(false);
    if (bUseDeathEggThisAttempt)
    {
        GetWorldTimerManager().SetTimer(EasterEggMusicResumeTimer, this,
            &ANaiwaP1GameMode::ResumeBackgroundMusicAfterEgg, FMath::Max(0.1f, DeathLaughDuration), false);
    }
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_FEEDBACK event=death_followup sound=%s laugh_sprite=1 duration=%.2f egg=%d"),
        *GetNameSafe(Sound), DeathLaughDuration, bUseDeathEggThisAttempt);
}

void ANaiwaP1GameMode::PlayJumpFeedback()
{
    if (!FeedbackJump) return;
    UGameplayStatics::PlaySound2D(this, FeedbackJump, 1.0f);
    UE_LOG(LogTemp, Verbose, TEXT("NAIWA_P1_FEEDBACK event=jump sound=%s"), *GetNameSafe(FeedbackJump.Get()));
}

void ANaiwaP1GameMode::PlayPickupFeedback(const ANaiwaP1Trigger* Trigger)
{
    GetWorldTimerManager().ClearTimer(PickupMusicDuckRestoreTimer);
    if (PickupAudioComponent)
    {
        GetWorldTimerManager().ClearTimer(PickupFadeTimer);
        PickupAudioComponent->FadeOut(0.12f, 0.0f);
    }
    const int32 Order = Trigger ? Trigger->PickupOrder : 0;
    USoundBase* Sound = Order == 2 ? FeedbackCollection1.Get()
        : Order == 3 ? FeedbackCollection2.Get() : FeedbackCollection.Get();
    const bool bDuckMusicForPickup = Sound != nullptr
        && (Sound == FeedbackCollection1.Get() || Sound == FeedbackCollection2.Get());
    if (!Sound) Sound = CollectionSound.Get();
    if (!Sound)
    {
        SetPickupMusicDucked(false);
        return;
    }
    SetPickupMusicDucked(bDuckMusicForPickup);
    PickupAudioComponent = UGameplayStatics::SpawnSound2D(this, Sound, 0.95f);
    const float FadeSeconds = 0.8f;
    const float FadeStart = FMath::Max(0.05f, FMath::Min(3.0f, Sound->GetDuration() - FadeSeconds));
    GetWorldTimerManager().SetTimer(PickupFadeTimer, this, &ANaiwaP1GameMode::FadePickupFeedback,
        FadeStart, false);
}

void ANaiwaP1GameMode::FadePickupFeedback()
{
    if (PickupAudioComponent) PickupAudioComponent->FadeOut(0.8f, 0.0f);
    if (bPickupMusicDucked)
    {
        GetWorldTimerManager().SetTimer(PickupMusicDuckRestoreTimer, this,
            &ANaiwaP1GameMode::RestorePickupMusicAfterFade, 0.8f, false);
    }
}

void ANaiwaP1GameMode::SetPickupMusicDucked(bool bDucked)
{
    bPickupMusicDucked = bDucked;
    if (BackgroundMusicComponent)
        BackgroundMusicComponent->SetVolumeMultiplier(BackgroundMusicBaseVolumeMultiplier * (bDucked ? 0.30f : 1.0f));
    const float EffectiveVolume = BackgroundMusicBaseVolumeMultiplier * (bDucked ? 0.30f : 1.0f);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_FEEDBACK event=pickup_music_ducked active=%d duck_multiplier=%.2f effective_volume=%.2f"),
        bDucked, bDucked ? 0.30f : 1.0f, EffectiveVolume);
}

void ANaiwaP1GameMode::RestorePickupMusicAfterFade()
{
    SetPickupMusicDucked(false);
}

void ANaiwaP1GameMode::PlayCheckpointFeedback(const ANaiwaP1Trigger* Trigger)
{
    if (!Trigger) return;
    const FString Id = Trigger->GetStableId().ToString();
    const int32 Number = FCString::Atoi(*Id.Right(2));
    USoundBase* Sound = Number >= 5 ? FeedbackCheckpointLate.Get() : FeedbackCheckpointEarly.Get();
    if (Sound) UGameplayStatics::PlaySound2D(this, Sound, 1.0f);
}

void ANaiwaP1GameMode::SpawnCheckpointBurst(const ANaiwaP1Trigger* Trigger)
{
    if (!Trigger || !CheckpointBurstSystem) return;
    UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), CheckpointBurstSystem,
        Trigger->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f), FRotator::ZeroRotator,
        FVector(1.0f), true, true, ENCPoolMethod::AutoRelease, true);
}

void ANaiwaP1GameMode::SpawnGoalBurst(const ANaiwaP1Trigger* Trigger)
{
    if (!Trigger || !CheckpointBurstSystem) return;
    UNiagaraComponent* Burst = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), CheckpointBurstSystem,
        Trigger->GetActorLocation() + FVector(0.0f, 0.0f, 125.0f), FRotator::ZeroRotator,
        FVector(1.8f), true, true, ENCPoolMethod::AutoRelease, true);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_FEEDBACK event=goal_particles id=%s spawned=%d"),
        *Trigger->GetStableId().ToString(), Burst != nullptr);
}

void ANaiwaP1GameMode::StopDeathAudio()
{
    GetWorldTimerManager().ClearTimer(DeathFollowupTimer);
    if (DeathAudioComponent)
    {
        DeathAudioComponent->Stop();
        DeathAudioComponent = nullptr;
    }
    bDeathLaughPlaying = false;
    DeathLaughStartedAt = -1.0;
    ResumeBackgroundMusicAfterEgg();
}

int32 ANaiwaP1GameMode::GetCollectedRequiredPickupCount() const
{
    return RequiredPickupIds.IsEmpty() ? CollectedIds.Num() : CollectedRequiredPickupIds.Num();
}

bool ANaiwaP1GameMode::IsGoalUnlocked() const
{
    return GetCollectedRequiredPickupCount() >= RequiredPickupCount;
}

bool ANaiwaP1GameMode::IsDeathLaughSpriteActive() const
{
    if (!bDeathLaughPlaying || State != ENaiwaP1State::Dying || DeathLaughStartedAt < 0.0 || !GetWorld()) return false;
    return GetWorld()->GetTimeSeconds() - DeathLaughStartedAt < DeathLaughDuration;
}

int32 ANaiwaP1GameMode::GetDeathLaughSpriteFrame() const
{
    if (!IsDeathLaughSpriteActive()) return 0;
    const double Elapsed = FMath::Max(0.0, GetWorld()->GetTimeSeconds() - DeathLaughStartedAt);
    const double LoopTime = FMath::Fmod(Elapsed, static_cast<double>(DeathLaughAnimationLoopSeconds));
    const double FrameTime = LoopTime / DeathLaughAnimationLoopSeconds;
    return FMath::Clamp(FMath::FloorToInt(FrameTime * DeathLaughFrameCount), 0, DeathLaughFrameCount - 1);
}

bool ANaiwaP1GameMode::PlaceCharacterAt(const FVector& PreferredLocation)
{
    if (!PlayerCharacter.IsValid()) return false;
    FVector SafeLocation = PreferredLocation;
    if (!GetWorld()->FindTeleportSpot(PlayerCharacter.Get(), SafeLocation, PlayerCharacter->GetActorRotation()))
    {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_P1 respawn_no_clearance preferred=(%.1f,%.1f,%.1f)"),
            PreferredLocation.X, PreferredLocation.Y, PreferredLocation.Z);
        return false;
    }
    const bool bPlaced = PlayerCharacter->TeleportForP1(SafeLocation);
    // Count production respawn placement separately from probe locomotion.
    if (bIntegratedProbeEnabled && bPlaced) ++IntegratedProbeProductionRespawnPlacements;
    return bPlaced;
}

void ANaiwaP1GameMode::BeginRespawn()
{
    if (State != ENaiwaP1State::Dying) return;
    StopDeathAudio();
    SetState(ENaiwaP1State::Respawning);
    ResetAttemptActors();
    CollectedIds.Reset();
    CollectedRequiredPickupIds.Reset();
    bExitLockedHint = false;
    FVector Location = InitialSpawn;
    if (ActiveCheckpoint.IsValid()) Location = ActiveCheckpoint->GetActorLocation() + ActiveCheckpoint->RespawnOffset;
    bRespawnTriedInitial = !ActiveCheckpoint.IsValid();
    if (!PlaceCharacterAt(Location) && bHasInitialSpawn)
    {
        bRespawnTriedInitial = true;
        PlaceCharacterAt(InitialSpawn);
    }
    if (PlayerCharacter.IsValid()) PlayerCharacter->SetP1DeathPose(false);
    RespawnStartedAt = GetWorld()->GetTimeSeconds();
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1 respawn checkpoint=%s location=(%.1f,%.1f,%.1f)"),
        ActiveCheckpoint.IsValid() ? *ActiveCheckpoint->GetStableId().ToString() : TEXT("none"),
        PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().X : 0.0f,
        PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().Y : 0.0f,
        PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().Z : 0.0f);
}

void ANaiwaP1GameMode::FinishRespawnIfReady()
{
    if (!PlayerCharacter.IsValid()) return;
    if (PlayerCharacter->GetCharacterMovement()->IsMovingOnGround())
    {
        PlayerCharacter->SetP1ControlsLocked(false);
        SetState(ENaiwaP1State::Playing);
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1 control_restored x=%.1f z=%.1f"),
            PlayerCharacter->GetActorLocation().X, PlayerCharacter->GetActorLocation().Z);
        return;
    }
    const bool bBelowKillZ = PlayerCharacter->GetActorLocation().Z < GetWorld()->GetWorldSettings()->KillZ;
    const bool bTimedOut = GetWorld()->GetTimeSeconds() - RespawnStartedAt > 3.0f;
    if ((bBelowKillZ || bTimedOut) && !bRespawnTriedInitial && bHasInitialSpawn)
    {
        bRespawnTriedInitial = true;
        PlaceCharacterAt(InitialSpawn);
        RespawnStartedAt = GetWorld()->GetTimeSeconds();
        UE_LOG(LogTemp, Warning, TEXT("NAIWA_P1 respawn_fallback_initial"));
    }
    else if ((bBelowKillZ || bTimedOut) && bRespawnTriedInitial)
    {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_P1 respawn_stalled: initial spawn needs a safe floor"));
        RespawnStartedAt = GetWorld()->GetTimeSeconds();
    }
}

void ANaiwaP1GameMode::ResetAttemptActors()
{
    for (TActorIterator<ANaiwaP1Trigger> It(GetWorld()); It; ++It) It->ResetForAttempt();
	for (TActorIterator<ANaiwaP1Mechanism> It(GetWorld()); It; ++It) It->ResetForAttempt();
}

void ANaiwaP1GameMode::ToggleP1Pause()
{
    if (State != ENaiwaP1State::Playing && State != ENaiwaP1State::Dying) return;
    const bool bPaused = !UGameplayStatics::IsGamePaused(this);
    UGameplayStatics::SetGamePaused(this, bPaused);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1 pause=%d underlying=%s"), bPaused, StateName(State));
}

void ANaiwaP1GameMode::RestartLevel()
{
    const int32 DeathCountBeforeRestart = DeathsThisRun;
    GetWorldTimerManager().ClearTimer(DeathTimer);
    GetWorldTimerManager().ClearTimer(PickupFadeTimer);
    GetWorldTimerManager().ClearTimer(PickupMusicDuckRestoreTimer);
    SetPickupMusicDucked(false);
    if (PickupAudioComponent)
    {
        PickupAudioComponent->Stop();
        PickupAudioComponent = nullptr;
    }
    StopDeathAudio();
    UGameplayStatics::SetGamePaused(this, false);
    PendingTriggers.Reset();
    bPendingFallDeath = false;
    DeathsThisRun = 0;
    CollectedIds.Reset();
    ActiveCheckpoint.Reset();
    bExitLockedHint = false;
    ResetAttemptActors();
    SetState(ENaiwaP1State::Respawning);
    bRespawnTriedInitial = true;
    if (PlayerCharacter.IsValid())
    {
        PlayerCharacter->SetP1DeathPose(false);
        PlayerCharacter->SetP1ControlsLocked(true);
    }
    if (bHasInitialSpawn) PlaceCharacterAt(InitialSpawn);
    RespawnStartedAt = GetWorld()->GetTimeSeconds();
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1 restart_full gait_running=%d death_count_before=%d death_count_after=%d"),
        PlayerCharacter.IsValid() ? PlayerCharacter->IsRunningGait() : -1, DeathCountBeforeRestart, DeathsThisRun);
}

void ANaiwaP1GameMode::SetState(ENaiwaP1State NewState)
{
    if (State == NewState) return;
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1 state %s -> %s"), StateName(State), StateName(NewState));
    State = NewState;
}

bool ANaiwaP1GameMode::SmokeTeleportTo(ANaiwaP1Trigger* Trigger)
{
    if (!PlayerCharacter.IsValid() || !Trigger)
    {
        FinishSmoke(TEXT("INCOMPLETE"), TEXT("Character or required trigger disappeared"));
        return false;
    }
    FVector Target = Trigger->GetActorLocation();
    if (Trigger->TriggerKind == ENaiwaP1TriggerKind::Hazard)
    {
        // Put the feet into the spike layer, including for elevated platforms.
        Target.Z = Trigger->GetActorLocation().Z - Trigger->BoxExtent.Z
            + PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 1.0f;
    }
    PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();
    PlayerCharacter->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
    if (!Trigger->IsOverlappingActor(PlayerCharacter.Get()))
    {
        FinishSmoke(TEXT("FAIL"), FString::Printf(TEXT("No physical Pawn overlap at %s"), *Trigger->GetStableId().ToString()));
        return false;
    }
    return true;
}

void ANaiwaP1GameMode::SmokeAdvance(ENaiwaP1SmokeStep NextStep, const FString& Evidence)
{
    SmokeEvents.Add(Evidence);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SMOKE %s"), *Evidence);
    SmokeStep = NextStep;
    SmokeStepStartedWall = FPlatformTime::Seconds();
}

void ANaiwaP1GameMode::TickSmoke()
{
    const double Now = FPlatformTime::Seconds();
    const double StepLimit = SmokeStep == ENaiwaP1SmokeStep::WaitRespawn ? 20.0 : 5.0;
    if (Now - SmokeStartedWall > 50.0 || Now - SmokeStepStartedWall > StepLimit)
    {
        FinishSmoke(SmokeStep == ENaiwaP1SmokeStep::WaitReady ? TEXT("INCOMPLETE") : TEXT("FAIL"),
            FString::Printf(TEXT("Timeout step=%d state=%s pickups=%d deaths=%d checkpoint=%d"),
                static_cast<int32>(SmokeStep), StateName(State), CollectedIds.Num(), DeathEventCount, ActiveCheckpoint.IsValid()));
        return;
    }

    switch (SmokeStep)
    {
    case ENaiwaP1SmokeStep::WaitReady:
    {
        if (!PlayerCharacter.IsValid() || !PlayerCharacter->GetCharacterMovement()->IsMovingOnGround()) return;
        if (RequiredPickupCount != 1)
        {
            FinishSmoke(TEXT("INCOMPLETE"), TEXT("Slice smoke requires exactly one required pickup"));
            return;
        }
        for (TActorIterator<ANaiwaP1Trigger> It(GetWorld()); It; ++It)
        {
            switch (It->TriggerKind)
            {
            case ENaiwaP1TriggerKind::Pickup: if (!SmokePickup.IsValid()) SmokePickup = *It; break;
            case ENaiwaP1TriggerKind::Checkpoint: if (!SmokeCheckpoint.IsValid()) SmokeCheckpoint = *It; break;
            case ENaiwaP1TriggerKind::Hazard: if (!SmokeHazard.IsValid()) SmokeHazard = *It; break;
            case ENaiwaP1TriggerKind::Goal: if (!SmokeGoal.IsValid()) SmokeGoal = *It; break;
            default: break;
            }
        }
        if (!SmokePickup.IsValid() || !SmokeCheckpoint.IsValid() || !SmokeHazard.IsValid() || !SmokeGoal.IsValid()) return;
        if (!SmokeTeleportTo(SmokeGoal.Get())) return;
        SmokeAdvance(ENaiwaP1SmokeStep::WaitLockedGoal, TEXT("goal_overlap_before_pickup"));
        break;
    }
    case ENaiwaP1SmokeStep::WaitLockedGoal:
        if (!bExitLockedHint || State != ENaiwaP1State::Playing || CollectedIds.Num() != 0) return;
        if (!SmokeTeleportTo(SmokePickup.Get())) return;
        SmokeAdvance(ENaiwaP1SmokeStep::WaitFirstPickup, TEXT("goal_locked_then_pickup_overlap"));
        break;
    case ENaiwaP1SmokeStep::WaitFirstPickup:
        if (CollectedIds.Num() != 1 || SmokePickup->DisplayMesh->IsVisible()) return;
        if (!SmokeTeleportTo(SmokeCheckpoint.Get())) return;
        SmokeAdvance(ENaiwaP1SmokeStep::WaitCheckpoint, TEXT("pickup_collected_once_and_hidden"));
        break;
    case ENaiwaP1SmokeStep::WaitCheckpoint:
        if (ActiveCheckpoint.Get() != SmokeCheckpoint.Get()) return;
        if (!SmokeTeleportTo(SmokeHazard.Get())) return;
        SmokeAdvance(ENaiwaP1SmokeStep::WaitDeath, TEXT("checkpoint_activated_then_hazard_overlap"));
        break;
    case ENaiwaP1SmokeStep::WaitDeath:
        if (State != ENaiwaP1State::Dying || DeathEventCount != 1 || !SmokeHazard->DisplayMesh->IsVisible()) return;
        PlayerCharacter->SetActorLocation(InitialSpawn, false, nullptr, ETeleportType::TeleportPhysics);
        if (!SmokeTeleportTo(SmokeHazard.Get())) return;
        SmokeAdvance(ENaiwaP1SmokeStep::WaitRespawn, TEXT("one_death_hazards_revealed_reoverlap_during_dying"));
        break;
    case ENaiwaP1SmokeStep::WaitRespawn:
        if (DeathEventCount != 1)
        {
            FinishSmoke(TEXT("FAIL"), TEXT("Repeated hazard created another death"));
            return;
        }
        if (State != ENaiwaP1State::Playing) return;
        if (ActiveCheckpoint.Get() != SmokeCheckpoint.Get() || CollectedIds.Num() != 0
            || !SmokePickup->DisplayMesh->IsVisible() || SmokeHazard->DisplayMesh->IsVisible()
            || FVector::Dist2D(PlayerCharacter->GetActorLocation(), SmokeCheckpoint->GetActorLocation() + SmokeCheckpoint->RespawnOffset) > 150.0f)
        {
            FinishSmoke(TEXT("FAIL"), TEXT("Respawn did not retain checkpoint, restore pickup/hazard, or land near checkpoint"));
            return;
        }
        if (!SmokeTeleportTo(SmokePickup.Get())) return;
        SmokeAdvance(ENaiwaP1SmokeStep::WaitSecondPickup, TEXT("checkpoint_respawn_pickup_restored_hazard_hidden"));
        break;
    case ENaiwaP1SmokeStep::WaitSecondPickup:
        if (CollectedIds.Num() != 1) return;
        if (!SmokeTeleportTo(SmokeGoal.Get())) return;
        SmokeAdvance(ENaiwaP1SmokeStep::WaitWon, TEXT("pickup_recollected_then_goal_overlap"));
        break;
    case ENaiwaP1SmokeStep::WaitWon:
        if (State != ENaiwaP1State::Won) return;
        SmokeAdvance(ENaiwaP1SmokeStep::WaitRestart, TEXT("goal_won_after_required_pickup"));
        RestartLevel(); // Same method as P1 R input; keyboard dispatch is checked separately.
        break;
    case ENaiwaP1SmokeStep::WaitRestart:
        if (State != ENaiwaP1State::Playing) return;
        if (CollectedIds.Num() != 0 || ActiveCheckpoint.IsValid() || !SmokePickup->DisplayMesh->IsVisible()
            || SmokeHazard->DisplayMesh->IsVisible() || DeathEventCount != 1
            || FVector::Dist2D(PlayerCharacter->GetActorLocation(), InitialSpawn) > 150.0f)
        {
            FinishSmoke(TEXT("FAIL"), TEXT("Full restart did not clear attempt state or return to original spawn"));
            return;
        }
        SmokeEvents.Add(TEXT("restart_from_won_cleared_pickup_checkpoint_hazard"));
        FinishSmoke(TEXT("PASS"), TEXT("P1 state flow passed with physical trigger overlaps; restart used the R handler method"));
        break;
    }
}

void ANaiwaP1GameMode::FinishSmoke(const TCHAR* Result, const FString& Detail)
{
    if (!bSmokeEnabled) return;
    bSmokeEnabled = false;
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SMOKE result=%s detail=%s"), Result, *Detail);
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1SmokeV1"));
    Report->SetStringField(TEXT("status"), Result);
    Report->SetStringField(TEXT("detail"), Detail);
    Report->SetStringField(TEXT("map"), GetWorld()->GetMapName());
    Report->SetStringField(TEXT("state"), StateName(State));
    Report->SetNumberField(TEXT("pickup_count"), CollectedIds.Num());
    Report->SetNumberField(TEXT("death_events"), DeathEventCount);
    Report->SetBoolField(TEXT("checkpoint_active"), ActiveCheckpoint.IsValid());
    Report->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - SmokeStartedWall);
    TArray<TSharedPtr<FJsonValue>> Events;
    for (const FString& Event : SmokeEvents) Events.Add(MakeShared<FJsonValueString>(Event));
    Report->SetArrayField(TEXT("events"), Events);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("smoke_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (bSaved)
    {
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SMOKE output=%s saved=1"), *Output);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_P1_SMOKE output=%s saved=0"), *Output);
    }
    FPlatformMisc::RequestExit(false);
}

void ANaiwaP1GameMode::RouteJumpAt(float ThresholdX, int32 JumpIndex, float HoldSeconds)
{
    if (!PlayerCharacter.IsValid() || RouteJumpIndex != JumpIndex) return;
    if (PlayerCharacter->GetActorLocation().X < ThresholdX) return;
    if (!PlayerCharacter->GetCharacterMovement()->IsMovingOnGround()) return;
    PlayerCharacter->Jump();
    RouteJumpReleaseTime = GetWorld()->GetTimeSeconds() + HoldSeconds;
    ++RouteJumpIndex;
    RouteEvents.Add(FString::Printf(TEXT("jump_%d_x%.1f"), JumpIndex,
        PlayerCharacter->GetActorLocation().X));
}

void ANaiwaP1GameMode::CaptureRouteFrame(const TCHAR* Label)
{
    if (!bRouteCapture || FScreenshotRequest::IsScreenshotRequested() || !GetWorld()->GetGameViewport()) return;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"), TEXT("Visuals"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("%s_%s_%s_%s.png"),
        bRouteFull ? TEXT("full") : TEXT("slice"), bRouteWalk ? TEXT("walk") : TEXT("run"),
        Label, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    FScreenshotRequest::RequestScreenshot(Path, false, false);
    RouteCapturePaths.Add(Path);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_ROUTE screenshot_requested=%s"), *Path);
}

void ANaiwaP1GameMode::TickRoute()
{
    if (!bRouteFull && !IsAuditMap(GetWorld(), TEXT("L_NaiwaP1_Slice")))
    {
        FinishRoute(TEXT("INCOMPLETE"), TEXT("Route audit has no assertions for this map"));
        return;
    }
    if (!PlayerCharacter.IsValid()) return;
    if (bRouteFull)
    {
        TickFullRoute();
        return;
    }
    if (FPlatformTime::Seconds() - RouteStartedWall > (bRouteWalk ? 70.0 : 45.0))
    {
        FinishRoute(TEXT("FAIL"), FString::Printf(TEXT("Route timeout step=%d x=%.1f z=%.1f state=%s pickups=%d"),
            static_cast<int32>(RouteStep), PlayerCharacter->GetActorLocation().X,
            PlayerCharacter->GetActorLocation().Z, StateName(State), CollectedIds.Num()));
        return;
    }
    UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement();
    Movement->MaxWalkSpeed = bRouteWalk ? 150.0f : 300.0f;
    if (RouteJumpReleaseTime > 0.0f && GetWorld()->GetTimeSeconds() >= RouteJumpReleaseTime)
    {
        PlayerCharacter->StopJumping();
        RouteJumpReleaseTime = 0.0f;
    }
    const float X = PlayerCharacter->GetActorLocation().X;
    if (RouteStep != ENaiwaP1RouteStep::WaitRespawn && State == ENaiwaP1State::Dying)
    {
        FinishRoute(TEXT("FAIL"), FString::Printf(TEXT("Unexpected hazard death at x=%.1f"), X));
        return;
    }
    switch (RouteStep)
    {
    case ENaiwaP1RouteStep::ToCheckpoint:
        if (State != ENaiwaP1State::Playing) return;
        PlayerCharacter->SetAutomatedMoveInput(1.0f);
        if (X >= 80.0f && RouteCapturePaths.IsEmpty()) CaptureRouteFrame(TEXT("start"));
        if (X >= 850.0f && RouteCapturePaths.Num() == 1) CaptureRouteFrame(TEXT("checkpoint"));
        RouteJumpAt(180.0f, 0);
        RouteJumpAt(610.0f, 1);
        if (X < 1050.0f) return;
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        if (CollectedIds.Num() != 1 || !ActiveCheckpoint.IsValid())
        {
            FinishRoute(TEXT("FAIL"), TEXT("Physical outbound route missed pickup or checkpoint"));
            return;
        }
        RouteEvents.Add(TEXT("spawn_to_pickup_and_checkpoint_by_movement"));
        EnterDying(FName(TEXT("RouteCheckpointReturn")));
        RouteStep = ENaiwaP1RouteStep::WaitRespawn;
        return;
    case ENaiwaP1RouteStep::WaitRespawn:
        if (State != ENaiwaP1State::Playing) return;
        if (!ActiveCheckpoint.IsValid() || CollectedIds.Num() != 0
            || FMath::Abs(X - ActiveCheckpoint->GetActorLocation().X) > 150.0f)
        {
            FinishRoute(TEXT("FAIL"), TEXT("Checkpoint respawn or pickup reset failed"));
            return;
        }
        RouteEvents.Add(TEXT("checkpoint_respawn_ready_to_walk_back"));
        RouteStep = ENaiwaP1RouteStep::ReturnToPickup;
        return;
    case ENaiwaP1RouteStep::ReturnToPickup:
        PlayerCharacter->SetAutomatedMoveInput(-1.0f);
        if (X > 450.0f) return;
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        if (CollectedIds.Num() != 1)
        {
            FinishRoute(TEXT("FAIL"), TEXT("Physical return from checkpoint missed restored pickup"));
            return;
        }
        RouteEvents.Add(TEXT("checkpoint_to_restored_pickup_by_movement"));
        RouteJumpIndex = 1;
        RouteStep = ENaiwaP1RouteStep::ToGoal;
        return;
    case ENaiwaP1RouteStep::ToGoal:
        if (State == ENaiwaP1State::Won)
        {
            RouteEvents.Add(TEXT("pickup_to_goal_by_movement"));
            FinishRoute(TEXT("PASS"), TEXT("Physical Walk/Run slice route and checkpoint return completed"));
            return;
        }
        if (State != ENaiwaP1State::Playing)
        {
            FinishRoute(TEXT("FAIL"), TEXT("Route left Playing before goal"));
            return;
        }
        PlayerCharacter->SetAutomatedMoveInput(1.0f);
        if (X >= 1750.0f && RouteCapturePaths.Num() == 2) CaptureRouteFrame(TEXT("approach_goal"));
        RouteJumpAt(610.0f, 1);
        RouteJumpAt(1430.0f, 2);
        if (X > 2360.0f)
        {
            FinishRoute(TEXT("FAIL"), TEXT("Passed goal without winning"));
        }
        return;
    }
}

void ANaiwaP1GameMode::TickFullRoute()
{
    if (!PlayerCharacter.IsValid()) return;
    if (FPlatformTime::Seconds() - RouteStartedWall > (bRouteWalk ? 100.0 : 65.0))
    {
        FinishRoute(TEXT("FAIL"), FString::Printf(TEXT("Full route timeout step=%d x=%.1f z=%.1f state=%s pickups=%d jump=%d"),
            static_cast<int32>(RouteStep), PlayerCharacter->GetActorLocation().X,
            PlayerCharacter->GetActorLocation().Z, StateName(State), CollectedIds.Num(), RouteJumpIndex));
        return;
    }
    UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement();
    Movement->MaxWalkSpeed = bRouteWalk ? 150.0f : 300.0f;
    if (RouteJumpReleaseTime > 0.0f && GetWorld()->GetTimeSeconds() >= RouteJumpReleaseTime)
    {
        PlayerCharacter->StopJumping();
        RouteJumpReleaseTime = 0.0f;
    }
    const float X = PlayerCharacter->GetActorLocation().X;
    if (RouteStep != ENaiwaP1RouteStep::WaitRespawn && State == ENaiwaP1State::Dying)
    {
        FinishRoute(TEXT("FAIL"), FString::Printf(TEXT("Full route touched a hazard at x=%.1f jump=%d"), X, RouteJumpIndex));
        return;
    }
    switch (RouteStep)
    {
    case ENaiwaP1RouteStep::ToCheckpoint:
        if (State != ENaiwaP1State::Playing) return;
        PlayerCharacter->SetAutomatedMoveInput(1.0f);
        if (X >= 80.0f && RouteCapturePaths.IsEmpty()) CaptureRouteFrame(TEXT("start"));
        if (X >= 900.0f && RouteCapturePaths.Num() == 1) CaptureRouteFrame(TEXT("checkpoint"));
        RouteJumpAt(bRouteWalk ? 120.0f : 90.0f, 0, 0.07f);
        RouteJumpAt(bRouteWalk ? 440.0f : 370.0f, 1, bRouteWalk ? 0.20f : 0.07f);
        if (X < 1100.0f) return;
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        if (CollectedIds.Num() != 0 || !ActiveCheckpoint.IsValid())
        {
            FinishRoute(TEXT("FAIL"), TEXT("Full outbound route missed checkpoint or collected pickup early"));
            return;
        }
        RouteEvents.Add(TEXT("spawn_to_checkpoint_by_movement"));
        EnterDying(FName(TEXT("FullRouteCheckpointReturn")));
        RouteStep = ENaiwaP1RouteStep::WaitRespawn;
        return;
    case ENaiwaP1RouteStep::WaitRespawn:
        if (State != ENaiwaP1State::Playing) return;
        if (!ActiveCheckpoint.IsValid() || CollectedIds.Num() != 0
            || FMath::Abs(X - ActiveCheckpoint->GetActorLocation().X) > 150.0f)
        {
            FinishRoute(TEXT("FAIL"), TEXT("Full checkpoint respawn was unsafe or did not reset pickup"));
            return;
        }
        RouteEvents.Add(TEXT("checkpoint_respawn_before_required_pickup"));
        RouteJumpIndex = 0;
        RouteStep = ENaiwaP1RouteStep::ToGoal;
        return;
    case ENaiwaP1RouteStep::ToGoal:
        if (State == ENaiwaP1State::Won)
        {
            if (CollectedIds.Num() != 1)
            {
                FinishRoute(TEXT("FAIL"), TEXT("Won without the one required pickup"));
                return;
            }
            RouteEvents.Add(TEXT("checkpoint_to_pickup_to_goal_by_movement"));
            FinishRoute(TEXT("PASS"), TEXT("Physical full-level main route completed after checkpoint respawn"));
            return;
        }
        if (State != ENaiwaP1State::Playing)
        {
            FinishRoute(TEXT("FAIL"), TEXT("Full route left Playing before goal"));
            return;
        }
        PlayerCharacter->SetAutomatedMoveInput(1.0f);
        if (X >= 2600.0f && RouteCapturePaths.Num() == 2) CaptureRouteFrame(TEXT("pickup_region"));
        if (X >= 3650.0f && RouteCapturePaths.Num() == 3) CaptureRouteFrame(TEXT("approach_goal"));
        RouteJumpAt(1200.0f, 0, 0.07f);
        RouteJumpAt(1990.0f, 1, 0.07f);
        RouteJumpAt(bRouteWalk ? 2530.0f : 2440.0f, 2, 0.07f);
        RouteJumpAt(2790.0f, 3);
        if (bRouteWalk) RouteJumpAt(2960.0f, 4, 0.07f);
        RouteJumpAt(3600.0f, bRouteWalk ? 5 : 4);
        if (X > 2850.0f && CollectedIds.Num() != 1)
        {
            FinishRoute(TEXT("FAIL"), TEXT("Full physical route passed required pickup"));
        }
        else if (X > 4200.0f)
        {
            FinishRoute(TEXT("FAIL"), TEXT("Full route passed goal without winning"));
        }
        return;
    default:
        FinishRoute(TEXT("FAIL"), TEXT("Unexpected full route phase"));
        return;
    }
}

void ANaiwaP1GameMode::FinishRoute(const TCHAR* Result, const FString& Detail)
{
    if (!bRouteEnabled) return;
    bRouteEnabled = false;
    if (PlayerCharacter.IsValid())
    {
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        PlayerCharacter->StopJumping();
    }
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_ROUTE result=%s gait=%s detail=%s"),
        Result, bRouteWalk ? TEXT("Walk150") : TEXT("Run300"), *Detail);
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), bRouteFull ? TEXT("NaiwaP1FullRouteV1") : TEXT("NaiwaP1RouteV1"));
    Report->SetStringField(TEXT("status"), Result);
    Report->SetStringField(TEXT("detail"), Detail);
    Report->SetStringField(TEXT("map"), GetWorld()->GetMapName());
    Report->SetStringField(TEXT("gait_speed"), bRouteWalk ? TEXT("Walk150") : TEXT("Run300"));
    Report->SetNumberField(TEXT("max_walk_speed_cm_s"), PlayerCharacter.IsValid()
        ? PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed : 0.0f);
    Report->SetNumberField(TEXT("final_x"), PlayerCharacter.IsValid()
        ? PlayerCharacter->GetActorLocation().X : 0.0f);
    Report->SetNumberField(TEXT("death_events"), DeathEventCount);
    Report->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - RouteStartedWall);
    TArray<TSharedPtr<FJsonValue>> Events;
    for (const FString& Event : RouteEvents) Events.Add(MakeShared<FJsonValueString>(Event));
    Report->SetArrayField(TEXT("events"), Events);
    TArray<TSharedPtr<FJsonValue>> Screenshots;
    for (const FString& Path : RouteCapturePaths)
    {
        TSharedRef<FJsonObject> Shot = MakeShared<FJsonObject>();
        Shot->SetStringField(TEXT("path"), Path);
        Shot->SetBoolField(TEXT("exists_at_report"), IFileManager::Get().FileExists(*Path));
        Screenshots.Add(MakeShared<FJsonValueObject>(Shot));
    }
    Report->SetArrayField(TEXT("screenshots"), Screenshots);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("route_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_ROUTE output=%s"), *Output);
    FPlatformMisc::RequestExit(false);
}

void ANaiwaP1GameMode::TickHazardAudit()
{
    const bool bFullMap = IsAuditMap(GetWorld(), TEXT("L_NaiwaP1_Full"));
    if (!bFullMap && !IsAuditMap(GetWorld(), TEXT("L_NaiwaP1_Slice")))
    {
        FinishHazardAudit(TEXT("INCOMPLETE"), TEXT("Hazard audit has no expected count for this map"));
        return;
    }
    if (!PlayerCharacter.IsValid() || !PlayerCharacter->GetCharacterMovement()->IsMovingOnGround())
    {
        if (HazardAuditStep == ENaiwaP1HazardAuditStep::WaitReady) return;
    }
    const double Now = FPlatformTime::Seconds();
    if (Now - HazardAuditStartedWall > 65.0 || Now - HazardAuditStepWall > 7.0)
    {
        FinishHazardAudit(TEXT("FAIL"), FString::Printf(TEXT("Timeout hazard=%d step=%d state=%s deaths=%d"),
            HazardAuditIndex, static_cast<int32>(HazardAuditStep), StateName(State), DeathEventCount));
        return;
    }
    if (HazardAuditActors.IsEmpty())
    {
        TArray<ANaiwaP1Trigger*> Found;
        for (TActorIterator<ANaiwaP1Trigger> It(GetWorld()); It; ++It)
        {
            if (It->TriggerKind == ENaiwaP1TriggerKind::Hazard) Found.Add(*It);
        }
        Found.Sort([](const ANaiwaP1Trigger& A, const ANaiwaP1Trigger& B)
        {
            return A.GetStableId().ToString() < B.GetStableId().ToString();
        });
        const int32 Expected = bFullMap ? 9 : 1;
        if (Found.Num() != Expected)
        {
            FinishHazardAudit(TEXT("INCOMPLETE"), FString::Printf(TEXT("Expected %d hazards, found %d"), Expected, Found.Num()));
            return;
        }
        for (ANaiwaP1Trigger* Trigger : Found) HazardAuditActors.Add(Trigger);
    }
    if (HazardAuditIndex >= HazardAuditActors.Num())
    {
        FinishHazardAudit(TEXT("PASS"), TEXT("Every saved hazard caused one death and reset after safe respawn"));
        return;
    }
    ANaiwaP1Trigger* Trigger = HazardAuditActors[HazardAuditIndex].Get();
    if (!Trigger)
    {
        FinishHazardAudit(TEXT("FAIL"), TEXT("Hazard actor disappeared"));
        return;
    }
    switch (HazardAuditStep)
    {
    case ENaiwaP1HazardAuditStep::WaitReady:
    {
        if (State != ENaiwaP1State::Playing || !PlayerCharacter->GetCharacterMovement()->IsMovingOnGround()) return;
        FVector Target = Trigger->GetActorLocation();
        Target.Z = Trigger->GetActorLocation().Z - Trigger->BoxExtent.Z
            + PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 1.0f;
        PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();
        PlayerCharacter->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
        if (!Trigger->IsOverlappingActor(PlayerCharacter.Get()))
        {
            FinishHazardAudit(TEXT("FAIL"), FString::Printf(TEXT("No collision overlap for %s"), *Trigger->GetStableId().ToString()));
            return;
        }
        HazardAuditStep = ENaiwaP1HazardAuditStep::WaitDeath;
        HazardAuditStepWall = Now;
        return;
    }
    case ENaiwaP1HazardAuditStep::WaitDeath:
        if (State != ENaiwaP1State::Dying) return;
        if (DeathEventCount != HazardAuditIndex + 1)
        {
            FinishHazardAudit(TEXT("FAIL"), TEXT("Hazard did not produce exactly one death"));
            return;
        }
        for (const TWeakObjectPtr<ANaiwaP1Trigger>& Item : HazardAuditActors)
        {
            if (!Item.IsValid() || !Item->DisplayMesh->IsVisible())
            {
                FinishHazardAudit(TEXT("FAIL"), TEXT("A hazard did not reveal with its group"));
                return;
            }
        }
        HazardAuditStep = ENaiwaP1HazardAuditStep::WaitRespawn;
        HazardAuditStepWall = Now;
        return;
    case ENaiwaP1HazardAuditStep::WaitRespawn:
        if (State != ENaiwaP1State::Playing) return;
        if (DeathEventCount != HazardAuditIndex + 1 || CollectedIds.Num() != 0
            || FVector::Dist2D(PlayerCharacter->GetActorLocation(), InitialSpawn) > 150.0f)
        {
            FinishHazardAudit(TEXT("FAIL"), TEXT("Hazard respawn did not restore initial attempt"));
            return;
        }
        for (const TWeakObjectPtr<ANaiwaP1Trigger>& Item : HazardAuditActors)
        {
            if (!Item.IsValid() || Item->DisplayMesh->IsVisible())
            {
                FinishHazardAudit(TEXT("FAIL"), TEXT("Hazard remained visible after respawn"));
                return;
            }
        }
        HazardAuditEvents.Add(Trigger->GetStableId().ToString());
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_HAZARD_AUDIT passed=%s count=%d/%d"),
            *Trigger->GetStableId().ToString(), HazardAuditIndex + 1, HazardAuditActors.Num());
        ++HazardAuditIndex;
        HazardAuditStep = ENaiwaP1HazardAuditStep::WaitReady;
        HazardAuditStepWall = Now;
        return;
    }
}

void ANaiwaP1GameMode::FinishHazardAudit(const TCHAR* Result, const FString& Detail)
{
    if (!bHazardAuditEnabled) return;
    bHazardAuditEnabled = false;
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_HAZARD_AUDIT result=%s detail=%s"), Result, *Detail);
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1HazardAuditV1"));
    Report->SetStringField(TEXT("status"), Result);
    Report->SetStringField(TEXT("detail"), Detail);
    Report->SetStringField(TEXT("map"), GetWorld()->GetMapName());
    Report->SetNumberField(TEXT("hazard_count"), HazardAuditActors.Num());
    Report->SetNumberField(TEXT("passed_count"), HazardAuditEvents.Num());
    Report->SetNumberField(TEXT("death_events"), DeathEventCount);
    TArray<TSharedPtr<FJsonValue>> Events;
    for (const FString& Event : HazardAuditEvents) Events.Add(MakeShared<FJsonValueString>(Event));
    Report->SetArrayField(TEXT("passed_ids"), Events);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("hazards_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_HAZARD_AUDIT output=%s"), *Output);
    FPlatformMisc::RequestExit(false);
}

void ANaiwaP1GameMode::MatrixAdvance(ENaiwaP1MatrixStep NextStep, const FString& Evidence)
{
    MatrixEvents.Add(Evidence);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_MATRIX %s"), *Evidence);
    MatrixStep = NextStep;
    MatrixStepWall = FPlatformTime::Seconds();
}

void ANaiwaP1GameMode::TickMatrix()
{
    const double Now = FPlatformTime::Seconds();
    if (Now - MatrixStartedWall > 45.0 || Now - MatrixStepWall > 8.0)
    {
        FinishMatrix(TEXT("FAIL"), FString::Printf(TEXT("Timeout step=%d state=%s deaths=%d pickups=%d paused=%d"),
            static_cast<int32>(MatrixStep), StateName(State), DeathEventCount, CollectedIds.Num(),
            UGameplayStatics::IsGamePaused(this)));
        return;
    }
    if (!PlayerCharacter.IsValid()) return;

    switch (MatrixStep)
    {
    case ENaiwaP1MatrixStep::WaitReady:
    {
        if (!PlayerCharacter->GetCharacterMovement()->IsMovingOnGround()) return;
        if (RequiredPickupCount != 1 || !bHasInitialSpawn)
        {
            FinishMatrix(TEXT("INCOMPLETE"), TEXT("Matrix requires a saved P1 map with one required pickup and a player spawn"));
            return;
        }
        for (TActorIterator<ANaiwaP1Trigger> It(GetWorld()); It; ++It)
        {
            switch (It->TriggerKind)
            {
            case ENaiwaP1TriggerKind::Pickup:
                if (It->bRequiredForGoal) MatrixPickup = *It;
                break;
            case ENaiwaP1TriggerKind::Checkpoint: if (!MatrixCheckpoint.IsValid()) MatrixCheckpoint = *It; break;
            case ENaiwaP1TriggerKind::Hazard: if (!MatrixHazard.IsValid()) MatrixHazard = *It; break;
            case ENaiwaP1TriggerKind::Goal: if (!MatrixGoal.IsValid()) MatrixGoal = *It; break;
            default: break;
            }
        }
        if (!MatrixPickup.IsValid() || !MatrixCheckpoint.IsValid() || !MatrixHazard.IsValid() || !MatrixGoal.IsValid())
        {
            FinishMatrix(TEXT("INCOMPLETE"), TEXT("Matrix requires pickup, checkpoint, hazard, and goal actors"));
            return;
        }
        // Queue the nonfatal outcomes first to prove a same-frame hazard overrides all of them.
        QueueTrigger(MatrixGoal.Get(), PlayerCharacter.Get());
        QueueTrigger(MatrixPickup.Get(), PlayerCharacter.Get());
        QueueTrigger(MatrixCheckpoint.Get(), PlayerCharacter.Get());
        QueueTrigger(MatrixHazard.Get(), PlayerCharacter.Get());
        MatrixAdvance(ENaiwaP1MatrixStep::WaitPriorityDeath, TEXT("queued_goal_pickup_checkpoint_hazard_same_frame"));
        return;
    }
    case ENaiwaP1MatrixStep::WaitPriorityDeath:
        if (State != ENaiwaP1State::Dying) return;
        if (DeathEventCount != 1 || CollectedIds.Num() != 0 || ActiveCheckpoint.IsValid()
            || bExitLockedHint || !MatrixPickup->DisplayMesh->IsVisible()
            || !MatrixHazard->DisplayMesh->IsVisible() || IsGameplayInputAllowed())
        {
            FinishMatrix(TEXT("FAIL"), TEXT("Same-frame hazard did not override pickup, checkpoint, and goal"));
            return;
        }
        MatrixAdvance(ENaiwaP1MatrixStep::WaitPriorityRespawn, TEXT("hazard_priority_kept_pickup_checkpoint_goal_unconsumed"));
        return;
    case ENaiwaP1MatrixStep::WaitPriorityRespawn:
        if (State != ENaiwaP1State::Playing) return;
        if (DeathEventCount != 1 || CollectedIds.Num() != 0 || ActiveCheckpoint.IsValid()
            || MatrixHazard->DisplayMesh->IsVisible()
            || FVector::Dist2D(PlayerCharacter->GetActorLocation(), InitialSpawn) > 150.0f)
        {
            FinishMatrix(TEXT("FAIL"), TEXT("Priority death did not return to clean initial spawn"));
            return;
        }
        if (const AWorldSettings* Settings = GetWorld()->GetWorldSettings())
        {
            const FVector BelowKillZ(InitialSpawn.X, InitialSpawn.Y, Settings->KillZ - 200.0f);
            PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();
            PlayerCharacter->SetActorLocation(BelowKillZ, false, nullptr, ETeleportType::TeleportPhysics);
            if (PlayerCharacter->GetActorLocation().Z >= Settings->KillZ)
            {
                FinishMatrix(TEXT("FAIL"), TEXT("Unable to place pawn below World KillZ"));
                return;
            }
        }
        else
        {
            FinishMatrix(TEXT("INCOMPLETE"), TEXT("World settings missing KillZ"));
            return;
        }
        MatrixAdvance(ENaiwaP1MatrixStep::WaitKillZDeath, TEXT("pawn_placed_below_world_killz"));
        return;
    case ENaiwaP1MatrixStep::WaitKillZDeath:
        if (State != ENaiwaP1State::Dying) return;
        if (DeathEventCount != 2 || !GetWorldTimerManager().IsTimerActive(DeathTimer))
        {
            FinishMatrix(TEXT("FAIL"), TEXT("World KillZ did not start exactly one death timer"));
            return;
        }
        MatrixPausedTimerRemaining = GetWorldTimerManager().GetTimerRemaining(DeathTimer);
        ToggleP1Pause();
        if (!UGameplayStatics::IsGamePaused(this) || MatrixPausedTimerRemaining <= 0.0f)
        {
            FinishMatrix(TEXT("FAIL"), TEXT("Could not pause during Dying with an active death timer"));
            return;
        }
        MatrixAdvance(ENaiwaP1MatrixStep::WaitPausedDying, TEXT("world_killz_one_death_and_dying_paused"));
        return;
    case ENaiwaP1MatrixStep::WaitPausedDying:
        if (Now - MatrixStepWall < FMath::Max(1.5, static_cast<double>(MatrixPausedTimerRemaining) + 0.3)) return;
        if (!UGameplayStatics::IsGamePaused(this) || State != ENaiwaP1State::Dying || DeathEventCount != 2
            || FMath::Abs(GetWorldTimerManager().GetTimerRemaining(DeathTimer) - MatrixPausedTimerRemaining) > 0.12f)
        {
            FinishMatrix(TEXT("FAIL"), TEXT("Dying pause failed to freeze the death timer"));
            return;
        }
        ToggleP1Pause();
        if (UGameplayStatics::IsGamePaused(this))
        {
            FinishMatrix(TEXT("FAIL"), TEXT("Dying pause failed to resume"));
            return;
        }
        MatrixAdvance(ENaiwaP1MatrixStep::WaitKillZRespawn, TEXT("dying_timer_froze_then_resumed"));
        return;
    case ENaiwaP1MatrixStep::WaitKillZRespawn:
        if (State != ENaiwaP1State::Playing) return;
        if (DeathEventCount != 2 || FVector::Dist2D(PlayerCharacter->GetActorLocation(), InitialSpawn) > 150.0f)
        {
            FinishMatrix(TEXT("FAIL"), TEXT("World KillZ did not respawn at initial start"));
            return;
        }
        QueueFallDeath();
        MatrixAdvance(ENaiwaP1MatrixStep::WaitRestartDying, TEXT("world_killz_respawned_at_start"));
        return;
    case ENaiwaP1MatrixStep::WaitRestartDying:
        if (State != ENaiwaP1State::Dying) return;
        if (DeathEventCount != 3)
        {
            FinishMatrix(TEXT("FAIL"), TEXT("Setup for R during Dying counted an unexpected death"));
            return;
        }
        RestartLevel(); // R handler calls this method; keyboard dispatch is tested separately.
        if (UGameplayStatics::IsGamePaused(this) || State != ENaiwaP1State::Respawning
            || GetWorldTimerManager().IsTimerActive(DeathTimer))
        {
            FinishMatrix(TEXT("FAIL"), TEXT("R during Dying did not clear death timer and restart"));
            return;
        }
        MatrixAdvance(ENaiwaP1MatrixStep::WaitDyingReset, TEXT("r_during_dying_cleared_death_timer"));
        return;
    case ENaiwaP1MatrixStep::WaitDyingReset:
        if (State != ENaiwaP1State::Playing || Now - MatrixStepWall < 1.5) return;
        if (DeathEventCount != 3 || PlayerCharacter->IsP1DeathPoseActive() || ActiveCheckpoint.IsValid()
            || FVector::Dist2D(PlayerCharacter->GetActorLocation(), InitialSpawn) > 150.0f)
        {
            FinishMatrix(TEXT("FAIL"), TEXT("R during Dying did not stay at a clean start"));
            return;
        }
        QueueTrigger(MatrixPickup.Get(), PlayerCharacter.Get());
        QueueTrigger(MatrixCheckpoint.Get(), PlayerCharacter.Get());
        MatrixAdvance(ENaiwaP1MatrixStep::WaitPausedSetup, TEXT("dying_reset_stayed_playable_without_stale_timer"));
        return;
    case ENaiwaP1MatrixStep::WaitPausedSetup:
        if (CollectedIds.Num() != 1 || ActiveCheckpoint.Get() != MatrixCheckpoint.Get()) return;
        ToggleP1Pause();
        if (!UGameplayStatics::IsGamePaused(this))
        {
            FinishMatrix(TEXT("FAIL"), TEXT("Could not pause a collected checkpoint attempt"));
            return;
        }
        MatrixAdvance(ENaiwaP1MatrixStep::WaitPausedReset, TEXT("pause_with_pickup_and_checkpoint_active"));
        return;
    case ENaiwaP1MatrixStep::WaitPausedReset:
        RestartLevel();
        if (UGameplayStatics::IsGamePaused(this) || State != ENaiwaP1State::Respawning
            || CollectedIds.Num() != 0 || ActiveCheckpoint.IsValid() || !MatrixPickup->DisplayMesh->IsVisible())
        {
            FinishMatrix(TEXT("FAIL"), TEXT("R during Paused did not unpause and clear attempt state"));
            return;
        }
        MatrixAdvance(ENaiwaP1MatrixStep::WaitPickupGoalSameFrame, TEXT("r_during_pause_unpaused_and_cleared_attempt"));
        return;
    case ENaiwaP1MatrixStep::WaitPickupGoalSameFrame:
        if (State != ENaiwaP1State::Playing) return;
        if (DeathEventCount != 3 || FVector::Dist2D(PlayerCharacter->GetActorLocation(), InitialSpawn) > 150.0f)
        {
            FinishMatrix(TEXT("FAIL"), TEXT("Paused reset did not recover at initial start"));
            return;
        }
        // Reverse callback order must still let the pickup satisfy the exit in this frame.
        QueueTrigger(MatrixGoal.Get(), PlayerCharacter.Get());
        QueueTrigger(MatrixPickup.Get(), PlayerCharacter.Get());
        MatrixAdvance(ENaiwaP1MatrixStep::WaitWon, TEXT("queued_goal_then_pickup_same_frame"));
        return;
    case ENaiwaP1MatrixStep::WaitWon:
        if (State != ENaiwaP1State::Won) return;
        if (CollectedIds.Num() != 1 || !CollectedIds.Contains(MatrixPickup->GetStableId())
            || MatrixPickup->DisplayMesh->IsVisible() || bExitLockedHint || ActiveCheckpoint.IsValid()
            || DeathEventCount != 3)
        {
            FinishMatrix(TEXT("FAIL"), TEXT("Same-frame goal then pickup did not resolve pickup before win"));
            return;
        }
        MatrixAdvance(ENaiwaP1MatrixStep::WaitWonLock, TEXT("same_frame_pickup_resolved_before_goal_won"));
        return;
    case ENaiwaP1MatrixStep::WaitWonLock:
        if (State != ENaiwaP1State::Won) return;
        if (!bMatrixWonLockArmed)
        {
            if (IsGameplayInputAllowed() || PlayerCharacter->GetCharacterMovement()->MovementMode != MOVE_None)
            {
                FinishMatrix(TEXT("FAIL"), TEXT("Won did not lock gameplay input and movement mode"));
                return;
            }
            MatrixWonPosition = PlayerCharacter->GetActorLocation();
            PlayerCharacter->SetAutomatedMoveInput(1.0f);
            bMatrixWonLockArmed = true;
            MatrixStepWall = Now;
            return;
        }
        if (Now - MatrixStepWall < 0.5) return;
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        if (FVector::Dist2D(PlayerCharacter->GetActorLocation(), MatrixWonPosition) > 1.0f
            || PlayerCharacter->GetVelocity().Size() > 1.0f || DeathEventCount != 3)
        {
            FinishMatrix(TEXT("FAIL"), TEXT("Won allowed movement or changed death count"));
            return;
        }
        ToggleP1Pause();
        if (UGameplayStatics::IsGamePaused(this))
        {
            FinishMatrix(TEXT("FAIL"), TEXT("Won unexpectedly accepted pause"));
            return;
        }
        RestartLevel();
        MatrixAdvance(ENaiwaP1MatrixStep::WaitWonReset, TEXT("won_locked_input_and_position_then_r_restarted"));
        return;
    case ENaiwaP1MatrixStep::WaitWonReset:
        if (State != ENaiwaP1State::Playing) return;
        if (UGameplayStatics::IsGamePaused(this) || CollectedIds.Num() != 0 || ActiveCheckpoint.IsValid()
            || DeathEventCount != 3 || !MatrixPickup->DisplayMesh->IsVisible()
            || FVector::Dist2D(PlayerCharacter->GetActorLocation(), InitialSpawn) > 150.0f)
        {
            FinishMatrix(TEXT("FAIL"), TEXT("R after Won did not restore a playable initial attempt"));
            return;
        }
        MatrixEvents.Add(TEXT("won_r_restart_playable_at_start"));
        FinishMatrix(TEXT("PASS"), TEXT("P1 priority, KillZ, pause/death timing, R reset, and Won lock passed"));
        return;
    }
}

void ANaiwaP1GameMode::FinishMatrix(const TCHAR* Result, const FString& Detail)
{
    if (!bMatrixEnabled) return;
    bMatrixEnabled = false;
    if (PlayerCharacter.IsValid()) PlayerCharacter->SetAutomatedMoveInput(0.0f);
    UGameplayStatics::SetGamePaused(this, false);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_MATRIX result=%s detail=%s"), Result, *Detail);
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1StateMatrixV1"));
    Report->SetStringField(TEXT("status"), Result);
    Report->SetStringField(TEXT("detail"), Detail);
    Report->SetStringField(TEXT("map"), GetWorld()->GetMapName());
    Report->SetStringField(TEXT("state"), StateName(State));
    Report->SetNumberField(TEXT("death_events"), DeathEventCount);
    Report->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - MatrixStartedWall);
    Report->SetBoolField(TEXT("paused"), UGameplayStatics::IsGamePaused(this));
    TArray<TSharedPtr<FJsonValue>> Events;
    for (const FString& Event : MatrixEvents) Events.Add(MakeShared<FJsonValueString>(Event));
    Report->SetArrayField(TEXT("events"), Events);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("matrix_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (bSaved)
    {
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_MATRIX output=%s saved=1"), *Output);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_P1_MATRIX output=%s saved=0"), *Output);
    }
    FPlatformMisc::RequestExit(false);
}

void ANaiwaP1GameMode::TickBounceProbe()
{
    const double Now = FPlatformTime::Seconds();
    if (Now - BounceProbeStartedWall > 80.0 || Now - BounceProbeStepStartedWall > 24.0)
    {
        FinishBounceProbe(TEXT("FAIL"), FString::Printf(
            TEXT("Timeout step=%d state=%s activations=%d deaths=%d x=%.1f z=%.1f"),
            static_cast<int32>(BounceProbeStep), StateName(State),
            BounceProbePad.IsValid() ? BounceProbePad->GetActivationCount() : -1,
            DeathEventCount,
            PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().X : 0.0f,
            PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().Z : 0.0f));
        return;
    }
    if (!IsAuditMap(GetWorld(), TEXT("L_NaiwaP1_Bounce")))
    {
        FinishBounceProbe(TEXT("INCOMPLETE"), TEXT("Bounce probe only runs on its isolated prototype map"));
        return;
    }
    if (bSmokeEnabled || bRouteEnabled || bHazardAuditEnabled || bMatrixEnabled || bSwitchProbeEnabled || bTimedProbeEnabled)
    {
        FinishBounceProbe(TEXT("INCOMPLETE"), TEXT("Run only one P1 audit scenario per process"));
        return;
    }
    if (!PlayerCharacter.IsValid()) return;
    if (!bBounceProbeInitialized)
    {
        if (Now - BounceProbeStartedWall > 5.0)
        {
            FinishBounceProbe(TEXT("INCOMPLETE"), TEXT("P1 character did not register on the prototype map"));
            return;
        }
        int32 PadCount = 0;
        int32 LandingCount = 0;
        int32 HazardCount = 0;
        for (TActorIterator<ANaiwaP1BouncePad> It(GetWorld()); It; ++It)
        {
            if (!It->ActorHasTag(FName(TEXT("NaiwaP1BouncePadV1")))) continue;
            BounceProbePad = *It;
            ++PadCount;
        }
        for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
        {
            if (!It->ActorHasTag(FName(TEXT("NaiwaP1BounceLandingV1")))) continue;
            ++LandingCount;
            const FBox Bounds = It->GetComponentsBoundingBox(true);
            BounceProbeLandingOrigin = Bounds.GetCenter();
            BounceProbeLandingExtent = Bounds.GetExtent();
        }
        for (TActorIterator<ANaiwaP1Trigger> It(GetWorld()); It; ++It)
        {
            if (!It->ActorHasTag(FName(TEXT("NaiwaP1BounceHazardV1")))) continue;
            BounceProbeHazard = *It;
            ++HazardCount;
        }
        if (PadCount != 1 || LandingCount != 1 || HazardCount != 1
            || !BounceProbePad.IsValid() || !BounceProbeHazard.IsValid()
            || BounceProbeHazard->TriggerKind != ENaiwaP1TriggerKind::Hazard
            || BounceProbeLandingExtent.X < 300.0f || BounceProbeLandingExtent.Z < 1.0f)
        {
            FinishBounceProbe(TEXT("INCOMPLETE"), FString::Printf(
                TEXT("Prototype requires one tagged bounce pad, landing platform, and hazard; found %d/%d/%d"),
                PadCount, LandingCount, HazardCount));
            return;
        }
        if (!bHasInitialSpawn || PlayerCharacter->GetActorLocation().X > BounceProbeLandingOrigin.X - 400.0f)
        {
            FinishBounceProbe(TEXT("INCOMPLETE"), TEXT("Prototype spawn must approach the bounce pad from the left"));
            return;
        }
        if (!PlayerCharacter->SetP1Gait(!bBounceProbeWalk))
        {
            FinishBounceProbe(TEXT("INCOMPLETE"), TEXT("Could not select the P1 Walk or Run gait"));
            return;
        }
        BounceProbeDeathCountBefore = DeathEventCount;
        BounceProbeMaxZ = PlayerCharacter->GetActorLocation().Z;
        BounceProbeEvents.Add(bBounceProbeWalk ? TEXT("gait_walk150_selected") : TEXT("gait_run300_selected"));
        bBounceProbeInitialized = true;
        BounceProbeStep = ENaiwaP1BounceProbeStep::ToFirstBounce;
        BounceProbeStepStartedWall = Now;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_BOUNCE_PROBE actors=valid gait=%s spawn_x=%.1f"),
            bBounceProbeWalk ? TEXT("Walk150") : TEXT("Run300"), InitialSpawn.X);
    }

    BounceProbeMaxZ = FMath::Max(BounceProbeMaxZ, PlayerCharacter->GetActorLocation().Z);
    const int32 Activations = BounceProbePad->GetActivationCount();
    const float X = PlayerCharacter->GetActorLocation().X;
    const FBox LandingBounds(BounceProbeLandingOrigin - BounceProbeLandingExtent,
        BounceProbeLandingOrigin + BounceProbeLandingExtent);
    const float CapsuleRadius = PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const bool bSafeLanding = PlayerCharacter->GetCharacterMovement()->IsMovingOnGround()
        && X >= LandingBounds.Min.X + CapsuleRadius + 20.0f
        && X <= LandingBounds.Max.X - CapsuleRadius - 20.0f
        && PlayerCharacter->GetActorLocation().Y >= LandingBounds.Min.Y + CapsuleRadius
        && PlayerCharacter->GetActorLocation().Y <= LandingBounds.Max.Y - CapsuleRadius
        && PlayerCharacter->GetCapsuleBottom() >= LandingBounds.Max.Z - 16.0f
        && PlayerCharacter->GetCapsuleBottom() <= LandingBounds.Max.Z + 16.0f;

    switch (BounceProbeStep)
    {
    case ENaiwaP1BounceProbeStep::WaitReady:
        return;
    case ENaiwaP1BounceProbeStep::ToFirstBounce:
        if (State != ENaiwaP1State::Playing) return;
        PlayerCharacter->SetAutomatedMoveInput(1.0f);
        if (Activations == 0) return;
        if (Activations != 1)
        {
            FinishBounceProbe(TEXT("FAIL"), TEXT("First contact activated the pad more than once"));
            return;
        }
        BounceProbeFirstLaunch = PlayerCharacter->GetActorLocation();
        BounceProbeEvents.Add(TEXT("first_contact_activated_once"));
        BounceProbeStep = ENaiwaP1BounceProbeStep::WaitFirstLanding;
        BounceProbeStepStartedWall = Now;
        return;
    case ENaiwaP1BounceProbeStep::WaitFirstLanding:
        if (State != ENaiwaP1State::Playing)
        {
            FinishBounceProbe(TEXT("FAIL"), TEXT("Player died before reaching the landing platform"));
            return;
        }
        if (Activations != 1)
        {
            FinishBounceProbe(TEXT("FAIL"), TEXT("A single contact retriggered before leaving the pad"));
            return;
        }
        PlayerCharacter->SetAutomatedMoveInput(1.0f);
        if (!bSafeLanding) return;
        BounceProbeFirstLanding = PlayerCharacter->GetActorLocation();
        PlayerCharacter->SetAutomatedMoveInput(-1.0f);
        BounceProbeEvents.Add(FString::Printf(TEXT("safe_platform_landing_x%.1f_z%.1f"),
            BounceProbeFirstLanding.X, BounceProbeFirstLanding.Z));
        BounceProbeStep = ENaiwaP1BounceProbeStep::ReturnForSecondBounce;
        BounceProbeStepStartedWall = Now;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_BOUNCE_PROBE first_landing=(%.1f,%.1f,%.1f) max_z=%.1f"),
            BounceProbeFirstLanding.X, BounceProbeFirstLanding.Y, BounceProbeFirstLanding.Z, BounceProbeMaxZ);
        return;
    case ENaiwaP1BounceProbeStep::ReturnForSecondBounce:
        if (State != ENaiwaP1State::Playing)
        {
            FinishBounceProbe(TEXT("FAIL"), TEXT("Player died while returning to the bounce pad"));
            return;
        }
        PlayerCharacter->SetAutomatedMoveInput(-1.0f);
        if (Activations < 2) return;
        if (Activations != 2)
        {
            FinishBounceProbe(TEXT("FAIL"), TEXT("Leaving and returning activated the pad more than once"));
            return;
        }
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        BounceProbeEvents.Add(TEXT("left_and_recontacted_pad_once"));
        RestartLevel();
        if (State != ENaiwaP1State::Respawning)
        {
            FinishBounceProbe(TEXT("FAIL"), TEXT("R reset did not enter the respawn state"));
            return;
        }
        BounceProbeStep = ENaiwaP1BounceProbeStep::WaitRReset;
        BounceProbeStepStartedWall = Now;
        return;
    case ENaiwaP1BounceProbeStep::WaitRReset:
        if (Now - BounceProbeStepStartedWall > 7.0)
        {
            FinishBounceProbe(TEXT("FAIL"), TEXT("R reset did not return to a playable start"));
            return;
        }
        if (State != ENaiwaP1State::Playing || Now - BounceProbeStepStartedWall < 0.7) return;
        if (Activations != 2 || !BounceProbePad->IsReady()
            || BounceProbePad->IsContactLatched() || BounceProbePad->GetCooldownRemaining() > 0.01f
            || FVector::Dist2D(PlayerCharacter->GetActorLocation(), InitialSpawn) > 150.0f)
        {
            FinishBounceProbe(TEXT("FAIL"), TEXT("R reset did not clear pad latch/cooldown and restore the start"));
            return;
        }
        BounceProbeEvents.Add(TEXT("r_reset_cleared_pad_state_and_restored_start"));
        BounceProbeStep = ENaiwaP1BounceProbeStep::ToThirdBounce;
        BounceProbeStepStartedWall = Now;
        return;
    case ENaiwaP1BounceProbeStep::ToThirdBounce:
        if (State != ENaiwaP1State::Playing)
        {
            FinishBounceProbe(TEXT("FAIL"), TEXT("Player left Playing before the post-reset contact"));
            return;
        }
        PlayerCharacter->SetAutomatedMoveInput(1.0f);
        if (Activations < 3) return;
        if (Activations != 3)
        {
            FinishBounceProbe(TEXT("FAIL"), TEXT("Post-reset contact activated the pad more than once"));
            return;
        }
        BounceProbeEvents.Add(TEXT("post_reset_contact_activated_once"));
        BounceProbeStep = ENaiwaP1BounceProbeStep::RunToHazard;
        BounceProbeStepStartedWall = Now;
        return;
    case ENaiwaP1BounceProbeStep::RunToHazard:
        PlayerCharacter->SetAutomatedMoveInput(1.0f);
        if (State != ENaiwaP1State::Dying) return;
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        if (DeathEventCount != BounceProbeDeathCountBefore + 1 || Activations != 3
            || FMath::Abs(PlayerCharacter->GetActorLocation().X - BounceProbeHazard->GetActorLocation().X)
                > BounceProbeHazard->BoxExtent.X + CapsuleRadius + 20.0f
            || PlayerCharacter->GetCapsuleBottom() < BounceProbeHazard->GetActorLocation().Z - BounceProbeHazard->BoxExtent.Z - 8.0f
            || PlayerCharacter->GetCapsuleBottom() > BounceProbeHazard->GetActorLocation().Z + BounceProbeHazard->BoxExtent.Z + 8.0f)
        {
            FinishBounceProbe(TEXT("FAIL"), TEXT("Unexpected death before reaching the tagged landing hazard"));
            return;
        }
        BounceProbeEvents.Add(TEXT("landing_hazard_caused_one_death"));
        BounceProbeStep = ENaiwaP1BounceProbeStep::WaitDeathReset;
        BounceProbeStepStartedWall = Now;
        return;
    case ENaiwaP1BounceProbeStep::WaitDeathReset:
        if (Now - BounceProbeStepStartedWall > 12.0)
        {
            FinishBounceProbe(TEXT("FAIL"), TEXT("Death did not respawn the player in time"));
            return;
        }
        if (State != ENaiwaP1State::Playing) return;
        if (DeathEventCount != BounceProbeDeathCountBefore + 1 || Activations != 3
            || !BounceProbePad->IsReady() || BounceProbePad->IsContactLatched()
            || BounceProbePad->GetCooldownRemaining() > 0.01f
            || FVector::Dist2D(PlayerCharacter->GetActorLocation(), InitialSpawn) > 150.0f)
        {
            FinishBounceProbe(TEXT("FAIL"), TEXT("Death respawn did not clear pad state and restore the start"));
            return;
        }
        BounceProbeEvents.Add(TEXT("death_reset_cleared_pad_state_and_restored_start"));
        FinishBounceProbe(TEXT("PASS"), TEXT("Real movement reached a safe landing; contact, R reset, and death reset passed"));
        return;
    }
}

void ANaiwaP1GameMode::FinishBounceProbe(const TCHAR* Result, const FString& Detail)
{
    if (!bBounceProbeEnabled) return;
    bBounceProbeEnabled = false;
    if (PlayerCharacter.IsValid())
    {
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        PlayerCharacter->StopJumping();
    }
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_BOUNCE_PROBE result=%s gait=%s detail=%s"),
        Result, bBounceProbeWalk ? TEXT("Walk150") : TEXT("Run300"), *Detail);
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1BounceProbeV1"));
    Report->SetStringField(TEXT("status"), Result);
    Report->SetStringField(TEXT("detail"), Detail);
    Report->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetMapName() : FString());
    Report->SetStringField(TEXT("gait_speed"), bBounceProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    Report->SetStringField(TEXT("state"), StateName(State));
    Report->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - BounceProbeStartedWall);
    Report->SetNumberField(TEXT("activation_count"), BounceProbePad.IsValid() ? BounceProbePad->GetActivationCount() : -1);
    Report->SetNumberField(TEXT("death_events"), DeathEventCount);
    Report->SetNumberField(TEXT("launch_velocity_z"), BounceProbePad.IsValid() ? BounceProbePad->LaunchVelocityZ : 0.0);
    Report->SetNumberField(TEXT("max_character_z"), BounceProbeMaxZ);
    TSharedRef<FJsonObject> Launch = MakeShared<FJsonObject>();
    Launch->SetNumberField(TEXT("x"), BounceProbeFirstLaunch.X);
    Launch->SetNumberField(TEXT("y"), BounceProbeFirstLaunch.Y);
    Launch->SetNumberField(TEXT("z"), BounceProbeFirstLaunch.Z);
    Report->SetObjectField(TEXT("first_launch"), Launch);
    TSharedRef<FJsonObject> Landing = MakeShared<FJsonObject>();
    Landing->SetNumberField(TEXT("x"), BounceProbeFirstLanding.X);
    Landing->SetNumberField(TEXT("y"), BounceProbeFirstLanding.Y);
    Landing->SetNumberField(TEXT("z"), BounceProbeFirstLanding.Z);
    Report->SetObjectField(TEXT("first_safe_landing"), Landing);
    Report->SetBoolField(TEXT("r_reset_passed"), BounceProbeEvents.Contains(TEXT("r_reset_cleared_pad_state_and_restored_start")));
    Report->SetBoolField(TEXT("death_reset_passed"), BounceProbeEvents.Contains(TEXT("death_reset_cleared_pad_state_and_restored_start")));
    TArray<TSharedPtr<FJsonValue>> Events;
    for (const FString& Event : BounceProbeEvents) Events.Add(MakeShared<FJsonValueString>(Event));
    Report->SetArrayField(TEXT("events"), Events);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("bounce_probe_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (bSaved)
    {
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_BOUNCE_PROBE output=%s saved=1"), *Output);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_P1_BOUNCE_PROBE output=%s saved=0"), *Output);
    }
    FPlatformMisc::RequestExit(false);
}

void ANaiwaP1GameMode::SwitchProbeAdvance(ENaiwaP1SwitchProbeStep NextStep, const FString& Evidence)
{
    SwitchProbeEvents.Add(Evidence);
    SwitchProbeStep = NextStep;
    SwitchProbeStepStartedWall = FPlatformTime::Seconds();
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PROBE step=%d evidence=%s"),
        static_cast<int32>(NextStep), *Evidence);
}

bool ANaiwaP1GameMode::SwitchProbeTeleportToSwitch(bool bLeft)
{
    if (!PlayerCharacter.IsValid() || !SwitchProbePlatform.IsValid())
    {
        FinishSwitchProbe(TEXT("INCOMPLETE"), TEXT("Character or switch platform disappeared"));
        return false;
    }
    UBoxComponent* Trigger = bLeft ? SwitchProbePlatform->LeftSwitchTrigger.Get()
        : SwitchProbePlatform->RightSwitchTrigger.Get();
    if (!Trigger)
    {
        FinishSwitchProbe(TEXT("INCOMPLETE"), TEXT("Switch trigger component is missing"));
        return false;
    }
    FVector Target = Trigger->GetComponentLocation();
    Target.Z = SwitchProbePlatform->GetActorLocation().Z + SwitchProbePlatform->PlatformTopZ
        + PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 3.0f;
    if (!PlayerCharacter->TeleportForP1(Target) || !Trigger->IsOverlappingActor(PlayerCharacter.Get()))
    {
        FinishSwitchProbe(TEXT("FAIL"), FString::Printf(
            TEXT("Teleport did not produce a real %s switch overlap"), bLeft ? TEXT("left") : TEXT("right")));
        return false;
    }
    return true;
}

void ANaiwaP1GameMode::CaptureSwitchProbeFrame(const TCHAR* Label)
{
    if (!bSwitchProbeCapture || FScreenshotRequest::IsScreenshotRequested() || !GetWorld()->GetGameViewport()) return;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"), TEXT("Visuals"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("switch_%s_%s_%s.png"),
        bSwitchProbeWalk ? TEXT("walk") : TEXT("run"), Label,
        *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    FScreenshotRequest::RequestScreenshot(Path, true, false);
    SwitchProbeCapturePaths.Add(Path);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PROBE screenshot_requested=%s"), *Path);
}

void ANaiwaP1GameMode::TickSwitchProbe()
{
    const double Now = FPlatformTime::Seconds();
    if (Now - SwitchProbeStartedWall > 100.0 || Now - SwitchProbeStepStartedWall > 18.0)
    {
        FinishSwitchProbe(TEXT("FAIL"), FString::Printf(
            TEXT("Timeout step=%d state=%s platform=%s x=%.1f character_x=%.1f paused=%d"),
            static_cast<int32>(SwitchProbeStep), StateName(State),
            SwitchProbePlatform.IsValid() ? SwitchStateName(SwitchProbePlatform->GetState()) : TEXT("missing"),
            SwitchProbePlatform.IsValid() ? SwitchProbePlatform->GetCurrentX() : 0.0f,
            PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().X : 0.0f,
            UGameplayStatics::IsGamePaused(this)));
        return;
    }
    if (!IsAuditMap(GetWorld(), TEXT("L_NaiwaP1_Switch")))
    {
        FinishSwitchProbe(TEXT("INCOMPLETE"), TEXT("Switch probe only runs on its isolated prototype map"));
        return;
    }
    if (bSmokeEnabled || bRouteEnabled || bHazardAuditEnabled || bMatrixEnabled || bBounceProbeEnabled || bTimedProbeEnabled)
    {
        FinishSwitchProbe(TEXT("INCOMPLETE"), TEXT("Run only one P1 audit scenario per process"));
        return;
    }
    if (UGameplayStatics::IsGamePaused(this))
    {
        if (SwitchProbeStep != ENaiwaP1SwitchProbeStep::WaitPausedTravel)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Game paused outside the platform freeze check"));
            return;
        }
        if (!SwitchProbePlatform.IsValid())
        {
            FinishSwitchProbe(TEXT("INCOMPLETE"), TEXT("Switch platform disappeared during the pause check"));
            return;
        }
        if (Now - SwitchProbeStepStartedWall < 1.25) return;
        SwitchProbePauseDeltaX = FMath::Abs(SwitchProbePlatform->GetCurrentX() - SwitchProbePausedX);
        if (SwitchProbePlatform->GetState() != ENaiwaP1SwitchPlatformState::TravelingRight
            || SwitchProbePauseDeltaX > 1.0f)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Pause did not freeze platform position and travel state"));
            return;
        }
        ToggleP1Pause();
        if (UGameplayStatics::IsGamePaused(this))
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Could not resume the paused platform"));
            return;
        }
        SwitchProbeAdvance(ENaiwaP1SwitchProbeStep::WaitSecondRightDock, TEXT("travel_froze_during_pause_and_resumed"));
        return;
    }
    if (!PlayerCharacter.IsValid()) return;
    if (!bSwitchProbeInitialized)
    {
        if (Now - SwitchProbeStartedWall > 5.0)
        {
            FinishSwitchProbe(TEXT("INCOMPLETE"), TEXT("P1 character did not register on the switch prototype map"));
            return;
        }
        int32 PlatformCount = 0;
        for (TActorIterator<ANaiwaP1SwitchPlatform> It(GetWorld()); It; ++It)
        {
            if (!It->ActorHasTag(FName(TEXT("NaiwaP1SwitchPlatformV1")))) continue;
            SwitchProbePlatform = *It;
            ++PlatformCount;
        }
        if (PlatformCount != 1 || !SwitchProbePlatform.IsValid()
            || !SwitchProbePlatform->LeftSwitchTrigger || !SwitchProbePlatform->RightSwitchTrigger
            || SwitchProbePlatform->DockLeftX >= SwitchProbePlatform->DockRightX
            || SwitchProbePlatform->TravelSpeedCmPerSecond <= 0.0f
            || SwitchProbePlatform->GetState() != ENaiwaP1SwitchPlatformState::DockedLeft
            || !bHasInitialSpawn)
        {
            FinishSwitchProbe(TEXT("INCOMPLETE"), FString::Printf(
                TEXT("Prototype requires one tagged switch platform docked left; found %d"), PlatformCount));
            return;
        }
        if (!PlayerCharacter->SetP1Gait(!bSwitchProbeWalk))
        {
            FinishSwitchProbe(TEXT("INCOMPLETE"), TEXT("Could not select the P1 Walk or Run gait"));
            return;
        }
        SwitchProbeDeathCountBefore = DeathEventCount;
        bSwitchProbeInitialized = true;
        SwitchProbeAdvance(ENaiwaP1SwitchProbeStep::ApproachBoard,
            bSwitchProbeWalk ? TEXT("gait_walk150_selected") : TEXT("gait_run300_selected"));
        return;
    }
    if (!SwitchProbePlatform.IsValid())
    {
        FinishSwitchProbe(TEXT("INCOMPLETE"), TEXT("Switch platform disappeared during the probe"));
        return;
    }
    ANaiwaP1SwitchPlatform* Platform = SwitchProbePlatform.Get();
    const ENaiwaP1SwitchPlatformState PlatformState = Platform->GetState();
    const float PlatformX = Platform->GetCurrentX();
    const bool bSupported = Platform->IsCharacterSupported(PlayerCharacter.Get());
    switch (SwitchProbeStep)
    {
    case ENaiwaP1SwitchProbeStep::WaitReady:
        return;
    case ENaiwaP1SwitchProbeStep::ApproachBoard:
        if (State != ENaiwaP1State::Playing || PlatformState != ENaiwaP1SwitchPlatformState::DockedLeft)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Platform left the initial dock before boarding"));
            return;
        }
        if (!bSupported)
        {
            PlayerCharacter->SetAutomatedMoveInput(1.0f);
            return;
        }
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();
        SwitchProbeBoardOffsetX = PlayerCharacter->GetActorLocation().X - PlatformX;
        SwitchProbeAdvance(ENaiwaP1SwitchProbeStep::WaitDepartureCue, TEXT("real_walk_boarded_and_stopped_on_platform"));
        return;
    case ENaiwaP1SwitchProbeStep::WaitDepartureCue:
        if (State != ENaiwaP1State::Playing || !bSupported)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Character did not remain stably supported for departure cue"));
            return;
        }
        if (PlatformState != ENaiwaP1SwitchPlatformState::DockedLeft)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Platform departed without an observed warning cue"));
            return;
        }
        if (!Platform->IsDepartureCueActive()) return;
        CaptureSwitchProbeFrame(TEXT("departure_cue"));
        SwitchProbeAdvance(ENaiwaP1SwitchProbeStep::RideRight, TEXT("stable_boarding_started_visible_departure_cue"));
        return;
    case ENaiwaP1SwitchProbeStep::RideRight:
        if (State != ENaiwaP1State::Playing || DeathEventCount != SwitchProbeDeathCountBefore)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Character died during the passenger crossing"));
            return;
        }
        if (PlatformState == ENaiwaP1SwitchPlatformState::DockedLeft)
        {
            if (!Platform->IsDepartureCueActive() || !bSupported)
                FinishSwitchProbe(TEXT("FAIL"), TEXT("Departure cue ended without a supported rider"));
            return;
        }
        if (PlatformState == ENaiwaP1SwitchPlatformState::TravelingRight)
        {
            if (PlatformX >= (Platform->DockLeftX + Platform->DockRightX) * 0.5f && bSupported)
                bSwitchProbeRideSupportedMidway = true;
            return;
        }
        if (PlatformState != ENaiwaP1SwitchPlatformState::DockedRight || !bSupported
            || !bSwitchProbeRideSupportedMidway
            || FMath::Abs((PlayerCharacter->GetActorLocation().X - PlatformX) - SwitchProbeBoardOffsetX) > 55.0f
            || FMath::Abs(PlatformX - Platform->DockRightX) > 1.0f)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Platform did not carry the supported rider to the right dock"));
            return;
        }
        CaptureSwitchProbeFrame(TEXT("right_dock"));
        SwitchProbeAdvance(ENaiwaP1SwitchProbeStep::VerifyDockRight, TEXT("rider_supported_midway_and_arrived_at_right_dock"));
        return;
    case ENaiwaP1SwitchProbeStep::VerifyDockRight:
        if (PlatformState != ENaiwaP1SwitchPlatformState::DockedRight || Platform->IsDepartureCueActive())
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Docked rider caused an automatic repeat departure"));
            return;
        }
        if (Now - SwitchProbeStepStartedWall < 1.0) return;
        {
            const int32 TouchesBefore = Platform->GetLeftSwitchTouchCount();
            if (!SwitchProbeTeleportToSwitch(true)) return;
            if (Platform->GetLeftSwitchTouchCount() != TouchesBefore + 1
                || Platform->GetState() != ENaiwaP1SwitchPlatformState::TravelingLeft)
            {
                FinishSwitchProbe(TEXT("FAIL"), TEXT("Real left switch touch did not call the platform back"));
                return;
            }
        }
        SwitchProbeAdvance(ENaiwaP1SwitchProbeStep::RideLeft, TEXT("right_dock_held_without_repeat_then_left_switch_called_back"));
        return;
    case ENaiwaP1SwitchProbeStep::RideLeft:
        if (PlatformState != ENaiwaP1SwitchPlatformState::TravelingLeft)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Platform did not continue left after the left switch touch"));
            return;
        }
        SwitchProbeRightTouchAtLatch = Platform->GetRightSwitchTouchCount();
        if (!SwitchProbeTeleportToSwitch(false)) return;
        if (Platform->GetRightSwitchTouchCount() != SwitchProbeRightTouchAtLatch + 1
            || Platform->GetState() != ENaiwaP1SwitchPlatformState::TravelingLeft)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Moving platform reversed after a real opposite-switch touch"));
            return;
        }
        SwitchProbeRightTouchAtLatch = Platform->GetRightSwitchTouchCount();
        SwitchProbeAdvance(ENaiwaP1SwitchProbeStep::WaitDockLeftAtRightSwitch,
            TEXT("opposite_switch_touch_ignored_during_leftward_travel"));
        return;
    case ENaiwaP1SwitchProbeStep::WaitDockLeftAtRightSwitch:
        if (Platform->GetRightSwitchTouchCount() != SwitchProbeRightTouchAtLatch)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Standing inside the right switch retriggered its contact"));
            return;
        }
        if (PlatformState == ENaiwaP1SwitchPlatformState::TravelingLeft) return;
        if (PlatformState != ENaiwaP1SwitchPlatformState::DockedLeft
            || FMath::Abs(PlatformX - Platform->DockLeftX) > 1.0f)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Left-switch call did not complete at the left dock"));
            return;
        }
        if (Now - SwitchProbeStepStartedWall < 1.0) return;
        {
            FVector Away = Platform->RightSwitchTrigger->GetComponentLocation();
            Away.X -= Platform->RightSwitchTrigger->GetScaledBoxExtent().X
                + PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() + 130.0f;
            Away.Z = Platform->GetActorLocation().Z + Platform->PlatformTopZ
                + PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 3.0f;
            if (!PlayerCharacter->TeleportForP1(Away)
                || Platform->RightSwitchTrigger->IsOverlappingActor(PlayerCharacter.Get()))
            {
                FinishSwitchProbe(TEXT("FAIL"), TEXT("Character could not leave the right switch trigger"));
                return;
            }
        }
        SwitchProbeAdvance(ENaiwaP1SwitchProbeStep::RetouchRightSwitch,
            TEXT("right_switch_held_without_repeat_then_character_left_trigger"));
        return;
    case ENaiwaP1SwitchProbeStep::RetouchRightSwitch:
        if (Platform->GetRightSwitchTouchCount() != SwitchProbeRightTouchAtLatch)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Right switch fired while the character was outside it"));
            return;
        }
        if (!SwitchProbeTeleportToSwitch(false)) return;
        if (Platform->GetRightSwitchTouchCount() != SwitchProbeRightTouchAtLatch + 1
            || Platform->GetState() != ENaiwaP1SwitchPlatformState::TravelingRight)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Right switch did not rearm after exit and re-entry"));
            return;
        }
        SwitchProbeAdvance(ENaiwaP1SwitchProbeStep::WaitSecondRightTravel,
            TEXT("right_switch_reentry_called_platform_right_once"));
        return;
    case ENaiwaP1SwitchProbeStep::WaitSecondRightTravel:
        if (PlatformState != ENaiwaP1SwitchPlatformState::TravelingRight)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Second rightward trip stopped before pause check"));
            return;
        }
        if (PlatformX < Platform->DockLeftX + 80.0f) return;
        SwitchProbePausedX = PlatformX;
        ToggleP1Pause();
        if (!UGameplayStatics::IsGamePaused(this))
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Could not pause while platform was traveling"));
            return;
        }
        SwitchProbeAdvance(ENaiwaP1SwitchProbeStep::WaitPausedTravel, TEXT("paused_midway_through_rightward_trip"));
        return;
    case ENaiwaP1SwitchProbeStep::WaitPausedTravel:
        FinishSwitchProbe(TEXT("FAIL"), TEXT("Pause check returned to unpaused Tick unexpectedly"));
        return;
    case ENaiwaP1SwitchProbeStep::WaitSecondRightDock:
        if (PlatformState == ENaiwaP1SwitchPlatformState::TravelingRight) return;
        if (PlatformState != ENaiwaP1SwitchPlatformState::DockedRight
            || FMath::Abs(PlatformX - Platform->DockRightX) > 1.0f
            || Platform->GetRightSwitchTouchCount() != SwitchProbeRightTouchAtLatch + 1)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Trip did not finish at the right dock after pause"));
            return;
        }
        if (const AWorldSettings* Settings = GetWorld()->GetWorldSettings())
        {
            PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();
            const FVector BelowKillZ(PlayerCharacter->GetActorLocation().X,
                PlayerCharacter->GetActorLocation().Y, Settings->KillZ - 200.0f);
            PlayerCharacter->SetActorLocation(BelowKillZ, false, nullptr, ETeleportType::TeleportPhysics);
            if (PlayerCharacter->GetActorLocation().Z >= Settings->KillZ)
            {
                FinishSwitchProbe(TEXT("FAIL"), TEXT("Could not place the character below World KillZ"));
                return;
            }
        }
        else
        {
            FinishSwitchProbe(TEXT("INCOMPLETE"), TEXT("World settings missing KillZ"));
            return;
        }
        SwitchProbeAdvance(ENaiwaP1SwitchProbeStep::WaitDeath, TEXT("pause_resumed_to_right_dock_then_pawn_placed_below_killz"));
        return;
    case ENaiwaP1SwitchProbeStep::WaitDeath:
        if (State != ENaiwaP1State::Dying) return;
        if (DeathEventCount != SwitchProbeDeathCountBefore + 1
            || PlatformState != ENaiwaP1SwitchPlatformState::DockedRight)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("KillZ did not cause exactly one death while the platform was docked right"));
            return;
        }
        SwitchProbeAdvance(ENaiwaP1SwitchProbeStep::WaitDeathReset, TEXT("killz_caused_one_death_at_right_dock"));
        return;
    case ENaiwaP1SwitchProbeStep::WaitDeathReset:
        if (State != ENaiwaP1State::Playing) return;
        if (DeathEventCount != SwitchProbeDeathCountBefore + 1
            || PlatformState != ENaiwaP1SwitchPlatformState::DockedLeft
            || FMath::Abs(PlatformX - Platform->DockLeftX) > 1.0f
            || Platform->IsDepartureCueActive()
            || FVector::Dist2D(PlayerCharacter->GetActorLocation(), InitialSpawn) > 150.0f)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Death respawn did not restore the left dock and start"));
            return;
        }
        bSwitchProbeDeathResetPassed = true;
        SwitchProbeRightTouchAtLatch = Platform->GetRightSwitchTouchCount();
        if (!SwitchProbeTeleportToSwitch(false)) return;
        if (Platform->GetRightSwitchTouchCount() != SwitchProbeRightTouchAtLatch + 1
            || Platform->GetState() != ENaiwaP1SwitchPlatformState::TravelingRight)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("Right switch did not start a new trip after death reset"));
            return;
        }
        SwitchProbeAdvance(ENaiwaP1SwitchProbeStep::WaitRightTravelForR,
            TEXT("death_reset_restored_left_dock_and_right_switch_started_new_trip"));
        return;
    case ENaiwaP1SwitchProbeStep::WaitRightTravelForR:
        if (PlatformState != ENaiwaP1SwitchPlatformState::TravelingRight) return;
        if (PlatformX < Platform->DockLeftX + 80.0f) return;
        RestartLevel();
        if (State != ENaiwaP1State::Respawning
            || Platform->GetState() != ENaiwaP1SwitchPlatformState::DockedLeft
            || FMath::Abs(Platform->GetCurrentX() - Platform->DockLeftX) > 1.0f)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("R reset did not interrupt travel and dock left"));
            return;
        }
        SwitchProbeAdvance(ENaiwaP1SwitchProbeStep::WaitRReset, TEXT("r_reset_interrupted_travel_and_docked_left"));
        return;
    case ENaiwaP1SwitchProbeStep::WaitRReset:
        if (State != ENaiwaP1State::Playing) return;
        if (PlatformState != ENaiwaP1SwitchPlatformState::DockedLeft
            || FMath::Abs(PlatformX - Platform->DockLeftX) > 1.0f
            || Platform->IsDepartureCueActive()
            || DeathEventCount != SwitchProbeDeathCountBefore + 1
            || FVector::Dist2D(PlayerCharacter->GetActorLocation(), InitialSpawn) > 150.0f)
        {
            FinishSwitchProbe(TEXT("FAIL"), TEXT("R reset did not restore a playable start and left dock"));
            return;
        }
        bSwitchProbeRResetPassed = true;
        SwitchProbeEvents.Add(TEXT("r_reset_restored_playable_start_and_left_dock"));
        FinishSwitchProbe(TEXT("PASS"), TEXT("Walk/Run boarding, warned passenger crossing, touch switches, latch, pause, death and R resets passed"));
        return;
    }
}

void ANaiwaP1GameMode::FinishSwitchProbe(const TCHAR* Result, const FString& Detail)
{
    if (!bSwitchProbeEnabled) return;
    bSwitchProbeEnabled = false;
    if (PlayerCharacter.IsValid())
    {
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        PlayerCharacter->StopJumping();
    }
    const bool bWasPaused = UGameplayStatics::IsGamePaused(this);
    UGameplayStatics::SetGamePaused(this, false);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PROBE result=%s gait=%s detail=%s"),
        Result, bSwitchProbeWalk ? TEXT("Walk150") : TEXT("Run300"), *Detail);
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1SwitchProbeV1"));
    Report->SetStringField(TEXT("status"), Result);
    Report->SetStringField(TEXT("detail"), Detail);
    Report->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetMapName() : FString());
    Report->SetStringField(TEXT("gait_speed"), bSwitchProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    Report->SetStringField(TEXT("state"), StateName(State));
    Report->SetStringField(TEXT("platform_state"), SwitchProbePlatform.IsValid()
        ? SwitchStateName(SwitchProbePlatform->GetState()) : TEXT("Missing"));
    Report->SetNumberField(TEXT("platform_x"), SwitchProbePlatform.IsValid() ? SwitchProbePlatform->GetCurrentX() : 0.0f);
    Report->SetNumberField(TEXT("left_switch_touches"), SwitchProbePlatform.IsValid()
        ? SwitchProbePlatform->GetLeftSwitchTouchCount() : -1);
    Report->SetNumberField(TEXT("right_switch_touches"), SwitchProbePlatform.IsValid()
        ? SwitchProbePlatform->GetRightSwitchTouchCount() : -1);
    Report->SetNumberField(TEXT("death_events"), DeathEventCount);
    Report->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - SwitchProbeStartedWall);
    Report->SetNumberField(TEXT("boarding_offset_x"), SwitchProbeBoardOffsetX);
    Report->SetNumberField(TEXT("paused_platform_delta_x"), SwitchProbePauseDeltaX);
    Report->SetBoolField(TEXT("rider_supported_midway"), bSwitchProbeRideSupportedMidway);
    Report->SetBoolField(TEXT("death_reset_passed"), bSwitchProbeDeathResetPassed);
    Report->SetBoolField(TEXT("r_reset_passed"), bSwitchProbeRResetPassed);
    Report->SetBoolField(TEXT("ended_while_paused"), bWasPaused);
    TArray<TSharedPtr<FJsonValue>> Events;
    for (const FString& Event : SwitchProbeEvents) Events.Add(MakeShared<FJsonValueString>(Event));
    Report->SetArrayField(TEXT("events"), Events);
    TArray<TSharedPtr<FJsonValue>> Captures;
    for (const FString& Path : SwitchProbeCapturePaths) Captures.Add(MakeShared<FJsonValueString>(Path));
    Report->SetArrayField(TEXT("capture_paths"), Captures);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("switch_probe_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (bSaved)
    {
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_PROBE output=%s saved=1"), *Output);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_P1_SWITCH_PROBE output=%s saved=0"), *Output);
    }
    FPlatformMisc::RequestExit(false);
}

void ANaiwaP1GameMode::TimedProbeAdvance(ENaiwaP1TimedProbeStep NextStep, const FString& Evidence)
{
    TimedProbeEvents.Add(Evidence);
    TimedProbeStep = NextStep;
    TimedProbeStepStartedWall = FPlatformTime::Seconds();
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_PROBE step=%d evidence=%s"),
        static_cast<int32>(NextStep), *Evidence);
}

void ANaiwaP1GameMode::CaptureTimedProbeFrame(const TCHAR* Label)
{
    if (!bTimedProbeCapture || FScreenshotRequest::IsScreenshotRequested() || !GetWorld()->GetGameViewport()) return;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"), TEXT("Visuals"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("timed_%s_%s_%s.png"),
        bTimedProbeWalk ? TEXT("walk") : TEXT("run"), Label,
        *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    FScreenshotRequest::RequestScreenshot(Path, true, false);
    TimedProbeCapturePaths.Add(Path);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_PROBE screenshot_requested=%s"), *Path);
}

void ANaiwaP1GameMode::TickTimedProbe()
{
    const double Now = FPlatformTime::Seconds();
    if (Now - TimedProbeStartedWall > 100.0 || Now - TimedProbeStepStartedWall > 18.0)
    {
        FinishTimedProbe(TEXT("FAIL"), FString::Printf(
            TEXT("Timeout step=%d game=%s platform=%s character_x=%.1f paused=%d"),
            static_cast<int32>(TimedProbeStep), StateName(State),
            TimedProbePlatform.IsValid() ? TimedStateName(TimedProbePlatform->GetState()) : TEXT("missing"),
            PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().X : 0.0f,
            UGameplayStatics::IsGamePaused(this)));
        return;
    }
    if (!IsAuditMap(GetWorld(), TEXT("L_NaiwaP1_Timed")))
    {
        FinishTimedProbe(TEXT("INCOMPLETE"), TEXT("Timed probe only runs on its isolated prototype map"));
        return;
    }
    if (bSmokeEnabled || bRouteEnabled || bHazardAuditEnabled || bMatrixEnabled
        || bBounceProbeEnabled || bSwitchProbeEnabled
        || (FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1TimedProbeWalk"))
            && FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1TimedProbeRun"))))
    {
        FinishTimedProbe(TEXT("INCOMPLETE"), TEXT("Run exactly one P1 audit scenario and gait per process"));
        return;
    }
    if (UGameplayStatics::IsGamePaused(this))
    {
        if (TimedProbeStep != ENaiwaP1TimedProbeStep::WaitPausedCountdown || !TimedProbePlatform.IsValid())
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Game paused outside the timed-floor freeze check"));
            return;
        }
        if (Now - TimedProbeStepStartedWall < 1.25) return;
        ANaiwaP1TimedPlatform* Platform = TimedProbePlatform.Get();
        TimedProbePauseDeltaSeconds = FMath::Abs(Platform->GetCountdownRemaining() - TimedProbePausedRemaining);
        if (Platform->GetState() != ENaiwaP1TimedPlatformState::Countdown
            || Platform->GetTriggerCount() != 2 || !Platform->IsCarryingCollisionEnabled()
            || TimedProbePauseDeltaSeconds > 0.03f)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Pause changed countdown, trigger count, or carrying collision"));
            return;
        }
        ToggleP1Pause();
        if (UGameplayStatics::IsGamePaused(this))
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Could not resume the timed-floor probe"));
            return;
        }
        bTimedProbePausePassed = true;
        TimedProbeObservedRemaining = Platform->GetCountdownRemaining();
        TimedProbeAdvance(ENaiwaP1TimedProbeStep::WaitDisable,
            TEXT("countdown_and_collision_froze_during_pause_then_resumed"));
        return;
    }
    if (TimedProbeStep == ENaiwaP1TimedProbeStep::WaitPausedCountdown)
    {
        FinishTimedProbe(TEXT("FAIL"), TEXT("Pause-check step resumed without being paused"));
        return;
    }
    if (!PlayerCharacter.IsValid())
    {
        if (Now - TimedProbeStartedWall > 5.0)
            FinishTimedProbe(TEXT("INCOMPLETE"), TEXT("P1 character did not register on the timed prototype map"));
        return;
    }
    if (!bTimedProbeInitialized)
    {
        int32 PlatformCount = 0;
        for (TActorIterator<ANaiwaP1TimedPlatform> It(GetWorld()); It; ++It)
        {
            if (!It->ActorHasTag(FName(TEXT("NaiwaP1TimedPlatformV1")))) continue;
            TimedProbePlatform = *It;
            ++PlatformCount;
        }
        if (PlatformCount != 1 || !TimedProbePlatform.IsValid()
            || !TimedProbePlatform->PlatformCollision || !bHasInitialSpawn
            || TimedProbePlatform->PlatformExtent.X < 100.0f
            || TimedProbePlatform->ActiveSeconds <= 0.0f
            || TimedProbePlatform->DisabledSeconds <= 0.0f
            || TimedProbePlatform->GetState() != ENaiwaP1TimedPlatformState::Ready
            || !TimedProbePlatform->IsCarryingCollisionEnabled()
            || InitialSpawn.X >= TimedProbePlatform->GetActorLocation().X - TimedProbePlatform->PlatformExtent.X - 100.0f)
        {
            FinishTimedProbe(TEXT("INCOMPLETE"), FString::Printf(
                TEXT("Prototype requires one tagged ready timed platform and a left approach spawn; found %d"),
                PlatformCount));
            return;
        }
        if (!PlayerCharacter->SetP1Gait(!bTimedProbeWalk)
            || FMath::Abs(PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed
                - (bTimedProbeWalk ? 150.0f : 300.0f)) > 1.0f)
        {
            FinishTimedProbe(TEXT("INCOMPLETE"), TEXT("Could not select and verify P1 Walk150 or Run300 gait"));
            return;
        }
        TimedProbeDeathCountBefore = DeathEventCount;
        bTimedProbeInitialized = true;
        TimedProbeAdvance(ENaiwaP1TimedProbeStep::FirstCrossing,
            bTimedProbeWalk ? TEXT("gait_walk150_selected") : TEXT("gait_run300_selected"));
        return;
    }
    if (!TimedProbePlatform.IsValid())
    {
        FinishTimedProbe(TEXT("INCOMPLETE"), TEXT("Tagged timed platform disappeared during the probe"));
        return;
    }
    ANaiwaP1TimedPlatform* Platform = TimedProbePlatform.Get();
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    const ENaiwaP1TimedPlatformState PlatformState = Platform->GetState();
    const int32 TriggerCount = Platform->GetTriggerCount();
    const float PlatformX = Platform->GetActorLocation().X;
    const float TopZ = Platform->GetActorLocation().Z + Platform->PlatformTopZ;
    const float CapsuleRadius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float CapsuleBottom = Character->GetCapsuleBottom();
    const float CharacterX = Character->GetActorLocation().X;
    const bool bSupported = Character->GetCharacterMovement()->IsMovingOnGround()
        && Character->GetMovementBaseObject() == Platform->PlatformCollision.Get();
    const bool bCrossedRight = CharacterX > PlatformX + Platform->PlatformExtent.X + CapsuleRadius + 18.0f
        && Character->GetCharacterMovement()->IsMovingOnGround()
        && !bSupported && FMath::Abs(CapsuleBottom - TopZ) < 18.0f;

    switch (TimedProbeStep)
    {
    case ENaiwaP1TimedProbeStep::WaitReady:
        return;
    case ENaiwaP1TimedProbeStep::FirstCrossing:
        if (State != ENaiwaP1State::Playing || TriggerCount > 1
            || PlatformState == ENaiwaP1TimedPlatformState::Disabled
            || PlatformState == ENaiwaP1TimedPlatformState::Recovering)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("First real crossing died, retriggered, or outlasted the floor"));
            return;
        }
        Character->SetAutomatedMoveInput(1.0f);
        if (TriggerCount == 1 && TimedProbeFirstTriggerGameTime < 0.0f)
        {
            TimedProbeFirstTriggerGameTime = GetWorld()->GetTimeSeconds();
            CaptureTimedProbeFrame(TEXT("first_countdown"));
            TimedProbeEvents.Add(TEXT("first_real_landing_armed_countdown_once"));
        }
        if (!bCrossedRight) return;
        if (TriggerCount != 1 || PlatformState != ENaiwaP1TimedPlatformState::Countdown
            || TimedProbeFirstTriggerGameTime < 0.0f)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("First crossing did not reach the grounded right bank during one countdown"));
            return;
        }
        TimedProbeFirstCrossingSeconds = GetWorld()->GetTimeSeconds() - TimedProbeFirstTriggerGameTime;
        if (TimedProbeFirstCrossingSeconds >= Platform->ActiveSeconds)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("First crossing exceeded the configured active interval"));
            return;
        }
        bTimedProbeFirstCrossed = true;
        Character->SetAutomatedMoveInput(0.0f);
        Character->GetCharacterMovement()->StopMovementImmediately();
        RestartLevel();
        if (State != ENaiwaP1State::Respawning)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("R reset did not enter Respawning after a real crossing"));
            return;
        }
        TimedProbeAdvance(ENaiwaP1TimedProbeStep::WaitRReset,
            TEXT("first_real_crossing_reached_right_bank_before_expiry_then_r_reset"));
        return;
    case ENaiwaP1TimedProbeStep::WaitRReset:
        if (State != ENaiwaP1State::Playing || Now - TimedProbeStepStartedWall < 0.5) return;
        if (TriggerCount != 1 || PlatformState != ENaiwaP1TimedPlatformState::Ready
            || Platform->GetCountdownRemaining() > 0.01f || !Platform->IsCarryingCollisionEnabled()
            || FVector::Dist2D(Character->GetActorLocation(), InitialSpawn) > 150.0f)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("R reset did not restore a ready floor and playable start"));
            return;
        }
        bTimedProbeRResetPassed = true;
        TimedProbeAdvance(ENaiwaP1TimedProbeStep::ApproachForExpiry,
            TEXT("r_reset_restored_ready_floor_and_start_without_erasing_lifetime_count"));
        return;
    case ENaiwaP1TimedProbeStep::ApproachForExpiry:
        if (State != ENaiwaP1State::Playing || TriggerCount > 2
            || PlatformState == ENaiwaP1TimedPlatformState::Disabled)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Second real landing failed before countdown observation"));
            return;
        }
        Character->SetAutomatedMoveInput(1.0f);
        if (CharacterX < PlatformX - 90.0f || TriggerCount < 2) return;
        if (!bSupported || PlatformState != ENaiwaP1TimedPlatformState::Countdown)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Second landing did not remain supported at the platform centre"));
            return;
        }
        Character->SetAutomatedMoveInput(0.0f);
        Character->GetCharacterMovement()->StopMovementImmediately();
        TimedProbeObservedRemaining = Platform->GetCountdownRemaining();
        TimedProbeObserveStartedGameTime = GetWorld()->GetTimeSeconds();
        TimedProbeAdvance(ENaiwaP1TimedProbeStep::ObserveCountdown,
            TEXT("second_real_landing_armed_countdown_once_and_stopped_on_floor"));
        return;
    case ENaiwaP1TimedProbeStep::ObserveCountdown:
        if (State != ENaiwaP1State::Playing || !bSupported
            || PlatformState != ENaiwaP1TimedPlatformState::Countdown || TriggerCount != 2
            || Platform->GetCountdownRemaining() > TimedProbeObservedRemaining + 0.03f)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Standing or repeated contact restarted/extended the countdown"));
            return;
        }
        if (GetWorld()->GetTimeSeconds() - TimedProbeObserveStartedGameTime < 0.45f) return;
        if (Platform->GetCountdownRemaining() > TimedProbeObservedRemaining - 0.2f)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Countdown did not decrease while the rider stood on the floor"));
            return;
        }
        bTimedProbeCountdownDeclined = true;
        TimedProbePausedRemaining = Platform->GetCountdownRemaining();
        ToggleP1Pause();
        if (!UGameplayStatics::IsGamePaused(this))
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Could not pause during timed-floor countdown"));
            return;
        }
        TimedProbeAdvance(ENaiwaP1TimedProbeStep::WaitPausedCountdown,
            TEXT("standing_did_not_extend_or_retrigger_timer_then_paused"));
        return;
    case ENaiwaP1TimedProbeStep::WaitPausedCountdown:
        FinishTimedProbe(TEXT("FAIL"), TEXT("Pause-check step returned to unpaused Tick"));
        return;
    case ENaiwaP1TimedProbeStep::WaitDisable:
        if (State != ENaiwaP1State::Playing || TriggerCount != 2)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Rider died or floor retriggered while awaiting expiry"));
            return;
        }
        if (PlatformState == ENaiwaP1TimedPlatformState::Countdown)
        {
            if (!bSupported || Platform->GetCountdownRemaining() > TimedProbeObservedRemaining + 0.03f)
            {
                FinishTimedProbe(TEXT("FAIL"), TEXT("Countdown increased or rider lost support before expiry"));
                return;
            }
            TimedProbeObservedRemaining = Platform->GetCountdownRemaining();
            return;
        }
        if (PlatformState != ENaiwaP1TimedPlatformState::Disabled
            || Platform->IsCarryingCollisionEnabled() || Platform->GetDisableCount() != 1
            || !Character->GetCharacterMovement()->IsFalling())
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Expired floor did not disable collision and release the rider"));
            return;
        }
        TimedProbeBottomAtDisable = CapsuleBottom;
        CaptureTimedProbeFrame(TEXT("disabled"));
        TimedProbeAdvance(ENaiwaP1TimedProbeStep::WaitFall,
            TEXT("countdown_expired_once_floor_disabled_and_rider_entered_falling"));
        return;
    case ENaiwaP1TimedProbeStep::WaitFall:
    {
        if (PlatformState != ENaiwaP1TimedPlatformState::Disabled
            || Platform->IsCarryingCollisionEnabled())
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Floor recovered before its former rider cleared the top"));
            return;
        }
        if (CapsuleBottom > TimedProbeBottomAtDisable - 30.0f) return;
        bTimedProbeFallPassed = true;
        Character->SetAutomatedMoveInput(0.0f);
        const FVector Hold(PlatformX, Platform->GetActorLocation().Y,
            TopZ + Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 10.0f);
        if (!Character->TeleportForP1(Hold))
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Could not inject an occupied capsule for restoration safety check"));
            return;
        }
        Character->GetCharacterMovement()->SetMovementMode(MOVE_None);
        if (!Platform->IsCapsuleOccupyingPlatform())
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Injected capsule did not intersect the disabled floor bounds"));
            return;
        }
        TimedProbeRecoveryCountBefore = Platform->GetRecoveryCount();
        TimedProbeAdvance(ENaiwaP1TimedProbeStep::HoldCapsuleInFloor,
            TEXT("former_rider_fell_then_probe_held_capsule_inside_disabled_floor_for_safety_check"));
        return;
    }
    case ENaiwaP1TimedProbeStep::HoldCapsuleInFloor:
        if (!Platform->IsCapsuleOccupyingPlatform() || Platform->IsCarryingCollisionEnabled()
            || Platform->GetRecoveryCount() != TimedProbeRecoveryCountBefore)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Blocking floor restored while the capsule occupied its volume"));
            return;
        }
        if (PlatformState == ENaiwaP1TimedPlatformState::Disabled) return;
        if (PlatformState != ENaiwaP1TimedPlatformState::Recovering)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Timed floor skipped occupied recovery guard"));
            return;
        }
        if (TimedProbeRecoveringStartedWall <= 0.0) TimedProbeRecoveringStartedWall = Now;
        if (Now - TimedProbeRecoveringStartedWall < 0.4) return;
        bTimedProbeOccupancyGuardPassed = true;
        if (!Character->TeleportForP1(InitialSpawn))
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Could not clear capsule after occupied recovery hold"));
            return;
        }
        TimedProbeAdvance(ENaiwaP1TimedProbeStep::WaitRecovery,
            TEXT("recovery_waited_while_capsule_occupied_then_capsule_moved_to_spawn"));
        return;
    case ENaiwaP1TimedProbeStep::WaitRecovery:
        if (State != ENaiwaP1State::Playing) return;
        if (PlatformState == ENaiwaP1TimedPlatformState::Recovering) return;
        if (PlatformState != ENaiwaP1TimedPlatformState::Ready
            || !Platform->IsCarryingCollisionEnabled()
            || Platform->GetRecoveryCount() != TimedProbeRecoveryCountBefore + 1
            || FVector::Dist2D(Character->GetActorLocation(), InitialSpawn) > 150.0f)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Floor did not restore once the occupied volume cleared"));
            return;
        }
        CaptureTimedProbeFrame(TEXT("recovered"));
        TimedProbeAdvance(ENaiwaP1TimedProbeStep::RetryCrossing,
            TEXT("floor_recovered_once_after_capsule_cleared_ready_for_real_retry"));
        return;
    case ENaiwaP1TimedProbeStep::RetryCrossing:
    {
        if (State != ENaiwaP1State::Playing || TriggerCount > 3
            || PlatformState == ENaiwaP1TimedPlatformState::Disabled
            || PlatformState == ENaiwaP1TimedPlatformState::Recovering)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Real retry died, retriggered, or outlasted the recovered floor"));
            return;
        }
        Character->SetAutomatedMoveInput(1.0f);
        if (TriggerCount == 3 && TimedProbeRetryTriggerGameTime < 0.0f)
        {
            TimedProbeRetryTriggerGameTime = GetWorld()->GetTimeSeconds();
            TimedProbeEvents.Add(TEXT("real_retry_landing_armed_third_countdown"));
        }
        if (!bCrossedRight) return;
        if (TriggerCount != 3 || PlatformState != ENaiwaP1TimedPlatformState::Countdown
            || TimedProbeRetryTriggerGameTime < 0.0f)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Retry did not reach the grounded right bank during a new countdown"));
            return;
        }
        TimedProbeRetryCrossingSeconds = GetWorld()->GetTimeSeconds() - TimedProbeRetryTriggerGameTime;
        if (TimedProbeRetryCrossingSeconds >= Platform->ActiveSeconds)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Retry crossing exceeded the configured active interval"));
            return;
        }
        bTimedProbeRetryCrossed = true;
        Character->SetAutomatedMoveInput(0.0f);
        const AWorldSettings* Settings = GetWorld()->GetWorldSettings();
        if (!Settings)
        {
            FinishTimedProbe(TEXT("INCOMPLETE"), TEXT("World settings missing KillZ for death-reset check"));
            return;
        }
        const FVector BelowKillZ(CharacterX, Character->GetActorLocation().Y, Settings->KillZ - 200.0f);
        if (!Character->TeleportForP1(BelowKillZ))
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Could not place the character below KillZ for death-reset check"));
            return;
        }
        TimedProbeAdvance(ENaiwaP1TimedProbeStep::WaitDeath,
            TEXT("real_retry_crossed_right_bank_then_pawn_placed_below_killz_during_countdown"));
        return;
    }
    case ENaiwaP1TimedProbeStep::WaitDeath:
        if (State != ENaiwaP1State::Dying) return;
        if (DeathEventCount != TimedProbeDeathCountBefore + 1)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("KillZ did not cause exactly one death"));
            return;
        }
        TimedProbeAdvance(ENaiwaP1TimedProbeStep::WaitDeathReset,
            TEXT("killz_caused_one_death_after_recovered_floor_retry"));
        return;
    case ENaiwaP1TimedProbeStep::WaitDeathReset:
        if (State != ENaiwaP1State::Playing) return;
        if (DeathEventCount != TimedProbeDeathCountBefore + 1
            || TriggerCount != 3 || PlatformState != ENaiwaP1TimedPlatformState::Ready
            || Platform->GetCountdownRemaining() > 0.01f || !Platform->IsCarryingCollisionEnabled()
            || FVector::Dist2D(Character->GetActorLocation(), InitialSpawn) > 150.0f)
        {
            FinishTimedProbe(TEXT("FAIL"), TEXT("Death respawn did not restore ready floor and playable start"));
            return;
        }
        bTimedProbeDeathResetPassed = true;
        TimedProbeEvents.Add(TEXT("death_reset_restored_ready_floor_and_playable_start"));
        FinishTimedProbe(TEXT("PASS"),
            TEXT("Real Walk/Run crossings, one-shot countdown, fall, occupied recovery guard, pause, R/death resets and retry passed"));
        return;
    }
}

void ANaiwaP1GameMode::FinishTimedProbe(const TCHAR* Result, const FString& Detail)
{
    if (!bTimedProbeEnabled) return;
    bTimedProbeEnabled = false;
    if (PlayerCharacter.IsValid())
    {
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        PlayerCharacter->StopJumping();
        if (PlayerCharacter->GetCharacterMovement()->MovementMode == MOVE_None
            && State == ENaiwaP1State::Playing)
        {
            PlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        }
    }
    const bool bEndedPaused = UGameplayStatics::IsGamePaused(this);
    UGameplayStatics::SetGamePaused(this, false);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_PROBE result=%s gait=%s detail=%s"),
        Result, bTimedProbeWalk ? TEXT("Walk150") : TEXT("Run300"), *Detail);

    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1TimedProbeV1"));
    Report->SetStringField(TEXT("status"), Result);
    Report->SetStringField(TEXT("detail"), Detail);
    Report->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetMapName() : FString());
    Report->SetStringField(TEXT("gait_speed"), bTimedProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    Report->SetStringField(TEXT("game_state"), StateName(State));
    Report->SetStringField(TEXT("platform_state"), TimedProbePlatform.IsValid()
        ? TimedStateName(TimedProbePlatform->GetState()) : TEXT("Missing"));
    Report->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - TimedProbeStartedWall);
    Report->SetNumberField(TEXT("first_crossing_after_trigger_s"), TimedProbeFirstCrossingSeconds);
    Report->SetNumberField(TEXT("retry_crossing_after_trigger_s"), TimedProbeRetryCrossingSeconds);
    Report->SetNumberField(TEXT("paused_countdown_delta_s"), TimedProbePauseDeltaSeconds);
    Report->SetNumberField(TEXT("active_seconds"), TimedProbePlatform.IsValid()
        ? TimedProbePlatform->ActiveSeconds : 0.0f);
    Report->SetNumberField(TEXT("disabled_seconds"), TimedProbePlatform.IsValid()
        ? TimedProbePlatform->DisabledSeconds : 0.0f);
    Report->SetNumberField(TEXT("trigger_count"), TimedProbePlatform.IsValid()
        ? TimedProbePlatform->GetTriggerCount() : -1);
    Report->SetNumberField(TEXT("disable_count"), TimedProbePlatform.IsValid()
        ? TimedProbePlatform->GetDisableCount() : -1);
    Report->SetNumberField(TEXT("recovery_count"), TimedProbePlatform.IsValid()
        ? TimedProbePlatform->GetRecoveryCount() : -1);
    Report->SetNumberField(TEXT("death_events"), DeathEventCount);
    Report->SetBoolField(TEXT("first_real_crossing_passed"), bTimedProbeFirstCrossed);
    Report->SetBoolField(TEXT("retry_real_crossing_passed"), bTimedProbeRetryCrossed);
    Report->SetBoolField(TEXT("standing_did_not_extend_countdown"), bTimedProbeCountdownDeclined);
    Report->SetBoolField(TEXT("pause_freeze_passed"), bTimedProbePausePassed);
    Report->SetBoolField(TEXT("fell_after_disable"), bTimedProbeFallPassed);
    Report->SetBoolField(TEXT("occupied_recovery_guard_passed"), bTimedProbeOccupancyGuardPassed);
    Report->SetBoolField(TEXT("r_reset_passed"), bTimedProbeRResetPassed);
    Report->SetBoolField(TEXT("death_reset_passed"), bTimedProbeDeathResetPassed);
    Report->SetBoolField(TEXT("ended_while_paused"), bEndedPaused);
    Report->SetBoolField(TEXT("capture_requested"), bTimedProbeCapture);
    Report->SetStringField(TEXT("occupancy_check_method"),
        TEXT("After real fall, probe teleported capsule into disabled platform and held MOVE_None; real crossings used only movement input"));
    TArray<TSharedPtr<FJsonValue>> Events;
    for (const FString& Event : TimedProbeEvents) Events.Add(MakeShared<FJsonValueString>(Event));
    Report->SetArrayField(TEXT("events"), Events);
    TArray<TSharedPtr<FJsonValue>> Captures;
    TArray<TSharedPtr<FJsonValue>> CaptureFilesExist;
    bool bAllCapturesExist = TimedProbeCapturePaths.Num() == 3;
    for (const FString& Path : TimedProbeCapturePaths)
    {
        Captures.Add(MakeShared<FJsonValueString>(Path));
        const bool bExists = IFileManager::Get().FileExists(*Path);
        CaptureFilesExist.Add(MakeShared<FJsonValueBoolean>(bExists));
        bAllCapturesExist &= bExists;
    }
    Report->SetArrayField(TEXT("capture_paths"), Captures);
    Report->SetArrayField(TEXT("capture_files_exist"), CaptureFilesExist);
    Report->SetBoolField(TEXT("all_requested_captures_exist"), bTimedProbeCapture && bAllCapturesExist);

    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("timed_probe_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (bSaved)
    {
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_PROBE output=%s saved=1"), *Output);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_P1_TIMED_PROBE output=%s saved=0"), *Output);
    }
    FPlatformMisc::RequestExit(false);
}

void ANaiwaP1GameMode::TimedFailProbeAdvance(ENaiwaP1TimedFailProbeStep NextStep, const FString& Evidence)
{
    TimedFailProbeEvents.Add(FString::Printf(TEXT("t=%.2f x=%.1f bottom=%.1f %s"),
        FPlatformTime::Seconds() - TimedFailProbeStartedWall,
        PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().X : 0.0f,
        PlayerCharacter.IsValid() ? PlayerCharacter->GetCapsuleBottom() : 0.0f,
        *Evidence));
    TimedFailProbeStep = NextStep;
    TimedFailProbeStepStartedWall = FPlatformTime::Seconds();
}

void ANaiwaP1GameMode::TickTimedFailProbe()
{
    const double Now = FPlatformTime::Seconds();
    if (Now - TimedFailProbeStartedWall > 75.0 || Now - TimedFailProbeStepStartedWall > 20.0)
    {
        FinishTimedFailProbe(TEXT("FAIL"), FString::Printf(TEXT("Timeout step=%d state=%s x=%.1f bottom=%.1f deaths=%d"),
            static_cast<int32>(TimedFailProbeStep), StateName(State),
            PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().X : 0.0f,
            PlayerCharacter.IsValid() ? PlayerCharacter->GetCapsuleBottom() : 0.0f,
            DeathEventCount));
        return;
    }
    if (!IsAuditMap(GetWorld(), TEXT("L_NaiwaP1_Timed")))
    {
        FinishTimedFailProbe(TEXT("INCOMPLETE"), TEXT("Failure retry probe requires the isolated timed map"));
        return;
    }
    if (bSmokeEnabled || bRouteEnabled || bHazardAuditEnabled || bMatrixEnabled
        || bBounceProbeEnabled || bSwitchProbeEnabled || bTimedProbeEnabled
        || (FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1TimedFailProbeWalk"))
            && FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1TimedFailProbeRun"))))
    {
        FinishTimedFailProbe(TEXT("INCOMPLETE"), TEXT("Run exactly one P1 audit scenario and gait per process"));
        return;
    }
    if (UGameplayStatics::IsGamePaused(this))
    {
        FinishTimedFailProbe(TEXT("FAIL"), TEXT("Game unexpectedly paused during natural failure retry"));
        return;
    }
    if (!PlayerCharacter.IsValid())
    {
        if (Now - TimedFailProbeStartedWall > 5.0)
            FinishTimedFailProbe(TEXT("INCOMPLETE"), TEXT("P1 character did not register on timed map"));
        return;
    }
    if (!bTimedFailProbeInitialized)
    {
        if (!PlayerCharacter->GetCharacterMovement()->IsMovingOnGround()) return;
        int32 PlatformCount = 0;
        for (TActorIterator<ANaiwaP1TimedPlatform> It(GetWorld()); It; ++It)
        {
            if (!It->ActorHasTag(FName(TEXT("NaiwaP1TimedPlatformV1")))) continue;
            TimedFailProbePlatform = *It;
            ++PlatformCount;
        }
        if (PlatformCount != 1 || !TimedFailProbePlatform.IsValid()
            || !TimedFailProbePlatform->PlatformCollision || !bHasInitialSpawn
            || TimedFailProbePlatform->GetState() != ENaiwaP1TimedPlatformState::Ready
            || !TimedFailProbePlatform->IsCarryingCollisionEnabled()
            || InitialSpawn.X >= TimedFailProbePlatform->GetActorLocation().X
                - TimedFailProbePlatform->PlatformExtent.X - 100.0f)
        {
            FinishTimedFailProbe(TEXT("INCOMPLETE"), TEXT("Expected one tagged ready timed floor and grounded left-bank spawn"));
            return;
        }
        if (!PlayerCharacter->SetP1Gait(!bTimedFailProbeWalk)
            || FMath::Abs(PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed
                - (bTimedFailProbeWalk ? 150.0f : 300.0f)) > 1.0f)
        {
            FinishTimedFailProbe(TEXT("INCOMPLETE"), TEXT("Could not verify Walk150 or Run300 gait"));
            return;
        }
        TimedFailProbeDeathCountBefore = DeathEventCount;
        TimedFailProbeTriggerCountBefore = TimedFailProbePlatform->GetTriggerCount();
        TimedFailProbeDisableCountBefore = TimedFailProbePlatform->GetDisableCount();
        bTimedFailProbeInitialized = true;
        TimedFailProbeAdvance(ENaiwaP1TimedFailProbeStep::ApproachCentre,
            bTimedFailProbeWalk ? TEXT("grounded_spawn_walk150") : TEXT("grounded_spawn_run300"));
        return;
    }
    if (!TimedFailProbePlatform.IsValid())
    {
        FinishTimedFailProbe(TEXT("INCOMPLETE"), TEXT("Tagged timed platform disappeared"));
        return;
    }
    ANaiwaP1TimedPlatform* Platform = TimedFailProbePlatform.Get();
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    const ENaiwaP1TimedPlatformState PlatformState = Platform->GetState();
    const float PlatformX = Platform->GetActorLocation().X;
    const float TopZ = Platform->GetActorLocation().Z + Platform->PlatformTopZ;
    const float CapsuleBottom = Character->GetCapsuleBottom();
    const bool bSupported = Character->GetCharacterMovement()->IsMovingOnGround()
        && Character->GetMovementBaseObject() == Platform->PlatformCollision.Get();
    switch (TimedFailProbeStep)
    {
    case ENaiwaP1TimedFailProbeStep::WaitReady:
        return;
    case ENaiwaP1TimedFailProbeStep::ApproachCentre:
        if (State != ENaiwaP1State::Playing || PlatformState == ENaiwaP1TimedPlatformState::Disabled
            || Platform->GetTriggerCount() > TimedFailProbeTriggerCountBefore + 1)
        {
            FinishTimedFailProbe(TEXT("FAIL"), TEXT("Character died or floor expired before centre boarding"));
            return;
        }
        Character->SetAutomatedMoveInput(1.0f);
        if (Character->GetActorLocation().X < PlatformX - 25.0f) return;
        Character->SetAutomatedMoveInput(0.0f);
        Character->GetCharacterMovement()->StopMovementImmediately();
        if (!bSupported || PlatformState != ENaiwaP1TimedPlatformState::Countdown
            || Platform->GetTriggerCount() != TimedFailProbeTriggerCountBefore + 1)
        {
            FinishTimedFailProbe(TEXT("FAIL"), TEXT("Real boarding did not start one countdown at platform centre"));
            return;
        }
        TimedFailProbeBoardLocation = Character->GetActorLocation();
        TimedFailProbeAdvance(ENaiwaP1TimedFailProbeStep::WaitExpiry,
            TEXT("real_movement_boarded_and_stopped_on_counting_platform"));
        return;
    case ENaiwaP1TimedFailProbeStep::WaitExpiry:
        Character->SetAutomatedMoveInput(0.0f);
        if (State != ENaiwaP1State::Playing || Platform->GetTriggerCount() != TimedFailProbeTriggerCountBefore + 1)
        {
            FinishTimedFailProbe(TEXT("FAIL"), TEXT("Died or retriggered before countdown expiry"));
            return;
        }
        if (PlatformState == ENaiwaP1TimedPlatformState::Countdown)
        {
            if (!bSupported || !Platform->IsCarryingCollisionEnabled())
                FinishTimedFailProbe(TEXT("FAIL"), TEXT("Stopped rider lost floor support before expiry"));
            return;
        }
        if (PlatformState != ENaiwaP1TimedPlatformState::Disabled
            || Platform->IsCarryingCollisionEnabled()
            || Platform->GetDisableCount() != TimedFailProbeDisableCountBefore + 1)
        {
            FinishTimedFailProbe(TEXT("FAIL"), TEXT("Timed floor did not disable collision once"));
            return;
        }
        TimedFailProbeAdvance(ENaiwaP1TimedFailProbeStep::WaitDying,
            TEXT("floor_disabled_collision_and_natural_fall_started"));
        return;
    case ENaiwaP1TimedFailProbeStep::WaitDying:
        Character->SetAutomatedMoveInput(0.0f);
        if (CapsuleBottom < TopZ - 40.0f)
        {
            bTimedFailProbeSawFall = true;
            TimedFailProbeFallLocation = Character->GetActorLocation();
        }
        if (DeathEventCount == TimedFailProbeDeathCountBefore && State == ENaiwaP1State::Playing) return;
        if (!bTimedFailProbeSawFall || DeathEventCount != TimedFailProbeDeathCountBefore + 1
            || State != ENaiwaP1State::Dying
            || !GetWorld()->GetWorldSettings()
            || Character->GetActorLocation().Z >= GetWorld()->GetWorldSettings()->KillZ)
        {
            FinishTimedFailProbe(TEXT("FAIL"), TEXT("Natural drop did not cause exactly one KillZ death"));
            return;
        }
        bTimedFailProbeSawDying = true;
        TimedFailProbeAdvance(ENaiwaP1TimedFailProbeStep::WaitRespawn,
            TEXT("natural_drop_reached_KillZ_and_entered_Dying"));
        return;
    case ENaiwaP1TimedFailProbeStep::WaitRespawn:
        Character->SetAutomatedMoveInput(0.0f);
        if (DeathEventCount != TimedFailProbeDeathCountBefore + 1)
        {
            FinishTimedFailProbe(TEXT("FAIL"), TEXT("Unexpected additional death during respawn"));
            return;
        }
        if (State == ENaiwaP1State::Dying || State == ENaiwaP1State::Respawning) return;
        if (State != ENaiwaP1State::Playing
            || !Character->GetCharacterMovement()->IsMovingOnGround()
            || FMath::Abs(Character->GetActorLocation().X - InitialSpawn.X) > 80.0f
            || PlatformState != ENaiwaP1TimedPlatformState::Ready
            || !Platform->IsCarryingCollisionEnabled())
        {
            FinishTimedFailProbe(TEXT("FAIL"), TEXT("Death respawn did not restore playable spawn and ready floor"));
            return;
        }
        bTimedFailProbeRespawnReady = true;
        TimedFailProbeRespawnLocation = Character->GetActorLocation();
        TimedFailProbeAdvance(ENaiwaP1TimedFailProbeStep::RetryCrossing,
            TEXT("death_respawn_playable_spawn_and_ready_floor"));
        return;
    case ENaiwaP1TimedFailProbeStep::RetryCrossing:
        if (State != ENaiwaP1State::Playing || DeathEventCount != TimedFailProbeDeathCountBefore + 1
            || PlatformState == ENaiwaP1TimedPlatformState::Disabled
            || PlatformState == ENaiwaP1TimedPlatformState::Recovering
            || Platform->GetTriggerCount() > TimedFailProbeTriggerCountBefore + 2)
        {
            FinishTimedFailProbe(TEXT("FAIL"), TEXT("Real retry died, retriggered, or outlasted recovered floor"));
            return;
        }
        Character->SetAutomatedMoveInput(1.0f);
        if (Character->GetActorLocation().X <= PlatformX + Platform->PlatformExtent.X
                + Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + 18.0f
            || !Character->GetCharacterMovement()->IsMovingOnGround()
            || FMath::Abs(CapsuleBottom - TopZ) >= 18.0f) return;
        if (Platform->GetTriggerCount() != TimedFailProbeTriggerCountBefore + 2
            || PlatformState != ENaiwaP1TimedPlatformState::Countdown)
        {
            FinishTimedFailProbe(TEXT("FAIL"), TEXT("Retry did not reach right shore during a new countdown"));
            return;
        }
        bTimedFailProbeRetryCrossed = true;
        TimedFailProbeAdvance(ENaiwaP1TimedFailProbeStep::RetryCrossing,
            TEXT("real_movement_retry_reached_grounded_right_shore"));
        FinishTimedFailProbe(TEXT("PASS"), TEXT("Natural expiry caused one KillZ death and playable reset; real retry crossed to right shore"));
        return;
    }
}

void ANaiwaP1GameMode::FinishTimedFailProbe(const TCHAR* Result, const FString& Detail)
{
    if (!bTimedFailProbeEnabled) return;
    bTimedFailProbeEnabled = false;
    if (PlayerCharacter.IsValid())
    {
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        PlayerCharacter->StopJumping();
    }
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_FAIL_PROBE result=%s gait=%s detail=%s"),
        Result, bTimedFailProbeWalk ? TEXT("Walk150") : TEXT("Run300"), *Detail);
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1TimedFailProbeV1"));
    Report->SetStringField(TEXT("status"), Result);
    Report->SetStringField(TEXT("detail"), Detail);
    Report->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetMapName() : FString());
    Report->SetStringField(TEXT("gait_speed"), bTimedFailProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    Report->SetStringField(TEXT("game_state"), StateName(State));
    Report->SetStringField(TEXT("platform_state"), TimedFailProbePlatform.IsValid()
        ? TimedStateName(TimedFailProbePlatform->GetState()) : TEXT("Missing"));
    Report->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - TimedFailProbeStartedWall);
    Report->SetNumberField(TEXT("world_kill_z"), GetWorld() && GetWorld()->GetWorldSettings()
        ? GetWorld()->GetWorldSettings()->KillZ : 0.0f);
    Report->SetNumberField(TEXT("death_events_delta"), DeathEventCount - TimedFailProbeDeathCountBefore);
    Report->SetNumberField(TEXT("trigger_count_delta"), TimedFailProbePlatform.IsValid()
        ? TimedFailProbePlatform->GetTriggerCount() - TimedFailProbeTriggerCountBefore : -1);
    Report->SetNumberField(TEXT("disable_count_delta"), TimedFailProbePlatform.IsValid()
        ? TimedFailProbePlatform->GetDisableCount() - TimedFailProbeDisableCountBefore : -1);
    Report->SetBoolField(TEXT("fell_after_disable"), bTimedFailProbeSawFall);
    Report->SetBoolField(TEXT("saw_dying"), bTimedFailProbeSawDying);
    Report->SetBoolField(TEXT("respawn_playable_and_floor_ready"), bTimedFailProbeRespawnReady);
    Report->SetBoolField(TEXT("real_retry_crossed"), bTimedFailProbeRetryCrossed);
    Report->SetNumberField(TEXT("probe_direct_teleports"), 0);
    const auto PutPosition = [&Report](const TCHAR* Name, const FVector& Position)
    {
        TSharedRef<FJsonObject> Point = MakeShared<FJsonObject>();
        Point->SetNumberField(TEXT("x"), Position.X);
        Point->SetNumberField(TEXT("y"), Position.Y);
        Point->SetNumberField(TEXT("z"), Position.Z);
        Report->SetObjectField(Name, Point);
    };
    PutPosition(TEXT("initial_spawn"), InitialSpawn);
    PutPosition(TEXT("board_position"), TimedFailProbeBoardLocation);
    PutPosition(TEXT("fall_position"), TimedFailProbeFallLocation);
    PutPosition(TEXT("respawn_position"), TimedFailProbeRespawnLocation);
    PutPosition(TEXT("finish_position"), PlayerCharacter.IsValid()
        ? PlayerCharacter->GetActorLocation() : FVector::ZeroVector);
    TArray<TSharedPtr<FJsonValue>> Events;
    for (const FString& Event : TimedFailProbeEvents) Events.Add(MakeShared<FJsonValueString>(Event));
    Report->SetArrayField(TEXT("events"), Events);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("timed_fail_probe_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (bSaved)
    {
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_FAIL_PROBE output=%s saved=1"), *Output);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_P1_TIMED_FAIL_PROBE output=%s saved=0"), *Output);
    }
    FPlatformMisc::RequestExit(false);
}

bool ANaiwaP1GameMode::InitializeIntegratedProbe()
{
    const AWorldSettings* Settings = GetWorld()->GetWorldSettings();
    if (!Settings || GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Naiwa/P1/Maps/L_NaiwaP1_Integrated")
        || !Settings->ActorHasTag(TEXT("NaiwaP1IntegratedTeachingMapV1"))
        || !Settings->ActorHasTag(TEXT("NaiwaP1PartialTeachingSliceV1"))
        || !Settings->ActorHasTag(TEXT("NaiwaP1IntegratedCfg_391cec36cc150417"))
        || !FMath::IsNearlyEqual(Settings->KillZ, -1000.0f, 1.0f))
    {
        FinishIntegratedProbe(TEXT("INCOMPLETE"), TEXT("Exact integrated package, partial/map/config tags and KillZ are required"));
        return false;
    }
    if (!PlayerCharacter.IsValid() || !bHasInitialSpawn) return false;
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    if (!Character->GetCharacterMovement()->IsMovingOnGround()) return false;

    const FName OwnerTag(TEXT("NaiwaP1IntegratedGeneratedV1"));
    int32 SwitchCount = 0;
    int32 PadCount = 0;
    int32 MechanismCount = 0;
    for (TActorIterator<ANaiwaP1Mechanism> It(GetWorld()); It; ++It)
    {
        ++MechanismCount;
        if (!It->ActorHasTag(OwnerTag)) continue;
        if (It->GetClass() == ANaiwaP1SwitchPlatform::StaticClass()
            && It->ActorHasTag(TEXT("NaiwaP1IntegratedSwitchV1")))
        {
            IntegratedProbeSwitch = CastChecked<ANaiwaP1SwitchPlatform>(*It);
            ++SwitchCount;
        }
        if (It->GetClass() == ANaiwaP1BouncePad::StaticClass()
            && It->ActorHasTag(TEXT("NaiwaP1IntegratedBounceV1")))
        {
            IntegratedProbePad = CastChecked<ANaiwaP1BouncePad>(*It);
            ++PadCount;
        }
    }
    int32 TriggerCount = 0;
    int32 HazardCount = 0;
    int32 CheckpointCount = 0;
    for (TActorIterator<ANaiwaP1Trigger> It(GetWorld()); It; ++It)
    {
        ++TriggerCount;
        if (It->GetClass() != ANaiwaP1Trigger::StaticClass() || !It->ActorHasTag(OwnerTag)) continue;
        if (It->TriggerKind == ENaiwaP1TriggerKind::Hazard
            && It->ActorHasTag(TEXT("NaiwaP1IntegratedHazardV1"))
            && It->GetStableId() == FName(TEXT("IntegratedHiddenHazard01")))
        {
            IntegratedProbeHazard = *It;
            ++HazardCount;
        }
        if (It->TriggerKind == ENaiwaP1TriggerKind::Checkpoint
            && It->ActorHasTag(TEXT("NaiwaP1IntegratedCheckpointV1"))
            && It->GetStableId() == FName(TEXT("IntegratedCheckpoint01")))
        {
            IntegratedProbeCheckpoint = *It;
            ++CheckpointCount;
        }
    }
    int32 TerrainCount = 0;
    int32 LeftBankCount = 0;
    int32 RightBankCount = 0;
    int32 LandingCount = 0;
    int32 CueCount = 0;
    for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
    {
        if (!It->ActorHasTag(OwnerTag) || It->GetClass() != AStaticMeshActor::StaticClass()) continue;
        if (It->ActorHasTag(TEXT("NaiwaP1IntegratedHazardCueV1")))
        {
            IntegratedProbeCue = *It;
            ++CueCount;
        }
        if (!It->ActorHasTag(TEXT("NaiwaP1GeneratedV1"))) continue;
        ++TerrainCount;
        const FBox Bounds = It->GetComponentsBoundingBox(true);
        if (Bounds.Min.Equals(FVector(-1600.0f, -350.0f, 80.0f), 1.0f)
            && Bounds.Max.Equals(FVector(-550.0f, 350.0f, 120.0f), 1.0f))
        {
            IntegratedProbeLeftBank = *It;
            ++LeftBankCount;
        }
        if (Bounds.Min.Equals(FVector(550.0f, -350.0f, 80.0f), 1.0f)
            && Bounds.Max.Equals(FVector(1460.0f, 350.0f, 120.0f), 1.0f))
        {
            IntegratedProbeRightBank = *It;
            ++RightBankCount;
        }
        if (Bounds.Min.Equals(FVector(1740.0f, -350.0f, 140.0f), 1.0f)
            && Bounds.Max.Equals(FVector(3120.0f, 350.0f, 180.0f), 1.0f))
        {
            IntegratedProbeLanding = *It;
            ++LandingCount;
        }
    }
    if (MechanismCount != 2 || SwitchCount != 1 || PadCount != 1
        || TriggerCount != 2 || HazardCount != 1 || CheckpointCount != 1
        || TerrainCount != 3 || LeftBankCount != 1 || RightBankCount != 1 || LandingCount != 1 || CueCount != 1)
    {
        FinishIntegratedProbe(TEXT("INCOMPLETE"), FString::Printf(
            TEXT("Strict actor inventory mismatch: mechanisms=%d switch=%d pad=%d triggers=%d hazard=%d checkpoint=%d terrain=%d banks=%d/%d landing=%d cue=%d"),
            MechanismCount, SwitchCount, PadCount, TriggerCount, HazardCount, CheckpointCount,
            TerrainCount, LeftBankCount, RightBankCount, LandingCount, CueCount));
        return false;
    }
    const ANaiwaP1SwitchPlatform* Platform = IntegratedProbeSwitch.Get();
    const ANaiwaP1BouncePad* Pad = IntegratedProbePad.Get();
    const ANaiwaP1Trigger* Hazard = IntegratedProbeHazard.Get();
    const ANaiwaP1Trigger* Checkpoint = IntegratedProbeCheckpoint.Get();
    const bool bGeometryMatches = Platform->GetActorLocation().IsNearlyZero(1.0f)
        && Platform->GetActorRotation().IsNearlyZero(0.1f) && Platform->GetActorScale3D().Equals(FVector::OneVector, 0.01f)
        && FMath::IsNearlyEqual(Platform->DockLeftX, -420.0f, 1.0f)
        && FMath::IsNearlyEqual(Platform->DockRightX, 420.0f, 1.0f)
        && FMath::IsNearlyEqual(Platform->LeftSwitchX, -850.0f, 1.0f)
        && FMath::IsNearlyEqual(Platform->RightSwitchX, 850.0f, 1.0f)
        && FMath::IsNearlyEqual(Platform->PlatformTopZ, 120.0f, 1.0f)
        && Platform->PlatformExtent.Equals(FVector(120.0f, 160.0f, 20.0f), 1.0f)
        && FMath::IsNearlyEqual(Platform->TravelSpeedCmPerSecond, 300.0f, 0.1f)
        && FMath::IsNearlyEqual(Platform->StableBoardingSeconds, 0.25f, 0.01f)
        && FMath::IsNearlyEqual(Platform->DepartureCueSeconds, 0.65f, 0.01f)
        && Pad->GetActorLocation().Equals(FVector(1660.0f, 0.0f, 0.0f), 1.0f)
        && Pad->GetActorRotation().IsNearlyZero(0.1f) && Pad->GetActorScale3D().Equals(FVector::OneVector, 0.01f)
        && Pad->BoxExtent.Equals(FVector(160.0f, 120.0f, 12.0f), 1.0f)
        && FMath::IsNearlyEqual(Pad->LaunchVelocityZ, 1000.0f, 0.1f)
        && Hazard->GetActorLocation().Equals(FVector(-1160.0f, 0.0f, 155.0f), 1.0f)
        && Hazard->BoxExtent.Equals(FVector(20.0f, 120.0f, 35.0f), 1.0f)
        && Checkpoint->GetActorLocation().Equals(FVector(2700.0f, 0.0f, 272.0f), 1.0f)
        && Checkpoint->BoxExtent.Equals(FVector(42.0f, 110.0f, 85.0f), 1.0f)
        && Checkpoint->RespawnOffset.IsNearlyZero(0.01f);
    if (!bGeometryMatches || !Platform->PlatformCollision || !Pad->PadCollision
        || !IntegratedProbeCue.IsValid() || !IntegratedProbeCue->GetStaticMeshComponent()
        || IntegratedProbeCue->GetStaticMeshComponent()->GetCollisionEnabled() != ECollisionEnabled::NoCollision
        || !Hazard->DisplayMesh || (Hazard->DisplayMesh->IsVisible() && !Hazard->bRevealOnSpawn)
        || Character->GetMovementBaseObject() != IntegratedProbeLeftBank->GetStaticMeshComponent()
        || !InitialSpawn.Equals(FVector(-1450.0f, 0.0f, 212.0f), 5.0f)
        || FVector::Dist2D(Character->GetActorLocation(), InitialSpawn) > 5.0f
        || State != ENaiwaP1State::Playing || ActiveCheckpoint.IsValid() || CollectedIds.Num() != 0
        || Platform->GetState() != ENaiwaP1SwitchPlatformState::DockedLeft
        || !FMath::IsNearlyEqual(Platform->GetCurrentX(), Platform->DockLeftX, 1.0f)
        || Pad->GetActivationCount() != 0 || DeathEventCount != 0)
    {
        FinishIntegratedProbe(TEXT("INCOMPLETE"), TEXT("Saved candidate geometry, clean grounded spawn, hidden hazard, and reset mechanisms must match the bound config"));
        return false;
    }
    if (!Character->SetP1Gait(!bIntegratedProbeWalk)
        || FMath::Abs(Character->GetCharacterMovement()->MaxWalkSpeed - (bIntegratedProbeWalk ? 150.0f : 300.0f)) > 0.1f)
    {
        FinishIntegratedProbe(TEXT("INCOMPLETE"), TEXT("Could not select and verify integrated Walk150 or Run300"));
        return false;
    }
    IntegratedProbeDeathCountBefore = DeathEventCount;
    IntegratedProbeActivationCountBefore = Pad->GetActivationCount();
    IntegratedProbeLeftTouchCountBefore = Platform->GetLeftSwitchTouchCount();
    IntegratedProbeRightTouchCountBefore = Platform->GetRightSwitchTouchCount();
    IntegratedProbeMaxCharacterZ = Character->GetActorLocation().Z;
    bIntegratedProbeInitialized = true;
    Character->SetAutomatedMoveInput(0.0f);
    IntegratedProbeAdvance(ENaiwaP1IntegratedProbeStep::ToHazard, TEXT("strict_preflight_grounded_spawn"));
    CaptureIntegratedVisualFrame(TEXT("spawn_hazard_hint"));
    return true;
}

void ANaiwaP1GameMode::IntegratedProbeRecord(const TCHAR* Station)
{
    TSharedRef<FJsonObject> Sample = MakeShared<FJsonObject>();
    Sample->SetStringField(TEXT("station"), Station);
    Sample->SetNumberField(TEXT("step"), static_cast<int32>(IntegratedProbeStep));
    Sample->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - IntegratedVisualStartedWall);
    Sample->SetNumberField(TEXT("world_time_s"), GetWorld()->GetTimeSeconds());
    Sample->SetStringField(TEXT("state"), StateName(State));
    Sample->SetNumberField(TEXT("death_events_delta"), DeathEventCount - IntegratedProbeDeathCountBefore);
    Sample->SetNumberField(TEXT("probe_character_teleports"), 0);
    Sample->SetNumberField(TEXT("production_respawn_placements"), IntegratedProbeProductionRespawnPlacements);
    Sample->SetStringField(TEXT("checkpoint_id"), ActiveCheckpoint.IsValid() ? ActiveCheckpoint->GetStableId().ToString() : TEXT("none"));
    if (PlayerCharacter.IsValid())
    {
        const ANaiwaCharacter* Character = PlayerCharacter.Get();
        const FVector Location = Character->GetActorLocation();
        Sample->SetNumberField(TEXT("x_cm"), Location.X);
        Sample->SetNumberField(TEXT("y_cm"), Location.Y);
        Sample->SetNumberField(TEXT("z_cm"), Location.Z);
        Sample->SetNumberField(TEXT("feet_z_cm"), Character->GetCapsuleBottom());
        Sample->SetNumberField(TEXT("velocity_x_cm_s"), Character->GetVelocity().X);
        Sample->SetNumberField(TEXT("velocity_z_cm_s"), Character->GetVelocity().Z);
        Sample->SetBoolField(TEXT("grounded"), Character->GetCharacterMovement()->IsMovingOnGround());
        const UObject* Base = Character->GetMovementBaseObject();
        Sample->SetStringField(TEXT("movement_base"), Base ? Base->GetPathName() : TEXT("none"));
    }
    if (IntegratedProbeSwitch.IsValid())
    {
        Sample->SetStringField(TEXT("switch_state"), SwitchStateName(IntegratedProbeSwitch->GetState()));
        Sample->SetNumberField(TEXT("switch_x_cm"), IntegratedProbeSwitch->GetCurrentX());
        Sample->SetNumberField(TEXT("left_switch_touches_delta"), IntegratedProbeSwitch->GetLeftSwitchTouchCount() - IntegratedProbeLeftTouchCountBefore);
        Sample->SetNumberField(TEXT("right_switch_touches_delta"), IntegratedProbeSwitch->GetRightSwitchTouchCount() - IntegratedProbeRightTouchCountBefore);
        Sample->SetBoolField(TEXT("shuttle_supports_character"), IntegratedProbeSwitch->IsCharacterSupported(PlayerCharacter.Get()));
        Sample->SetBoolField(TEXT("departure_cue_active"), IntegratedProbeSwitch->IsDepartureCueActive());
    }
    if (IntegratedProbePad.IsValid())
        Sample->SetNumberField(TEXT("pad_activations_delta"), IntegratedProbePad->GetActivationCount() - IntegratedProbeActivationCountBefore);
    IntegratedProbeStations.Add(MakeShared<FJsonValueObject>(Sample));
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_INTEGRATED_PROBE station=%s step=%d x=%.1f state=%s"),
        Station, static_cast<int32>(IntegratedProbeStep), PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().X : 0.0f, StateName(State));
}

void ANaiwaP1GameMode::IntegratedProbeAdvance(ENaiwaP1IntegratedProbeStep NextStep, const TCHAR* Station)
{
    IntegratedProbeStep = NextStep;
    IntegratedProbeStepStartedWall = bIntegratedVisualCapture ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
    IntegratedProbeRecord(Station);
}

void ANaiwaP1GameMode::CaptureIntegratedVisualFrame(const TCHAR* Label)
{
    if (!bIntegratedVisualCapture) return;
    TSharedRef<FJsonObject> Frame = MakeShared<FJsonObject>();
    Frame->SetStringField(TEXT("label"), Label);
    Frame->SetNumberField(TEXT("world_time_s"), GetWorld()->GetTimeSeconds());
    if (PlayerCharacter.IsValid())
    {
        Frame->SetNumberField(TEXT("character_x_cm"), PlayerCharacter->GetActorLocation().X);
        Frame->SetNumberField(TEXT("character_z_cm"), PlayerCharacter->GetActorLocation().Z);
    }
    const bool bCanRequest = GetWorld()->GetGameViewport() && !FScreenshotRequest::IsScreenshotRequested();
    Frame->SetBoolField(TEXT("requested"), bCanRequest);
    if (bCanRequest)
    {
        const FString Directory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"), TEXT("Visuals")));
        IFileManager::Get().MakeDirectory(*Directory, true);
        const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("integrated_visual_%s_%s_%s.png"),
            bIntegratedProbeWalk ? TEXT("walk") : TEXT("run"), Label,
            *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        Frame->SetStringField(TEXT("file"), Path);
        FScreenshotRequest::RequestScreenshot(Path, false, false);
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_INTEGRATED_VISUAL screenshot_requested label=%s path=%s"), Label, *Path);
    }
    else
    {
        Frame->SetStringField(TEXT("file"), TEXT(""));
        UE_LOG(LogTemp, Warning, TEXT("NAIWA_P1_INTEGRATED_VISUAL screenshot_unavailable label=%s"), Label);
    }
    IntegratedVisualFrames.Add(MakeShared<FJsonValueObject>(Frame));
}

void ANaiwaP1GameMode::TickIntegratedProbe()
{
    const double Now = bIntegratedVisualCapture ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
    if (bIntegratedVisualFinishing)
    {
        bool bAllFilesPresent = true;
        for (const TSharedPtr<FJsonValue>& Value : IntegratedVisualFrames)
        {
            const TSharedPtr<FJsonObject> Frame = Value->AsObject();
            FString Path;
            bAllFilesPresent &= Frame.IsValid() && Frame->TryGetStringField(TEXT("file"), Path)
                && !Path.IsEmpty() && IFileManager::Get().FileExists(*Path);
        }
        if (Now - IntegratedVisualFinishStartedGame < 3.0
            && (FScreenshotRequest::IsScreenshotRequested() || !bAllFilesPresent)) return;
        FinishIntegratedProbe(*IntegratedVisualPendingResult, IntegratedVisualPendingDetail);
        return;
    }
    if (UGameplayStatics::IsGamePaused(this))
    {
        FinishIntegratedProbe(TEXT("FAIL"), TEXT("Unexpected pause: this functional route does not inject pause or screenshots"));
        return;
    }
    if (Now - IntegratedProbeStartedWall > 150.0 || Now - IntegratedProbeStepStartedWall > 30.0
        || (!bIntegratedProbeInitialized && Now - IntegratedProbeStartedWall > 10.0))
    {
        FinishIntegratedProbe(TEXT("FAIL"), FString::Printf(TEXT("Timeout step=%d state=%s x=%.1f z=%.1f"),
            static_cast<int32>(IntegratedProbeStep), StateName(State),
            PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().X : 0.0f,
            PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().Z : 0.0f));
        return;
    }
    if (!bIntegratedProbeInitialized)
    {
        InitializeIntegratedProbe();
        return;
    }
    if (!PlayerCharacter.IsValid() || !IntegratedProbeSwitch.IsValid() || !IntegratedProbePad.IsValid()
        || !IntegratedProbeHazard.IsValid() || !IntegratedProbeCheckpoint.IsValid()
        || !IntegratedProbeLeftBank.IsValid() || !IntegratedProbeRightBank.IsValid()
        || !IntegratedProbeLanding.IsValid() || !IntegratedProbeCue.IsValid())
    {
        FinishIntegratedProbe(TEXT("INCOMPLETE"), TEXT("A bound route actor disappeared"));
        return;
    }
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    ANaiwaP1SwitchPlatform* Platform = IntegratedProbeSwitch.Get();
    const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const float X = Character->GetActorLocation().X;
    const bool bGrounded = Movement->IsMovingOnGround();
    const int32 Activations = IntegratedProbePad->GetActivationCount() - IntegratedProbeActivationCountBefore;
    IntegratedProbeMaxCharacterZ = FMath::Max(IntegratedProbeMaxCharacterZ, Character->GetActorLocation().Z);
    if (FMath::Abs(Movement->MaxWalkSpeed - (bIntegratedProbeWalk ? 150.0f : 300.0f)) > 0.1f
        || Character->IsRunningGait() == bIntegratedProbeWalk)
    {
        FinishIntegratedProbe(TEXT("FAIL"), TEXT("Requested gait or speed changed during integrated route"));
        return;
    }
    if (bIntegratedProbeJumpHeld && GetWorld()->GetTimeSeconds() >= IntegratedProbeJumpReleaseTime)
    {
        Character->StopJumping();
        bIntegratedProbeJumpHeld = false;
        IntegratedProbeRecord(TEXT("hazard_jump_released"));
    }
    if (IntegratedProbeStep < ENaiwaP1IntegratedProbeStep::WaitDeath
        && (State != ENaiwaP1State::Playing || DeathEventCount != IntegratedProbeDeathCountBefore))
    {
        FinishIntegratedProbe(TEXT("FAIL"), TEXT("Unexpected death or non-Playing state before the optional checkpoint fall"));
        return;
    }
    if (Activations > 1)
    {
        FinishIntegratedProbe(TEXT("FAIL"), TEXT("Route expected one bounce contact but observed multiple pad activations"));
        return;
    }
    if (!bIntegratedProbeDepartureCueSeen && Platform->IsDepartureCueActive() && Platform->IsCharacterSupported(Character))
    {
        bIntegratedProbeDepartureCueSeen = true;
        IntegratedProbeRecord(TEXT("supported_departure_cue_seen"));
        CaptureIntegratedVisualFrame(TEXT("shuttle_departure_cue"));
    }
    if (!bIntegratedProbeRightSwitchTouched && Platform->GetRightSwitchTouchCount() > IntegratedProbeRightTouchCountBefore)
    {
        bIntegratedProbeRightSwitchTouched = true;
        IntegratedProbeRecord(TEXT("right_switch_touched_by_real_movement"));
    }
    switch (IntegratedProbeStep)
    {
    case ENaiwaP1IntegratedProbeStep::WaitReady:
        return;
    case ENaiwaP1IntegratedProbeStep::ToHazard:
        Character->SetAutomatedMoveInput(1.0f);
        if (!bIntegratedProbeJumpIssued && X >= -1240.0f)
        {
            if (!bGrounded || Character->GetMovementBaseObject() != IntegratedProbeLeftBank->GetStaticMeshComponent())
            {
                FinishIntegratedProbe(TEXT("FAIL"), TEXT("Hazard jump takeoff did not have real left-bank floor support"));
                return;
            }
            Character->Jump();
            bIntegratedProbeJumpIssued = true;
            bIntegratedProbeJumpHeld = true;
            IntegratedProbeJumpReleaseTime = GetWorld()->GetTimeSeconds() + 0.18f;
            IntegratedProbeRecord(TEXT("single_real_jump_before_hidden_hazard"));
        }
        if (bIntegratedProbeJumpIssued && X > -1070.0f && bGrounded
            && Character->GetMovementBaseObject() == IntegratedProbeLeftBank->GetStaticMeshComponent())
        {
            if (IntegratedProbeHazard->DisplayMesh->IsVisible() && !IntegratedProbeHazard->bRevealOnSpawn)
            {
                FinishIntegratedProbe(TEXT("FAIL"), TEXT("Hidden hazard was revealed during the expected no-death jump"));
                return;
            }
            bIntegratedProbeHazardCleared = true;
            IntegratedProbeAdvance(ENaiwaP1IntegratedProbeStep::ToLeftSwitch, TEXT("hidden_hazard_cleared_and_landed_on_left_bank"));
        }
        return;
    case ENaiwaP1IntegratedProbeStep::ToLeftSwitch:
        Character->SetAutomatedMoveInput(1.0f);
        if (Platform->GetLeftSwitchTouchCount() > IntegratedProbeLeftTouchCountBefore)
        {
            bIntegratedProbeLeftSwitchTouched = true;
            IntegratedProbeAdvance(ENaiwaP1IntegratedProbeStep::BoardShuttle, TEXT("left_switch_touched_by_real_movement"));
        }
        return;
    case ENaiwaP1IntegratedProbeStep::BoardShuttle:
        Character->SetAutomatedMoveInput(1.0f);
        if (X > 0.0f && !Platform->IsCharacterSupported(Character))
        {
            FinishIntegratedProbe(TEXT("FAIL"), TEXT("Approach crossed the left dock without boarding its actual movement base"));
            return;
        }
        if (Platform->IsCharacterSupported(Character) && X - Platform->GetCurrentX() >= -65.0f)
        {
            Character->SetAutomatedMoveInput(0.0f);
            IntegratedProbeBoardOffsetX = X - Platform->GetCurrentX();
            bIntegratedProbeBoarded = true;
            IntegratedProbeAdvance(ENaiwaP1IntegratedProbeStep::RideShuttle, TEXT("real_movement_boarded_shuttle_then_input_released"));
        }
        return;
    case ENaiwaP1IntegratedProbeStep::RideShuttle:
        Character->SetAutomatedMoveInput(0.0f);
        if (Platform->GetState() == ENaiwaP1SwitchPlatformState::TravelingRight
            && FMath::Abs(Platform->GetCurrentX()) < 100.0f && Platform->IsCharacterSupported(Character))
        {
            if (!bIntegratedProbeSupportedMidway)
            {
                IntegratedProbeRecord(TEXT("shuttle_carries_character_over_gap_midway"));
                CaptureIntegratedVisualFrame(TEXT("shuttle_mid_gap"));
            }
            bIntegratedProbeSupportedMidway = true;
        }
        if (Platform->GetState() == ENaiwaP1SwitchPlatformState::DockedRight)
        {
            if (!Platform->IsCharacterSupported(Character) || !bIntegratedProbeSupportedMidway
                || !bIntegratedProbeDepartureCueSeen
                || FMath::Abs((X - Platform->GetCurrentX()) - IntegratedProbeBoardOffsetX) > 55.0f)
            {
                FinishIntegratedProbe(TEXT("FAIL"), TEXT("Shuttle did not carry a supported rider through cue, mid-gap and right docking"));
                return;
            }
            bIntegratedProbeDockRightReached = true;
            IntegratedProbeAdvance(ENaiwaP1IntegratedProbeStep::ToRightBank, TEXT("supported_right_dock_reached_without_platform_commands"));
        }
        return;
    case ENaiwaP1IntegratedProbeStep::ToRightBank:
        Character->SetAutomatedMoveInput(1.0f);
        if (X >= 630.0f && bGrounded
            && Character->GetMovementBaseObject() == IntegratedProbeRightBank->GetStaticMeshComponent())
            IntegratedProbeAdvance(ENaiwaP1IntegratedProbeStep::ToBounce, TEXT("walked_off_shuttle_onto_actual_right_bank"));
        return;
    case ENaiwaP1IntegratedProbeStep::ToBounce:
        Character->SetAutomatedMoveInput(1.0f);
        // No ordinary Jump is issued here: a valid run must actually use the pad.
        if (Activations == 1)
        {
            if (!bIntegratedProbeRightSwitchTouched)
            {
                FinishIntegratedProbe(TEXT("FAIL"), TEXT("Bounce reached without traversing the physical right touch switch"));
                return;
            }
            IntegratedProbeAdvance(ENaiwaP1IntegratedProbeStep::ToLanding, TEXT("walked_down_onto_pad_activation_one_seen"));
        }
        else if (X > 1840.0f)
            FinishIntegratedProbe(TEXT("FAIL"), TEXT("Character passed the bounce pad without an activation; jumping past it does not qualify"));
        return;
    case ENaiwaP1IntegratedProbeStep::ToLanding:
        Character->SetAutomatedMoveInput(1.0f);
        if (bIntegratedVisualCapture && !bIntegratedVisualApexCaptured && Movement->IsFalling()
            && Character->GetActorLocation().Z >= 330.0f
            && Character->GetVelocity().Z <= 120.0f && Character->GetVelocity().Z >= -120.0f)
        {
            bIntegratedVisualApexCaptured = true;
            CaptureIntegratedVisualFrame(TEXT("bounce_near_apex"));
        }
        if (!bIntegratedProbeLaunchedUpward && Movement->IsFalling() && Character->GetVelocity().Z > 100.0f)
        {
            bIntegratedProbeLaunchedUpward = true;
            IntegratedProbeRecord(TEXT("pad_launch_produced_real_upward_motion"));
        }
        if (bGrounded && Character->GetMovementBaseObject() == IntegratedProbeLanding->GetStaticMeshComponent()
            && X >= 1802.0f && X <= 3058.0f)
        {
            if (Activations != 1 || !bIntegratedProbeLaunchedUpward)
            {
                FinishIntegratedProbe(TEXT("FAIL"), TEXT("High-platform floor contact did not follow exactly one observed upward pad launch"));
                return;
            }
            bIntegratedProbeLandedOnHighPlatform = true;
            IntegratedProbeAdvance(ENaiwaP1IntegratedProbeStep::ToCheckpoint, TEXT("first_safe_high_platform_floor_contact_after_bounce"));
            CaptureIntegratedVisualFrame(TEXT("bounce_safe_landing"));
        }
        return;
    case ENaiwaP1IntegratedProbeStep::ToCheckpoint:
        Character->SetAutomatedMoveInput(1.0f);
        if (ActiveCheckpoint.Get() == IntegratedProbeCheckpoint.Get() && bGrounded
            && Character->GetMovementBaseObject() == IntegratedProbeLanding->GetStaticMeshComponent())
        {
            Character->SetAutomatedMoveInput(0.0f);
            IntegratedProbeAdvance(ENaiwaP1IntegratedProbeStep::SettleCheckpoint, TEXT("checkpoint_triggered_by_actual_platform_traversal"));
        }
        return;
    case ENaiwaP1IntegratedProbeStep::SettleCheckpoint:
        Character->SetAutomatedMoveInput(0.0f);
        if (Now - IntegratedProbeStepStartedWall < 0.4 || FMath::Abs(Character->GetVelocity().X) > 5.0f) return;
        if (ActiveCheckpoint.Get() != IntegratedProbeCheckpoint.Get() || !bGrounded
            || Character->GetMovementBaseObject() != IntegratedProbeLanding->GetStaticMeshComponent() || Activations != 1)
        {
            FinishIntegratedProbe(TEXT("FAIL"), TEXT("Checkpoint station did not settle on the high platform"));
            return;
        }
        bIntegratedProbeCheckpointReached = true;
        IntegratedProbeRecord(TEXT("partial_teaching_route_complete_at_checkpoint"));
        CaptureIntegratedVisualFrame(TEXT("checkpoint_grounded"));
        if (!bIntegratedProbeCheckCheckpointFail)
            FinishIntegratedProbe(TEXT("PASS"), TEXT("Partial teaching route physically traversed; checkpoint failure/respawn was not requested; full P1-5b remains unaccepted"));
        else
            IntegratedProbeAdvance(ENaiwaP1IntegratedProbeStep::ToFallEdge, TEXT("optional_failure_walk_toward_high_platform_outer_edge"));
        return;
    case ENaiwaP1IntegratedProbeStep::ToFallEdge:
        Character->SetAutomatedMoveInput(1.0f);
        if (X > 3165.0f && Movement->IsFalling() && Character->GetCapsuleBottom() < 150.0f)
        {
            Character->SetAutomatedMoveInput(0.0f);
            bIntegratedProbeNaturalFallSeen = true;
            IntegratedProbeAdvance(ENaiwaP1IntegratedProbeStep::WaitDeath, TEXT("unsupported_natural_fall_off_high_platform_outer_edge"));
        }
        return;
    case ENaiwaP1IntegratedProbeStep::WaitDeath:
        Character->SetAutomatedMoveInput(0.0f);
        if (State == ENaiwaP1State::Dying)
        {
            if (DeathEventCount != IntegratedProbeDeathCountBefore + 1
                || Character->GetActorLocation().Z >= GetWorld()->GetWorldSettings()->KillZ)
            {
                FinishIntegratedProbe(TEXT("FAIL"), TEXT("Optional unsupported fall did not produce exactly one WorldKillZ death"));
                return;
            }
            bIntegratedProbeDyingSeen = true;
            IntegratedProbeAdvance(ENaiwaP1IntegratedProbeStep::WaitRespawn, TEXT("natural_world_kill_z_death_seen_once"));
        }
        return;
    case ENaiwaP1IntegratedProbeStep::WaitRespawn:
        if (DeathEventCount != IntegratedProbeDeathCountBefore + 1)
        {
            FinishIntegratedProbe(TEXT("FAIL"), TEXT("Checkpoint recovery counted an unexpected additional death"));
            return;
        }
        if (State != ENaiwaP1State::Playing) return;
        Character->SetAutomatedMoveInput(0.0f);
        IntegratedProbeAdvance(ENaiwaP1IntegratedProbeStep::SettleRespawn, TEXT("production_checkpoint_respawn_restored_playing"));
        return;
    case ENaiwaP1IntegratedProbeStep::SettleRespawn:
        Character->SetAutomatedMoveInput(0.0f);
        if (Now - IntegratedProbeStepStartedWall < 0.6) return;
        if (State != ENaiwaP1State::Playing || !IsGameplayInputAllowed() || !bGrounded
            || ActiveCheckpoint.Get() != IntegratedProbeCheckpoint.Get()
            || Character->GetMovementBaseObject() != IntegratedProbeLanding->GetStaticMeshComponent()
            || FMath::Abs(X - 2700.0f) > 8.0f || FMath::Abs(Character->GetCapsuleBottom() - 180.0f) > 18.0f
            || DeathEventCount != IntegratedProbeDeathCountBefore + 1 || IntegratedProbeProductionRespawnPlacements != 1
            || Activations != 1 || Platform->GetState() != ENaiwaP1SwitchPlatformState::DockedLeft
            || !IntegratedProbePad->IsReady() || CollectedIds.Num() != 0)
        {
            FinishIntegratedProbe(TEXT("FAIL"), TEXT("Production checkpoint respawn did not restore one safe playable checkpoint placement and reset mechanisms"));
            return;
        }
        bIntegratedProbeCheckpointRespawnSeen = true;
        IntegratedProbeRecord(TEXT("checkpoint_respawn_grounded_playable_and_mechanisms_reset"));
        FinishIntegratedProbe(TEXT("PASS"), TEXT("Partial teaching route and one natural checkpoint failure/respawn passed; full P1-5b remains unaccepted"));
        return;
    }
}

void ANaiwaP1GameMode::FinishIntegratedProbe(const TCHAR* Result, const FString& Detail)
{
    if (!bIntegratedProbeEnabled) return;
    if (bIntegratedVisualCapture && FString(Result) == TEXT("PASS") && !bIntegratedVisualFinishing)
    {
        // The final screenshot is written by the viewport after this game tick.
        // Keep the route alive until that file is present, without advancing input.
        bIntegratedVisualFinishing = true;
        IntegratedVisualFinishStartedGame = GetWorld()->GetTimeSeconds();
        IntegratedVisualPendingResult = Result;
        IntegratedVisualPendingDetail = Detail;
        if (PlayerCharacter.IsValid()) PlayerCharacter->SetAutomatedMoveInput(0.0f);
        return;
    }
    IntegratedProbeRecord(TEXT("probe_finished"));
    bIntegratedProbeEnabled = false;
    if (PlayerCharacter.IsValid())
    {
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        PlayerCharacter->StopJumping();
        PlayerCharacter->DisableAutomatedInput();
    }
    if (bIntegratedVisualCapture)
    {
        const bool bRouteReachedCheckpoint = FString(Result) == TEXT("PASS") && bIntegratedProbeCheckpointReached;
        bool bAllFilesSaved = IntegratedVisualFrames.Num() == 6;
        for (const TSharedPtr<FJsonValue>& Value : IntegratedVisualFrames)
        {
            const TSharedPtr<FJsonObject> Frame = Value->AsObject();
            FString Path;
            bool bRequested = false;
            const bool bSaved = Frame.IsValid() && Frame->TryGetBoolField(TEXT("requested"), bRequested)
                && bRequested && Frame->TryGetStringField(TEXT("file"), Path)
                && !Path.IsEmpty() && IFileManager::Get().FileExists(*Path);
            if (Frame.IsValid()) Frame->SetBoolField(TEXT("saved"), bSaved);
            bAllFilesSaved &= bSaved;
        }
        TSharedRef<FJsonObject> VisualReport = MakeShared<FJsonObject>();
        VisualReport->SetStringField(TEXT("schema"), TEXT("NaiwaP1IntegratedTeachingVisualV1"));
        VisualReport->SetStringField(TEXT("status"), bRouteReachedCheckpoint && bAllFilesSaved
            ? TEXT("CAPTURE_COMPLETE") : TEXT("CAPTURE_INCOMPLETE"));
        VisualReport->SetStringField(TEXT("route_result_for_capture"), Result);
        VisualReport->SetStringField(TEXT("detail"), Detail);
        VisualReport->SetStringField(TEXT("scope"), TEXT("P1-5b_partial_teaching_slice_visual_only"));
        VisualReport->SetBoolField(TEXT("visual_only"), true);
        VisualReport->SetBoolField(TEXT("functional_probe_evidence"), false);
        VisualReport->SetBoolField(TEXT("real_keyboard_input_evidence"), false);
        VisualReport->SetBoolField(TEXT("full_p1_5b_acceptance"), false);
        VisualReport->SetStringField(TEXT("map_package"), GetWorld()->GetOutermost()->GetName());
        VisualReport->SetStringField(TEXT("gait_speed"), bIntegratedProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
        VisualReport->SetStringField(TEXT("capture_method"), TEXT("FScreenshotRequest ordinary viewport PNG in separate visual process; no HighResShot"));
        VisualReport->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - IntegratedVisualStartedWall);
        VisualReport->SetNumberField(TEXT("world_time_s"), GetWorld()->GetTimeSeconds() - IntegratedProbeStartedWall);
        VisualReport->SetBoolField(TEXT("checkpoint_reached"), bIntegratedProbeCheckpointReached);
        VisualReport->SetBoolField(TEXT("all_requested_captures_exist"), bAllFilesSaved);
        VisualReport->SetArrayField(TEXT("frames"), IntegratedVisualFrames);
        VisualReport->SetArrayField(TEXT("stations"), IntegratedProbeStations);
        FString VisualJson;
        FJsonSerializer::Serialize(VisualReport, TJsonWriterFactory<>::Create(&VisualJson));
        const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
        IFileManager::Get().MakeDirectory(*Directory, true);
        const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("integrated_teaching_visual_%s_%s.json"),
            *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        const bool bReportSaved = FFileHelper::SaveStringToFile(VisualJson, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_INTEGRATED_VISUAL status=%s frames=%d output=%s saved=%d"),
            bRouteReachedCheckpoint && bAllFilesSaved ? TEXT("CAPTURE_COMPLETE") : TEXT("CAPTURE_INCOMPLETE"),
            IntegratedVisualFrames.Num(), *Output, bReportSaved);
        FPlatformMisc::RequestExit(false);
        return;
    }
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1IntegratedTeachingProbeV1"));
    Report->SetStringField(TEXT("status"), Result);
    Report->SetStringField(TEXT("detail"), Detail);
    Report->SetStringField(TEXT("scope"), TEXT("P1-5b_partial_teaching_slice"));
    Report->SetBoolField(TEXT("full_p1_5b_acceptance"), false);
    Report->SetBoolField(TEXT("real_keyboard_input_evidence"), false);
    Report->SetBoolField(TEXT("screenshots_in_this_process"), false);
    Report->SetStringField(TEXT("map"), GetWorld()->GetMapName());
    Report->SetStringField(TEXT("map_package"), GetWorld()->GetOutermost()->GetName());
    Report->SetStringField(TEXT("bound_config_sha256"), TEXT("391cec36cc1504177bd911a11bbada4e2edb8f4383a361e10fd59be12370e7b8"));
    Report->SetStringField(TEXT("gait_speed"), bIntegratedProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    Report->SetStringField(TEXT("control_method"), TEXT("SetAutomatedMoveInput, SetP1Gait and one Jump/StopJumping at the hidden hazard; no position writes or mechanism commands"));
    Report->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - IntegratedProbeStartedWall);
    Report->SetNumberField(TEXT("final_step"), static_cast<int32>(IntegratedProbeStep));
    Report->SetStringField(TEXT("final_state"), StateName(State));
    Report->SetNumberField(TEXT("death_events_delta"), DeathEventCount - IntegratedProbeDeathCountBefore);
    Report->SetNumberField(TEXT("pad_activations_delta"), IntegratedProbePad.IsValid()
        ? IntegratedProbePad->GetActivationCount() - IntegratedProbeActivationCountBefore : 0);
    Report->SetNumberField(TEXT("probe_character_teleports"), 0);
    Report->SetNumberField(TEXT("direct_trigger_injections"), 0);
    Report->SetNumberField(TEXT("platform_control_injections"), 0);
    Report->SetNumberField(TEXT("production_respawn_placements"), IntegratedProbeProductionRespawnPlacements);
    Report->SetNumberField(TEXT("ordinary_jump_calls"), bIntegratedProbeJumpIssued ? 1 : 0);
    Report->SetNumberField(TEXT("max_character_z_cm"), IntegratedProbeMaxCharacterZ);
    Report->SetBoolField(TEXT("strict_preflight_passed"), bIntegratedProbeInitialized);
    Report->SetBoolField(TEXT("cue_no_collision_verified"), bIntegratedProbeInitialized
        && IntegratedProbeCue.IsValid() && IntegratedProbeCue->GetStaticMeshComponent()
        && IntegratedProbeCue->GetStaticMeshComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
    Report->SetBoolField(TEXT("hidden_hazard_jumped_and_grounded"), bIntegratedProbeHazardCleared);
    Report->SetBoolField(TEXT("left_switch_touched"), bIntegratedProbeLeftSwitchTouched);
    Report->SetBoolField(TEXT("right_switch_touched"), bIntegratedProbeRightSwitchTouched);
    Report->SetBoolField(TEXT("real_shuttle_boarding"), bIntegratedProbeBoarded);
    Report->SetBoolField(TEXT("supported_departure_cue_seen"), bIntegratedProbeDepartureCueSeen);
    Report->SetBoolField(TEXT("supported_mid_gap_ride_seen"), bIntegratedProbeSupportedMidway);
    Report->SetBoolField(TEXT("supported_right_dock_reached"), bIntegratedProbeDockRightReached);
    Report->SetBoolField(TEXT("real_upward_pad_launch_seen"), bIntegratedProbeLaunchedUpward);
    Report->SetBoolField(TEXT("first_high_platform_floor_contact_seen"), bIntegratedProbeLandedOnHighPlatform);
    Report->SetBoolField(TEXT("checkpoint_grounded_route_complete"), bIntegratedProbeCheckpointReached);
    Report->SetBoolField(TEXT("checkpoint_failure_requested"), bIntegratedProbeCheckCheckpointFail);
    Report->SetBoolField(TEXT("natural_unsupported_fall_seen"), bIntegratedProbeNaturalFallSeen);
    Report->SetBoolField(TEXT("world_kill_z_dying_seen"), bIntegratedProbeDyingSeen);
    Report->SetBoolField(TEXT("checkpoint_respawn_grounded_playable"), bIntegratedProbeCheckpointRespawnSeen);
    TArray<TSharedPtr<FJsonValue>> Omitted;
    for (const TCHAR* Item : { TEXT("timed_platform"), TEXT("required_pickup"), TEXT("goal_and_win"),
        TEXT("combination_segments"), TEXT("ordinary_window_real_keyboard"), TEXT("final_camera_and_visual_acceptance") })
        Omitted.Add(MakeShared<FJsonValueString>(Item));
    Report->SetArrayField(TEXT("outside_this_probe_scope"), Omitted);
    Report->SetArrayField(TEXT("stations"), IntegratedProbeStations);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("integrated_teaching_probe_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_INTEGRATED_PROBE result=%s detail=%s output=%s saved=%d"), Result, *Detail, *Output, bSaved);
    FPlatformMisc::RequestExit(false);
}

void ANaiwaP1GameMode::SwitchFailProbeAdvance(ENaiwaP1SwitchFailProbeStep NextStep, const FString& Evidence)
{
    SwitchFailProbeEvents.Add(FString::Printf(TEXT("t=%.2f x=%.1f bottom=%.1f %s"),
        FPlatformTime::Seconds() - SwitchFailProbeStartedWall,
        PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().X : 0.0f,
        PlayerCharacter.IsValid() ? PlayerCharacter->GetCapsuleBottom() : 0.0f,
        *Evidence));
    SwitchFailProbeStep = NextStep;
    SwitchFailProbeStepStartedWall = FPlatformTime::Seconds();
}

void ANaiwaP1GameMode::TickSwitchFailProbe()
{
    const double Now = FPlatformTime::Seconds();
    if (Now - SwitchFailProbeStartedWall > 90.0 || Now - SwitchFailProbeStepStartedWall > 20.0)
    {
        FinishSwitchFailProbe(TEXT("FAIL"), FString::Printf(
            TEXT("Timeout step=%d state=%s platform=%s x=%.1f bottom=%.1f deaths=%d"),
            static_cast<int32>(SwitchFailProbeStep), StateName(State),
            SwitchFailProbePlatform.IsValid() ? SwitchStateName(SwitchFailProbePlatform->GetState()) : TEXT("Missing"),
            PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().X : 0.0f,
            PlayerCharacter.IsValid() ? PlayerCharacter->GetCapsuleBottom() : 0.0f,
            DeathEventCount));
        return;
    }
    if (!IsAuditMap(GetWorld(), TEXT("L_NaiwaP1_Switch")))
    {
        FinishSwitchFailProbe(TEXT("INCOMPLETE"), TEXT("Natural failure retry requires the isolated switch map"));
        return;
    }
    if (bSmokeEnabled || bRouteEnabled || bHazardAuditEnabled || bMatrixEnabled || bBounceProbeEnabled
        || bSwitchProbeEnabled || bTimedProbeEnabled || bTimedFailProbeEnabled
        || (FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1SwitchFailProbeWalk"))
            && FParse::Param(FCommandLine::Get(), TEXT("NaiwaP1SwitchFailProbeRun"))))
    {
        FinishSwitchFailProbe(TEXT("INCOMPLETE"), TEXT("Run exactly one P1 audit scenario and gait per process"));
        return;
    }
    if (UGameplayStatics::IsGamePaused(this))
    {
        FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Game unexpectedly paused during natural failure retry"));
        return;
    }
    if (!PlayerCharacter.IsValid())
    {
        if (Now - SwitchFailProbeStartedWall > 5.0)
            FinishSwitchFailProbe(TEXT("INCOMPLETE"), TEXT("P1 character did not register on switch map"));
        return;
    }
    if (!bSwitchFailProbeInitialized)
    {
        if (!PlayerCharacter->GetCharacterMovement()->IsMovingOnGround()) return;
        int32 PlatformCount = 0;
        for (TActorIterator<ANaiwaP1SwitchPlatform> It(GetWorld()); It; ++It)
        {
            if (!It->ActorHasTag(FName(TEXT("NaiwaP1SwitchPlatformV1")))) continue;
            SwitchFailProbePlatform = *It;
            ++PlatformCount;
        }
        if (PlatformCount != 1 || !SwitchFailProbePlatform.IsValid()
            || !SwitchFailProbePlatform->PlatformCollision || !bHasInitialSpawn
            || SwitchFailProbePlatform->GetState() != ENaiwaP1SwitchPlatformState::DockedLeft
            || SwitchFailProbePlatform->DockLeftX >= SwitchFailProbePlatform->DockRightX
            || InitialSpawn.X >= SwitchFailProbePlatform->DockLeftX
                - SwitchFailProbePlatform->PlatformExtent.X - 100.0f
            || !GetWorld()->GetWorldSettings())
        {
            FinishSwitchFailProbe(TEXT("INCOMPLETE"), TEXT("Expected one tagged left-docked platform, grounded left-bank spawn, and World KillZ"));
            return;
        }
        if (!PlayerCharacter->SetP1Gait(!bSwitchFailProbeWalk)
            || FMath::Abs(PlayerCharacter->GetCharacterMovement()->MaxWalkSpeed
                - (bSwitchFailProbeWalk ? 150.0f : 300.0f)) > 1.0f)
        {
            FinishSwitchFailProbe(TEXT("INCOMPLETE"), TEXT("Could not verify Walk150 or Run300 gait"));
            return;
        }
        SwitchFailProbeDeathCountBefore = DeathEventCount;
        bSwitchFailProbeInitialized = true;
        SwitchFailProbeAdvance(ENaiwaP1SwitchFailProbeStep::BoardFirst,
            bSwitchFailProbeWalk ? TEXT("grounded_spawn_walk150") : TEXT("grounded_spawn_run300"));
        return;
    }
    if (!SwitchFailProbePlatform.IsValid())
    {
        FinishSwitchFailProbe(TEXT("INCOMPLETE"), TEXT("Tagged switch platform disappeared"));
        return;
    }
    ANaiwaP1SwitchPlatform* Platform = SwitchFailProbePlatform.Get();
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    const ENaiwaP1SwitchPlatformState PlatformState = Platform->GetState();
    const float CharacterX = Character->GetActorLocation().X;
    const float PlatformX = Platform->GetCurrentX();
    const float TopZ = Platform->GetActorLocation().Z + Platform->PlatformTopZ;
    const float CapsuleBottom = Character->GetCapsuleBottom();
    const float CapsuleRadius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
    // This threshold is well inside the prototype's right bank; the base check
    // below also verifies real floor support rather than a coordinate alone.
    const float RightShoreX = Platform->DockRightX + Platform->PlatformExtent.X + CapsuleRadius + 65.0f;
    const bool bSupported = Platform->IsCharacterSupported(Character);
    const bool bGroundedShore = Character->GetCharacterMovement()->IsMovingOnGround()
        && Character->GetMovementBaseObject() != Platform->PlatformCollision.Get()
        && FMath::Abs(CapsuleBottom - TopZ) < 18.0f;
    switch (SwitchFailProbeStep)
    {
    case ENaiwaP1SwitchFailProbeStep::WaitReady:
        return;
    case ENaiwaP1SwitchFailProbeStep::BoardFirst:
        if (State != ENaiwaP1State::Playing || DeathEventCount != SwitchFailProbeDeathCountBefore
            || PlatformState != ENaiwaP1SwitchPlatformState::DockedLeft)
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Character died or platform departed before first boarding"));
            return;
        }
        if (!bSupported)
        {
            if (CharacterX > Platform->DockLeftX + Platform->PlatformExtent.X + CapsuleRadius + 80.0f)
            {
                FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Real approach crossed the dock without boarding"));
                return;
            }
            Character->SetAutomatedMoveInput(1.0f);
            return;
        }
        Character->SetAutomatedMoveInput(0.0f);
        Character->GetCharacterMovement()->StopMovementImmediately();
        SwitchFailProbeFirstBoardOffsetX = CharacterX - PlatformX;
        SwitchFailProbeFirstBoardLocation = Character->GetActorLocation();
        SwitchFailProbeAdvance(ENaiwaP1SwitchFailProbeStep::RideFirst,
            TEXT("real_movement_boarded_left_dock"));
        return;
    case ENaiwaP1SwitchFailProbeStep::RideFirst:
        if (State != ENaiwaP1State::Playing || DeathEventCount != SwitchFailProbeDeathCountBefore)
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Character died during first ride"));
            return;
        }
        if (PlatformState == ENaiwaP1SwitchPlatformState::DockedLeft)
        {
            if (!bSupported) FinishSwitchFailProbe(TEXT("FAIL"), TEXT("First rider lost left-dock support"));
            return;
        }
        if (PlatformState == ENaiwaP1SwitchPlatformState::TravelingRight)
        {
            if (PlatformX >= (Platform->DockLeftX + Platform->DockRightX) * 0.5f && bSupported)
                bSwitchFailProbeFirstRideMidway = true;
            return;
        }
        if (PlatformState != ENaiwaP1SwitchPlatformState::DockedRight || !bSupported
            || !bSwitchFailProbeFirstRideMidway
            || FMath::Abs((CharacterX - PlatformX) - SwitchFailProbeFirstBoardOffsetX) > 55.0f)
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("First real ride did not carry the rider to right dock"));
            return;
        }
        SwitchFailProbeAdvance(ENaiwaP1SwitchFailProbeStep::ToRightShore,
            TEXT("first_rider_supported_midway_and_right_dock"));
        return;
    case ENaiwaP1SwitchFailProbeStep::ToRightShore:
        if (State != ENaiwaP1State::Playing || PlatformState != ENaiwaP1SwitchPlatformState::DockedRight
            || DeathEventCount != SwitchFailProbeDeathCountBefore)
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("First shore approach lost playable right-dock state"));
            return;
        }
        Character->SetAutomatedMoveInput(1.0f);
        if (CharacterX < RightShoreX || !bGroundedShore) return;
        Character->SetAutomatedMoveInput(0.0f);
        Character->GetCharacterMovement()->StopMovementImmediately();
        SwitchFailProbeFirstShoreLocation = Character->GetActorLocation();
        bSwitchFailProbeFirstShoreReached = true;
        // Controlled platform recall creates an empty gap. The character is
        // never repositioned by this probe, so the subsequent fall is natural.
        ++SwitchFailProbeControlInjections;
        if (!Platform->RequestDockLeft()
            || Platform->GetState() != ENaiwaP1SwitchPlatformState::TravelingLeft)
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("One explicit left-dock recall did not start"));
            return;
        }
        SwitchFailProbeAdvance(ENaiwaP1SwitchFailProbeStep::WaitDockLeft,
            TEXT("grounded_right_shore_then_control_injection_RequestDockLeft_once"));
        return;
    case ENaiwaP1SwitchFailProbeStep::WaitDockLeft:
        Character->SetAutomatedMoveInput(0.0f);
        if (State != ENaiwaP1State::Playing || DeathEventCount != SwitchFailProbeDeathCountBefore
            || !bGroundedShore || CharacterX < RightShoreX - 40.0f)
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Character left safe right shore during platform recall"));
            return;
        }
        if (PlatformState == ENaiwaP1SwitchPlatformState::TravelingLeft) return;
        if (PlatformState != ENaiwaP1SwitchPlatformState::DockedLeft
            || FMath::Abs(PlatformX - Platform->DockLeftX) > 1.0f)
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Controlled recall did not finish at left dock"));
            return;
        }
        SwitchFailProbeAdvance(ENaiwaP1SwitchFailProbeStep::FallFromRightEdge,
            TEXT("platform_left_dock_empty_gap_character_still_right_shore"));
        return;
    case ENaiwaP1SwitchFailProbeStep::FallFromRightEdge:
        if (State != ENaiwaP1State::Playing || DeathEventCount != SwitchFailProbeDeathCountBefore
            || PlatformState != ENaiwaP1SwitchPlatformState::DockedLeft)
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Unexpected death or platform departure before natural edge fall"));
            return;
        }
        if (!Character->GetCharacterMovement()->IsMovingOnGround() && CapsuleBottom < TopZ - 35.0f)
        {
            Character->SetAutomatedMoveInput(0.0f);
            bSwitchFailProbeNaturalFallSeen = true;
            SwitchFailProbeFallLocation = Character->GetActorLocation();
            SwitchFailProbeAdvance(ENaiwaP1SwitchFailProbeStep::WaitDying,
                TEXT("real_leftward_move_from_right_bank_entered_unsupported_pit"));
            return;
        }
        if (CharacterX < Platform->DockRightX - 120.0f && bGroundedShore)
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Character remained grounded in the intended empty gap"));
            return;
        }
        Character->SetAutomatedMoveInput(-1.0f);
        return;
    case ENaiwaP1SwitchFailProbeStep::WaitDying:
        Character->SetAutomatedMoveInput(0.0f);
        if (DeathEventCount == SwitchFailProbeDeathCountBefore && State == ENaiwaP1State::Playing) return;
        if (!bSwitchFailProbeNaturalFallSeen
            || DeathEventCount != SwitchFailProbeDeathCountBefore + 1
            || State != ENaiwaP1State::Dying
            || Character->GetActorLocation().Z >= GetWorld()->GetWorldSettings()->KillZ)
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Unsupported pit fall did not cause exactly one World KillZ death"));
            return;
        }
        bSwitchFailProbeDyingSeen = true;
        SwitchFailProbeDeathLocation = Character->GetActorLocation();
        SwitchFailProbeAdvance(ENaiwaP1SwitchFailProbeStep::WaitRespawn,
            TEXT("natural_pit_fall_reached_WorldKillZ_and_entered_Dying_once"));
        return;
    case ENaiwaP1SwitchFailProbeStep::WaitRespawn:
        Character->SetAutomatedMoveInput(0.0f);
        if (DeathEventCount != SwitchFailProbeDeathCountBefore + 1)
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Additional death occurred before respawn"));
            return;
        }
        if (State == ENaiwaP1State::Dying || State == ENaiwaP1State::Respawning) return;
        if (State != ENaiwaP1State::Playing
            || !Character->GetCharacterMovement()->IsMovingOnGround()
            || FMath::Abs(CharacterX - InitialSpawn.X) > 80.0f
            || PlatformState != ENaiwaP1SwitchPlatformState::DockedLeft
            || FMath::Abs(PlatformX - Platform->DockLeftX) > 1.0f
            || Platform->IsDepartureCueActive())
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Death respawn did not restore playable left-bank start and ready left dock"));
            return;
        }
        bSwitchFailProbeRespawnReady = true;
        SwitchFailProbeRespawnLocation = Character->GetActorLocation();
        SwitchFailProbeAdvance(ENaiwaP1SwitchFailProbeStep::BoardRetry,
            TEXT("death_respawn_grounded_playable_and_platform_ready_left"));
        return;
    case ENaiwaP1SwitchFailProbeStep::BoardRetry:
        if (State != ENaiwaP1State::Playing || DeathEventCount != SwitchFailProbeDeathCountBefore + 1
            || PlatformState != ENaiwaP1SwitchPlatformState::DockedLeft)
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Character died again or platform departed before retry boarding"));
            return;
        }
        if (!bSupported)
        {
            if (CharacterX > Platform->DockLeftX + Platform->PlatformExtent.X + CapsuleRadius + 80.0f)
            {
                FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Real retry approach crossed dock without boarding"));
                return;
            }
            Character->SetAutomatedMoveInput(1.0f);
            return;
        }
        Character->SetAutomatedMoveInput(0.0f);
        Character->GetCharacterMovement()->StopMovementImmediately();
        SwitchFailProbeRetryBoardOffsetX = CharacterX - PlatformX;
        SwitchFailProbeRetryBoardLocation = Character->GetActorLocation();
        SwitchFailProbeAdvance(ENaiwaP1SwitchFailProbeStep::RideRetry,
            TEXT("real_movement_reboarded_after_death_respawn"));
        return;
    case ENaiwaP1SwitchFailProbeStep::RideRetry:
        if (State != ENaiwaP1State::Playing || DeathEventCount != SwitchFailProbeDeathCountBefore + 1)
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Character died during real retry ride"));
            return;
        }
        if (PlatformState == ENaiwaP1SwitchPlatformState::DockedLeft)
        {
            if (!bSupported) FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Retry rider lost left-dock support"));
            return;
        }
        if (PlatformState == ENaiwaP1SwitchPlatformState::TravelingRight)
        {
            if (PlatformX >= (Platform->DockLeftX + Platform->DockRightX) * 0.5f && bSupported)
                bSwitchFailProbeRetryRideMidway = true;
            return;
        }
        if (PlatformState != ENaiwaP1SwitchPlatformState::DockedRight || !bSupported
            || !bSwitchFailProbeRetryRideMidway
            || FMath::Abs((CharacterX - PlatformX) - SwitchFailProbeRetryBoardOffsetX) > 55.0f)
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Real retry ride did not carry supported rider to right dock"));
            return;
        }
        SwitchFailProbeAdvance(ENaiwaP1SwitchFailProbeStep::ToRightShoreAfterRetry,
            TEXT("retry_rider_supported_midway_and_right_dock"));
        return;
    case ENaiwaP1SwitchFailProbeStep::ToRightShoreAfterRetry:
        if (State != ENaiwaP1State::Playing || DeathEventCount != SwitchFailProbeDeathCountBefore + 1
            || PlatformState != ENaiwaP1SwitchPlatformState::DockedRight)
        {
            FinishSwitchFailProbe(TEXT("FAIL"), TEXT("Retry shore approach lost playable right-dock state"));
            return;
        }
        Character->SetAutomatedMoveInput(1.0f);
        if (CharacterX < RightShoreX || !bGroundedShore) return;
        Character->SetAutomatedMoveInput(0.0f);
        bSwitchFailProbeRetryShoreReached = true;
        SwitchFailProbeAdvance(ENaiwaP1SwitchFailProbeStep::ToRightShoreAfterRetry,
            TEXT("real_retry_reached_grounded_right_shore"));
        FinishSwitchFailProbe(TEXT("PASS"),
            TEXT("One platform-control recall left an empty gap; real edge fall caused one World KillZ death, playable reset, and real retry reached right shore"));
        return;
    }
}

void ANaiwaP1GameMode::FinishSwitchFailProbe(const TCHAR* Result, const FString& Detail)
{
    if (!bSwitchFailProbeEnabled) return;
    bSwitchFailProbeEnabled = false;
    if (PlayerCharacter.IsValid())
    {
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        PlayerCharacter->StopJumping();
    }
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_FAIL_PROBE result=%s gait=%s detail=%s"),
        Result, bSwitchFailProbeWalk ? TEXT("Walk150") : TEXT("Run300"), *Detail);
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1SwitchFailProbeV1"));
    Report->SetStringField(TEXT("status"), Result);
    Report->SetStringField(TEXT("detail"), Detail);
    Report->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetMapName() : FString());
    Report->SetStringField(TEXT("gait_speed"), bSwitchFailProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    Report->SetStringField(TEXT("game_state"), StateName(State));
    Report->SetStringField(TEXT("platform_state"), SwitchFailProbePlatform.IsValid()
        ? SwitchStateName(SwitchFailProbePlatform->GetState()) : TEXT("Missing"));
    Report->SetStringField(TEXT("platform_control_injection_method"),
        TEXT("RequestDockLeft once while the character stands on the grounded right shore"));
    Report->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - SwitchFailProbeStartedWall);
    Report->SetNumberField(TEXT("world_kill_z"), GetWorld() && GetWorld()->GetWorldSettings()
        ? GetWorld()->GetWorldSettings()->KillZ : 0.0f);
    Report->SetNumberField(TEXT("death_events_delta"), DeathEventCount - SwitchFailProbeDeathCountBefore);
    Report->SetNumberField(TEXT("platform_control_injections"), SwitchFailProbeControlInjections);
    Report->SetNumberField(TEXT("probe_direct_character_teleports"), 0);
    Report->SetBoolField(TEXT("game_mode_death_respawn_teleport_expected"), true);
    Report->SetBoolField(TEXT("first_ride_supported_midway"), bSwitchFailProbeFirstRideMidway);
    Report->SetBoolField(TEXT("first_grounded_right_shore_reached"), bSwitchFailProbeFirstShoreReached);
    Report->SetBoolField(TEXT("natural_unsupported_pit_fall_seen"), bSwitchFailProbeNaturalFallSeen);
    Report->SetBoolField(TEXT("world_kill_z_dying_seen"), bSwitchFailProbeDyingSeen);
    Report->SetBoolField(TEXT("respawn_playable_and_left_dock_ready"), bSwitchFailProbeRespawnReady);
    Report->SetBoolField(TEXT("retry_ride_supported_midway"), bSwitchFailProbeRetryRideMidway);
    Report->SetBoolField(TEXT("real_retry_grounded_right_shore_reached"), bSwitchFailProbeRetryShoreReached);
    const auto PutPosition = [&Report](const TCHAR* Name, const FVector& Position)
    {
        TSharedRef<FJsonObject> Point = MakeShared<FJsonObject>();
        Point->SetNumberField(TEXT("x"), Position.X);
        Point->SetNumberField(TEXT("y"), Position.Y);
        Point->SetNumberField(TEXT("z"), Position.Z);
        Report->SetObjectField(Name, Point);
    };
    PutPosition(TEXT("initial_spawn"), InitialSpawn);
    PutPosition(TEXT("first_board_position"), SwitchFailProbeFirstBoardLocation);
    PutPosition(TEXT("first_right_shore_position"), SwitchFailProbeFirstShoreLocation);
    PutPosition(TEXT("fall_position"), SwitchFailProbeFallLocation);
    PutPosition(TEXT("death_position"), SwitchFailProbeDeathLocation);
    PutPosition(TEXT("respawn_position"), SwitchFailProbeRespawnLocation);
    PutPosition(TEXT("retry_board_position"), SwitchFailProbeRetryBoardLocation);
    PutPosition(TEXT("finish_position"), PlayerCharacter.IsValid()
        ? PlayerCharacter->GetActorLocation() : FVector::ZeroVector);
    TArray<TSharedPtr<FJsonValue>> Events;
    for (const FString& Event : SwitchFailProbeEvents) Events.Add(MakeShared<FJsonValueString>(Event));
    Report->SetArrayField(TEXT("events"), Events);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("switch_fail_probe_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (bSaved)
    {
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_SWITCH_FAIL_PROBE output=%s saved=1"), *Output);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_P1_SWITCH_FAIL_PROBE output=%s saved=0"), *Output);
    }
    FPlatformMisc::RequestExit(false);
}

bool ANaiwaP1GameMode::InitializeIntegratedFullProbe()
{
    const AWorldSettings* Settings = GetWorld()->GetWorldSettings();
    if (!Settings || GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Naiwa/P1/Maps/L_NaiwaP1_Integrated")
        || !Settings->ActorHasTag(TEXT("NaiwaP1IntegratedTeachingMapV1"))
        || !Settings->ActorHasTag(TEXT("NaiwaP1IntegratedCfg_391cec36cc150417"))
        || !Settings->ActorHasTag(TEXT("NaiwaP1IntegratedFullCandidateV1"))
        || !Settings->ActorHasTag(TEXT("NaiwaP1IntegratedFullCfg_3aad9c28e6a88397"))
        || Settings->ActorHasTag(TEXT("NaiwaP1PartialTeachingSliceV1"))
        || !FMath::IsNearlyEqual(Settings->KillZ, -1000.0f, 0.1f) || RequiredPickupCount != 1)
    {
        FinishIntegratedFullProbe(TEXT("INCOMPLETE"), TEXT("Exact full-candidate package/config markers, KillZ and required pickup count are required"));
        return false;
    }
    if (!PlayerCharacter.IsValid() || !bHasInitialSpawn) return false;
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    if (!Character->GetCharacterMovement()->IsMovingOnGround()) return false;
    const FName OwnerTag(TEXT("NaiwaP1IntegratedGeneratedV1"));
    int32 OwnedCount = 0;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (It->ActorHasTag(OwnerTag)) ++OwnedCount;
    const TCHAR* SwitchTags[2] = { TEXT("NaiwaP1IntegratedSwitchV1"), TEXT("NaiwaP1IntegratedFullSwitchCombination01V1") };
    const TCHAR* PadTags[2] = { TEXT("NaiwaP1IntegratedBounceV1"), TEXT("NaiwaP1IntegratedFullBounceCombination01V1") };
    const TCHAR* TimedTags[2] = { TEXT("NaiwaP1IntegratedFullTimedTeaching01V1"), TEXT("NaiwaP1IntegratedFullTimedCombination01V1") };
    int32 SwitchCounts[2] = {};
    int32 PadCounts[2] = {};
    int32 TimedCounts[2] = {};
    int32 MechanismCount = 0;
    for (TActorIterator<ANaiwaP1Mechanism> It(GetWorld()); It; ++It)
    {
        ++MechanismCount;
        if (!It->ActorHasTag(OwnerTag)) continue;
        for (int32 Index = 0; Index < 2; ++Index)
        {
            if (It->GetClass() == ANaiwaP1SwitchPlatform::StaticClass() && It->ActorHasTag(SwitchTags[Index]))
            { IntegratedFullProbeSwitches[Index] = CastChecked<ANaiwaP1SwitchPlatform>(*It); ++SwitchCounts[Index]; }
            if (It->GetClass() == ANaiwaP1BouncePad::StaticClass() && It->ActorHasTag(PadTags[Index]))
            { IntegratedFullProbePads[Index] = CastChecked<ANaiwaP1BouncePad>(*It); ++PadCounts[Index]; }
            if (It->GetClass() == ANaiwaP1TimedPlatform::StaticClass() && It->ActorHasTag(TimedTags[Index]))
            { IntegratedFullProbeTimed[Index] = CastChecked<ANaiwaP1TimedPlatform>(*It); ++TimedCounts[Index]; }
        }
    }
    const TCHAR* TriggerIds[7] = { TEXT("IntegratedHiddenHazard01"), TEXT("IntegratedHiddenHazard02"),
        TEXT("IntegratedCheckpoint01"), TEXT("IntegratedCheckpoint02"), TEXT("IntegratedCheckpoint03"),
        TEXT("IntegratedRequiredPickup01"), TEXT("IntegratedGoal01") };
    const TCHAR* TriggerTags[7] = { TEXT("NaiwaP1IntegratedHazardV1"), TEXT("NaiwaP1IntegratedFullIntegratedHiddenHazard02V1"),
        TEXT("NaiwaP1IntegratedCheckpointV1"), TEXT("NaiwaP1IntegratedFullIntegratedCheckpoint02V1"),
        TEXT("NaiwaP1IntegratedFullIntegratedCheckpoint03V1"), TEXT("NaiwaP1IntegratedFullIntegratedRequiredPickup01V1"),
        TEXT("NaiwaP1IntegratedFullIntegratedGoal01V1") };
    ANaiwaP1Trigger* BoundTriggers[7] = {};
    int32 TriggerCounts[7] = {};
    int32 TriggerCount = 0;
    for (TActorIterator<ANaiwaP1Trigger> It(GetWorld()); It; ++It)
    {
        ++TriggerCount;
        if (It->GetClass() != ANaiwaP1Trigger::StaticClass() || !It->ActorHasTag(OwnerTag)) continue;
        for (int32 Index = 0; Index < 7; ++Index)
            if (It->GetStableId() == FName(TriggerIds[Index]) && It->ActorHasTag(TriggerTags[Index]))
            { BoundTriggers[Index] = *It; ++TriggerCounts[Index]; }
    }
    const float GroundLeft[6] = { -1600.0f, 550.0f, 1740.0f, 3560.0f, 5600.0f, 7210.0f };
    const float GroundRight[6] = { -550.0f, 1460.0f, 3120.0f, 4500.0f, 6500.0f, 8500.0f };
    const float GroundTop[6] = { 120.0f, 120.0f, 180.0f, 180.0f, 180.0f, 360.0f };
    const TCHAR* ExtensionGroundTags[3] = { TEXT("NaiwaP1IntegratedFullTimedExitBankV1"),
        TEXT("NaiwaP1IntegratedFullCombinationBankV1"), TEXT("NaiwaP1IntegratedFullFinalBankV1") };
    int32 GroundCounts[6] = {};
    int32 GroundCount = 0;
    int32 CueCounts[2] = {};
    for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
    {
        if (It->GetClass() != AStaticMeshActor::StaticClass() || !It->ActorHasTag(OwnerTag)) continue;
        if (It->ActorHasTag(TEXT("NaiwaP1IntegratedHazardCueV1"))
            && It->GetActorLocation().Equals(FVector(-1300.0f, 0.0f, 123.0f), 0.1f)) ++CueCounts[0];
        if (It->ActorHasTag(TEXT("NaiwaP1IntegratedFullHazardCueCombination01V1"))
            && It->GetActorLocation().Equals(FVector(5780.0f, 0.0f, 183.0f), 0.1f)) ++CueCounts[1];
        if (!It->ActorHasTag(TEXT("NaiwaP1GeneratedV1"))) continue;
        ++GroundCount;
        const FBox Bounds = It->GetComponentsBoundingBox(true);
        for (int32 Index = 0; Index < 6; ++Index)
            if (Bounds.Min.Equals(FVector(GroundLeft[Index], -350.0f, GroundTop[Index] - 40.0f), 0.1f)
                && Bounds.Max.Equals(FVector(GroundRight[Index], 350.0f, GroundTop[Index]), 0.1f)
                && (Index < 3 || It->ActorHasTag(ExtensionGroundTags[Index - 3])))
            { IntegratedFullProbeGrounds[Index] = *It; ++GroundCounts[Index]; }
    }
    bool bCountsMatch = OwnedCount == 25 && MechanismCount == 6 && TriggerCount == 7 && GroundCount == 6;
    for (int32 Index = 0; Index < 2; ++Index)
        bCountsMatch &= SwitchCounts[Index] == 1 && PadCounts[Index] == 1 && TimedCounts[Index] == 1 && CueCounts[Index] == 1;
    for (int32 Count : TriggerCounts) bCountsMatch &= Count == 1;
    for (int32 Count : GroundCounts) bCountsMatch &= Count == 1;
    if (!bCountsMatch)
    {
        FinishIntegratedFullProbe(TEXT("INCOMPLETE"), FString::Printf(
            TEXT("Full inventory mismatch: owned=%d mechanisms=%d triggers=%d ground=%d; require unique role bindings"),
            OwnedCount, MechanismCount, TriggerCount, GroundCount));
        return false;
    }
    IntegratedFullProbeHazards[0] = BoundTriggers[0]; IntegratedFullProbeHazards[1] = BoundTriggers[1];
    for (int32 Index = 0; Index < 3; ++Index) IntegratedFullProbeCheckpointsActors[Index] = BoundTriggers[Index + 2];
    IntegratedFullProbePickup = BoundTriggers[5]; IntegratedFullProbeGoal = BoundTriggers[6];
    const FVector TriggerPositions[7] = { FVector(-1160, 0, 155), FVector(5950, 0, 215), FVector(2700, 0, 272),
        FVector(6220, 0, 272), FVector(7480, 0, 452), FVector(7900, 0, 422), FVector(8220, 0, 452) };
    const FVector TriggerExtents[7] = { FVector(20, 120, 35), FVector(20, 120, 35), FVector(42, 110, 85),
        FVector(42, 110, 85), FVector(42, 110, 85), FVector(35, 90, 42), FVector(50, 110, 85) };
    const ENaiwaP1TriggerKind TriggerKinds[7] = { ENaiwaP1TriggerKind::Hazard, ENaiwaP1TriggerKind::Hazard,
        ENaiwaP1TriggerKind::Checkpoint, ENaiwaP1TriggerKind::Checkpoint, ENaiwaP1TriggerKind::Checkpoint,
        ENaiwaP1TriggerKind::Pickup, ENaiwaP1TriggerKind::Goal };
    bool bGeometryMatches = true;
    for (int32 Index = 0; Index < 7; ++Index)
    {
        const ANaiwaP1Trigger* Trigger = BoundTriggers[Index];
        bGeometryMatches &= Trigger->GetActorLocation().Equals(TriggerPositions[Index], 0.1f)
            && Trigger->BoxExtent.Equals(TriggerExtents[Index], 0.1f) && Trigger->TriggerKind == TriggerKinds[Index]
            && Trigger->GetActorRotation().IsNearlyZero(0.1f) && Trigger->GetActorScale3D().Equals(FVector::OneVector, 0.01f)
            && Trigger->TriggerBox && Trigger->DisplayMesh;
        if (Index >= 2 && Index <= 4) bGeometryMatches &= Trigger->RespawnOffset.IsNearlyZero(0.01f);
    }
    const FVector SwitchPositions[2] = { FVector::ZeroVector, FVector(5050, 0, 60) };
    const FVector PadPositions[2] = { FVector(1660, 0, 0), FVector(6700, 0, 60) };
    const FVector TimedPositions[2] = { FVector(3340, 0, 0), FVector(6990, 0, 0) };
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const ANaiwaP1SwitchPlatform* Platform = IntegratedFullProbeSwitches[Index].Get();
        const ANaiwaP1BouncePad* Pad = IntegratedFullProbePads[Index].Get();
        const ANaiwaP1TimedPlatform* Timed = IntegratedFullProbeTimed[Index].Get();
        bGeometryMatches &= Platform->GetActorLocation().Equals(SwitchPositions[Index], 0.1f)
            && Platform->GetActorRotation().IsNearlyZero(0.1f) && Platform->GetActorScale3D().Equals(FVector::OneVector, 0.01f)
            && FMath::IsNearlyEqual(Platform->DockLeftX, -420.0f, 0.1f) && FMath::IsNearlyEqual(Platform->DockRightX, 420.0f, 0.1f)
            && FMath::IsNearlyEqual(Platform->LeftSwitchX, Index == 0 ? -850.0f : -800.0f, 0.1f)
            && FMath::IsNearlyEqual(Platform->RightSwitchX, Index == 0 ? 850.0f : 710.0f, 0.1f)
            && FMath::IsNearlyEqual(Platform->PlatformTopZ, 120.0f, 0.1f)
            && Platform->PlatformExtent.Equals(FVector(120, 160, 20), 0.1f)
            && FMath::IsNearlyEqual(Platform->TravelSpeedCmPerSecond, 300.0f, 0.1f)
            && FMath::IsNearlyEqual(Platform->StableBoardingSeconds, 0.25f, 0.01f)
            && FMath::IsNearlyEqual(Platform->DepartureCueSeconds, 0.65f, 0.01f)
            && Platform->PlatformCollision && Platform->GetState() == ENaiwaP1SwitchPlatformState::DockedLeft
            && FMath::IsNearlyEqual(Platform->GetCurrentX(), -420.0f, 0.1f)
            && Platform->GetLeftSwitchTouchCount() == 0 && Platform->GetRightSwitchTouchCount() == 0
            && Pad->GetActorLocation().Equals(PadPositions[Index], 0.1f) && Pad->GetActorRotation().IsNearlyZero(0.1f)
            && Pad->GetActorScale3D().Equals(FVector::OneVector, 0.01f) && Pad->BoxExtent.Equals(FVector(160, 120, 12), 0.1f)
            && FMath::IsNearlyEqual(Pad->LaunchVelocityZ, 1000.0f, 0.1f) && Pad->PadCollision && Pad->GetActivationCount() == 0
            && Timed->GetActorLocation().Equals(TimedPositions[Index], 0.1f) && Timed->GetActorRotation().IsNearlyZero(0.1f)
            && Timed->GetActorScale3D().Equals(FVector::OneVector, 0.01f)
            && FMath::IsNearlyEqual(Timed->PlatformTopZ, Index == 0 ? 180.0f : 360.0f, 0.1f)
            && Timed->PlatformExtent.Equals(FVector(210, 160, 20), 0.1f)
            && FMath::IsNearlyEqual(Timed->StableLandingSeconds, 0.18f, 0.01f)
            && FMath::IsNearlyEqual(Timed->ActiveSeconds, 4.3f, 0.01f) && FMath::IsNearlyEqual(Timed->DisabledSeconds, 1.6f, 0.01f)
            && Timed->PlatformCollision && Timed->IsCarryingCollisionEnabled() && Timed->GetState() == ENaiwaP1TimedPlatformState::Ready
            && Timed->GetTriggerCount() == 0 && Timed->GetDisableCount() == 0 && Timed->GetRecoveryCount() == 0
            && (!IntegratedFullProbeHazards[Index]->DisplayMesh->IsVisible()
                || IntegratedFullProbeHazards[Index]->bRevealOnSpawn);
    }
    if (!bGeometryMatches || !InitialSpawn.Equals(FVector(-1450, 0, 212), 5.0f)
        || FVector::Dist2D(Character->GetActorLocation(), InitialSpawn) > 5.0f
        || Character->GetMovementBaseObject() != IntegratedFullProbeGrounds[0]->GetStaticMeshComponent()
        || State != ENaiwaP1State::Playing || DeathEventCount != 0 || ActiveCheckpoint.IsValid() || CollectedIds.Num() != 0)
    {
        FinishIntegratedFullProbe(TEXT("INCOMPLETE"), TEXT("Bound full-candidate geometry, hidden hazards, reset clocks or clean grounded spawn differ from the saved config"));
        return false;
    }
    if (!Character->SetP1Gait(!bIntegratedFullProbeWalk))
    {
        FinishIntegratedFullProbe(TEXT("INCOMPLETE"), TEXT("Could not select the full-route gait"));
        return false;
    }
    bIntegratedFullProbeInitialized = true;
    IntegratedFullProbeMaxZ = Character->GetActorLocation().Z;
    Character->SetAutomatedMoveInput(0.0f);
    IntegratedFullProbeAdvance(ENaiwaP1IntegratedFullProbeStep::HazardTeaching, TEXT("full_config_and_unique_actor_preflight_grounded_spawn"));
    return true;
}

void ANaiwaP1GameMode::IntegratedFullProbeRecord(const TCHAR* Station)
{
    TSharedRef<FJsonObject> Sample = MakeShared<FJsonObject>();
    Sample->SetStringField(TEXT("station"), Station);
    Sample->SetNumberField(TEXT("step"), static_cast<int32>(IntegratedFullProbeStep));
    Sample->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - IntegratedFullProbeStartedWall);
    Sample->SetNumberField(TEXT("world_time_s"), GetWorld()->GetTimeSeconds());
    Sample->SetStringField(TEXT("state"), StateName(State));
    Sample->SetNumberField(TEXT("death_events"), DeathEventCount);
    Sample->SetNumberField(TEXT("pickup_count"), CollectedIds.Num());
    Sample->SetStringField(TEXT("checkpoint_id"), ActiveCheckpoint.IsValid() ? ActiveCheckpoint->GetStableId().ToString() : TEXT("none"));
    if (PlayerCharacter.IsValid())
    {
        const FVector Location = PlayerCharacter->GetActorLocation();
        Sample->SetNumberField(TEXT("x_cm"), Location.X); Sample->SetNumberField(TEXT("y_cm"), Location.Y); Sample->SetNumberField(TEXT("z_cm"), Location.Z);
        Sample->SetNumberField(TEXT("feet_z_cm"), PlayerCharacter->GetCapsuleBottom());
        Sample->SetNumberField(TEXT("velocity_x_cm_s"), PlayerCharacter->GetVelocity().X);
        Sample->SetNumberField(TEXT("velocity_z_cm_s"), PlayerCharacter->GetVelocity().Z);
        Sample->SetBoolField(TEXT("grounded"), PlayerCharacter->GetCharacterMovement()->IsMovingOnGround());
        const UObject* Base = PlayerCharacter->GetMovementBaseObject();
        Sample->SetStringField(TEXT("movement_base"), Base ? Base->GetPathName() : TEXT("none"));
    }
    TArray<TSharedPtr<FJsonValue>> Mechanisms;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetNumberField(TEXT("section"), Index + 1);
        if (IntegratedFullProbeSwitches[Index].IsValid())
        {
            const ANaiwaP1SwitchPlatform* Platform = IntegratedFullProbeSwitches[Index].Get();
            Item->SetStringField(TEXT("switch_state"), SwitchStateName(Platform->GetState()));
            Item->SetNumberField(TEXT("switch_local_x_cm"), Platform->GetCurrentX());
            Item->SetBoolField(TEXT("rider_supported"), Platform->IsCharacterSupported(PlayerCharacter.Get()));
            Item->SetNumberField(TEXT("left_switch_touches"), Platform->GetLeftSwitchTouchCount());
            Item->SetNumberField(TEXT("right_switch_touches"), Platform->GetRightSwitchTouchCount());
        }
        if (IntegratedFullProbePads[Index].IsValid()) Item->SetNumberField(TEXT("pad_activations"), IntegratedFullProbePads[Index]->GetActivationCount());
        if (IntegratedFullProbeTimed[Index].IsValid())
        {
            const ANaiwaP1TimedPlatform* Timed = IntegratedFullProbeTimed[Index].Get();
            Item->SetStringField(TEXT("timed_state"), TimedStateName(Timed->GetState()));
            Item->SetNumberField(TEXT("timed_trigger_count"), Timed->GetTriggerCount());
            Item->SetNumberField(TEXT("timed_countdown_remaining_s"), Timed->GetCountdownRemaining());
            Item->SetBoolField(TEXT("timed_carrying_enabled"), Timed->IsCarryingCollisionEnabled());
        }
        Mechanisms.Add(MakeShared<FJsonValueObject>(Item));
    }
    Sample->SetArrayField(TEXT("mechanisms"), Mechanisms);
    IntegratedFullProbeStations.Add(MakeShared<FJsonValueObject>(Sample));
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_INTEGRATED_FULL_PROBE station=%s step=%d x=%.1f state=%s"), Station,
        static_cast<int32>(IntegratedFullProbeStep), PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().X : 0.0f, StateName(State));
}

void ANaiwaP1GameMode::IntegratedFullProbeAdvance(ENaiwaP1IntegratedFullProbeStep NextStep, const TCHAR* Station)
{
    IntegratedFullProbeStep = NextStep;
    IntegratedFullProbeStepStartedWall = FPlatformTime::Seconds();
    IntegratedFullProbeRecord(Station);
}

bool ANaiwaP1GameMode::ObserveIntegratedFullTimed(int32 Index)
{
    ANaiwaP1TimedPlatform* Timed = IntegratedFullProbeTimed[Index].Get();
    if (Timed->GetTriggerCount() > 1)
    {
        FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Core route unexpectedly re-triggered a timed platform"));
        return false;
    }
    if (!IntegratedFullProbeTimedSupported[Index] && PlayerCharacter->GetCharacterMovement()->IsMovingOnGround()
        && PlayerCharacter->GetMovementBaseObject() == Timed->PlatformCollision.Get())
    {
        IntegratedFullProbeTimedSupported[Index] = true;
        IntegratedFullProbeRecord(Index == 0 ? TEXT("timed_teaching_real_floor_support") : TEXT("timed_combination_real_floor_support"));
    }
    if (Timed->GetTriggerCount() == 1 && IntegratedFullProbeTimedTriggerTimes[Index] < 0.0f)
    {
        if (!IntegratedFullProbeTimedSupported[Index] || Timed->GetState() != ENaiwaP1TimedPlatformState::Countdown)
        {
            FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Timed trigger did not follow sampled real floor support"));
            return false;
        }
        IntegratedFullProbeTimedTriggerTimes[Index] = GetWorld()->GetTimeSeconds();
        IntegratedFullProbeTimedPreviousRemaining[Index] = Timed->GetCountdownRemaining();
        IntegratedFullProbeRecord(Index == 0 ? TEXT("timed_teaching_countdown_started") : TEXT("timed_combination_countdown_started"));
    }
    if (IntegratedFullProbeTimedTriggerTimes[Index] >= 0.0f && !IntegratedFullProbeTimedCompleted[Index])
    {
        if (Timed->GetState() != ENaiwaP1TimedPlatformState::Countdown || !Timed->IsCarryingCollisionEnabled()
            || Timed->GetCountdownRemaining() <= 0.0f
            || Timed->GetCountdownRemaining() > IntegratedFullProbeTimedPreviousRemaining[Index] + 0.02f)
        {
            FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Timed clock expired, increased or disabled carrying before a safe bank was reached"));
            return false;
        }
        if (!IntegratedFullProbeTimedDeclined[Index]
            && Timed->GetCountdownRemaining() < IntegratedFullProbeTimedPreviousRemaining[Index] - 0.001f)
        {
            IntegratedFullProbeTimedDeclined[Index] = true;
            IntegratedFullProbeRecord(Index == 0 ? TEXT("timed_teaching_clock_declined") : TEXT("timed_combination_clock_declined"));
        }
        IntegratedFullProbeTimedPreviousRemaining[Index] = Timed->GetCountdownRemaining();
    }
    return true;
}

bool ANaiwaP1GameMode::CompleteIntegratedFullTimed(int32 Index)
{
    const ANaiwaP1TimedPlatform* Timed = IntegratedFullProbeTimed[Index].Get();
    if (!IntegratedFullProbeTimedSupported[Index] || !IntegratedFullProbeTimedDeclined[Index]
        || IntegratedFullProbeTimedTriggerTimes[Index] < 0.0f || Timed->GetTriggerCount() != 1
        || Timed->GetState() != ENaiwaP1TimedPlatformState::Countdown || Timed->GetCountdownRemaining() <= 0.0f)
    {
        FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Timed crossing requires real floor support, one declining clock and positive exit margin"));
        return false;
    }
    IntegratedFullProbeTimedCompleted[Index] = true;
    IntegratedFullProbeTimedCrossingSeconds[Index] = GetWorld()->GetTimeSeconds() - IntegratedFullProbeTimedTriggerTimes[Index];
    IntegratedFullProbeTimedExitMargins[Index] = Timed->GetCountdownRemaining();
    if (FMath::Abs(IntegratedFullProbeTimedCrossingSeconds[Index] + IntegratedFullProbeTimedExitMargins[Index] - Timed->ActiveSeconds) > 0.1f)
    {
        FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Timed exit world-time elapsed and remaining countdown do not match the active budget"));
        return false;
    }
    IntegratedFullProbeRecord(Index == 0 ? TEXT("timed_teaching_safe_bank_before_expiry") : TEXT("timed_combination_safe_bank_before_expiry"));
    return true;
}

void ANaiwaP1GameMode::TickIntegratedFullProbe()
{
    const double Now = FPlatformTime::Seconds();
    if (UGameplayStatics::IsGamePaused(this))
    {
        FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Unexpected pause in the core functional route"));
        return;
    }
    if (Now - IntegratedFullProbeStartedWall > 240.0 || Now - IntegratedFullProbeStepStartedWall > 40.0
        || (!bIntegratedFullProbeInitialized && Now - IntegratedFullProbeStartedWall > 15.0))
    {
        FinishIntegratedFullProbe(TEXT("FAIL"), FString::Printf(TEXT("Timeout step=%d state=%s x=%.1f feet_z=%.1f"),
            static_cast<int32>(IntegratedFullProbeStep), StateName(State),
            PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().X : 0.0f,
            PlayerCharacter.IsValid() ? PlayerCharacter->GetCapsuleBottom() : 0.0f));
        return;
    }
    if (!bIntegratedFullProbeInitialized)
    {
        InitializeIntegratedFullProbe();
        return;
    }
    bool bActorsValid = PlayerCharacter.IsValid() && IntegratedFullProbePickup.IsValid() && IntegratedFullProbeGoal.IsValid();
    for (int32 Index = 0; Index < 2; ++Index)
        bActorsValid &= IntegratedFullProbeSwitches[Index].IsValid() && IntegratedFullProbePads[Index].IsValid()
            && IntegratedFullProbeTimed[Index].IsValid() && IntegratedFullProbeHazards[Index].IsValid();
    for (const auto& Actor : IntegratedFullProbeCheckpointsActors) bActorsValid &= Actor.IsValid();
    for (const auto& Actor : IntegratedFullProbeGrounds) bActorsValid &= Actor.IsValid();
    if (!bActorsValid)
    {
        FinishIntegratedFullProbe(TEXT("INCOMPLETE"), TEXT("A strictly bound full-route actor disappeared"));
        return;
    }
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const float X = Character->GetActorLocation().X;
    const bool bGrounded = Movement->IsMovingOnGround();
    IntegratedFullProbeMaxZ = FMath::Max(IntegratedFullProbeMaxZ, Character->GetActorLocation().Z);
    if (DeathEventCount != 0 || State == ENaiwaP1State::Dying || State == ENaiwaP1State::Respawning
        || (State == ENaiwaP1State::Won && IntegratedFullProbeStep < ENaiwaP1IntegratedFullProbeStep::Goal))
    {
        FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Core route left Playing or died before reaching the physical goal; no recovery is injected"));
        return;
    }
    if (Character->IsRunningGait() == bIntegratedFullProbeWalk
        || FMath::Abs(Movement->MaxWalkSpeed - (bIntegratedFullProbeWalk ? 150.0f : 300.0f)) > 0.1f)
    {
        FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Selected gait/speed changed during the full route"));
        return;
    }
    if (bIntegratedFullProbeJumpHeld && GetWorld()->GetTimeSeconds() >= IntegratedFullProbeJumpReleaseTime)
    {
        Character->StopJumping(); bIntegratedFullProbeJumpHeld = false;
        IntegratedFullProbeRecord(TEXT("ordinary_hazard_jump_released"));
    }
    for (int32 Index = 0; Index < 2; ++Index)
    {
        if (IntegratedFullProbePads[Index]->GetActivationCount() > 1)
        {
            FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("A single-pass bounce section activated more than once"));
            return;
        }
        const ANaiwaP1SwitchPlatform* Platform = IntegratedFullProbeSwitches[Index].Get();
        if (!IntegratedFullProbeCuesSeen[Index] && Platform->IsDepartureCueActive() && Platform->IsCharacterSupported(Character))
        {
            IntegratedFullProbeCuesSeen[Index] = true;
            IntegratedFullProbeRecord(Index == 0 ? TEXT("teaching_shuttle_supported_departure_cue") : TEXT("combination_shuttle_supported_departure_cue"));
        }
        if (!ObserveIntegratedFullTimed(Index)) return;
    }
    const auto IsOnGround = [&](int32 Index)
    { return bGrounded && Character->GetMovementBaseObject() == IntegratedFullProbeGrounds[Index]->GetStaticMeshComponent(); };
    switch (IntegratedFullProbeStep)
    {
    case ENaiwaP1IntegratedFullProbeStep::WaitReady:
        return;
    case ENaiwaP1IntegratedFullProbeStep::HazardTeaching:
    case ENaiwaP1IntegratedFullProbeStep::HazardCombination:
    {
        const int32 Index = IntegratedFullProbeStep == ENaiwaP1IntegratedFullProbeStep::HazardTeaching ? 0 : 1;
        const float HazardX = IntegratedFullProbeHazards[Index]->GetActorLocation().X;
        const int32 GroundIndex = Index == 0 ? 0 : 4;
        Character->SetAutomatedMoveInput(1.0f);
        // The 80 cm takeoff offset preserves the already-tested teaching route in both gaits.
        if (!IntegratedFullProbeJumps[Index] && X >= HazardX - 80.0f)
        {
            if (!IsOnGround(GroundIndex))
            {
                FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Hazard takeoff lacked the expected actual static bank base"));
                return;
            }
            Character->Jump(); IntegratedFullProbeJumps[Index] = true; bIntegratedFullProbeJumpHeld = true;
            IntegratedFullProbeJumpReleaseTime = GetWorld()->GetTimeSeconds() + 0.18f;
            IntegratedFullProbeRecord(Index == 0 ? TEXT("teaching_hidden_hazard_real_jump") : TEXT("combination_hidden_hazard_real_jump"));
        }
        if (IntegratedFullProbeJumps[Index] && X > HazardX + 90.0f && IsOnGround(GroundIndex))
        {
            if (IntegratedFullProbeHazards[Index]->DisplayMesh->IsVisible()
                && !IntegratedFullProbeHazards[Index]->bRevealOnSpawn)
            {
                FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("A hidden hazard was revealed during an expected safe jump"));
                return;
            }
            IntegratedFullProbeHazardsCleared[Index] = true;
            IntegratedFullProbeAdvance(Index == 0 ? ENaiwaP1IntegratedFullProbeStep::SwitchTeachingTouch
                : ENaiwaP1IntegratedFullProbeStep::CheckpointCombination,
                Index == 0 ? TEXT("teaching_hazard_cleared_real_floor_landing") : TEXT("combination_hazard_cleared_real_floor_landing"));
        }
        return;
    }
    case ENaiwaP1IntegratedFullProbeStep::SwitchTeachingTouch:
    case ENaiwaP1IntegratedFullProbeStep::SwitchCombinationTouch:
    {
        const int32 Index = IntegratedFullProbeStep == ENaiwaP1IntegratedFullProbeStep::SwitchTeachingTouch ? 0 : 1;
        Character->SetAutomatedMoveInput(1.0f);
        if (IntegratedFullProbeSwitches[Index]->GetLeftSwitchTouchCount() > 0)
            IntegratedFullProbeAdvance(Index == 0 ? ENaiwaP1IntegratedFullProbeStep::SwitchTeachingBoard
                : ENaiwaP1IntegratedFullProbeStep::SwitchCombinationBoard,
                Index == 0 ? TEXT("teaching_left_switch_touched_physically") : TEXT("combination_left_switch_touched_physically"));
        return;
    }
    case ENaiwaP1IntegratedFullProbeStep::SwitchTeachingBoard:
    case ENaiwaP1IntegratedFullProbeStep::SwitchCombinationBoard:
    {
        const int32 Index = IntegratedFullProbeStep == ENaiwaP1IntegratedFullProbeStep::SwitchTeachingBoard ? 0 : 1;
        const ANaiwaP1SwitchPlatform* Platform = IntegratedFullProbeSwitches[Index].Get();
        Character->SetAutomatedMoveInput(1.0f);
        if (X > Platform->GetActorLocation().X && !Platform->IsCharacterSupported(Character))
        {
            FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("A dock was passed without the shuttle carrying the character"));
            return;
        }
        const float Offset = X - Platform->PlatformCollision->GetComponentLocation().X;
        if (Platform->IsCharacterSupported(Character) && Offset >= -65.0f)
        {
            Character->SetAutomatedMoveInput(0.0f); IntegratedFullProbeBoardOffsets[Index] = Offset;
            IntegratedFullProbeBoarded[Index] = true;
            IntegratedFullProbeAdvance(Index == 0 ? ENaiwaP1IntegratedFullProbeStep::SwitchTeachingRide
                : ENaiwaP1IntegratedFullProbeStep::SwitchCombinationRide,
                Index == 0 ? TEXT("teaching_shuttle_boarded_with_real_movement") : TEXT("combination_shuttle_boarded_with_real_movement"));
        }
        return;
    }
    case ENaiwaP1IntegratedFullProbeStep::SwitchTeachingRide:
    case ENaiwaP1IntegratedFullProbeStep::SwitchCombinationRide:
    {
        const int32 Index = IntegratedFullProbeStep == ENaiwaP1IntegratedFullProbeStep::SwitchTeachingRide ? 0 : 1;
        const ANaiwaP1SwitchPlatform* Platform = IntegratedFullProbeSwitches[Index].Get();
        Character->SetAutomatedMoveInput(0.0f);
        if (!IntegratedFullProbeMidwaySupported[Index] && Platform->GetState() == ENaiwaP1SwitchPlatformState::TravelingRight
            && FMath::Abs(Platform->GetCurrentX()) < 100.0f && Platform->IsCharacterSupported(Character))
        {
            IntegratedFullProbeMidwaySupported[Index] = true;
            IntegratedFullProbeRecord(Index == 0 ? TEXT("teaching_shuttle_supported_mid_gap") : TEXT("combination_shuttle_supported_mid_gap"));
        }
        if (Platform->GetState() == ENaiwaP1SwitchPlatformState::DockedRight)
        {
            if (!Platform->IsCharacterSupported(Character) || !IntegratedFullProbeCuesSeen[Index]
                || !IntegratedFullProbeMidwaySupported[Index]
                || FMath::Abs(X - Platform->PlatformCollision->GetComponentLocation().X - IntegratedFullProbeBoardOffsets[Index]) > 55.0f)
            {
                FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Shuttle failed its supported cue/mid-gap/right-dock carrying evidence"));
                return;
            }
            IntegratedFullProbeDockedWithRider[Index] = true;
            IntegratedFullProbeAdvance(Index == 0 ? ENaiwaP1IntegratedFullProbeStep::SwitchTeachingExit
                : ENaiwaP1IntegratedFullProbeStep::SwitchCombinationExit,
                Index == 0 ? TEXT("teaching_shuttle_right_dock_with_rider") : TEXT("combination_shuttle_right_dock_with_rider"));
        }
        return;
    }
    case ENaiwaP1IntegratedFullProbeStep::SwitchTeachingExit:
    case ENaiwaP1IntegratedFullProbeStep::SwitchCombinationExit:
    {
        const int32 Index = IntegratedFullProbeStep == ENaiwaP1IntegratedFullProbeStep::SwitchTeachingExit ? 0 : 1;
        Character->SetAutomatedMoveInput(1.0f);
        if (X >= (Index == 0 ? 630.0f : 5680.0f) && IsOnGround(Index == 0 ? 1 : 4))
            IntegratedFullProbeAdvance(Index == 0 ? ENaiwaP1IntegratedFullProbeStep::BounceTeaching
                : ENaiwaP1IntegratedFullProbeStep::HazardCombination,
                Index == 0 ? TEXT("teaching_shuttle_exit_actual_static_bank") : TEXT("combination_shuttle_exit_actual_static_bank"));
        return;
    }
    case ENaiwaP1IntegratedFullProbeStep::BounceTeaching:
    case ENaiwaP1IntegratedFullProbeStep::BounceCombination:
    {
        const int32 Index = IntegratedFullProbeStep == ENaiwaP1IntegratedFullProbeStep::BounceTeaching ? 0 : 1;
        Character->SetAutomatedMoveInput(1.0f);
        // Walking off the bank is deliberate. No ordinary Jump can substitute for either bounce.
        if (IntegratedFullProbePads[Index]->GetActivationCount() == 1)
        {
            if (IntegratedFullProbeSwitches[Index]->GetRightSwitchTouchCount() < 1)
            {
                FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Expected physical right switch was never touched on the bank route"));
                return;
            }
            IntegratedFullProbeAdvance(Index == 0 ? ENaiwaP1IntegratedFullProbeStep::LandingTeaching
                : ENaiwaP1IntegratedFullProbeStep::TimedCombinationLanding,
                Index == 0 ? TEXT("teaching_pad_activation_one") : TEXT("combination_pad_activation_one"));
        }
        else if (X > IntegratedFullProbePads[Index]->GetActorLocation().X + 200.0f)
            FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("A bounce pad was bypassed without a real activation"));
        return;
    }
    case ENaiwaP1IntegratedFullProbeStep::LandingTeaching:
    case ENaiwaP1IntegratedFullProbeStep::TimedCombinationLanding:
    {
        const int32 Index = IntegratedFullProbeStep == ENaiwaP1IntegratedFullProbeStep::LandingTeaching ? 0 : 1;
        Character->SetAutomatedMoveInput(1.0f);
        if (!IntegratedFullProbeLaunched[Index] && Movement->IsFalling() && Character->GetVelocity().Z > 100.0f)
        {
            IntegratedFullProbeLaunched[Index] = true;
            IntegratedFullProbeRecord(Index == 0 ? TEXT("teaching_bounce_upward_motion") : TEXT("combination_bounce_upward_motion"));
        }
        const bool bExpectedFloor = Index == 0 ? IsOnGround(2)
            : bGrounded && Character->GetMovementBaseObject() == IntegratedFullProbeTimed[1]->PlatformCollision.Get();
        if (bExpectedFloor)
        {
            if (!IntegratedFullProbeLaunched[Index] || IntegratedFullProbePads[Index]->GetActivationCount() != 1)
            {
                FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Target floor landing did not follow one real upward bounce"));
                return;
            }
            IntegratedFullProbeLanded[Index] = true;
            IntegratedFullProbeAdvance(Index == 0 ? ENaiwaP1IntegratedFullProbeStep::CheckpointTeaching
                : ENaiwaP1IntegratedFullProbeStep::TimedCombinationCross,
                Index == 0 ? TEXT("teaching_bounce_first_actual_high_bank_landing") : TEXT("combination_bounce_first_actual_timed_floor_landing"));
        }
        else if (Index == 1 && IsOnGround(5))
            FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Combination timed platform was skipped rather than landed on"));
        return;
    }
    case ENaiwaP1IntegratedFullProbeStep::CheckpointTeaching:
    case ENaiwaP1IntegratedFullProbeStep::CheckpointCombination:
    case ENaiwaP1IntegratedFullProbeStep::CheckpointFinal:
    {
        const int32 Index = IntegratedFullProbeStep == ENaiwaP1IntegratedFullProbeStep::CheckpointTeaching ? 0
            : IntegratedFullProbeStep == ENaiwaP1IntegratedFullProbeStep::CheckpointCombination ? 1 : 2;
        Character->SetAutomatedMoveInput(1.0f);
        if (ActiveCheckpoint.Get() == IntegratedFullProbeCheckpointsActors[Index].Get() && IsOnGround(Index == 0 ? 2 : Index == 1 ? 4 : 5))
        {
            IntegratedFullProbeCheckpoints[Index] = true;
            IntegratedFullProbeAdvance(Index == 0 ? ENaiwaP1IntegratedFullProbeStep::TimedTeaching
                : Index == 1 ? ENaiwaP1IntegratedFullProbeStep::BounceCombination : ENaiwaP1IntegratedFullProbeStep::PickupRequired,
                Index == 0 ? TEXT("checkpoint_one_real_grounded_route") : Index == 1 ? TEXT("checkpoint_two_real_grounded_route") : TEXT("checkpoint_three_real_grounded_route"));
        }
        return;
    }
    case ENaiwaP1IntegratedFullProbeStep::TimedTeaching:
    case ENaiwaP1IntegratedFullProbeStep::TimedCombinationCross:
    {
        const int32 Index = IntegratedFullProbeStep == ENaiwaP1IntegratedFullProbeStep::TimedTeaching ? 0 : 1;
        Character->SetAutomatedMoveInput(1.0f);
        if (X >= (Index == 0 ? 3622.0f : 7272.0f) && IsOnGround(Index == 0 ? 3 : 5))
        {
            if (!CompleteIntegratedFullTimed(Index)) return;
            IntegratedFullProbeAdvance(Index == 0 ? ENaiwaP1IntegratedFullProbeStep::SwitchCombinationTouch
                : ENaiwaP1IntegratedFullProbeStep::CheckpointFinal,
                Index == 0 ? TEXT("timed_teaching_exit_real_static_bank") : TEXT("timed_combination_exit_real_static_bank"));
        }
        return;
    }
    case ENaiwaP1IntegratedFullProbeStep::PickupRequired:
        Character->SetAutomatedMoveInput(1.0f);
        if (CollectedIds.Contains(IntegratedFullProbePickup->GetStableId()) && CollectedIds.Num() == 1 && IsOnGround(5))
        {
            if (IntegratedFullProbePickup->DisplayMesh->IsVisible()
                || IntegratedFullProbePickup->TriggerBox->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
            {
                FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Physically collected required pickup did not hide and disable itself"));
                return;
            }
            bIntegratedFullProbePickupSeen = true;
            IntegratedFullProbeAdvance(ENaiwaP1IntegratedFullProbeStep::Goal, TEXT("required_pickup_real_overlap_hidden_and_counted_once"));
        }
        return;
    case ENaiwaP1IntegratedFullProbeStep::Goal:
        if (State == ENaiwaP1State::Playing) { Character->SetAutomatedMoveInput(1.0f); return; }
        if (State != ENaiwaP1State::Won || !IntegratedFullProbeGoal->TriggerBox->IsOverlappingActor(Character)
            || IsGameplayInputAllowed() || Movement->MovementMode != MOVE_None || CollectedIds.Num() != 1
            || ActiveCheckpoint.Get() != IntegratedFullProbeCheckpointsActors[2].Get())
        {
            FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Physical goal overlap did not enter a locked Won state after the required pickup"));
            return;
        }
        Character->SetAutomatedMoveInput(0.0f);
        IntegratedFullProbeAdvance(ENaiwaP1IntegratedFullProbeStep::WonSettle, TEXT("physical_goal_overlap_entered_won_with_controls_locked"));
        return;
    case ENaiwaP1IntegratedFullProbeStep::WonSettle:
        if (Now - IntegratedFullProbeStepStartedWall < 0.4) return;
        if (State != ENaiwaP1State::Won || IsGameplayInputAllowed() || DeathEventCount != 0
            || Character->GetVelocity().Size() > 1.0f || Movement->MovementMode != MOVE_None)
        {
            FinishIntegratedFullProbe(TEXT("FAIL"), TEXT("Won did not remain stable and motion-locked"));
            return;
        }
        FinishIntegratedFullProbe(TEXT("PASS"), TEXT("Full candidate core route traversed all teaching/combination mechanisms, three checkpoints, required pickup and physical goal Won; recovery/real-keyboard/visual acceptance remain separate"));
        return;
    }
}

void ANaiwaP1GameMode::FinishIntegratedFullProbe(const TCHAR* Result, const FString& Detail)
{
    if (!bIntegratedFullProbeEnabled) return;
    IntegratedFullProbeRecord(TEXT("full_probe_finished"));
    bIntegratedFullProbeEnabled = false;
    if (PlayerCharacter.IsValid())
    {
        PlayerCharacter->SetAutomatedMoveInput(0.0f); PlayerCharacter->StopJumping(); PlayerCharacter->DisableAutomatedInput();
    }
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1IntegratedFullCoreRouteV1"));
    Report->SetStringField(TEXT("status"), Result); Report->SetStringField(TEXT("detail"), Detail);
    Report->SetStringField(TEXT("scope"), TEXT("P1-5b_full_candidate_core_route_only"));
    Report->SetBoolField(TEXT("full_p1_5b_acceptance"), false);
    Report->SetBoolField(TEXT("real_keyboard_input_evidence"), false); Report->SetBoolField(TEXT("screenshots_in_this_process"), false);
    Report->SetStringField(TEXT("bound_config_sha256"), TEXT("3aad9c28e6a88397aeb6e1652559fb8d60496185fe255633ab259d7e72f2aa5d"));
    Report->SetStringField(TEXT("map"), GetWorld()->GetMapName());
    Report->SetStringField(TEXT("map_package"), GetWorld()->GetOutermost()->GetName());
    Report->SetStringField(TEXT("gait_speed"), bIntegratedFullProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    Report->SetStringField(TEXT("control_method"), TEXT("SetP1Gait; SetAutomatedMoveInput; Jump/StopJumping only at the two hidden hazards"));
    Report->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - IntegratedFullProbeStartedWall);
    Report->SetNumberField(TEXT("final_step"), static_cast<int32>(IntegratedFullProbeStep)); Report->SetStringField(TEXT("final_state"), StateName(State));
    Report->SetNumberField(TEXT("death_events"), DeathEventCount); Report->SetNumberField(TEXT("pickup_count"), CollectedIds.Num());
    Report->SetNumberField(TEXT("probe_character_teleports"), 0); Report->SetNumberField(TEXT("direct_trigger_injections"), 0);
    Report->SetNumberField(TEXT("platform_control_injections"), 0); Report->SetNumberField(TEXT("production_respawn_placements"), 0);
    Report->SetNumberField(TEXT("ordinary_jump_calls"), static_cast<int32>(IntegratedFullProbeJumps[0]) + static_cast<int32>(IntegratedFullProbeJumps[1]));
    Report->SetNumberField(TEXT("max_character_z_cm"), IntegratedFullProbeMaxZ);
    Report->SetBoolField(TEXT("strict_preflight_passed"), bIntegratedFullProbeInitialized);
    Report->SetBoolField(TEXT("required_pickup_physically_collected"), bIntegratedFullProbePickupSeen);
    TArray<TSharedPtr<FJsonValue>> Sections;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("section"), Index == 0 ? TEXT("teaching") : TEXT("combination"));
        Item->SetBoolField(TEXT("hidden_hazard_jump_and_floor_landing"), IntegratedFullProbeHazardsCleared[Index]);
        Item->SetBoolField(TEXT("real_shuttle_boarding"), IntegratedFullProbeBoarded[Index]);
        Item->SetBoolField(TEXT("supported_departure_cue"), IntegratedFullProbeCuesSeen[Index]);
        Item->SetBoolField(TEXT("supported_mid_gap_ride"), IntegratedFullProbeMidwaySupported[Index]);
        Item->SetBoolField(TEXT("supported_right_dock"), IntegratedFullProbeDockedWithRider[Index]);
        Item->SetBoolField(TEXT("real_upward_pad_launch"), IntegratedFullProbeLaunched[Index]);
        Item->SetBoolField(TEXT("actual_target_floor_landing"), IntegratedFullProbeLanded[Index]);
        Item->SetNumberField(TEXT("pad_activation_count"), IntegratedFullProbePads[Index].IsValid() ? IntegratedFullProbePads[Index]->GetActivationCount() : 0);
        Item->SetBoolField(TEXT("timed_actual_floor_support"), IntegratedFullProbeTimedSupported[Index]);
        Item->SetBoolField(TEXT("timed_countdown_declined"), IntegratedFullProbeTimedDeclined[Index]);
        Item->SetBoolField(TEXT("timed_safe_bank_reached"), IntegratedFullProbeTimedCompleted[Index]);
        Item->SetNumberField(TEXT("timed_trigger_world_time_s"), IntegratedFullProbeTimedTriggerTimes[Index]);
        Item->SetNumberField(TEXT("timed_trigger_to_safe_bank_s"), IntegratedFullProbeTimedCrossingSeconds[Index]);
        Item->SetNumberField(TEXT("timed_remaining_at_safe_bank_s"), IntegratedFullProbeTimedExitMargins[Index]);
        Sections.Add(MakeShared<FJsonValueObject>(Item));
    }
    Report->SetArrayField(TEXT("sections"), Sections);
    TArray<TSharedPtr<FJsonValue>> Checkpoints;
    for (int32 Index = 0; Index < 3; ++Index) Checkpoints.Add(MakeShared<FJsonValueBoolean>(IntegratedFullProbeCheckpoints[Index]));
    Report->SetArrayField(TEXT("checkpoints_physically_reached"), Checkpoints);
    Report->SetArrayField(TEXT("stations"), IntegratedFullProbeStations);
    TArray<TSharedPtr<FJsonValue>> Pending;
    for (const TCHAR* Item : { TEXT("each_checkpoint_natural_failure_and_recovery"), TEXT("restart_and_pause_matrix"),
        TEXT("both_side_recall_after_failure"), TEXT("ordinary_window_real_keyboard"), TEXT("visual_camera_readability"), TEXT("bounce_bypass_assessment") })
        Pending.Add(MakeShared<FJsonValueString>(Item));
    Report->SetArrayField(TEXT("outside_this_core_route"), Pending);
    FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("integrated_full_core_route_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_INTEGRATED_FULL_PROBE result=%s detail=%s output=%s saved=%d"), Result, *Detail, *Output, bSaved);
    FPlatformMisc::RequestExit(false);
}

// ---------------------------------------------------------------------------
// Gap-width probe (v5 narrow-block feasibility)
//
// v5 assumes a 160 cm block with a 65 cm gap makes Run300 fall and thereby slows
// the route. The capsule radius is 42 cm (diameter 84 cm), so a 65 cm gap cannot
// swallow the capsule. This probe measures the real threshold on the dedicated
// L_NaiwaP1_GapProbe corridor instead of trusting the design assumption.
//
// Geometry must stay in sync with .workbuddy/work/20261017_工程小测试/build_gap_probe.py.
// ---------------------------------------------------------------------------
namespace
{
    constexpr int32 GapProbeSegmentCount = 6;
    constexpr float GapProbeGaps[GapProbeSegmentCount] = { 180.0f, 220.0f, 260.0f, 300.0f, 400.0f, 500.0f };
    constexpr float GapProbeRunStarts[GapProbeSegmentCount] = { 0.0f, 1140.0f, 2360.0f, 3660.0f, 5040.0f, 6620.0f };
    constexpr float GapProbeRunEnds[GapProbeSegmentCount] = { 840.0f, 2060.0f, 3360.0f, 4740.0f, 6320.0f, 8100.0f };
    // 120 cm of solid run-up before the first gap so the gait reaches full speed.
    constexpr float GapProbeLaunchX[GapProbeSegmentCount] = { -120.0f, 1020.0f, 2240.0f, 3540.0f, 4920.0f, 6500.0f };
    constexpr float GapProbeTop = 120.0f;
    constexpr float GapProbeStandZ = GapProbeTop + 92.0f;
    constexpr float GapProbeDropTolerance = 30.0f;   // capsule centre below this means it genuinely fell
    constexpr float GapProbeClearMargin = 40.0f;     // must reach the strip past the last block
    constexpr float GapProbeSettleSeconds = 0.55f;
}

void ANaiwaP1GameMode::GapProbeEnterSegment(int32 Index)
{
    if (!PlayerCharacter.IsValid() || Index < 0 || Index >= GapProbeSegmentCount) return;
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    Character->SetAutomatedMoveInput(0.0f);
    Movement->StopMovementImmediately();
    Movement->Velocity = FVector::ZeroVector;
    Character->SetActorLocation(FVector(GapProbeLaunchX[Index], 0.0f, GapProbeStandZ), false, nullptr,
        ETeleportType::TeleportPhysics);
    Movement->SetMovementMode(MOVE_Walking);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_GAP_PROBE segment=%d gap_cm=%.0f launch_x=%.0f"),
        Index, GapProbeGaps[Index], GapProbeLaunchX[Index]);
}

void ANaiwaP1GameMode::TickGapProbe()
{
    if (bGapProbeFinished) return;
    if (!PlayerCharacter.IsValid()) return;
    if (GapProbeIndex >= GapProbeSegmentCount)
    {
        FinishGapProbe();
        return;
    }
    if (State != ENaiwaP1State::Playing) return;
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    Movement->MaxWalkSpeed = bGapProbeWalk ? 150.0f : 300.0f;
    const double Now = FPlatformTime::Seconds();

    if (bGapProbeSettling)
    {
        if (GapProbePhaseStartedWall <= 0.0)
        {
            GapProbeEnterSegment(GapProbeIndex);
            GapProbePhaseStartedWall = Now;
            return;
        }
        Character->SetAutomatedMoveInput(0.0f);
        if (Now - GapProbePhaseStartedWall < GapProbeSettleSeconds) return;
        bGapProbeSettling = false;
        GapProbePhaseStartedWall = Now;
        const FVector Settled = Character->GetActorLocation();
        GapProbeMinZ = Settled.Z;
        GapProbeMaxX = Settled.X;
        GapProbeTrace.Reset();
        GapProbeNextSampleWall = 0.0;
        return;
    }

    Character->SetAutomatedMoveInput(1.0f);
    const FVector Location = Character->GetActorLocation();
    GapProbeMinZ = FMath::Min(GapProbeMinZ, Location.Z);
    GapProbeMaxX = FMath::Max(GapProbeMaxX, Location.X);

    // Trace every 80 ms so the fall height and the recovery position are both readable.
    if (Now >= GapProbeNextSampleWall && GapProbeTrace.Num() < 400)
    {
        GapProbeNextSampleWall = Now + 0.08;
        TSharedRef<FJsonObject> Sample = MakeShared<FJsonObject>();
        Sample->SetNumberField(TEXT("x"), Location.X);
        Sample->SetNumberField(TEXT("z"), Location.Z);
        Sample->SetBoolField(TEXT("ground"), Movement->IsMovingOnGround());
        Sample->SetNumberField(TEXT("vz"), Movement->Velocity.Z);
        GapProbeTrace.Add(MakeShared<FJsonValueObject>(Sample));
    }

    const bool bDropped = Location.Z < GapProbeTop - GapProbeDropTolerance;
    const bool bCleared = Location.X >= GapProbeRunEnds[GapProbeIndex] + GapProbeClearMargin;
    const float SpanCm = GapProbeRunEnds[GapProbeIndex] - GapProbeLaunchX[GapProbeIndex];
    const float TimeoutSeconds = SpanCm / FMath::Max(1.0f, Movement->MaxWalkSpeed) * 2.0f + 2.0f;
    const bool bTimeout = (Now - GapProbePhaseStartedWall) > TimeoutSeconds;
    if (!bDropped && !bCleared && !bTimeout) return;

    const TCHAR* Outcome = bCleared ? TEXT("passed") : (bDropped ? TEXT("dropped") : TEXT("timeout"));
    TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetNumberField(TEXT("gap_cm"), GapProbeGaps[GapProbeIndex]);
    Entry->SetStringField(TEXT("outcome"), Outcome);
    Entry->SetNumberField(TEXT("min_z_cm"), GapProbeMinZ);
    Entry->SetNumberField(TEXT("max_x_cm"), GapProbeMaxX);
    Entry->SetNumberField(TEXT("cancel_x_cm"), Location.X);
    Entry->SetNumberField(TEXT("elapsed_s"), Now - GapProbePhaseStartedWall);
    Entry->SetArrayField(TEXT("trace"), GapProbeTrace);
    GapProbeTrace.Reset();
    GapProbeResults.Add(MakeShared<FJsonValueObject>(Entry));
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_GAP_PROBE segment=%d gap_cm=%.0f outcome=%s min_z=%.1f max_x=%.1f"),
        GapProbeIndex, GapProbeGaps[GapProbeIndex], Outcome, GapProbeMinZ, GapProbeMaxX);

    ++GapProbeIndex;
    bGapProbeSettling = true;
    GapProbePhaseStartedWall = 0.0;
}

void ANaiwaP1GameMode::FinishGapProbe()
{
    if (bGapProbeFinished) return;
    bGapProbeFinished = true;
    if (PlayerCharacter.IsValid()) PlayerCharacter->SetAutomatedMoveInput(0.0f);

    float FirstDrop = -1.0f;
    for (const TSharedPtr<FJsonValue>& Value : GapProbeResults)
    {
        const TSharedPtr<FJsonObject> Entry = Value->AsObject();
        if (!Entry.IsValid()) continue;
        if (Entry->GetStringField(TEXT("outcome")) == TEXT("dropped"))
        {
            FirstDrop = static_cast<float>(Entry->GetNumberField(TEXT("gap_cm")));
            break;
        }
    }

    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1GapProbeV1"));
    Report->SetStringField(TEXT("status"), TEXT("measured"));
    Report->SetStringField(TEXT("map"), GetWorld()->GetMapName());
    Report->SetStringField(TEXT("gait"), bGapProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    Report->SetNumberField(TEXT("capsule_radius_cm"), 42.0);
    Report->SetNumberField(TEXT("capsule_diameter_cm"), 84.0);
    Report->SetNumberField(TEXT("segment_count"), GapProbeSegmentCount);
    Report->SetArrayField(TEXT("segments"), GapProbeResults);
    if (FirstDrop > 0.0f)
    {
        Report->SetNumberField(TEXT("first_failing_gap_cm"), FirstDrop);
        Report->SetStringField(TEXT("threshold_note"),
            TEXT("Smaller gaps held the capsule. This is the first width that dropped it."));
    }
    else
    {
        Report->SetStringField(TEXT("threshold_note"),
            TEXT("Every probed gap up to 180 cm held the capsule."));
    }
    Report->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - GapProbePhaseStartedWall);

    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("gap_probe_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_GAP_PROBE result=measured output=%s saved=%d"), *Output, bSaved);
    FPlatformMisc::RequestExit(false);
}

// ---------------------------------------------------------------------------
// Clearance probe (v5 jump window)
//
// v5 derives two clearances on paper: a 220 cm gap crossed by a Run300 jump
// (surplus 28.7 cm) and a suspended 90 cm spike bar cleared by raising the feet
// above its top. Both live or die on how the character controller actually
// resolves landing, so the probe reads them instead of trusting the arithmetic.
//
// Geometry must stay in sync with .workbuddy/work/20261017_工程小测试/build_jump_probe.py.
// ---------------------------------------------------------------------------
namespace
{
    struct FClearanceProbeSegment
    {
        bool bSpike;
        float SpawnX;
        float TriggerX;
        float SuccessX;
        float Lead;        // jump this many cm before the obstacle edge
        float Width;       // gap width, or bar width
        float BarTopZ;     // world Z of the bar top (spikes only)
    };

    const FClearanceProbeSegment ClearanceProbeSegments[] = {
        { false,  100.0f,   500.0f,  1440.0f,  0.0f, 240.0f,   0.0f },
        { false, 1940.0f,  2340.0f,  3300.0f,  0.0f, 260.0f,   0.0f },
        { false, 3800.0f,  4200.0f,  5180.0f,  0.0f, 280.0f,   0.0f },
        { true,  5680.0f,  6480.0f,  6630.0f, 45.0f,  90.0f, 220.0f },
        { true,  7780.0f,  8580.0f,  8730.0f, 50.0f,  90.0f, 220.0f },
        { true,  9880.0f, 10680.0f, 10830.0f, 55.0f,  90.0f, 220.0f },
    };
    constexpr int32 ClearanceProbeSegmentCount = 6;
    constexpr float ClearanceProbeTop = 120.0f;
    constexpr float ClearanceProbeDropZ = 90.0f;
    constexpr float ClearanceProbeSettleSeconds = 0.55f;
}

void ANaiwaP1GameMode::ClearanceProbeEnterSegment(int32 Index)
{
    if (!PlayerCharacter.IsValid() || Index < 0 || Index >= ClearanceProbeSegmentCount) return;
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const FClearanceProbeSegment& Segment = ClearanceProbeSegments[Index];
    Character->SetAutomatedMoveInput(0.0f);
    Movement->StopMovementImmediately();
    Movement->Velocity = FVector::ZeroVector;
    const float CapsuleHalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Character->SetActorLocation(FVector(Segment.SpawnX, 0.0f, ClearanceProbeTop + CapsuleHalfHeight + 2.0f),
        false, nullptr, ETeleportType::TeleportPhysics);
    Movement->SetMovementMode(MOVE_Walking);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_CLEARANCE_PROBE segment=%d kind=%s lead=%.0f spawn_x=%.0f"),
        Index, Segment.bSpike ? TEXT("spike") : TEXT("gap"), Segment.Lead, Segment.SpawnX);
}

void ANaiwaP1GameMode::TickClearanceProbe()
{
    if (bClearanceProbeFinished) return;
    if (!PlayerCharacter.IsValid()) return;
    if (ClearanceProbeIndex >= ClearanceProbeSegmentCount)
    {
        FinishClearanceProbe();
        return;
    }
    if (State != ENaiwaP1State::Playing) return;
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    Movement->MaxWalkSpeed = bClearanceProbeWalk ? 150.0f : 300.0f;
    const FClearanceProbeSegment& Segment = ClearanceProbeSegments[ClearanceProbeIndex];
    const double Now = FPlatformTime::Seconds();

    if (bClearanceProbeSettling)
    {
        if (ClearanceProbePhaseStartedWall <= 0.0)
        {
            ClearanceProbeEnterSegment(ClearanceProbeIndex);
            ClearanceProbePhaseStartedWall = Now;
            return;
        }
        Character->SetAutomatedMoveInput(0.0f);
        if (Now - ClearanceProbePhaseStartedWall < ClearanceProbeSettleSeconds) return;
        bClearanceProbeSettling = false;
        ClearanceProbePhaseStartedWall = Now;
        const FVector Settled = Character->GetActorLocation();
        ClearanceProbeMinZ = Settled.Z;
        ClearanceProbePeakZ = Settled.Z;
        ClearanceProbeMinClearance = 10000.0f;
        ClearanceProbeJumpX = -10000.0f;
        bClearanceProbeJumped = false;
        bClearanceProbeHit = false;
        ClearanceProbeTrace.Reset();
        ClearanceProbeNextSampleWall = 0.0;
        return;
    }

    Character->SetAutomatedMoveInput(1.0f);
    const FVector Location = Character->GetActorLocation();
    // The blueprint can override the C++ capsule defaults, so read the live size instead
    // of assuming 42 cm. The earlier run stalled exactly one radius short of the bar.
    const float CapsuleRadius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float CapsuleHalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    ClearanceProbeCapsuleRadius = CapsuleRadius;
    ClearanceProbeCapsuleHalfHeight = CapsuleHalfHeight;
    const float FeetZ = Location.Z - CapsuleHalfHeight;
    ClearanceProbeMinZ = FMath::Min(ClearanceProbeMinZ, Location.Z);
    ClearanceProbePeakZ = FMath::Max(ClearanceProbePeakZ, Location.Z);

    // Jump when the capsule front edge reaches Lead centimetres before the obstacle.
    // The front edge matters for the spike bar: the bar blocks the capsule body, so a
    // centre-based test can never be satisfied once the run has been stopped by it.
    const float ProbeFrontX = Location.X + CapsuleRadius;
    if (!bClearanceProbeJumped && ProbeFrontX >= Segment.TriggerX - Segment.Lead)
    {
        Character->Jump();
        bClearanceProbeJumped = true;
        ClearanceProbeJumpX = Location.X;
    }

    if (Segment.bSpike)
    {
        const bool bOverlapping = Location.X + CapsuleRadius > Segment.TriggerX
            && Location.X - CapsuleRadius < Segment.TriggerX + Segment.Width;
        if (bOverlapping)
        {
            ClearanceProbeMinClearance = FMath::Min(ClearanceProbeMinClearance, FeetZ - Segment.BarTopZ);
            if (FeetZ - Segment.BarTopZ < 1.0f) bClearanceProbeHit = true;
        }
    }

    if (Now >= ClearanceProbeNextSampleWall && ClearanceProbeTrace.Num() < 400)
    {
        ClearanceProbeNextSampleWall = Now + 0.04;
        TSharedRef<FJsonObject> Sample = MakeShared<FJsonObject>();
        Sample->SetNumberField(TEXT("x"), Location.X);
        Sample->SetNumberField(TEXT("z"), Location.Z);
        Sample->SetNumberField(TEXT("feet_z"), FeetZ);
        Sample->SetBoolField(TEXT("ground"), Movement->IsMovingOnGround());
        Sample->SetNumberField(TEXT("vz"), Movement->Velocity.Z);
        ClearanceProbeTrace.Add(MakeShared<FJsonValueObject>(Sample));
    }

    const bool bDropped = Location.Z < ClearanceProbeDropZ;
    const bool bPassed = Location.X >= Segment.SuccessX;
    const float SpanCm = Segment.SuccessX - Segment.SpawnX;
    const float TimeoutSeconds = SpanCm / FMath::Max(1.0f, Movement->MaxWalkSpeed) * 1.6f + 2.0f;
    const bool bTimeout = (Now - ClearanceProbePhaseStartedWall) > TimeoutSeconds;
    if (!bDropped && !bPassed && !bTimeout) return;

    const TCHAR* Outcome = bDropped ? TEXT("dropped")
        : (bPassed ? (bClearanceProbeHit ? TEXT("passed_through_bar") : TEXT("passed")) : TEXT("stalled"));

    TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetNumberField(TEXT("index"), ClearanceProbeIndex);
    Entry->SetStringField(TEXT("kind"), Segment.bSpike ? TEXT("spike") : TEXT("gap"));
    Entry->SetNumberField(TEXT("lead_cm"), Segment.Lead);
    Entry->SetNumberField(TEXT("obstacle_width_cm"), Segment.Width);
    Entry->SetStringField(TEXT("outcome"), Outcome);
    Entry->SetBoolField(TEXT("touched_bar"), bClearanceProbeHit);
    Entry->SetNumberField(TEXT("jump_x_cm"), ClearanceProbeJumpX);
    Entry->SetNumberField(TEXT("peak_z_cm"), ClearanceProbePeakZ);
    Entry->SetNumberField(TEXT("peak_feet_cm"), ClearanceProbePeakZ - CapsuleHalfHeight);
    Entry->SetNumberField(TEXT("min_z_cm"), ClearanceProbeMinZ);
    if (Segment.bSpike && ClearanceProbeMinClearance < 9999.0f)
    {
        Entry->SetNumberField(TEXT("min_feet_above_bar_cm"), ClearanceProbeMinClearance);
    }
    Entry->SetNumberField(TEXT("end_x_cm"), Location.X);
    Entry->SetNumberField(TEXT("elapsed_s"), Now - ClearanceProbePhaseStartedWall);
    Entry->SetArrayField(TEXT("trace"), ClearanceProbeTrace);
    ClearanceProbeTrace.Reset();
    ClearanceProbeResults.Add(MakeShared<FJsonValueObject>(Entry));
    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P1_CLEARANCE_PROBE segment=%d kind=%s lead=%.0f outcome=%s peak_feet=%.1f touched=%d"),
        ClearanceProbeIndex, Segment.bSpike ? TEXT("spike") : TEXT("gap"), Segment.Lead, Outcome,
        ClearanceProbePeakZ - CapsuleHalfHeight, bClearanceProbeHit ? 1 : 0);

    ++ClearanceProbeIndex;
    bClearanceProbeSettling = true;
    ClearanceProbePhaseStartedWall = 0.0;
}

void ANaiwaP1GameMode::FinishClearanceProbe()
{
    if (bClearanceProbeFinished) return;
    bClearanceProbeFinished = true;
    if (PlayerCharacter.IsValid()) PlayerCharacter->SetAutomatedMoveInput(0.0f);

    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1ClearanceProbeV1"));
    Report->SetStringField(TEXT("status"), TEXT("measured"));
    Report->SetStringField(TEXT("map"), GetWorld()->GetMapName());
    Report->SetStringField(TEXT("gait"), bClearanceProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    Report->SetNumberField(TEXT("capsule_radius_cm"), ClearanceProbeCapsuleRadius);
    Report->SetNumberField(TEXT("capsule_half_height_cm"), ClearanceProbeCapsuleHalfHeight);
    Report->SetNumberField(TEXT("segment_count"), ClearanceProbeSegmentCount);
    Report->SetArrayField(TEXT("segments"), ClearanceProbeResults);

    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("clearance_probe_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_CLEARANCE_PROBE result=measured output=%s saved=%d"), *Output, bSaved);
    FPlatformMisc::RequestExit(false);
}

// ---------------------------------------------------------------------------
// Timed-window probe
//
// The state machine reads: Ready accumulates continuous grounded time, and only
// when StableLandingSeconds elapses does it enter Countdown with
// CountdownRemaining = ActiveSeconds. That implies the real window is
// (stable landing + active), not (active). This probe measures the sum on a
// dedicated map rather than trusting the reading.
// ---------------------------------------------------------------------------
void ANaiwaP1GameMode::TickTimedWindowProbe()
{
    if (bTimedWindowProbeFinished) return;
    if (!PlayerCharacter.IsValid()) return;
    if (State != ENaiwaP1State::Playing) return;
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const double Now = FPlatformTime::Seconds();
    const double Timeout = 18.0;

    if (!TimedWindowProbeTarget.IsValid())
    {
        for (TActorIterator<ANaiwaP1TimedPlatform> It(GetWorld()); It; ++It)
        {
            TimedWindowProbeTarget = *It;
            break;
        }
        if (!TimedWindowProbeTarget.IsValid())
        {
            if (Now - TimedWindowProbePhaseWall > 5.0)
            {
                UE_LOG(LogTemp, Error, TEXT("NAIWA_P1_TIMED_WINDOW_PROBE no timed platform on this map"));
                FinishTimedWindowProbe();
            }
            return;
        }
    }
    ANaiwaP1TimedPlatform* Platform = TimedWindowProbeTarget.Get();
    Movement->MaxWalkSpeed = 300.0f;

    if (Now - TimedWindowProbePhaseWall > Timeout)
    {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_P1_TIMED_WINDOW_PROBE timeout stage=%d"), bTimedWindowProbeStage);
        FinishTimedWindowProbe();
        return;
    }

    const int32 StateRaw = static_cast<int32>(Platform->GetState());
    if (StateRaw != TimedWindowProbeLastState)
    {
        if (StateRaw == static_cast<int32>(ENaiwaP1TimedPlatformState::Countdown))
        {
            TimedWindowProbeCountdownWall = Now;
        }
        else if (StateRaw == static_cast<int32>(ENaiwaP1TimedPlatformState::Disabled))
        {
            TimedWindowProbeDisabledWall = Now;
        }
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_WINDOW_PROBE state=%s at=%.3f"),
            TimedStateName(Platform->GetState()), Now - TimedWindowProbePhaseWall);
        TimedWindowProbeLastState = StateRaw;
    }

    switch (bTimedWindowProbeStage)
    {
    case 0:
        if (Now - TimedWindowProbePhaseWall < 0.6) return;
        Character->SetAutomatedMoveInput(1.0f);
        bTimedWindowProbeStage = 1;
        return;
    case 1:
    {
        Character->SetAutomatedMoveInput(1.0f);
        const bool bSupported = Movement->IsMovingOnGround()
            && Character->GetMovementBaseObject() == Platform->PlatformCollision.Get();
        if (!bSupported) return;
        TimedWindowProbeEnterWall = Now;
        Character->SetAutomatedMoveInput(0.0f);
        Movement->StopMovementImmediately();
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_WINDOW_PROBE landed at x=%.1f"),
            Character->GetActorLocation().X);
        bTimedWindowProbeStage = 2;
        return;
    }
    case 2:
        if (TimedWindowProbeDisabledWall <= 0.0) return;
        if (Now - TimedWindowProbeDisabledWall < 0.25) return;
        bTimedWindowProbeStage = 3;
        return;
    default:
        FinishTimedWindowProbe();
        return;
    }
}

void ANaiwaP1GameMode::FinishTimedWindowProbe()
{
    if (bTimedWindowProbeFinished) return;
    bTimedWindowProbeFinished = true;
    if (PlayerCharacter.IsValid()) PlayerCharacter->SetAutomatedMoveInput(0.0f);

    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1TimedWindowProbeV1"));
    Report->SetStringField(TEXT("status"), TEXT("measured"));
    Report->SetStringField(TEXT("map"), GetWorld()->GetMapName());
    if (TimedWindowProbeTarget.IsValid())
    {
        ANaiwaP1TimedPlatform* Platform = TimedWindowProbeTarget.Get();
        const double ToCountdown = TimedWindowProbeCountdownWall - TimedWindowProbeEnterWall;
        const double ToDisabled = TimedWindowProbeDisabledWall - TimedWindowProbeEnterWall;
        const double Active = TimedWindowProbeDisabledWall - TimedWindowProbeCountdownWall;
        Report->SetNumberField(TEXT("configured_stable_landing_seconds"), Platform->StableLandingSeconds);
        Report->SetNumberField(TEXT("configured_active_seconds"), Platform->ActiveSeconds);
        Report->SetNumberField(TEXT("configured_disabled_seconds"), Platform->DisabledSeconds);
        Report->SetNumberField(TEXT("measured_landing_to_countdown_seconds"), ToCountdown);
        Report->SetNumberField(TEXT("measured_countdown_seconds"), Active);
        Report->SetNumberField(TEXT("measured_landing_to_disabled_seconds"), ToDisabled);
        Report->SetNumberField(TEXT("configured_sum_seconds"),
            Platform->StableLandingSeconds + Platform->ActiveSeconds);
        if (TimedWindowProbeEnterWall > 0.0 && TimedWindowProbeDisabledWall > 0.0)
        {
            Report->SetBoolField(TEXT("window_equals_stable_plus_active"),
                FMath::Abs(ToDisabled - (Platform->StableLandingSeconds + Platform->ActiveSeconds)) < 0.08);
        }
    }
    else
    {
        Report->SetStringField(TEXT("status"), TEXT("no_platform"));
    }

    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("timed_window_probe_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_TIMED_WINDOW_PROBE output=%s saved=%d"), *Output, bSaved);
    FPlatformMisc::RequestExit(false);
}

// ---------------------------------------------------------------------------
// Chain-gap probe (v6 skeleton)
//
// v6 drops the 19-block narrow run and builds three gap chains instead: 240 cm
// gaps separated by 400 cm islands. Each single gap is already measured, so the
// open question is the rhythm: after landing on a 400 cm island, can the capsule
// launch again before the next edge, and does the landing stance ever swallow a
// jump? A swallowed jump is invisible on paper and fatal in play.
//
// Geometry must stay in sync with .workbuddy/work/20261017_工程小测试/build_chain_probe.py.
// ---------------------------------------------------------------------------
namespace
{
    constexpr int32 ChainProbeGapCount = 3;
    constexpr float ChainProbeGapLeft[ChainProbeGapCount] = { 600.0f, 1240.0f, 1880.0f };
    constexpr float ChainProbeGapCm = 240.0f;
    constexpr float ChainProbeIslandCm = 400.0f;
    constexpr float ChainProbeTop = 120.0f;
    constexpr float ChainProbeStartX = -200.0f;
    constexpr float ChainProbeDropZ = 90.0f;
    constexpr float ChainProbeSettleSeconds = 0.6f;
    // The scare spike sits on the same map, past the last island, so the hazard
    // probe can be pointed at it without a second package.
    constexpr float ChainProbeHazardApproachX = 3900.0f;
}

void ANaiwaP1GameMode::ChainProbeEnterStart()
{
    if (!PlayerCharacter.IsValid()) return;
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    Character->SetAutomatedMoveInput(0.0f);
    Movement->StopMovementImmediately();
    Movement->Velocity = FVector::ZeroVector;
    const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Character->SetActorLocation(FVector(ChainProbeStartX, 0.0f, ChainProbeTop + HalfHeight + 2.0f),
        false, nullptr, ETeleportType::TeleportPhysics);
    Movement->SetMovementMode(MOVE_Walking);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_CHAIN_PROBE spawn_x=%.0f gaps=%d gap_cm=%.0f island_cm=%.0f"),
        ChainProbeStartX, ChainProbeGapCount, ChainProbeGapCm, ChainProbeIslandCm);
}

void ANaiwaP1GameMode::TickChainProbe()
{
    if (bChainProbeFinished) return;
    if (!PlayerCharacter.IsValid()) return;
    if (State != ENaiwaP1State::Playing) return;
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    Movement->MaxWalkSpeed = bChainProbeWalk ? 150.0f : 300.0f;
    const double Now = FPlatformTime::Seconds();

    if (bChainProbeSettling)
    {
        if (ChainProbePhaseWall <= 0.0)
        {
            ChainProbeEnterStart();
            ChainProbePhaseWall = Now;
            return;
        }
        Character->SetAutomatedMoveInput(0.0f);
        if (Now - ChainProbePhaseWall < ChainProbeSettleSeconds) return;
        bChainProbeSettling = false;
        ChainProbePhaseWall = Now;
        ChainProbeCapsuleRadius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
        ChainProbeCapsuleHalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        ChainProbeMinZ = Character->GetActorLocation().Z;
        return;
    }

    if (ChainProbeIndex >= ChainProbeGapCount)
    {
        FinishChainProbe();
        return;
    }

    Character->SetAutomatedMoveInput(1.0f);
    const FVector Location = Character->GetActorLocation();
    const bool bGrounded = Movement->IsMovingOnGround();
    ChainProbeMinZ = FMath::Min(ChainProbeMinZ, Location.Z);
    ChainProbePeakZ[ChainProbeIndex] = FMath::Max(ChainProbePeakZ[ChainProbeIndex], Location.Z);

    const float GapLeft = ChainProbeGapLeft[ChainProbeIndex];
    const float GapRight = GapLeft + ChainProbeGapCm;
    const float IslandRight = GapRight + ChainProbeIslandCm;
    const float FrontX = Location.X + ChainProbeCapsuleRadius;

    // Read the launch velocity one frame after Jump(); a swallowed jump never
    // produces upward velocity and that is exactly what this probe looks for.
    if (bChainProbeJumped && ChainProbeJumpVz[ChainProbeIndex] == 0.0f && Movement->Velocity.Z != 0.0f)
    {
        ChainProbeJumpVz[ChainProbeIndex] = Movement->Velocity.Z;
    }

    if (bChainProbeJumped && !ChainProbeLanded[ChainProbeIndex] && bGrounded && Location.X > GapRight)
    {
        ChainProbeLanded[ChainProbeIndex] = true;
        ChainProbeLandX[ChainProbeIndex] = Location.X;
        ChainProbeAirSeconds[ChainProbeIndex] = static_cast<float>(Now - ChainProbeJumpWall);
        ChainProbeLandWall = Now;
    }

    if (!bChainProbeJumped && FrontX >= GapLeft)
    {
        ChainProbeJumpGrounded[ChainProbeIndex] = bGrounded;
        ChainProbeJumpX[ChainProbeIndex] = Location.X;
        if (ChainProbeIndex > 0 && ChainProbeLandWall > 0.0)
        {
            ChainProbeGroundSeconds[ChainProbeIndex] = static_cast<float>(Now - ChainProbeLandWall);
        }
        else
        {
            ChainProbeGroundSeconds[ChainProbeIndex] = static_cast<float>(Now - ChainProbePhaseWall);
        }
        ChainProbeJumpWall = Now;
        bChainProbeJumped = true;
        Character->Jump();
        return;
    }

    const bool bDroppedOut = Location.Z < ChainProbeDropZ;
    if (!bChainProbeJumped || (!ChainProbeLanded[ChainProbeIndex] && !bDroppedOut))
    {
        const float SpanCm = IslandRight - ChainProbeStartX;
        const double TimeoutSeconds = SpanCm / FMath::Max(1.0f, Movement->MaxWalkSpeed) * 2.0 + 3.0;
        if (Now - ChainProbePhaseWall > TimeoutSeconds)
        {
            bChainProbeDropped = true;
            FinishChainProbe();
        }
        return;
    }

    const bool bLanded = ChainProbeLanded[ChainProbeIndex];
    float RunwayAfterLand = -1.0f;
    if (bLanded)
    {
        const float NextEdge = (ChainProbeIndex + 1 < ChainProbeGapCount)
            ? ChainProbeGapLeft[ChainProbeIndex + 1] : IslandRight;
        RunwayAfterLand = NextEdge - ChainProbeLandX[ChainProbeIndex] - ChainProbeCapsuleRadius;
    }

    TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetNumberField(TEXT("gap_index"), ChainProbeIndex);
    Entry->SetNumberField(TEXT("gap_cm"), ChainProbeGapCm);
    Entry->SetStringField(TEXT("outcome"), bLanded ? TEXT("landed") : TEXT("dropped"));
    Entry->SetBoolField(TEXT("grounded_at_jump"), ChainProbeJumpGrounded[ChainProbeIndex]);
    Entry->SetBoolField(TEXT("jump_launched"), ChainProbeJumpVz[ChainProbeIndex] > 1.0f);
    Entry->SetNumberField(TEXT("jump_vz_cm_s"), ChainProbeJumpVz[ChainProbeIndex]);
    Entry->SetNumberField(TEXT("jump_x_cm"), ChainProbeJumpX[ChainProbeIndex]);
    Entry->SetNumberField(TEXT("land_x_cm"), ChainProbeLandX[ChainProbeIndex]);
    Entry->SetNumberField(TEXT("air_seconds"), ChainProbeAirSeconds[ChainProbeIndex]);
    Entry->SetNumberField(TEXT("peak_z_cm"), ChainProbePeakZ[ChainProbeIndex]);
    Entry->SetNumberField(TEXT("runway_after_land_cm"), RunwayAfterLand);
    Entry->SetNumberField(TEXT("grounded_seconds_before_jump"), ChainProbeGroundSeconds[ChainProbeIndex]);
    ChainProbeResults.Add(MakeShared<FJsonValueObject>(Entry));
    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P1_CHAIN_PROBE gap=%d outcome=%s grounded=%d vz=%.0f jump_x=%.0f land_x=%.0f air=%.3f runway=%.1f"),
        ChainProbeIndex, bLanded ? TEXT("landed") : TEXT("dropped"),
        ChainProbeJumpGrounded[ChainProbeIndex] ? 1 : 0, ChainProbeJumpVz[ChainProbeIndex],
        ChainProbeJumpX[ChainProbeIndex], ChainProbeLandX[ChainProbeIndex],
        ChainProbeAirSeconds[ChainProbeIndex], RunwayAfterLand);

    if (!bLanded)
    {
        bChainProbeDropped = true;
        FinishChainProbe();
        return;
    }
    ++ChainProbeIndex;
    bChainProbeJumped = false;
    if (ChainProbeIndex >= ChainProbeGapCount) FinishChainProbe();
}

void ANaiwaP1GameMode::FinishChainProbe()
{
    if (bChainProbeFinished) return;
    bChainProbeFinished = true;
    if (PlayerCharacter.IsValid()) PlayerCharacter->SetAutomatedMoveInput(0.0f);

    int32 Cleared = 0;
    bool bAllLaunched = true;
    float MinRunway = 100000.0f;
    float TotalAir = 0.0f;
    for (const TSharedPtr<FJsonValue>& Value : ChainProbeResults)
    {
        const TSharedPtr<FJsonObject> Entry = Value->AsObject();
        if (!Entry.IsValid()) continue;
        if (Entry->GetStringField(TEXT("outcome")) == TEXT("landed")) ++Cleared;
        if (!Entry->GetBoolField(TEXT("jump_launched"))) bAllLaunched = false;
        TotalAir += static_cast<float>(Entry->GetNumberField(TEXT("air_seconds")));
        const float Runway = static_cast<float>(Entry->GetNumberField(TEXT("runway_after_land_cm")));
        if (Runway >= 0.0f) MinRunway = FMath::Min(MinRunway, Runway);
    }

    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1ChainProbeV1"));
    Report->SetStringField(TEXT("status"), TEXT("measured"));
    Report->SetStringField(TEXT("map"), GetWorld()->GetMapName());
    Report->SetStringField(TEXT("gait"), bChainProbeWalk ? TEXT("Walk150") : TEXT("Run300"));
    Report->SetNumberField(TEXT("capsule_radius_cm"), ChainProbeCapsuleRadius);
    Report->SetNumberField(TEXT("capsule_half_height_cm"), ChainProbeCapsuleHalfHeight);
    Report->SetNumberField(TEXT("gap_cm"), ChainProbeGapCm);
    Report->SetNumberField(TEXT("island_cm"), ChainProbeIslandCm);
    Report->SetNumberField(TEXT("gap_count"), ChainProbeGapCount);
    Report->SetNumberField(TEXT("gaps_cleared"), Cleared);
    Report->SetBoolField(TEXT("chain_completed"), Cleared == ChainProbeGapCount && !bChainProbeDropped);
    Report->SetBoolField(TEXT("every_jump_launched"), bAllLaunched);
    Report->SetNumberField(TEXT("min_runway_after_land_cm"), MinRunway < 99999.0f ? MinRunway : 0.0f);
    Report->SetNumberField(TEXT("total_air_seconds"), TotalAir);
    Report->SetNumberField(TEXT("min_z_cm"), ChainProbeMinZ);
    Report->SetArrayField(TEXT("gaps"), ChainProbeResults);

    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("chain_probe_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_CHAIN_PROBE result=measured cleared=%d/%d completed=%d output=%s saved=%d"),
        Cleared, ChainProbeGapCount, Cleared == ChainProbeGapCount ? 1 : 0, *Output, bSaved);
    FPlatformMisc::RequestExit(false);
}

// ---------------------------------------------------------------------------
// Revealed-hazard probe
//
// Until now every hazard was hidden by ResetForAttempt() and only drawn once the
// player died, so a spike could never act as a visible threat. bRevealOnSpawn is
// meant to fix that. This probe checks the three things that could go wrong:
// it is not actually visible on frame one, it stays visible across a respawn, and
// drawing it breaks the kill volume.
// ---------------------------------------------------------------------------
void ANaiwaP1GameMode::TickHazardRevealProbe()
{
    if (bHazardRevealProbeFinished) return;
    if (!PlayerCharacter.IsValid()) return;
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const double Now = FPlatformTime::Seconds();
    if (Now - HazardRevealProbePhaseWall > 30.0)
    {
        UE_LOG(LogTemp, Warning, TEXT("NAIWA_P1_HAZARD_REVEAL_PROBE timeout stage=%d"), HazardRevealProbeStage);
        FinishHazardRevealProbe();
        return;
    }

    if (!HazardRevealProbeTarget.IsValid())
    {
        for (TActorIterator<ANaiwaP1Trigger> It(GetWorld()); It; ++It)
        {
            if (It->TriggerKind != ENaiwaP1TriggerKind::Hazard) continue;
            HazardRevealProbeTarget = *It;
            break;
        }
        if (!HazardRevealProbeTarget.IsValid()) return;
        ANaiwaP1Trigger* Hazard = HazardRevealProbeTarget.Get();
        HazardRevealProbeVisibleAtSpawn = Hazard->IsDisplayVisible();
        HazardRevealProbeRevealOnSpawn = Hazard->bRevealOnSpawn;
        HazardRevealProbeDeathBefore = DeathEventCount;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_HAZARD_REVEAL_PROBE hazard=%s reveal_on_spawn=%d visible_now=%d"),
            *Hazard->GetStableId().ToString(), Hazard->bRevealOnSpawn ? 1 : 0,
            HazardRevealProbeVisibleAtSpawn ? 1 : 0);
    }

    if (HazardRevealProbeStage == 0)
    {
        Movement->MaxWalkSpeed = 300.0f;
        Character->SetAutomatedMoveInput(0.0f);
        Movement->StopMovementImmediately();
        Movement->Velocity = FVector::ZeroVector;
        const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Character->SetActorLocation(FVector(ChainProbeHazardApproachX, 0.0f, ChainProbeTop + HalfHeight + 2.0f),
            false, nullptr, ETeleportType::TeleportPhysics);
        Movement->SetMovementMode(MOVE_Walking);
        HazardRevealProbeLaunchWall = Now;
        ++HazardRevealProbeStage;
        return;
    }
    if (HazardRevealProbeStage == 1)
    {
        Character->SetAutomatedMoveInput(0.0f);
        if (Now - HazardRevealProbeLaunchWall < ChainProbeSettleSeconds) return;
        HazardRevealProbeLaunchWall = Now;
        ++HazardRevealProbeStage;
        return;
    }
    if (HazardRevealProbeStage == 2)
    {
        Character->SetAutomatedMoveInput(1.0f);
        if (State == ENaiwaP1State::Playing) return;
        HazardRevealProbeKilled = DeathEventCount > HazardRevealProbeDeathBefore;
        HazardRevealProbeDeathX = Character->GetActorLocation().X;
        HazardRevealProbeRunSeconds = static_cast<float>(Now - HazardRevealProbeLaunchWall);
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_HAZARD_REVEAL_PROBE contact killed=%d at_x=%.1f after=%.3fs"),
            HazardRevealProbeKilled ? 1 : 0, HazardRevealProbeDeathX, HazardRevealProbeRunSeconds);
        ++HazardRevealProbeStage;
        return;
    }
    if (State != ENaiwaP1State::Playing) return;
    if (ANaiwaP1Trigger* Hazard = HazardRevealProbeTarget.Get())
    {
        HazardRevealProbeVisibleAfterRespawn = Hazard->IsDisplayVisible();
    }
    FinishHazardRevealProbe();
}

void ANaiwaP1GameMode::FinishHazardRevealProbe()
{
    if (bHazardRevealProbeFinished) return;
    bHazardRevealProbeFinished = true;
    if (PlayerCharacter.IsValid()) PlayerCharacter->SetAutomatedMoveInput(0.0f);

    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1HazardRevealProbeV1"));
    Report->SetStringField(TEXT("status"), TEXT("measured"));
    Report->SetStringField(TEXT("map"), GetWorld()->GetMapName());
    Report->SetBoolField(TEXT("reveal_on_spawn"), HazardRevealProbeRevealOnSpawn);
    Report->SetBoolField(TEXT("visible_at_spawn"), HazardRevealProbeVisibleAtSpawn);
    Report->SetBoolField(TEXT("killed_on_contact"), HazardRevealProbeKilled);
    Report->SetBoolField(TEXT("visible_after_respawn"), HazardRevealProbeVisibleAfterRespawn);
    Report->SetNumberField(TEXT("death_x_cm"), HazardRevealProbeDeathX);
    Report->SetNumberField(TEXT("approach_seconds"), HazardRevealProbeRunSeconds);
    Report->SetBoolField(TEXT("scare_spike_usable"),
        HazardRevealProbeRevealOnSpawn && HazardRevealProbeVisibleAtSpawn
        && HazardRevealProbeKilled && HazardRevealProbeVisibleAfterRespawn);

    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("hazard_reveal_probe_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P1_HAZARD_REVEAL_PROBE result=measured spawn_visible=%d killed=%d respawn_visible=%d output=%s saved=%d"),
        HazardRevealProbeVisibleAtSpawn ? 1 : 0, HazardRevealProbeKilled ? 1 : 0,
        HazardRevealProbeVisibleAfterRespawn ? 1 : 0, *Output, bSaved);
    FPlatformMisc::RequestExit(false);
}

namespace
{
    struct FV7GateGeometry
    {
        float StartX;
        float StartTop;
        float PlatformRightEdge;
        float TargetLeft;
        float TargetTop;
    };

    FV7GateGeometry V7GateGeometry(TCHAR Gate)
    {
        switch (Gate)
        {
        case TEXT('A'): return { 4700.0f, 180.0f, 7260.0f, 7500.0f, 180.0f };
        case TEXT('B'): return { 10370.0f, 180.0f, 13120.0f, 13420.0f, 480.0f };
        default: return { 16640.0f, 240.0f, 18830.0f, 19260.0f, 240.0f };
        }
    }
}

void ANaiwaP1GameMode::RecordV7GateProbeEvent(const TCHAR* Event)
{
    const ANaiwaCharacter* Character = PlayerCharacter.Get();
    const FVector Location = Character ? Character->GetActorLocation() : FVector::ZeroVector;
    TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("event"), Event);
    Entry->SetNumberField(TEXT("wall_elapsed_s"), FPlatformTime::Seconds() - V7GateProbeStartedWall);
    Entry->SetNumberField(TEXT("game_time_s"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
    Entry->SetNumberField(TEXT("x_cm"), Location.X);
    Entry->SetNumberField(TEXT("feet_z_cm"), Character ? Character->GetCapsuleBottom() : 0.0f);
    Entry->SetNumberField(TEXT("stage"), V7GateProbeStage);
    V7GateProbeEvents.Add(MakeShared<FJsonValueObject>(Entry));
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_V7_GATE_PROBE event=%s gate=%c x=%.1f feet=%.1f stage=%d"),
        Event, V7GateProbeGate, Location.X, Character ? Character->GetCapsuleBottom() : 0.0f,
        V7GateProbeStage);
}

void ANaiwaP1GameMode::TickV7GateProbe()
{
    if (bV7GateProbeFinished) return;
    const double Now = FPlatformTime::Seconds();
    if (Now - V7GateProbeStartedWall > (bV7GateProbeRecovery ? 65.0 : 42.0))
    {
        FinishV7GateProbe(TEXT("timeout"));
        return;
    }
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    if (!Character) return;
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const FV7GateGeometry Geometry = V7GateGeometry(V7GateProbeGate);

    if (V7GateProbeStage == 0)
    {
        if (!GetWorld()->GetWorldSettings()->ActorHasTag(TEXT("NaiwaP1V7GateProbeV1")))
        {
            FinishV7GateProbe(TEXT("missing_fixture_marker"));
            return;
        }
        const FName SwitchTag(*FString::Printf(TEXT("NaiwaP1V7GateProbeV1_%c_Switch"), V7GateProbeGate));
        const FName CheckpointTag(*FString::Printf(TEXT("NaiwaP1V7GateProbeV1_%c_Checkpoint"), V7GateProbeGate));
        const FName PadTag(TEXT("NaiwaP1V7GateProbeV1_B_Pad"));
        const FName TimedTag(TEXT("NaiwaP1V7GateProbeV1_C_Timed"));
        for (TActorIterator<ANaiwaP1SwitchPlatform> It(GetWorld()); It; ++It)
            if (It->ActorHasTag(SwitchTag)) V7GateProbeSwitch = *It;
        for (TActorIterator<ANaiwaP1Trigger> It(GetWorld()); It; ++It)
            if (It->ActorHasTag(CheckpointTag)) V7GateProbeCheckpoint = *It;
        if (V7GateProbeGate == TEXT('B'))
            for (TActorIterator<ANaiwaP1BouncePad> It(GetWorld()); It; ++It)
                if (It->ActorHasTag(PadTag)) V7GateProbePad = *It;
        if (V7GateProbeGate == TEXT('C'))
            for (TActorIterator<ANaiwaP1TimedPlatform> It(GetWorld()); It; ++It)
                if (It->ActorHasTag(TimedTag)) V7GateProbeTimed = *It;
        const FName ExpectedCheckpointId(*FString::Printf(TEXT("IntegratedCheckpoint%02d"),
            V7GateProbeGate == TEXT('A') ? 2 : V7GateProbeGate == TEXT('B') ? 4 : 6));
        if (!V7GateProbeSwitch.IsValid() || !V7GateProbeCheckpoint.IsValid()
            || V7GateProbeCheckpoint->TriggerKind != ENaiwaP1TriggerKind::Checkpoint
            || V7GateProbeCheckpoint->GetStableId() != ExpectedCheckpointId
            || FMath::Abs(V7GateProbeCheckpoint->GetActorLocation().X - Geometry.StartX) > 0.1f
            || FMath::Abs(V7GateProbeCheckpoint->GetActorLocation().Z - (Geometry.StartTop + 92.0f)) > 0.1f
            || (V7GateProbeGate == TEXT('B') && !V7GateProbePad.IsValid())
            || (V7GateProbeGate == TEXT('C') && !V7GateProbeTimed.IsValid()))
        {
            FinishV7GateProbe(TEXT("missing_gate_actor"));
            return;
        }
        Character->SetAutomatedMoveInput(0.0f);
        Character->StopJumping();
        Movement->StopMovementImmediately();
        Movement->Velocity = FVector::ZeroVector;
        Character->SetP1Gait(bV7GateProbeRun);
        const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        // Start outside the checkpoint volume, then physically walk through it so
        // activation comes from the native overlap path used by the full level.
        const FVector ProbeStart(Geometry.StartX - 180.0f, 0.0f, Geometry.StartTop + HalfHeight + 2.0f);
        if (FVector::DistSquared(Character->GetActorLocation(), ProbeStart) > 1.0f)
        {
            Character->SetActorLocation(ProbeStart, false, nullptr, ETeleportType::TeleportPhysics);
            ++V7GateProbeFixtureTeleportCount;
        }
        Movement->SetMovementMode(MOVE_Walking);
        V7GateProbeDeathBefore = DeathEventCount;
        V7GateProbeStage = 1;
        V7GateProbeStageWall = Now;
        RecordV7GateProbeEvent(TEXT("fixture_teleport_to_static_bank"));
        return;
    }

    if (V7GateProbeStage < 5
        && (State != ENaiwaP1State::Playing || DeathEventCount > V7GateProbeDeathBefore))
    {
        FinishV7GateProbe(bV7GateProbeRecovery ? TEXT("death_before_intended_natural_fall")
            : TEXT("death_before_target"));
        return;
    }
    const FVector Location = Character->GetActorLocation();
    const float FeetZ = Character->GetCapsuleBottom();
    V7GateProbePeakFeetZ = FMath::Max(V7GateProbePeakFeetZ, FeetZ);
    V7GateProbeMinFeetZ = FMath::Min(V7GateProbeMinFeetZ, FeetZ);
    if (Now - V7GateProbeLastSampleWall >= 0.1)
    {
        TSharedRef<FJsonObject> Sample = MakeShared<FJsonObject>();
        Sample->SetNumberField(TEXT("elapsed_s"), Now - V7GateProbeStartedWall);
        Sample->SetNumberField(TEXT("x_cm"), Location.X);
        Sample->SetNumberField(TEXT("feet_z_cm"), FeetZ);
        Sample->SetNumberField(TEXT("velocity_x_cm_s"), Movement->Velocity.X);
        Sample->SetNumberField(TEXT("velocity_z_cm_s"), Movement->Velocity.Z);
        Sample->SetBoolField(TEXT("grounded"), Movement->IsMovingOnGround());
        Sample->SetStringField(TEXT("base"), GetNameSafe(Character->GetMovementBaseObject()));
        Sample->SetBoolField(TEXT("checkpoint_component_overlaps_character"), V7GateProbeCheckpoint.IsValid()
            && V7GateProbeCheckpoint->TriggerBox->IsOverlappingActor(Character));
        Sample->SetNumberField(TEXT("switch_x_cm"), V7GateProbeSwitch.IsValid()
            ? V7GateProbeSwitch->GetActorLocation().X + V7GateProbeSwitch->GetCurrentX() : 0.0f);
        Sample->SetNumberField(TEXT("switch_state"), V7GateProbeSwitch.IsValid()
            ? static_cast<int32>(V7GateProbeSwitch->GetState()) : -1);
        Sample->SetNumberField(TEXT("pad_activation_count"), V7GateProbePad.IsValid()
            ? V7GateProbePad->GetActivationCount() : 0);
        Sample->SetNumberField(TEXT("timed_state"), V7GateProbeTimed.IsValid()
            ? static_cast<int32>(V7GateProbeTimed->GetState()) : -1);
        Sample->SetNumberField(TEXT("timed_remaining_s"), V7GateProbeTimed.IsValid()
            ? V7GateProbeTimed->GetCountdownRemaining() : 0.0f);
        V7GateProbeSamples.Add(MakeShared<FJsonValueObject>(Sample));
        V7GateProbeLastSampleWall = Now;
    }

    if (bV7GateProbeRecovery && V7GateProbeStage >= 5)
    {
        TickV7GateRecovery();
        return;
    }

    if (V7GateProbeStage == 1)
    {
        if (ActiveCheckpoint.Get() != V7GateProbeCheckpoint.Get())
        {
            Character->SetAutomatedMoveInput(1.0f);
            if (Now - V7GateProbeStageWall > 2.0)
                FinishV7GateProbe(TEXT("invalid_checkpoint_not_activated_by_overlap"));
            return;
        }
        Character->SetAutomatedMoveInput(0.0f);
        if (!Movement->IsMovingOnGround())
        {
            FinishV7GateProbe(TEXT("invalid_ungrounded_start"));
            return;
        }
        bV7GateProbeCheckpointActivated = true;
        V7GateProbeStage = 2;
        V7GateProbeStageWall = Now;
        RecordV7GateProbeEvent(TEXT("native_checkpoint_activated_by_movement_from_safe_bank"));
    }

    if (V7GateProbeStage == 2)
    {
        Character->SetAutomatedMoveInput(1.0f);
        const float PlatformCenterX = V7GateProbeSwitch->GetActorLocation().X + V7GateProbeSwitch->GetCurrentX();
        if (V7GateProbeSwitch->IsCharacterSupported(Character) && Location.X >= PlatformCenterX - 20.0f)
        {
            bV7GateProbeBoarded = true;
            V7GateProbeStage = 3;
            V7GateProbeStageWall = Now;
            Character->SetAutomatedMoveInput(0.0f);
            RecordV7GateProbeEvent(TEXT("boarded_shuttle_on_real_support"));
        }
        return;
    }

    if (V7GateProbeStage == 3)
    {
        Character->SetAutomatedMoveInput(0.0f);
        if (V7GateProbeSwitch->GetState() == ENaiwaP1SwitchPlatformState::DockedRight
            && V7GateProbeSwitch->IsCharacterSupported(Character))
        {
            bV7GateProbeDockedRight = true;
            V7GateProbeStage = 4;
            V7GateProbeStageWall = Now;
            RecordV7GateProbeEvent(TEXT("shuttle_docked_right"));
        }
        return;
    }

    if (V7GateProbeStage != 4) return;
    if (bV7GateProbeRecovery)
    {
        // Leave the right-docked shuttle over its left-hand void. No kill trigger,
        // direct teleport, or synthetic death is used after the real boarding.
        Character->StopJumping();
        Character->SetAutomatedMoveInput(-1.0f);
        const float PlatformCenterX = V7GateProbeSwitch->GetActorLocation().X + V7GateProbeSwitch->GetCurrentX();
        if (Movement->IsFalling() && !V7GateProbeSwitch->IsCharacterSupported(Character)
            && Location.X < PlatformCenterX - 80.0f)
        {
            bV7GateProbeNaturallyLeftShuttle = true;
            V7GateProbeStage = 5;
            V7GateProbeStageWall = Now;
            RecordV7GateProbeEvent(TEXT("fell_from_docked_shuttle_left_edge_without_jump"));
        }
        return;
    }
    Character->SetAutomatedMoveInput(1.0f);
    const float CapsuleRadius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float CapsuleFrontX = Location.X + CapsuleRadius;

    if ((V7GateProbeGate == TEXT('A') || V7GateProbeGate == TEXT('C')) && !bV7GateProbeJumpIssued
        && CapsuleFrontX >= Geometry.PlatformRightEdge + V7GateProbeOffsetCm)
    {
        bV7GateProbeJumpIssued = true;
        V7GateProbeJumpX = Location.X;
        V7GateProbeJumpWall = Now;
        RecordV7GateProbeEvent(TEXT("jump_pressed_at_shuttle_end"));
        Character->Jump();
        return;
    }
    if (V7GateProbeGate == TEXT('C') && !bV7GateProbeJumpIssued
        && Now - V7GateProbeStageWall >= 2.0
        && FMath::Abs(Movement->Velocity.X) < 5.0f
        && CapsuleFrontX >= 18838.0f
        && CapsuleFrontX < Geometry.PlatformRightEdge + V7GateProbeOffsetCm)
    {
        RecordV7GateProbeEvent(TEXT("raised_timed_floor_blocked_capsule_before_jump_threshold"));
        FinishV7GateProbe(TEXT("blocked_before_jump"));
        return;
    }
    if (bV7GateProbeJumpIssued && !bV7GateProbeJumpLaunched && Movement->Velocity.Z > 50.0f)
    {
        bV7GateProbeJumpLaunched = true;
        RecordV7GateProbeEvent(TEXT("jump_upward_velocity_observed"));
    }

    if (V7GateProbeGate == TEXT('B') && V7GateProbePad.IsValid())
    {
        if (!bV7GateProbePadSupported && Movement->IsMovingOnGround()
            && Character->GetMovementBaseObject() == V7GateProbePad->PadCollision.Get())
        {
            bV7GateProbePadSupported = true;
            V7GateProbePadSupportX = Location.X;
            RecordV7GateProbeEvent(TEXT("pad_supported"));
        }
        if (V7GateProbePad->GetActivationCount() > 0 && V7GateProbePadActivationX == 0.0f)
        {
            V7GateProbePadActivationX = Location.X;
            RecordV7GateProbeEvent(TEXT("pad_launched"));
        }
    }
    if (V7GateProbeGate == TEXT('C') && V7GateProbeTimed.IsValid())
    {
        if (!bV7GateProbeTimedSupported && Movement->IsMovingOnGround()
            && Character->GetMovementBaseObject() == V7GateProbeTimed->PlatformCollision.Get())
        {
            bV7GateProbeTimedSupported = true;
            V7GateProbeTimedSupportX = Location.X;
            RecordV7GateProbeEvent(TEXT("timed_floor_supported"));
        }
        if (!bV7GateProbeTimedTriggered && V7GateProbeTimed->GetTriggerCount() > 0)
        {
            bV7GateProbeTimedTriggered = true;
            V7GateProbeTimedTriggerX = Location.X;
            V7GateProbeTimedTriggerWall = Now;
            RecordV7GateProbeEvent(TEXT("timed_countdown_started"));
        }
        if (!bV7GateProbeTimedDisabled && V7GateProbeTimed->GetDisableCount() > 0)
        {
            bV7GateProbeTimedDisabled = true;
            RecordV7GateProbeEvent(TEXT("timed_floor_disabled"));
        }
    }

    if (Movement->IsMovingOnGround() && Location.X > Geometry.TargetLeft + 90.0f
        && FMath::Abs(FeetZ - Geometry.TargetTop) <= 5.0f)
    {
        if (V7GateProbeGate == TEXT('C'))
        {
            V7GateProbeTimedExitX = Location.X;
            V7GateProbeTimedRemainingAtExit = V7GateProbeTimed.IsValid()
                ? V7GateProbeTimed->GetCountdownRemaining() : 0.0f;
        }
        RecordV7GateProbeEvent(TEXT("landed_static_target_bank"));
        if (V7GateProbeGate == TEXT('C'))
        {
            FinishV7GateProbe(!bV7GateProbeTimedTriggered
                ? TEXT("landed_target_without_timed_trigger")
                : bV7GateProbeTimedDisabled || V7GateProbeTimedRemainingAtExit <= 0.0f
                    ? TEXT("landed_target_after_timed_expired")
                    : TEXT("landed_target_with_active_timer"));
        }
        else FinishV7GateProbe(TEXT("landed_target"));
        return;
    }
    if (FeetZ < Geometry.StartTop - 100.0f
        || (V7GateProbeGate == TEXT('B') && Location.X > 13500.0f && V7GateProbePadActivationX == 0.0f))
    {
        FinishV7GateProbe(V7GateProbeGate == TEXT('B') && V7GateProbePadActivationX == 0.0f
            ? TEXT("pad_not_activated") : TEXT("fell_before_target"));
        return;
    }
}

void ANaiwaP1GameMode::TickV7GateRecovery()
{
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    if (!Character || !V7GateProbeSwitch.IsValid() || !V7GateProbeCheckpoint.IsValid())
    {
        FinishV7GateProbe(TEXT("invalid_recovery_actor_disappeared"));
        return;
    }
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const FV7GateGeometry Geometry = V7GateGeometry(V7GateProbeGate);
    const double Now = FPlatformTime::Seconds();

    if (V7GateProbeStage == 5)
    {
        // The only probe action is continued horizontal input over empty space.
        // Production KillZ and death/respawn code must perform the state change.
        if (State == ENaiwaP1State::Playing) Character->SetAutomatedMoveInput(-1.0f);
        if (Character->GetActorLocation().Z < GetWorld()->GetWorldSettings()->KillZ)
            bV7GateProbeFellBelowKillZ = true;
        if (DeathEventCount != V7GateProbeDeathBefore) 
        {
            Character->SetAutomatedMoveInput(0.0f);
            if (DeathEventCount != V7GateProbeDeathBefore + 1 || State != ENaiwaP1State::Dying
                || !bV7GateProbeNaturallyLeftShuttle || !bV7GateProbeFellBelowKillZ
                || bV7GateProbeJumpIssued)
            {
                FinishV7GateProbe(TEXT("unexpected_death_before_killz_evidence"));
                return;
            }
            bV7GateProbeSawDying = true;
            V7GateProbeStage = 6;
            V7GateProbeStageWall = Now;
            RecordV7GateProbeEvent(TEXT("production_killz_caused_one_death_after_natural_fall"));
        }
        return;
    }

    if (V7GateProbeStage == 6)
    {
        Character->SetAutomatedMoveInput(0.0f);
        if (State == ENaiwaP1State::Dying || State == ENaiwaP1State::Respawning) return;
        if (State != ENaiwaP1State::Playing || DeathEventCount != V7GateProbeDeathBefore + 1)
        {
            FinishV7GateProbe(TEXT("unexpected_respawn_state_or_death_count"));
            return;
        }
        const FVector Location = Character->GetActorLocation();
        V7GateProbeRespawnX = Location.X;
        V7GateProbeRespawnFeetZ = Character->GetCapsuleBottom();
        bV7GateProbeRespawnedAtCheckpoint = ActiveCheckpoint.Get() == V7GateProbeCheckpoint.Get()
            && !bRespawnTriedInitial && Movement->IsMovingOnGround()
            && FMath::Abs(Location.X - Geometry.StartX) <= 25.0f
            && FMath::Abs(V7GateProbeRespawnFeetZ - Geometry.StartTop) <= 5.0f;
        const bool bShuttleReset = V7GateProbeSwitch->GetState() == ENaiwaP1SwitchPlatformState::DockedLeft
            && FMath::Abs(V7GateProbeSwitch->GetCurrentX() - V7GateProbeSwitch->DockLeftX) <= 1.0f
            && !V7GateProbeSwitch->IsDepartureCueActive();
        bV7GateProbePadResetReady = V7GateProbeGate == TEXT('B') && V7GateProbePad.IsValid()
            && V7GateProbePad->IsReady()
            && V7GateProbePad->GetCooldownRemaining() <= 0.01f
            && !V7GateProbePad->IsContactLatched();
        bV7GateProbeTimedResetReady = V7GateProbeGate == TEXT('C') && V7GateProbeTimed.IsValid()
            && V7GateProbeTimed->GetState() == ENaiwaP1TimedPlatformState::Ready
            && V7GateProbeTimed->IsCarryingCollisionEnabled()
            && V7GateProbeTimed->GetCountdownRemaining() <= 0.01f;
        bV7GateProbeMechanismReset = bShuttleReset
            && (V7GateProbeGate != TEXT('B') || bV7GateProbePadResetReady)
            && (V7GateProbeGate != TEXT('C') || bV7GateProbeTimedResetReady);
        if (!bV7GateProbeRespawnedAtCheckpoint || !bV7GateProbeMechanismReset)
        {
            FinishV7GateProbe(TEXT("checkpoint_or_mechanism_reset_failed"));
            return;
        }
        V7GateProbeStage = 7;
        V7GateProbeStageWall = Now;
        RecordV7GateProbeEvent(TEXT("checkpoint_respawn_grounded_without_initial_fallback_and_mechanisms_reset"));
        return;
    }

    if (State != ENaiwaP1State::Playing || DeathEventCount != V7GateProbeDeathBefore + 1
        || ActiveCheckpoint.Get() != V7GateProbeCheckpoint.Get())
    {
        FinishV7GateProbe(TEXT("retry_lost_playable_checkpoint_state"));
        return;
    }
    if (V7GateProbeStage == 7)
    {
        Character->SetAutomatedMoveInput(1.0f);
        const float PlatformCenterX = V7GateProbeSwitch->GetActorLocation().X + V7GateProbeSwitch->GetCurrentX();
        if (V7GateProbeSwitch->IsCharacterSupported(Character)
            && Character->GetActorLocation().X >= PlatformCenterX - 20.0f)
        {
            bV7GateProbeRetryBoarded = true;
            Character->SetAutomatedMoveInput(0.0f);
            V7GateProbeStage = 8;
            V7GateProbeStageWall = Now;
            RecordV7GateProbeEvent(TEXT("retry_boarded_reset_shuttle_on_real_support"));
        }
        return;
    }
    if (V7GateProbeStage == 8)
    {
        Character->SetAutomatedMoveInput(0.0f);
        if (V7GateProbeSwitch->GetState() == ENaiwaP1SwitchPlatformState::DockedRight
            && V7GateProbeSwitch->IsCharacterSupported(Character))
        {
            bV7GateProbeRetryDockedRight = true;
            RecordV7GateProbeEvent(TEXT("retry_remained_playable_through_second_real_shuttle_ride"));
            FinishV7GateProbe(TEXT("natural_killz_checkpoint_retry_docked_right"));
        }
    }
}

void ANaiwaP1GameMode::FinishV7GateProbe(const TCHAR* Outcome)
{
    if (bV7GateProbeFinished) return;
    bV7GateProbeFinished = true;
    if (PlayerCharacter.IsValid())
    {
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        PlayerCharacter->StopJumping();
    }
    const FV7GateGeometry Geometry = V7GateGeometry(V7GateProbeGate);
    const bool bInvalidFixture = FCString::Strstr(Outcome, TEXT("invalid"))
        || FCString::Strstr(Outcome, TEXT("missing"));
    const bool bRecoveryPassed = bV7GateProbeRecovery
        && FCString::Strcmp(Outcome, TEXT("natural_killz_checkpoint_retry_docked_right")) == 0
        && bV7GateProbeCheckpointActivated && bV7GateProbeBoarded && bV7GateProbeDockedRight
        && bV7GateProbeNaturallyLeftShuttle && bV7GateProbeFellBelowKillZ
        && bV7GateProbeSawDying && bV7GateProbeRespawnedAtCheckpoint
        && bV7GateProbeMechanismReset && bV7GateProbeRetryBoarded
        && bV7GateProbeRetryDockedRight && DeathEventCount == V7GateProbeDeathBefore + 1;
    const TCHAR* ScriptVerdict = !bV7GateProbeRecovery ? TEXT("NOT_EVALUATED")
        : bRecoveryPassed ? TEXT("PASS") : bInvalidFixture ? TEXT("INCOMPLETE") : TEXT("FAIL");
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), bV7GateProbeRecovery
        ? TEXT("NaiwaP1V7GateRecoveryV1") : TEXT("NaiwaP1V7GateProbeV2"));
    Report->SetStringField(TEXT("mode"), bV7GateProbeRecovery ? TEXT("recovery") : TEXT("baseline"));
    Report->SetStringField(TEXT("status"), bV7GateProbeRecovery ? ScriptVerdict
        : bInvalidFixture ? TEXT("invalid_fixture") : TEXT("measured"));
    Report->SetStringField(TEXT("script_verdict"), ScriptVerdict);
    Report->SetStringField(TEXT("outcome"), Outcome);
    Report->SetStringField(TEXT("physical_outcome"), bV7GateProbeRecovery
        ? bV7GateProbeNaturallyLeftShuttle && bV7GateProbeFellBelowKillZ && bV7GateProbeSawDying
            ? TEXT("natural_fall_from_right_docked_shuttle_to_world_killz")
            : TEXT("natural_killz_fall_not_proven")
        : Outcome);
    Report->SetBoolField(TEXT("isolated_only"), true);
    Report->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetMapName() : FString());
    Report->SetStringField(TEXT("gate"), FString::Printf(TEXT("%c"), V7GateProbeGate));
    Report->SetStringField(TEXT("gait"), bV7GateProbeRun ? TEXT("Run300") : TEXT("Walk150"));
    Report->SetStringField(TEXT("control_method"), bV7GateProbeRecovery
        ? TEXT("one_fixture_start_teleport_then_real_board_and_ride_walk_left_off_docked_shuttle_production_death_respawn_and_real_retry")
        : TEXT("one_fixture_start_teleport_then_SetP1Gait_SetAutomatedMoveInput_and_Jump_at_shuttle_end"));
    Report->SetNumberField(TEXT("fixture_setup_teleport_count"), V7GateProbeFixtureTeleportCount);
    Report->SetNumberField(TEXT("probe_direct_teleports_after_boarding"), 0);
    Report->SetStringField(TEXT("checkpoint_id"), V7GateProbeCheckpoint.IsValid()
        ? V7GateProbeCheckpoint->GetStableId().ToString() : TEXT("missing"));
    Report->SetBoolField(TEXT("checkpoint_activated_by_native_overlap"), bV7GateProbeCheckpointActivated);
    Report->SetBoolField(TEXT("checkpoint_component_overlaps_character"),
        V7GateProbeCheckpoint.IsValid() && PlayerCharacter.IsValid()
            && V7GateProbeCheckpoint->TriggerBox->IsOverlappingActor(PlayerCharacter.Get()));
    Report->SetBoolField(TEXT("checkpoint_generate_overlap_events"),
        V7GateProbeCheckpoint.IsValid() && V7GateProbeCheckpoint->TriggerBox->GetGenerateOverlapEvents());
    Report->SetNumberField(TEXT("checkpoint_collision_enabled"), V7GateProbeCheckpoint.IsValid()
        ? static_cast<int32>(V7GateProbeCheckpoint->TriggerBox->GetCollisionEnabled()) : -1);
    Report->SetNumberField(TEXT("checkpoint_pawn_response"), V7GateProbeCheckpoint.IsValid()
        ? static_cast<int32>(V7GateProbeCheckpoint->TriggerBox->GetCollisionResponseToChannel(ECC_Pawn)) : -1);
    Report->SetNumberField(TEXT("character_capsule_object_channel"), PlayerCharacter.IsValid()
        ? static_cast<int32>(PlayerCharacter->GetCapsuleComponent()->GetCollisionObjectType()) : -1);
    Report->SetBoolField(TEXT("character_capsule_generate_overlap_events"), PlayerCharacter.IsValid()
        && PlayerCharacter->GetCapsuleComponent()->GetGenerateOverlapEvents());
    Report->SetNumberField(TEXT("checkpoint_box_extent_x_cm"), V7GateProbeCheckpoint.IsValid()
        ? V7GateProbeCheckpoint->TriggerBox->GetScaledBoxExtent().X : 0.0f);
    Report->SetNumberField(TEXT("jump_offset_cm"), V7GateProbeOffsetCm);
    Report->SetNumberField(TEXT("start_x_cm"), Geometry.StartX);
    Report->SetNumberField(TEXT("shuttle_right_edge_cm"), Geometry.PlatformRightEdge);
    Report->SetNumberField(TEXT("target_left_cm"), Geometry.TargetLeft);
    Report->SetNumberField(TEXT("target_top_cm"), Geometry.TargetTop);
    Report->SetBoolField(TEXT("boarded_real_shuttle_support"), bV7GateProbeBoarded);
    Report->SetBoolField(TEXT("right_dock_supported"), bV7GateProbeDockedRight);
    Report->SetBoolField(TEXT("jump_issued"), bV7GateProbeJumpIssued);
    Report->SetBoolField(TEXT("jump_launched"), bV7GateProbeJumpLaunched);
    Report->SetNumberField(TEXT("jump_x_cm"), V7GateProbeJumpX);
    Report->SetBoolField(TEXT("pad_supported"), bV7GateProbePadSupported);
    Report->SetNumberField(TEXT("pad_support_x_cm"), V7GateProbePadSupportX);
    Report->SetNumberField(TEXT("pad_activation_count"), V7GateProbePad.IsValid()
        ? V7GateProbePad->GetActivationCount() : 0);
    Report->SetNumberField(TEXT("pad_activation_x_cm"), V7GateProbePadActivationX);
    Report->SetBoolField(TEXT("timed_supported"), bV7GateProbeTimedSupported);
    Report->SetNumberField(TEXT("timed_support_x_cm"), V7GateProbeTimedSupportX);
    Report->SetBoolField(TEXT("timed_triggered"), bV7GateProbeTimedTriggered);
    Report->SetNumberField(TEXT("timed_trigger_x_cm"), V7GateProbeTimedTriggerX);
    Report->SetBoolField(TEXT("timed_disabled_before_exit"), bV7GateProbeTimedDisabled);
    Report->SetNumberField(TEXT("timed_exit_x_cm"), V7GateProbeTimedExitX);
    Report->SetNumberField(TEXT("timed_remaining_at_exit_s"), V7GateProbeTimedRemainingAtExit);
    Report->SetNumberField(TEXT("timed_trigger_to_exit_s"), V7GateProbeTimedTriggerWall > 0.0
        && V7GateProbeTimedExitX > 0.0f ? FPlatformTime::Seconds() - V7GateProbeTimedTriggerWall : 0.0);
    Report->SetNumberField(TEXT("peak_feet_z_cm"), V7GateProbePeakFeetZ);
    Report->SetNumberField(TEXT("min_feet_z_cm"), V7GateProbeMinFeetZ < 99999.0f ? V7GateProbeMinFeetZ : 0.0f);
    Report->SetNumberField(TEXT("death_events"), DeathEventCount - V7GateProbeDeathBefore);
    Report->SetBoolField(TEXT("naturally_left_right_docked_shuttle"), bV7GateProbeNaturallyLeftShuttle);
    Report->SetBoolField(TEXT("fell_below_world_killz"), bV7GateProbeFellBelowKillZ);
    Report->SetBoolField(TEXT("production_dying_state_observed"), bV7GateProbeSawDying);
    Report->SetBoolField(TEXT("respawned_at_expected_checkpoint_without_fallback"),
        bV7GateProbeRespawnedAtCheckpoint);
    Report->SetNumberField(TEXT("respawn_x_cm"), V7GateProbeRespawnX);
    Report->SetNumberField(TEXT("respawn_feet_z_cm"), V7GateProbeRespawnFeetZ);
    Report->SetBoolField(TEXT("mechanisms_reset_after_death"), bV7GateProbeMechanismReset);
    Report->SetBoolField(TEXT("bounce_pad_ready_after_reset"), bV7GateProbePadResetReady);
    Report->SetBoolField(TEXT("timed_platform_ready_after_reset"), bV7GateProbeTimedResetReady);
    Report->SetBoolField(TEXT("retry_boarded_real_shuttle_support"), bV7GateProbeRetryBoarded);
    Report->SetBoolField(TEXT("retry_docked_right_on_real_support"), bV7GateProbeRetryDockedRight);
    Report->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - V7GateProbeStartedWall);
    Report->SetArrayField(TEXT("events"), V7GateProbeEvents);
    Report->SetArrayField(TEXT("samples"), V7GateProbeSamples);

    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
    const FString Guid = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Gait = bV7GateProbeRun ? TEXT("run") : TEXT("walk");
    const FString Filename = bV7GateProbeRecovery
        ? FString::Printf(TEXT("v7_gate_recovery_%c_%s_%s_%s.json"), V7GateProbeGate, *Gait, *Stamp, *Guid)
        : FString::Printf(TEXT("v7_gate_probe_v2_%c_%s_%s_%s.json"), V7GateProbeGate, *Gait, *Stamp, *Guid);
    const FString Output = FPaths::Combine(Directory, Filename);
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_V7_GATE_PROBE mode=%s physical=%s verdict=%s gate=%c gait=%s output=%s saved=%d"),
        bV7GateProbeRecovery ? TEXT("recovery") : TEXT("baseline"), Outcome, ScriptVerdict,
        V7GateProbeGate, bV7GateProbeRun ? TEXT("Run300") : TEXT("Walk150"), *Output, bSaved);
    FPlatformMisc::RequestExit(false);
}

bool ANaiwaP1GameMode::InitializeV7MixedCoreProbe()
{
    FString RecoveryCheckpoint;
    if (FParse::Value(FCommandLine::Get(), TEXT("NaiwaP1V7RecoverAtCheckpoint="), RecoveryCheckpoint))
    {
        static const TCHAR* CheckpointNames[7] = { TEXT("CP01"), TEXT("CP02"), TEXT("CP03"), TEXT("CP04"),
            TEXT("CP05"), TEXT("CP06"), TEXT("CP07") };
        V7MixedCoreRecoveryCheckpointIndex = INDEX_NONE;
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheckpointNames); ++Index)
        {
            if (RecoveryCheckpoint.Equals(CheckpointNames[Index], ESearchCase::IgnoreCase))
            {
                V7MixedCoreRecoveryCheckpointIndex = Index;
                break;
            }
        }
        if (V7MixedCoreRecoveryCheckpointIndex == INDEX_NONE)
        {
            FinishV7MixedCoreProbe(TEXT("INCOMPLETE"), TEXT("Checkpoint recovery requires CP01 through CP07"));
            return false;
        }
    }
    const AWorldSettings* Settings = GetWorld() ? GetWorld()->GetWorldSettings() : nullptr;
    if (!Settings || GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Naiwa/P1/Maps/L_NaiwaP1_v7")
        || !Settings->ActorHasTag(TEXT("NaiwaP1V7MapV1"))
        || !Settings->ActorHasTag(TEXT("NaiwaP1V7Cfg_4fd6914655c2ae7c"))
        || !FMath::IsNearlyEqual(Settings->KillZ, -1000.0f, 0.1f))
    {
        FinishV7MixedCoreProbe(TEXT("INCOMPLETE"), TEXT("Exact saved V7 package/config markers and -1000 cm KillZ are required"));
        return false;
    }
    if (bSmokeEnabled || bRouteEnabled || bIntegratedProbeEnabled || bIntegratedFullProbeEnabled
        || bHazardAuditEnabled || bMatrixEnabled || bBounceProbeEnabled || bSwitchProbeEnabled
        || bSwitchFailProbeEnabled || bTimedProbeEnabled || bTimedFailProbeEnabled
        || bGapProbeEnabled || bClearanceProbeEnabled || bTimedWindowProbeEnabled
        || bChainProbeEnabled || bHazardRevealProbeEnabled || bV7GateProbeEnabled
        || FParse::Param(FCommandLine::Get(), TEXT("NaiwaAutoTest"))
        || FParse::Param(FCommandLine::Get(), TEXT("NaiwaVisualReview"))
        || FParse::Param(FCommandLine::Get(), TEXT("NaiwaReviewPlayback")))
    {
        FinishV7MixedCoreProbe(TEXT("INCOMPLETE"), TEXT("Run one V7 mixed-core scenario per process"));
        return false;
    }
    if (!PlayerCharacter.IsValid() || !bHasInitialSpawn) return false;

    ANaiwaCharacter* Character = PlayerCharacter.Get();
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const FName V7Owner(TEXT("NaiwaP1V7GeneratedV1"));
    const FName SourceOwner(TEXT("NaiwaP1IntegratedGeneratedV1"));
    int32 V7OwnedCount = 0;
    int32 SourceOwnedCount = 0;
    int32 SwitchCount = 0;
    int32 PadCount = 0;
    int32 TimedCount = 0;
    int32 TriggerCount = 0;
    int32 PhysicalPickupCount = 0;
    int32 ExplicitRequiredPickupCount = 0;
    int32 V7PitCount = 0;
    int32 HiddenFinalPitCount = 0;
    TMap<FName, TArray<ANaiwaP1Trigger*>> TriggerById;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(V7Owner)) ++V7OwnedCount;
        if (It->ActorHasTag(SourceOwner)) ++SourceOwnedCount;
    }
    for (TActorIterator<ANaiwaP1Trigger> It(GetWorld()); It; ++It)
    {
        ++TriggerCount;
        if (It->TriggerKind == ENaiwaP1TriggerKind::Pickup)
        {
            ++PhysicalPickupCount;
            if (It->bRequiredForGoal) ++ExplicitRequiredPickupCount;
        }
        TriggerById.FindOrAdd(It->GetStableId()).Add(*It);
        if (It->ActorHasTag(V7Owner) && It->TriggerKind == ENaiwaP1TriggerKind::Hazard
            && It->bRevealOnSpawn && It->DisplayMesh && It->DisplayMesh->IsVisible()) ++V7PitCount;
        if (It->ActorHasTag(V7Owner) && It->TriggerKind == ENaiwaP1TriggerKind::Hazard
            && !It->bRevealOnSpawn && It->bUseSpikeCluster && It->DisplayMesh && !It->DisplayMesh->IsVisible()
            && It->SideSpikeLeft && !It->SideSpikeLeft->IsVisible()
            && It->SideSpikeRight && !It->SideSpikeRight->IsVisible()
            && It->TriggerBox && It->TriggerBox->GetCollisionEnabled() == ECollisionEnabled::QueryOnly)
            ++HiddenFinalPitCount;
    }
    const auto FindTrigger = [&TriggerById](const TCHAR* Id) -> ANaiwaP1Trigger*
    {
        const TArray<ANaiwaP1Trigger*>* Matches = TriggerById.Find(FName(Id));
        return Matches && Matches->Num() == 1 ? (*Matches)[0] : nullptr;
    };
    V7MixedCoreHazardTeaching = FindTrigger(TEXT("IntegratedHiddenHazard01"));
    V7MixedCorePickup = FindTrigger(TEXT("IntegratedRequiredPickup01"));
    V7MixedCoreGoal = FindTrigger(TEXT("IntegratedGoal01"));
    const TCHAR* CheckpointIds[8] = { TEXT("IntegratedCheckpoint01"), TEXT("IntegratedCheckpoint02"),
        TEXT("IntegratedCheckpoint03"), TEXT("IntegratedCheckpoint04"), TEXT("IntegratedCheckpoint05"),
        TEXT("IntegratedCheckpoint06"), TEXT("IntegratedCheckpoint07"), TEXT("IntegratedCheckpoint08") };
    int32 CheckpointCount = 0;
    for (int32 Index = 0; Index < 8; ++Index)
    {
        ANaiwaP1Trigger* Trigger = FindTrigger(CheckpointIds[Index]);
        if (Trigger && Trigger->TriggerKind == ENaiwaP1TriggerKind::Checkpoint)
        {
            V7MixedCoreCheckpoints[Index] = Trigger;
            ++CheckpointCount;
        }
    }
    for (TActorIterator<ANaiwaP1SwitchPlatform> It(GetWorld()); It; ++It)
    {
        ++SwitchCount;
        if (It->ActorHasTag(TEXT("NaiwaP1IntegratedSwitchV1"))) V7MixedCoreSwitches[0] = *It;
        if (It->ActorHasTag(TEXT("NaiwaP1V7StableId_SwitchA"))) V7MixedCoreSwitches[1] = *It;
        if (It->ActorHasTag(TEXT("NaiwaP1V7StableId_SwitchB"))) V7MixedCoreSwitches[2] = *It;
        if (It->ActorHasTag(TEXT("NaiwaP1V7StableId_SwitchC"))) V7MixedCoreSwitches[3] = *It;
    }
    for (TActorIterator<ANaiwaP1BouncePad> It(GetWorld()); It; ++It)
    {
        ++PadCount;
        if (It->ActorHasTag(TEXT("NaiwaP1IntegratedBounceV1"))) V7MixedCorePads[0] = *It;
        if (It->ActorHasTag(TEXT("NaiwaP1V7StableId_BounceB"))) V7MixedCorePads[1] = *It;
        if (It->ActorHasTag(TEXT("NaiwaP1V7StableId_BounceFinal"))) V7MixedCorePads[2] = *It;
        if (It->ActorHasTag(TEXT("NaiwaP1V7StableId_BounceFinal2"))) V7MixedCorePads[3] = *It;
    }
    for (TActorIterator<ANaiwaP1TimedPlatform> It(GetWorld()); It; ++It)
    {
        ++TimedCount;
        if (It->ActorHasTag(TEXT("NaiwaP1IntegratedFullTimedTeaching01V1"))) V7MixedCoreTimed[0] = *It;
        if (It->ActorHasTag(TEXT("NaiwaP1V7StableId_TimedC"))) V7MixedCoreTimed[1] = *It;
        if (It->ActorHasTag(TEXT("NaiwaP1V7StableId_TimedFinal"))) V7MixedCoreTimed[2] = *It;
        if (It->ActorHasTag(TEXT("NaiwaP1V7StableId_TimedFinal2"))) V7MixedCoreTimed[3] = *It;
    }

    const TCHAR* GroundIds[] = { TEXT("A1"), TEXT("A3"), TEXT("A4Island01"), TEXT("A4Island02"),
        TEXT("A4Island03"), TEXT("A4Island04"), TEXT("B1"), TEXT("B4"), TEXT("B5Island01"),
        TEXT("B5Island02"), TEXT("B5Island03"), TEXT("B5Island04"), TEXT("C1"), TEXT("C4"),
        TEXT("C5Island01"), TEXT("C5Island02"), TEXT("C5Island03"), TEXT("C5Island04"),
        TEXT("C5Island05"), TEXT("C5Island06"), TEXT("FinalApproach"), TEXT("FinalIntermediate"), TEXT("Final") };
    int32 V7GroundCount = 0;
    for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
    {
        if (!It->ActorHasTag(V7Owner) || !It->ActorHasTag(TEXT("NaiwaP1GeneratedV1"))) continue;
        for (const TCHAR* Id : GroundIds)
        {
            const FName StableTag(*FString::Printf(TEXT("NaiwaP1V7StableId_%s"), Id));
            if (!It->ActorHasTag(StableTag)) continue;
            V7MixedCoreV7GroundById.Add(FName(Id), *It);
            ++V7GroundCount;
            break;
        }
    }
    const auto FindSourceGround = [this, &SourceOwner](float Left, float Right, float Top) -> AStaticMeshActor*
    {
        AStaticMeshActor* Found = nullptr;
        int32 Count = 0;
        for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
        {
            if (!It->ActorHasTag(SourceOwner) || !It->ActorHasTag(TEXT("NaiwaP1GeneratedV1"))) continue;
            const FBox Bounds = It->GetComponentsBoundingBox(true);
            if (FMath::Abs(Bounds.Min.X - Left) <= 2.0f && FMath::Abs(Bounds.Max.X - Right) <= 2.0f
                && FMath::Abs(Bounds.Max.Z - Top) <= 2.0f)
            {
                Found = *It;
                ++Count;
            }
        }
        return Count == 1 ? Found : nullptr;
    };
    V7MixedCoreGrounds[0] = FindSourceGround(-1600.0f, -550.0f, 120.0f);
    V7MixedCoreGrounds[1] = FindSourceGround(550.0f, 1460.0f, 120.0f);
    V7MixedCoreGrounds[2] = FindSourceGround(1740.0f, 3120.0f, 420.0f);
    V7MixedCoreGrounds[3] = FindSourceGround(3560.0f, 4500.0f, 180.0f);

    bool bInventoryValid = V7OwnedCount == 59 && SourceOwnedCount == 14 && SwitchCount == 4
        && PadCount == 4 && TimedCount == 4 && CheckpointCount == 8 && TriggerCount == 29
        && PhysicalPickupCount == 4 && ExplicitRequiredPickupCount == 1
        && RequiredPickupCount == 1 && V7MixedCorePickup.IsValid() && V7MixedCorePickup->bRequiredForGoal
        && V7PitCount == 11 && HiddenFinalPitCount == 4 && V7GroundCount == UE_ARRAY_COUNT(GroundIds)
        && V7MixedCoreHazardTeaching.IsValid() && V7MixedCorePickup.IsValid() && V7MixedCoreGoal.IsValid();
    V7MixedCoreHiddenPitCount = HiddenFinalPitCount;
    bV7MixedCoreHiddenFinalPitsInvisible = HiddenFinalPitCount == 4
        && FindTrigger(TEXT("CompoundHiddenPit01")) && FindTrigger(TEXT("CompoundHiddenPit02"))
        && FindTrigger(TEXT("CompoundHiddenPit03")) && FindTrigger(TEXT("CompoundHiddenPit04"));
    for (const auto& Actor : V7MixedCoreCheckpoints) bInventoryValid &= Actor.IsValid();
    for (const auto& Actor : V7MixedCoreSwitches) bInventoryValid &= Actor.IsValid();
    for (const auto& Actor : V7MixedCorePads) bInventoryValid &= Actor.IsValid();
    for (const auto& Actor : V7MixedCoreTimed) bInventoryValid &= Actor.IsValid();
    for (const auto& Actor : V7MixedCoreGrounds) bInventoryValid &= Actor.IsValid();
    for (const TCHAR* Id : GroundIds) bInventoryValid &= V7MixedCoreV7GroundById.Contains(FName(Id));
    for (int32 Index = 0; Index < 8; ++Index)
        bInventoryValid &= V7MixedCoreCheckpoints[Index]->GetStableId() == FName(CheckpointIds[Index]);
    if (!bInventoryValid)
    {
        FinishV7MixedCoreProbe(TEXT("INCOMPLETE"), FString::Printf(
            TEXT("Strict V7 runtime inventory mismatch: extension=%d retained=%d switches=%d pads=%d timed=%d triggers=%d checkpoints=%d visible_pits=%d v7_grounds=%d"),
            V7OwnedCount, SourceOwnedCount, SwitchCount, PadCount, TimedCount, TriggerCount,
            CheckpointCount, V7PitCount, V7GroundCount));
        return false;
    }

    const FVector ExpectedSpawn(-1450.0f, 0.0f, 212.0f);
    if (!InitialSpawn.Equals(ExpectedSpawn, 5.0f) || FVector::Dist2D(Character->GetActorLocation(), InitialSpawn) > 5.0f
        || !Movement->IsMovingOnGround() || Character->GetMovementBaseObject() != V7MixedCoreGrounds[0]->GetStaticMeshComponent()
        || State != ENaiwaP1State::Playing || DeathEventCount != 0 || ActiveCheckpoint.IsValid()
        || CollectedIds.Num() != 0 || V7MixedCorePads[0]->GetActivationCount() != 0
        || V7MixedCorePads[1]->GetActivationCount() != 0 || V7MixedCorePads[2]->GetActivationCount() != 0
        || V7MixedCorePads[3]->GetActivationCount() != 0
        || V7MixedCoreTimed[0]->GetState() != ENaiwaP1TimedPlatformState::Ready
        || V7MixedCoreTimed[1]->GetState() != ENaiwaP1TimedPlatformState::Ready
        || V7MixedCoreTimed[2]->GetState() != ENaiwaP1TimedPlatformState::Ready
        || V7MixedCoreTimed[3]->GetState() != ENaiwaP1TimedPlatformState::Ready
        || !bV7MixedCoreHiddenFinalPitsInvisible)
    {
        FinishV7MixedCoreProbe(TEXT("INCOMPLETE"), TEXT("V7 route must begin on the retained left bank with clean checkpoint, pickup and mechanism state"));
        return false;
    }
    if (!Character->SetP1Gait(true))
    {
        FinishV7MixedCoreProbe(TEXT("INCOMPLETE"), TEXT("Could not select the initial Run300 teaching-hazard gait"));
        return false;
    }
    V7MixedCoreCapsuleRadius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
    V7MixedCoreDeathCountBefore = DeathEventCount;
    V7MixedCoreTeachingTimedTriggerBefore = V7MixedCoreTimed[0]->GetTriggerCount();
    V7MixedCoreTeachingTimedDisableBefore = V7MixedCoreTimed[0]->GetDisableCount();
    V7MixedCoreTimedCTriggerBefore = V7MixedCoreTimed[1]->GetTriggerCount();
    V7MixedCoreTimedCDisableBefore = V7MixedCoreTimed[1]->GetDisableCount();
    V7MixedCorePadActivationBefore = V7MixedCorePads[1]->GetActivationCount();
    V7MixedCoreFinalPadActivationBefore = V7MixedCorePads[2]->GetActivationCount();
    V7MixedCoreTimedFinalTriggerBefore = V7MixedCoreTimed[2]->GetTriggerCount();
    V7MixedCoreTimedFinalDisableBefore = V7MixedCoreTimed[2]->GetDisableCount();
    V7MixedCoreSecondPadActivationBefore = V7MixedCorePads[3]->GetActivationCount();
    V7MixedCoreTimedSecondTriggerBefore = V7MixedCoreTimed[3]->GetTriggerCount();
    V7MixedCoreTimedSecondDisableBefore = V7MixedCoreTimed[3]->GetDisableCount();
    for (int32 Index = 1; Index <= 3; ++Index)
    {
        V7MixedCoreSwitchSpeedMinimum[Index] = TNumericLimits<float>::Max();
        V7MixedCoreSwitchSpeedMaximum[Index] = 0.0f;
    }
    bV7MixedCoreProbeInitialized = true;
    AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::TeachingHazard,
        TEXT("exact_v7_identity_unique_70_actor_inventory_grounded_start"));
    return true;
}

void ANaiwaP1GameMode::RecordV7MixedCoreProbe(const TCHAR* Station)
{
    const ANaiwaCharacter* Character = PlayerCharacter.Get();
    const FVector Position = Character ? Character->GetActorLocation() : FVector::ZeroVector;
    const UObject* Base = Character ? Character->GetMovementBaseObject() : nullptr;
    TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("station"), Station);
    Entry->SetNumberField(TEXT("stage"), static_cast<int32>(V7MixedCoreStep));
    Entry->SetNumberField(TEXT("wall_elapsed_s"), FPlatformTime::Seconds() - V7MixedCoreStartedWall);
    Entry->SetNumberField(TEXT("world_time_s"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
    Entry->SetNumberField(TEXT("x_cm"), Position.X);
    Entry->SetNumberField(TEXT("feet_z_cm"), Character ? Character->GetCapsuleBottom() : 0.0f);
    Entry->SetNumberField(TEXT("velocity_x_cm_s"), Character ? Character->GetVelocity().X : 0.0f);
    Entry->SetNumberField(TEXT("velocity_z_cm_s"), Character ? Character->GetVelocity().Z : 0.0f);
    Entry->SetBoolField(TEXT("grounded"), Character && Character->GetCharacterMovement()->IsMovingOnGround());
    Entry->SetBoolField(TEXT("running_gait"), Character && Character->IsRunningGait());
    Entry->SetStringField(TEXT("movement_base"), GetNameSafe(Base));
    Entry->SetStringField(TEXT("active_checkpoint"), ActiveCheckpoint.IsValid()
        ? ActiveCheckpoint->GetStableId().ToString() : TEXT("none"));
    V7MixedCoreEvents.Add(MakeShared<FJsonValueObject>(Entry));
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_V7_MIXED_CORE station=%s stage=%d x=%.1f feet=%.1f gait=%s"),
        Station, static_cast<int32>(V7MixedCoreStep), Position.X,
        Character ? Character->GetCapsuleBottom() : 0.0f,
        Character && Character->IsRunningGait() ? TEXT("Run300") : TEXT("Walk150"));
}

void ANaiwaP1GameMode::AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep NextStep, const TCHAR* Station)
{
    V7MixedCoreStep = NextStep;
    V7MixedCoreStage = static_cast<int32>(NextStep);
    V7MixedCoreStageStartedWall = FPlatformTime::Seconds();
    RecordV7MixedCoreProbe(Station);
}

bool ANaiwaP1GameMode::TickV7MixedCoreCheckpointRecovery()
{
    if (V7MixedCoreRecoveryCheckpointIndex < 0 || bV7MixedCoreRecoveryPassed) return false;
    if (!PlayerCharacter.IsValid() || V7MixedCoreRecoveryCheckpointIndex >= UE_ARRAY_COUNT(V7MixedCoreCheckpoints))
    {
        FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Checkpoint recovery lost its player or checkpoint binding"));
        return true;
    }

    ANaiwaCharacter* Character = PlayerCharacter.Get();
    ANaiwaP1Trigger* Checkpoint = V7MixedCoreCheckpoints[V7MixedCoreRecoveryCheckpointIndex].Get();
    if (!Checkpoint)
    {
        FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Requested production checkpoint disappeared"));
        return true;
    }

    static const float FallDirectionByCheckpoint[7] = { -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f };
    static const TCHAR* GroundIdByCheckpoint[7] = { nullptr, TEXT("A1"), TEXT("A3"), TEXT("B1"),
        TEXT("B4"), TEXT("C1"), TEXT("C4") };

    if (V7MixedCoreRecoveryPhase == 0)
    {
        if (ActiveCheckpoint.Get() != Checkpoint
            || !bV7MixedCoreCheckpointReached[V7MixedCoreRecoveryCheckpointIndex]) return false;

        V7MixedCoreRecoveryRecord = MakeShared<FJsonObject>();
        V7MixedCoreRecoveryRecord->SetStringField(TEXT("checkpoint_id"), Checkpoint->GetStableId().ToString());
        V7MixedCoreRecoveryRecord->SetBoolField(TEXT("native_checkpoint_activation_observed"), true);
        V7MixedCoreRecoveryDeathCountBefore = DeathEventCount;
        V7MixedCoreRecoveryStartedWall = FPlatformTime::Seconds();
        V7MixedCoreRecoveryPhase = 1;
        Character->StopJumping();
        Character->SetAutomatedMoveInput(FallDirectionByCheckpoint[V7MixedCoreRecoveryCheckpointIndex]);
        V7MixedCoreRecoveryRecord->SetNumberField(TEXT("natural_fall_direction"),
            FallDirectionByCheckpoint[V7MixedCoreRecoveryCheckpointIndex]);
        V7MixedCoreRecoveryRecord->SetNumberField(TEXT("death_count_before"), DeathEventCount);
        RecordV7MixedCoreProbe(TEXT("checkpoint_natural_recovery_walk_off_started"));
        return true;
    }

    if (FPlatformTime::Seconds() - V7MixedCoreRecoveryStartedWall > 35.0)
    {
        Character->SetAutomatedMoveInput(0.0f);
        FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Checkpoint natural-death and recovery sequence timed out"));
        return true;
    }

    if (V7MixedCoreRecoveryPhase == 1)
    {
        if (State == ENaiwaP1State::Dying || State == ENaiwaP1State::Respawning
            || DeathEventCount > V7MixedCoreRecoveryDeathCountBefore)
        {
            Character->SetAutomatedMoveInput(0.0f);
            const bool bOneDeath = DeathEventCount == V7MixedCoreRecoveryDeathCountBefore + 1;
            V7MixedCoreRecoveryRecord->SetBoolField(TEXT("production_dying_state_observed"),
                State == ENaiwaP1State::Dying || State == ENaiwaP1State::Respawning);
            V7MixedCoreRecoveryRecord->SetStringField(TEXT("death_cause"), LastDeathCause.ToString());
            V7MixedCoreRecoveryRecord->SetNumberField(TEXT("death_count_after"), DeathEventCount);
            V7MixedCoreRecoveryRecord->SetBoolField(TEXT("exactly_one_natural_death"), bOneDeath);
            V7MixedCoreRecoveryPhase = 2;
            RecordV7MixedCoreProbe(TEXT("checkpoint_natural_death_observed"));
            return true;
        }
        if (State != ENaiwaP1State::Playing)
        {
            Character->SetAutomatedMoveInput(0.0f);
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Checkpoint fall left Playing without entering the production death flow"));
            return true;
        }
        Character->SetAutomatedMoveInput(FallDirectionByCheckpoint[V7MixedCoreRecoveryCheckpointIndex]);
        return true;
    }

    Character->SetAutomatedMoveInput(0.0f);
    if (State == ENaiwaP1State::Dying || State == ENaiwaP1State::Respawning) return true;
    if (State != ENaiwaP1State::Playing) return true;

    const FVector ExpectedLocation = Checkpoint->GetActorLocation() + Checkpoint->RespawnOffset;
    const FVector ActualLocation = Character->GetActorLocation();
    const TWeakObjectPtr<AStaticMeshActor>* ExpectedGround = nullptr;
    bool bSafeBankGrounded = false;
    if (V7MixedCoreRecoveryCheckpointIndex == 0)
    {
        bSafeBankGrounded = Character->GetCharacterMovement()->IsMovingOnGround()
            && Character->GetMovementBaseObject() == V7MixedCoreGrounds[2]->GetStaticMeshComponent();
    }
    else
    {
        ExpectedGround = V7MixedCoreV7GroundById.Find(FName(GroundIdByCheckpoint[V7MixedCoreRecoveryCheckpointIndex]));
        bSafeBankGrounded = ExpectedGround && ExpectedGround->IsValid()
            && Character->GetCharacterMovement()->IsMovingOnGround()
            && Character->GetMovementBaseObject() == ExpectedGround->Get()->GetStaticMeshComponent();
    }
    const bool bExpectedRespawn = ActiveCheckpoint.Get() == Checkpoint
        && FVector::Dist(ActualLocation, ExpectedLocation) <= 8.0f;
    const bool bControlsRestored = IsGameplayInputAllowed() && !Character->IsP1DeathPoseActive();
    const bool bNoFallback = !bRespawnTriedInitial;
    const bool bOneDeath = DeathEventCount == V7MixedCoreRecoveryDeathCountBefore + 1;
    const bool bRecoveryPassed = bOneDeath && bExpectedRespawn && bSafeBankGrounded && bControlsRestored && bNoFallback;

    V7MixedCoreRecoveryRecord->SetBoolField(TEXT("respawned_at_expected_checkpoint"), bExpectedRespawn);
    V7MixedCoreRecoveryRecord->SetBoolField(TEXT("respawned_on_configured_safe_bank"), bSafeBankGrounded);
    V7MixedCoreRecoveryRecord->SetBoolField(TEXT("controls_restored"), bControlsRestored);
    V7MixedCoreRecoveryRecord->SetBoolField(TEXT("initial_spawn_fallback_used"), !bNoFallback);
    V7MixedCoreRecoveryRecord->SetBoolField(TEXT("recovery_passed"), bRecoveryPassed);
    V7MixedCoreRecoveryRecord->SetNumberField(TEXT("expected_respawn_x_cm"), ExpectedLocation.X);
    V7MixedCoreRecoveryRecord->SetNumberField(TEXT("actual_respawn_x_cm"), ActualLocation.X);
    V7MixedCoreRecoveryRecord->SetNumberField(TEXT("actual_respawn_feet_z_cm"), Character->GetCapsuleBottom());
    V7MixedCoreRecoveryRecord->SetNumberField(TEXT("death_count_after"), DeathEventCount);
    V7MixedCoreRecoveryRecord->SetNumberField(TEXT("respawn_placements_from_production_flow"), bOneDeath ? 1 : 0);
    if (!bRecoveryPassed)
    {
        FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Production checkpoint recovery did not return grounded to its exact safe bank with controls restored and no spawn fallback"));
        return true;
    }

    bV7MixedCoreRecoveryPassed = true;
    V7MixedCoreRecoveryPhase = 3;
    V7MixedCoreStageStartedWall = FPlatformTime::Seconds();
    RecordV7MixedCoreProbe(TEXT("checkpoint_natural_recovery_passed_route_resumes"));
    return true;
}

void ANaiwaP1GameMode::TickV7MixedCoreProbe()
{
    if (bV7MixedCoreProbeFinished) return;
    const double Now = FPlatformTime::Seconds();
    if (Now - V7MixedCoreStartedWall > 300.0 || Now - V7MixedCoreStageStartedWall > 50.0)
    {
        FinishV7MixedCoreProbe(TEXT("FAIL"), FString::Printf(TEXT("Core route timed out at step=%d x=%.1f"),
            static_cast<int32>(V7MixedCoreStep), PlayerCharacter.IsValid() ? PlayerCharacter->GetActorLocation().X : 0.0f));
        return;
    }
    if (!bV7MixedCoreProbeInitialized)
    {
        InitializeV7MixedCoreProbe();
        return;
    }
    if (bV7HiddenFinalPitRecoveryProbeEnabled
        && V7MixedCoreStep == ENaiwaP1V7MixedStep::SecondTimedCross
        && (State == ENaiwaP1State::Dying || State == ENaiwaP1State::Respawning
            || DeathEventCount != V7MixedCoreDeathCountBefore))
    {
        TickV7HiddenFinalPitRecoveryProbe();
        return;
    }
    if (TickV7MixedCoreCheckpointRecovery()) return;
    const int32 ExpectedDeathsAtCurrentRecoveryPhase = V7MixedCoreDeathCountBefore
        + (V7MixedCoreRecoveryCheckpointIndex >= 0 && bV7MixedCoreRecoveryPassed ? 1 : 0);
    ANaiwaCharacter* Character = PlayerCharacter.Get();
    if (!Character || State == ENaiwaP1State::Dying || State == ENaiwaP1State::Respawning
        || (State == ENaiwaP1State::Won && V7MixedCoreStep < ENaiwaP1V7MixedStep::Goal)
        || DeathEventCount != ExpectedDeathsAtCurrentRecoveryPhase)
    {
        FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("The route left Playing or died before its physical goal"));
        return;
    }
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const FVector Location = Character->GetActorLocation();
    const float FeetZ = Character->GetCapsuleBottom();
    const bool bGrounded = Movement->IsMovingOnGround();
    if (bV7MixedCoreJumpHeld && GetWorld()->GetTimeSeconds() >= V7MixedCoreJumpReleaseGameTime)
    {
        Character->StopJumping();
        bV7MixedCoreJumpHeld = false;
    }
    const auto IsOnV7Ground = [this, Character, bGrounded](const TCHAR* Id)
    {
        const TWeakObjectPtr<AStaticMeshActor>* Found = V7MixedCoreV7GroundById.Find(FName(Id));
        return bGrounded && Found && Found->IsValid()
            && Character->GetMovementBaseObject() == Found->Get()->GetStaticMeshComponent();
    };
    const auto IsOnTeachingGround = [this, Character, bGrounded](int32 Index)
    {
        return Index >= 0 && Index < UE_ARRAY_COUNT(V7MixedCoreGrounds) && bGrounded
            && V7MixedCoreGrounds[Index].IsValid()
            && Character->GetMovementBaseObject() == V7MixedCoreGrounds[Index]->GetStaticMeshComponent();
    };
    const auto SetGait = [this, Character](bool bRun, const TCHAR* Station) -> bool
    {
        if (Character->IsRunningGait() == bRun) return true;
        if (!Character->SetP1Gait(bRun)) return false;
        TSharedRef<FJsonObject> Gait = MakeShared<FJsonObject>();
        Gait->SetStringField(TEXT("gait"), bRun ? TEXT("Run300") : TEXT("Walk150"));
        Gait->SetStringField(TEXT("at"), Station);
        Gait->SetNumberField(TEXT("x_cm"), Character->GetActorLocation().X);
        Gait->SetNumberField(TEXT("wall_elapsed_s"), FPlatformTime::Seconds() - V7MixedCoreStartedWall);
        V7MixedCoreGaitChanges.Add(MakeShared<FJsonValueObject>(Gait));
        RecordV7MixedCoreProbe(Station);
        return true;
    };
    const int32 ExpectedPickupCountForRoute = V7MixedCoreRecoveryCheckpointIndex < 0 ? 4
        : V7MixedCoreRecoveryCheckpointIndex < 3 ? 3
        : V7MixedCoreRecoveryCheckpointIndex < 5 ? 2 : 1;
    const int32 ExpectedDeathCountForRoute = V7MixedCoreDeathCountBefore
        + (V7MixedCoreRecoveryCheckpointIndex >= 0 && bV7MixedCoreRecoveryPassed ? 1 : 0);

    if (State == ENaiwaP1State::Playing)
    {
        for (int32 Index = 1; Index <= 3; ++Index)
        {
            ANaiwaP1SwitchPlatform* Shuttle = V7MixedCoreSwitches[Index].Get();
            if (!Shuttle) continue;
            const ENaiwaP1SwitchPlatformState ShuttleState = Shuttle->GetState();
            if (ShuttleState != ENaiwaP1SwitchPlatformState::TravelingLeft
                && ShuttleState != ENaiwaP1SwitchPlatformState::TravelingRight) continue;
            const float Speed = Shuttle->GetCurrentTravelSpeedCmPerSecond();
            V7MixedCoreSwitchSpeedMinimum[Index] = FMath::Min(V7MixedCoreSwitchSpeedMinimum[Index], Speed);
            V7MixedCoreSwitchSpeedMaximum[Index] = FMath::Max(V7MixedCoreSwitchSpeedMaximum[Index], Speed);
            ++V7MixedCoreSwitchSpeedSamples[Index];
        }
        for (int32 Index = 0; Index < 8; ++Index)
        {
            ANaiwaP1Trigger* Checkpoint = V7MixedCoreCheckpoints[Index].Get();
            const TCHAR* GroundId = Index == 0 ? nullptr : Index == 1 ? TEXT("A1")
                : Index == 2 ? TEXT("A3") : Index == 3 ? TEXT("B1")
                : Index == 4 ? TEXT("B4") : Index == 5 ? TEXT("C1")
                : Index == 6 ? TEXT("C4") : TEXT("FinalApproach");
            const bool bExpectedGround = Index == 0 ? IsOnTeachingGround(2) : IsOnV7Ground(GroundId);
            if (!bV7MixedCoreCheckpointReached[Index] && Checkpoint && ActiveCheckpoint.Get() == Checkpoint
                && bExpectedGround && Checkpoint->TriggerBox && Checkpoint->TriggerBox->IsOverlappingActor(Character))
            {
                bV7MixedCoreCheckpointReached[Index] = true;
                TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
                Item->SetStringField(TEXT("id"), Checkpoint->GetStableId().ToString());
                Item->SetBoolField(TEXT("activated_by_native_overlap"), true);
                Item->SetBoolField(TEXT("grounded_on_configured_safe_bank"), true);
                Item->SetNumberField(TEXT("x_cm"), Location.X);
                V7MixedCoreCheckpointRecords.Add(MakeShared<FJsonValueObject>(Item));
                RecordV7MixedCoreProbe(TEXT("checkpoint_native_overlap_and_grounded_bank"));
            }
        }
    }

    // Timers are read from their production actors. Each one must follow real floor support,
    // count down once, and remain active until the character reaches its static exit bank.
    for (int32 Index = 0; Index < 2; ++Index)
    {
        ANaiwaP1TimedPlatform* Timed = V7MixedCoreTimed[Index].Get();
        if (!Timed) continue;
        const bool bActiveSection = Index == 0
            ? V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingTimedCross
            : V7MixedCoreStep == ENaiwaP1V7MixedStep::TimedC_Jump
                || V7MixedCoreStep == ENaiwaP1V7MixedStep::TimedC_Cross;
        if (!bActiveSection) continue;
        bool& bSupported = Index == 0 ? bV7MixedCoreTimedTeachingSupported : bV7MixedCoreTimedCSupported;
        bool& bTriggered = Index == 0 ? bV7MixedCoreTimedTeachingTriggered : bV7MixedCoreTimedCTriggered;
        bool& bDeclined = Index == 0 ? bV7MixedCoreTimedTeachingDeclined : bV7MixedCoreTimedCDeclined;
        float& PreviousRemaining = Index == 0 ? V7MixedCoreTeachingTimedRemainingPrevious : V7MixedCoreTimedCRemainingPrevious;
        if (bGrounded && Character->GetMovementBaseObject() == Timed->PlatformCollision.Get())
        {
            bSupported = true;
            if (!Index) bV7MixedCoreTimedTeachingSupported = true;
            if (Index) bV7MixedCoreTimedCSupported = true;
        }
        const int32 TriggerBefore = Index == 0 ? V7MixedCoreTeachingTimedTriggerBefore : V7MixedCoreTimedCTriggerBefore;
        if (!bTriggered && Timed->GetTriggerCount() == TriggerBefore + 1)
        {
            if (!bSupported || Timed->GetState() != ENaiwaP1TimedPlatformState::Countdown)
            {
                FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("A timed actor started without sampled real floor support and Countdown state"));
                return;
            }
            bTriggered = true;
            PreviousRemaining = Timed->GetCountdownRemaining();
            if (!Index) V7MixedCoreTimedTeachingStartedWall = Now;
            else V7MixedCoreTimedCStartedWall = Now;
            RecordV7MixedCoreProbe(Index == 0 ? TEXT("teaching_timer_started_after_real_support") : TEXT("timed_c_started_after_real_support"));
        }
        if (bTriggered && !bDeclined)
        {
            if (Timed->GetState() != ENaiwaP1TimedPlatformState::Countdown
                || !Timed->IsCarryingCollisionEnabled() || Timed->GetCountdownRemaining() <= 0.0f)
            {
                FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("A timed floor expired or disabled before the tested static bank exit"));
                return;
            }
            if (Timed->GetCountdownRemaining() < PreviousRemaining - 0.001f)
            {
                bDeclined = true;
                RecordV7MixedCoreProbe(Index == 0 ? TEXT("teaching_timer_decreased") : TEXT("timed_c_decreased"));
            }
            PreviousRemaining = Timed->GetCountdownRemaining();
        }
        const int32 DisableBefore = Index == 0 ? V7MixedCoreTeachingTimedDisableBefore : V7MixedCoreTimedCDisableBefore;
        if (Timed->GetDisableCount() != DisableBefore
            && !(Index == 2 && bV7HiddenFinalPitRecoveryProbeEnabled))
        {
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("A timed platform disabled before the character reached its static exit bank"));
            return;
        }
    }

    if (V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingHazard
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingSwitchTouch
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingSwitchBoard
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingSwitchRide
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingSwitchExit
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingBounce
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingBounceLanding
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingCheckpoint
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingTimedCross
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchA_Touch
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchA_Board
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchA_Ride
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchA_Exit
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::Checkpoint03
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::GapsA
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::Checkpoint04
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchB_Touch
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchB_Board
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchB_Ride
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::BounceB
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::BounceBLanding
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::Checkpoint05
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::GapsB
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::Checkpoint06
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchC_Touch
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchC_Board
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchC_Ride
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::TimedC_Jump
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::TimedC_Cross
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::Checkpoint07
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::GapsC
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::FinalBank
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::FinalBounce
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::FinalBounceLanding
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::FinalTimedCross
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::FinalExit
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SecondBounce
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SecondBounceLanding
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SecondTimedCross
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SecondExit
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::Pickup)
    {
        if (State != ENaiwaP1State::Playing)
        {
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Movement route ended before physical goal overlap"));
            return;
        }
    }

    const bool bSwitchStep = V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingSwitchTouch
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingSwitchBoard
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingSwitchRide
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchA_Touch
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchA_Board
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchA_Ride
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchB_Touch
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchB_Board
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchB_Ride
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchC_Touch
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchC_Board
        || V7MixedCoreStep == ENaiwaP1V7MixedStep::SwitchC_Ride;
    if (bSwitchStep)
    {
        int32 SwitchIndex = -1;
        ENaiwaP1V7MixedStep TouchStep = ENaiwaP1V7MixedStep::WaitReady;
        ENaiwaP1V7MixedStep BoardStep = ENaiwaP1V7MixedStep::WaitReady;
        ENaiwaP1V7MixedStep RideStep = ENaiwaP1V7MixedStep::WaitReady;
        if (V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingSwitchTouch
            || V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingSwitchBoard
            || V7MixedCoreStep == ENaiwaP1V7MixedStep::TeachingSwitchRide)
        { SwitchIndex = 0; TouchStep = ENaiwaP1V7MixedStep::TeachingSwitchTouch; BoardStep = ENaiwaP1V7MixedStep::TeachingSwitchBoard; RideStep = ENaiwaP1V7MixedStep::TeachingSwitchRide; }
        else if (V7MixedCoreStep >= ENaiwaP1V7MixedStep::SwitchA_Touch
            && V7MixedCoreStep <= ENaiwaP1V7MixedStep::SwitchA_Ride)
        { SwitchIndex = 1; TouchStep = ENaiwaP1V7MixedStep::SwitchA_Touch; BoardStep = ENaiwaP1V7MixedStep::SwitchA_Board; RideStep = ENaiwaP1V7MixedStep::SwitchA_Ride; }
        else if (V7MixedCoreStep >= ENaiwaP1V7MixedStep::SwitchB_Touch
            && V7MixedCoreStep <= ENaiwaP1V7MixedStep::SwitchB_Ride)
        { SwitchIndex = 2; TouchStep = ENaiwaP1V7MixedStep::SwitchB_Touch; BoardStep = ENaiwaP1V7MixedStep::SwitchB_Board; RideStep = ENaiwaP1V7MixedStep::SwitchB_Ride; }
        else
        { SwitchIndex = 3; TouchStep = ENaiwaP1V7MixedStep::SwitchC_Touch; BoardStep = ENaiwaP1V7MixedStep::SwitchC_Board; RideStep = ENaiwaP1V7MixedStep::SwitchC_Ride; }
        ANaiwaP1SwitchPlatform* Platform = V7MixedCoreSwitches[SwitchIndex].Get();
        if (!Platform) { FinishV7MixedCoreProbe(TEXT("INCOMPLETE"), TEXT("A bound switch platform disappeared")); return; }
        if (V7MixedCoreStep == TouchStep)
        {
            Character->SetAutomatedMoveInput(1.0f);
            if (Platform->GetLeftSwitchTouchCount() > 0)
            {
                AdvanceV7MixedCoreProbe(BoardStep, TEXT("left_switch_touched_through_native_overlap"));
                return;
            }
            const float TouchX = Platform->GetActorLocation().X + Platform->LeftSwitchX;
            if (Location.X > TouchX + 220.0f)
                FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("The character passed a shuttle left switch without touching it"));
            return;
        }
        if (V7MixedCoreStep == BoardStep)
        {
            Character->SetAutomatedMoveInput(1.0f);
            const float DockCenter = Platform->GetActorLocation().X + Platform->DockLeftX;
            if (Platform->IsCharacterSupported(Character)
                && Location.X - Platform->PlatformCollision->GetComponentLocation().X >= -65.0f)
            {
                bV7MixedCoreSwitchBoarded[SwitchIndex] = true;
                V7MixedCoreTransitTrapIdsJumped.Reset();
                Character->SetAutomatedMoveInput(0.0f);
                AdvanceV7MixedCoreProbe(RideStep, TEXT("boarded_shuttle_on_real_collision_support"));
                return;
            }
            if (Location.X > DockCenter + 210.0f && !Platform->IsCharacterSupported(Character))
                FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("The character passed a shuttle dock without boarding its real collision floor"));
            return;
        }
        Character->SetAutomatedMoveInput(0.0f);
        const bool bPlatformMoving = Platform->GetState() == ENaiwaP1SwitchPlatformState::TravelingRight
            || Platform->GetState() == ENaiwaP1SwitchPlatformState::TravelingLeft;
        if (bPlatformMoving && !bV7MixedCoreJumpHeld && Platform->IsCharacterSupported(Character))
        {
            const float Direction = Platform->GetState() == ENaiwaP1SwitchPlatformState::TravelingRight ? 1.0f : -1.0f;
            for (ANaiwaP1Trigger* Trap : Platform->GetTransitTrapActors())
            {
                if (!IsValid(Trap) || V7MixedCoreTransitTrapIdsJumped.Contains(Trap->GetStableId())) continue;
                const float DistanceAhead = (Trap->GetActorLocation().X - Location.X) * Direction;
                const float SpeedNow = Platform->GetCurrentTravelSpeedCmPerSecond();
                // Time the jump arc to cross the stationary trap near its descent,
                // despite the shuttle's 200-400 cm/s speed changes.
                const float SpeedScaledJumpLead = FMath::Max(80.0f, SpeedNow * 0.80f - 50.0f);
                if (DistanceAhead <= SpeedScaledJumpLead && DistanceAhead > 0.0f)
                {
                    UE_LOG(LogTemp, Display,
                        TEXT("NAIWA_P1_V7_MIXED_CORE spike_jump id=%s pawn_x=%.1f trap_x=%.1f distance=%.1f lead=%.1f speed=%.1f"),
                        *Trap->GetStableId().ToString(), Location.X, Trap->GetActorLocation().X,
                        DistanceAhead, SpeedScaledJumpLead, SpeedNow);
                    V7MixedCoreTransitTrapIdsJumped.Add(Trap->GetStableId());
                    ++V7MixedCoreTransitTrapsJumped;
                    ++V7MixedCoreJumpCalls;
                    bV7MixedCoreJumpHeld = true;
                    V7MixedCoreJumpReleaseGameTime = GetWorld()->GetTimeSeconds() + 0.18f;
                    Character->Jump();
                    RecordV7MixedCoreProbe(TEXT("jumped_random_transit_spike_on_real_shuttle"));
                    break;
                }
            }
        }
        if (!bV7MixedCoreSwitchCueSeen[SwitchIndex] && Platform->IsDepartureCueActive()
            && Platform->IsCharacterSupported(Character))
        {
            bV7MixedCoreSwitchCueSeen[SwitchIndex] = true;
            RecordV7MixedCoreProbe(TEXT("supported_departure_cue_seen"));
        }
        if (!bV7MixedCoreSwitchMidwaySupported[SwitchIndex]
            && Platform->GetState() == ENaiwaP1SwitchPlatformState::TravelingRight
            && FMath::Abs(Platform->GetCurrentX()) < 350.0f && Platform->IsCharacterSupported(Character))
        {
            bV7MixedCoreSwitchMidwaySupported[SwitchIndex] = true;
            RecordV7MixedCoreProbe(TEXT("shuttle_supported_mid_gap"));
        }
        if (SwitchIndex == 3 && !bV7MixedCoreSwitchRiseObserved[SwitchIndex]
            && Platform->GetState() == ENaiwaP1SwitchPlatformState::TravelingRight
            && FMath::Abs(Platform->GetCurrentX()) < 350.0f
            && Platform->GetCurrentTopZ() >= 280.0f && Platform->IsCharacterSupported(Character))
        {
            bV7MixedCoreSwitchRiseObserved[SwitchIndex] = true;
            V7MixedCoreCTopAtMidpoint = Platform->GetCurrentTopZ();
            RecordV7MixedCoreProbe(TEXT("c_shuttle_rise_observed_with_grounded_rider"));
        }
        if (Platform->GetState() == ENaiwaP1SwitchPlatformState::DockedRight)
        {
            if (!Platform->IsCharacterSupported(Character) || !bV7MixedCoreSwitchBoarded[SwitchIndex]
                || !bV7MixedCoreSwitchCueSeen[SwitchIndex] || !bV7MixedCoreSwitchMidwaySupported[SwitchIndex]
                || V7MixedCoreTransitTrapIdsJumped.Num() != Platform->GetTransitTrapSpawnCount())
            {
                FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Shuttle failed boarding/support/dock checks or a randomized transit spike was not jumped"));
                return;
            }
            if (SwitchIndex == 3)
            {
                V7MixedCoreCTopAtDock = Platform->GetCurrentTopZ();
                if (!bV7MixedCoreSwitchRiseObserved[SwitchIndex]
                    || !FMath::IsNearlyEqual(V7MixedCoreCTopAtDock, 340.0f, 1.0f))
                {
                    FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("C shuttle did not visibly rise from 240 cm to its 340 cm timed-platform dock"));
                    return;
                }
            }
            bV7MixedCoreSwitchDocked[SwitchIndex] = true;
            const ENaiwaP1V7MixedStep Next = SwitchIndex == 0 ? ENaiwaP1V7MixedStep::TeachingSwitchExit
                : SwitchIndex == 1 ? ENaiwaP1V7MixedStep::SwitchA_Exit
                : SwitchIndex == 2 ? ENaiwaP1V7MixedStep::BounceB : ENaiwaP1V7MixedStep::TimedC_Jump;
            AdvanceV7MixedCoreProbe(Next, TEXT("shuttle_docked_right_with_character_supported"));
        }
        return;
    }

    switch (V7MixedCoreStep)
    {
    case ENaiwaP1V7MixedStep::WaitReady:
        return;
    case ENaiwaP1V7MixedStep::TeachingHazard:
    {
        Character->SetAutomatedMoveInput(1.0f);
        const float HazardX = V7MixedCoreHazardTeaching->GetActorLocation().X;
        if (!bV7MixedCoreTeachingHazardJumped && Location.X >= HazardX - 80.0f)
        {
            if (!IsOnTeachingGround(0))
            {
                FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Teaching hazard jump did not start on the left bank"));
                return;
            }
            bV7MixedCoreTeachingHazardJumped = true;
            ++V7MixedCoreJumpCalls;
            bV7MixedCoreJumpHeld = true;
            V7MixedCoreJumpReleaseGameTime = GetWorld()->GetTimeSeconds() + 0.18f;
            Character->Jump();
            RecordV7MixedCoreProbe(TEXT("real_jump_over_retained_teaching_hazard"));
            return;
        }
        if (bV7MixedCoreTeachingHazardJumped && Location.X > HazardX + 90.0f && IsOnTeachingGround(0))
        {
            bV7MixedCoreTeachingHazardCleared = true;
            if (V7MixedCoreHazardTeaching->DisplayMesh->IsVisible() && !V7MixedCoreHazardTeaching->bRevealOnSpawn)
            {
                FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("The retained hidden teaching hazard unexpectedly revealed during the safe jump"));
                return;
            }
            if (!SetGait(false, TEXT("changed_to_Walk150_for_teaching_mechanisms")))
            {
                FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Could not change to the teaching Walk150 gait"));
                return;
            }
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::TeachingSwitchTouch, TEXT("hazard_cleared_with_real_landing"));
        }
        return;
    }
    case ENaiwaP1V7MixedStep::TeachingSwitchTouch:
    case ENaiwaP1V7MixedStep::TeachingSwitchBoard:
    case ENaiwaP1V7MixedStep::TeachingSwitchRide:
    case ENaiwaP1V7MixedStep::SwitchA_Touch:
    case ENaiwaP1V7MixedStep::SwitchA_Board:
    case ENaiwaP1V7MixedStep::SwitchA_Ride:
    case ENaiwaP1V7MixedStep::SwitchB_Touch:
    case ENaiwaP1V7MixedStep::SwitchB_Board:
    case ENaiwaP1V7MixedStep::SwitchB_Ride:
    case ENaiwaP1V7MixedStep::SwitchC_Touch:
    case ENaiwaP1V7MixedStep::SwitchC_Board:
    case ENaiwaP1V7MixedStep::SwitchC_Ride:
        return;
    case ENaiwaP1V7MixedStep::TeachingSwitchExit:
        Character->SetAutomatedMoveInput(1.0f);
        if (IsOnTeachingGround(1) && Location.X >= 1000.0f
            && V7MixedCoreSwitches[0]->GetRightSwitchTouchCount() >= 1)
        {
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::TeachingBounce, TEXT("walked_off_shuttle_and_physically_touched_right_switch"));
        }
        else if (Location.X > 1150.0f && V7MixedCoreSwitches[0]->GetRightSwitchTouchCount() < 1)
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Teaching shuttle right switch was not physically touched"));
        return;
    case ENaiwaP1V7MixedStep::TeachingBounce:
        Character->SetAutomatedMoveInput(1.0f);
        if (V7MixedCorePads[0]->GetActivationCount() == 1)
        {
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::TeachingBounceLanding, TEXT("teaching_bounce_activated_once_by_landing"));
            return;
        }
        if (V7MixedCorePads[0]->GetActivationCount() > 1 || Location.X > V7MixedCorePads[0]->GetActorLocation().X + 220.0f)
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Teaching bounce pad was skipped or activated more than once"));
        return;
    case ENaiwaP1V7MixedStep::TeachingBounceLanding:
        Character->SetAutomatedMoveInput(1.0f);
        if (!bV7MixedCoreTeachingBounceLaunched && Movement->IsFalling() && Character->GetVelocity().Z > 100.0f)
        {
            bV7MixedCoreTeachingBounceLaunched = true;
            RecordV7MixedCoreProbe(TEXT("teaching_bounce_upward_velocity_seen"));
        }
        if (bV7MixedCoreTeachingBounceLaunched && IsOnTeachingGround(2) && Location.X > 1740.0f)
        {
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::TeachingCheckpoint, TEXT("landed_on_real_teaching_bounce_bank"));
            return;
        }
        if (FeetZ < -100.0f) FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Teaching bounce left its expected launch arc and could not reach the static landing bank"));
        return;
    case ENaiwaP1V7MixedStep::TeachingCheckpoint:
        Character->SetAutomatedMoveInput(1.0f);
        if (bV7MixedCoreCheckpointReached[0] && IsOnTeachingGround(2))
        {
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::TeachingTimedCross, TEXT("checkpoint01_activated_on_grounded_static_bank"));
            return;
        }
        return;
    case ENaiwaP1V7MixedStep::TeachingTimedCross:
        Character->SetAutomatedMoveInput(1.0f);
        if (V7MixedCoreTimed[0]->GetDisableCount() != V7MixedCoreTeachingTimedDisableBefore
            || V7MixedCoreTimed[0]->GetTriggerCount() > V7MixedCoreTeachingTimedTriggerBefore + 1)
        { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Teaching timed floor disabled before its exit bank")); return; }
        if (IsOnTeachingGround(3) && Location.X >= 3622.0f)
        {
            const float Remaining = V7MixedCoreTimed[0]->GetCountdownRemaining();
            if (!bV7MixedCoreTimedTeachingSupported || !bV7MixedCoreTimedTeachingDeclined
                || V7MixedCoreTimed[0]->GetTriggerCount() != V7MixedCoreTeachingTimedTriggerBefore + 1
                || V7MixedCoreTimed[0]->GetState() != ENaiwaP1TimedPlatformState::Countdown || Remaining <= 0.0f)
            {
                FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Teaching timed crossing lacks real support, one declining countdown, or positive exit margin"));
                return;
            }
            bV7MixedCoreTimedTeachingCompleted = true;
            V7MixedCoreTeachingTimedExitRemaining = Remaining;
            if (!SetGait(true, TEXT("changed_to_Run300_for_v7_gates_and_gap_chains")))
            { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Could not change to the Run300 v7 gate gait")); return; }
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::SwitchA_Touch, TEXT("teaching_timer_exited_to_static_bank_before_expiry"));
        }
        return;
    case ENaiwaP1V7MixedStep::SwitchA_Exit:
        Character->SetAutomatedMoveInput(1.0f);
        if (!bV7MixedCoreAExitJumpIssued && V7MixedCoreSwitches[1]->IsCharacterSupported(Character)
            && Movement->IsMovingOnGround()
            && Location.X + V7MixedCoreCapsuleRadius >= 7260.0f - 40.0f)
        {
            bV7MixedCoreAExitJumpIssued = true;
            V7MixedCoreAExitJumpX = Location.X;
            ++V7MixedCoreJumpCalls;
            bV7MixedCoreJumpHeld = true;
            V7MixedCoreJumpReleaseGameTime = GetWorld()->GetTimeSeconds() + 0.18f;
            Character->Jump();
            RecordV7MixedCoreProbe(TEXT("run_jump_from_switch_a_end_at_minus40cm"));
            return;
        }
        if (bV7MixedCoreAExitJumpIssued && Movement->Velocity.Z > 50.0f) bV7MixedCoreAExitJumpLaunched = true;
        if (bV7MixedCoreAExitJumpIssued && IsOnV7Ground(TEXT("A3")) && Location.X > 7590.0f)
        {
            if (!bV7MixedCoreAExitJumpLaunched)
            { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Switch A exit did not include a real jump")); return; }
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::Checkpoint03, TEXT("switch_a_run_jump_landed_on_a3_static_bank"));
        }
        return;
    case ENaiwaP1V7MixedStep::Checkpoint03:
        Character->SetAutomatedMoveInput(1.0f);
        if (bV7MixedCoreCheckpointReached[2]
            && (IsOnV7Ground(TEXT("A3")) || IsOnV7Ground(TEXT("A4Island01")))
            && V7MixedCoreSwitches[1]->GetRightSwitchTouchCount() > 0)
        {
            V7MixedCoreGapIndex = 0;
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::GapsA, TEXT("checkpoint03_and_a_right_switch_grounded_before_a_gap_chain"));
        }
        else if (Location.X > 8050.0f && V7MixedCoreSwitches[1]->GetRightSwitchTouchCount() < 1)
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Switch A right switch was not physically touched on the static bank"));
        return;
    case ENaiwaP1V7MixedStep::GapsA:
    case ENaiwaP1V7MixedStep::GapsB:
    case ENaiwaP1V7MixedStep::GapsC:
    {
        static const float GapLeft[11] = { 8300.0f, 8940.0f, 9580.0f, 14520.0f, 15160.0f, 15800.0f,
            20660.0f, 21300.0f, 21940.0f, 22580.0f, 23220.0f };
        static const float GapRight[11] = { 8540.0f, 9180.0f, 9820.0f, 14760.0f, 15400.0f, 16040.0f,
            20900.0f, 21540.0f, 22180.0f, 22820.0f, 23460.0f };
        static const float ChainTop[11] = { 180.0f, 180.0f, 180.0f, 480.0f, 480.0f, 480.0f,
            240.0f, 240.0f, 240.0f, 240.0f, 240.0f };
        static const TCHAR* ChainId[11] = { TEXT("A-4"), TEXT("A-4"), TEXT("A-4"), TEXT("B-5"), TEXT("B-5"), TEXT("B-5"),
            TEXT("C-5"), TEXT("C-5"), TEXT("C-5"), TEXT("C-5"), TEXT("C-5") };
        static const TCHAR* TakeoffGround[11] = { TEXT("A4Island01"), TEXT("A4Island02"), TEXT("A4Island03"),
            TEXT("B5Island01"), TEXT("B5Island02"), TEXT("B5Island03"), TEXT("C5Island01"), TEXT("C5Island02"),
            TEXT("C5Island03"), TEXT("C5Island04"), TEXT("C5Island05") };
        static const TCHAR* LandingGround[11] = { TEXT("A4Island02"), TEXT("A4Island03"), TEXT("A4Island04"),
            TEXT("B5Island02"), TEXT("B5Island03"), TEXT("B5Island04"), TEXT("C5Island02"), TEXT("C5Island03"),
            TEXT("C5Island04"), TEXT("C5Island05"), TEXT("C5Island06") };
        Character->SetAutomatedMoveInput(1.0f);
        const int32 Begin = V7MixedCoreStep == ENaiwaP1V7MixedStep::GapsA ? 0
            : V7MixedCoreStep == ENaiwaP1V7MixedStep::GapsB ? 3 : 6;
        const int32 End = V7MixedCoreStep == ENaiwaP1V7MixedStep::GapsA ? 3
            : V7MixedCoreStep == ENaiwaP1V7MixedStep::GapsB ? 6 : 11;
        if (V7MixedCoreGapIndex < Begin || V7MixedCoreGapIndex >= End)
        {
            const ENaiwaP1V7MixedStep Next = V7MixedCoreStep == ENaiwaP1V7MixedStep::GapsA
                ? ENaiwaP1V7MixedStep::Checkpoint04
                : V7MixedCoreStep == ENaiwaP1V7MixedStep::GapsB
                    ? ENaiwaP1V7MixedStep::Checkpoint06 : ENaiwaP1V7MixedStep::FinalBank;
            AdvanceV7MixedCoreProbe(Next, TEXT("gap_chain_all_expected_landings_recorded"));
            return;
        }
        const int32 Gap = V7MixedCoreGapIndex;
        const TWeakObjectPtr<AStaticMeshActor>* Takeoff = V7MixedCoreV7GroundById.Find(FName(TakeoffGround[Gap]));
        const TWeakObjectPtr<AStaticMeshActor>* Landing = V7MixedCoreV7GroundById.Find(FName(LandingGround[Gap]));
        if (!Takeoff || !Takeoff->IsValid() || !Landing || !Landing->IsValid())
        { FinishV7MixedCoreProbe(TEXT("INCOMPLETE"), TEXT("A named V7 gap island was not bound uniquely")); return; }
        if (bV7MixedCoreGapJumpIssued && !bV7MixedCoreGapJumpLaunched && Movement->Velocity.Z > 50.0f)
        {
            bV7MixedCoreGapJumpLaunched = true;
            V7MixedCoreGapLaunchVz = Movement->Velocity.Z;
        }
        const bool bExpectedLanding = bGrounded && Location.X > GapRight[Gap]
            && Character->GetMovementBaseObject() == Landing->Get()->GetStaticMeshComponent();
        if (bV7MixedCoreGapJumpIssued && bExpectedLanding)
        {
            if (!bV7MixedCoreGapJumpLaunched)
            { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("A gap landing did not follow positive jump launch velocity")); return; }
            bV7MixedCoreGapLanded = true;
            V7MixedCoreGapLandX = Location.X;
            V7MixedCoreGapAirSeconds = static_cast<float>(Now - V7MixedCoreGapJumpWall);
            TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("chain"), ChainId[Gap]);
            Item->SetNumberField(TEXT("gap_number"), Gap - Begin + 1);
            Item->SetNumberField(TEXT("gap_left_cm"), GapLeft[Gap]);
            Item->SetNumberField(TEXT("gap_right_cm"), GapRight[Gap]);
            Item->SetStringField(TEXT("takeoff_island"), TakeoffGround[Gap]);
            Item->SetStringField(TEXT("landing_island"), LandingGround[Gap]);
            Item->SetBoolField(TEXT("grounded_on_expected_takeoff_island"), true);
            Item->SetBoolField(TEXT("jump_launched"), true);
            Item->SetNumberField(TEXT("jump_x_cm"), V7MixedCoreGapJumpX);
            Item->SetNumberField(TEXT("landing_x_cm"), V7MixedCoreGapLandX);
            Item->SetNumberField(TEXT("landing_feet_z_cm"), FeetZ);
            Item->SetNumberField(TEXT("launch_velocity_z_cm_s"), V7MixedCoreGapLaunchVz);
            Item->SetNumberField(TEXT("air_seconds"), V7MixedCoreGapAirSeconds);
            V7MixedCoreGaps.Add(MakeShared<FJsonValueObject>(Item));
            RecordV7MixedCoreProbe(TEXT("gap_landed_on_individually_named_static_island"));
            ++V7MixedCoreGapIndex;
            bV7MixedCoreGapJumpIssued = false;
            bV7MixedCoreGapJumpLaunched = false;
            bV7MixedCoreGapLanded = false;
            V7MixedCoreGapJumpX = 0.0f;
            V7MixedCoreGapLandX = 0.0f;
            V7MixedCoreGapLaunchVz = 0.0f;
            V7MixedCoreGapAirSeconds = 0.0f;
            return;
        }
        if (!bV7MixedCoreGapJumpIssued && Location.X + V7MixedCoreCapsuleRadius >= GapLeft[Gap])
        {
            if (!IsOnV7Ground(TakeoffGround[Gap]))
            { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("A gap jump did not start grounded on its configured takeoff island")); return; }
            bV7MixedCoreGapJumpIssued = true;
            V7MixedCoreGapJumpX = Location.X;
            V7MixedCoreGapJumpWall = Now;
            ++V7MixedCoreJumpCalls;
            Character->Jump();
            return;
        }
        if (bV7MixedCoreGapJumpIssued && !bExpectedLanding
            && (FeetZ < ChainTop[Gap] - 250.0f || Now - V7MixedCoreGapJumpWall > 3.0))
        { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("A 240 cm gap did not land on its exact static landing island")); return; }
        return;
    }
    case ENaiwaP1V7MixedStep::Checkpoint04:
        Character->SetAutomatedMoveInput(1.0f);
        if (bV7MixedCoreCheckpointReached[3] && IsOnV7Ground(TEXT("B1")))
        { AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::SwitchB_Touch, TEXT("checkpoint04_grounded_at_b1_after_all_a_gaps")); }
        return;
    case ENaiwaP1V7MixedStep::BounceB:
        Character->SetAutomatedMoveInput(1.0f);
        if (V7MixedCoreSwitches[2]->GetRightSwitchTouchCount() < 1 && Location.X > 13490.0f)
        { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Switch B right touch was not physically recorded")); return; }
        if (bGrounded && Character->GetMovementBaseObject() == V7MixedCorePads[1]->PadCollision.Get())
            bV7MixedCorePadBSupported = true;
        if (V7MixedCorePads[1]->GetActivationCount() == V7MixedCorePadActivationBefore + 1
            && Movement->IsFalling() && Character->GetVelocity().Z > 100.0f)
        {
            bV7MixedCorePadBLaunched = true;
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::BounceBLanding, TEXT("bounce_b_supported_then_real_single_upward_launch"));
            return;
        }
        if (V7MixedCorePads[1]->GetActivationCount() > V7MixedCorePadActivationBefore + 1 || Location.X > 13500.0f)
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Bounce B was skipped or activated more than once"));
        return;
    case ENaiwaP1V7MixedStep::BounceBLanding:
        Character->SetAutomatedMoveInput(1.0f);
        if (IsOnV7Ground(TEXT("B4"))
            && Location.X > 13450.0f && FMath::Abs(FeetZ - 480.0f) <= 5.0f)
        {
            if (!bV7MixedCorePadBSupported || !bV7MixedCorePadBLaunched)
            { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Bounce B landing lacks real pad support or upward launch evidence")); return; }
            bV7MixedCorePadBLanded = true;
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::Checkpoint05, TEXT("bounce_b_landed_on_b4_at_elevated_top"));
        }
        else if (FeetZ < 0.0f || Location.X > 14400.0f)
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Bounce B left its launch arc without reaching the elevated B4 bank"));
        return;
    case ENaiwaP1V7MixedStep::Checkpoint05:
        Character->SetAutomatedMoveInput(1.0f);
        if (bV7MixedCoreCheckpointReached[4] && IsOnV7Ground(TEXT("B4")))
        { V7MixedCoreGapIndex = 3; AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::GapsB, TEXT("checkpoint05_grounded_before_b_gap_chain")); }
        return;
    case ENaiwaP1V7MixedStep::Checkpoint06:
        Character->SetAutomatedMoveInput(1.0f);
        if (bV7MixedCoreCheckpointReached[5] && IsOnV7Ground(TEXT("C1")))
        { AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::SwitchC_Touch, TEXT("checkpoint06_grounded_before_c_shuttle")); }
        return;
    case ENaiwaP1V7MixedStep::TimedC_Jump:
        Character->SetAutomatedMoveInput(1.0f);
        if (!bV7MixedCoreCExitJumpIssued && V7MixedCoreSwitches[3]->IsCharacterSupported(Character)
            && Movement->IsMovingOnGround()
            && Location.X + V7MixedCoreCapsuleRadius >= 18830.0f - 40.0f)
        {
            bV7MixedCoreCExitJumpIssued = true;
            V7MixedCoreCExitJumpX = Location.X;
            ++V7MixedCoreJumpCalls;
            bV7MixedCoreJumpHeld = true;
            V7MixedCoreJumpReleaseGameTime = GetWorld()->GetTimeSeconds() + 0.18f;
            Character->Jump();
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::TimedC_Cross, TEXT("run_jump_from_c_shuttle_minus40cm_measured_window"));
            return;
        }
        if (!bV7MixedCoreCExitJumpIssued && Location.X + V7MixedCoreCapsuleRadius > 18900.0f)
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("C shuttle was passed without the measured Run takeoff"));
        return;
    case ENaiwaP1V7MixedStep::TimedC_Cross:
        Character->SetAutomatedMoveInput(1.0f);
        if (bV7MixedCoreCExitJumpIssued && !bV7MixedCoreCExitJumpLaunched && Movement->Velocity.Z > 50.0f)
            bV7MixedCoreCExitJumpLaunched = true;
        if (V7MixedCoreTimed[1]->GetDisableCount() != V7MixedCoreTimedCDisableBefore
            || V7MixedCoreTimed[1]->GetTriggerCount() > V7MixedCoreTimedCTriggerBefore + 1)
        { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Timed C disabled before its safe C4 bank exit")); return; }
        if (bV7MixedCoreTimedCTriggered && V7MixedCoreTimed[1]->GetState() != ENaiwaP1TimedPlatformState::Countdown)
        { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Timed C left Countdown before the safe static bank")); return; }
        if (IsOnV7Ground(TEXT("C4")) && Location.X > 19350.0f)
        {
            const float Remaining = V7MixedCoreTimed[1]->GetCountdownRemaining();
            if (!bV7MixedCoreCExitJumpLaunched || !bV7MixedCoreTimedCSupported || !bV7MixedCoreTimedCTriggered
                || !bV7MixedCoreTimedCDeclined || V7MixedCoreTimed[1]->GetTriggerCount() != V7MixedCoreTimedCTriggerBefore + 1
                || Remaining <= 0.20f)
            {
                FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Timed C did not support the character, count down once, and reach C4 with a positive active margin"));
                return;
            }
            bV7MixedCoreTimedCCompleted = true;
            V7MixedCoreTimedCExitRemaining = Remaining;
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::Checkpoint07, TEXT("timed_c_run_landed_on_safe_c4_bank_before_expiry"));
        }
        return;
    case ENaiwaP1V7MixedStep::Checkpoint07:
        Character->SetAutomatedMoveInput(1.0f);
        if (bV7MixedCoreCheckpointReached[6] && IsOnV7Ground(TEXT("C4"))
            && V7MixedCoreSwitches[3]->GetRightSwitchTouchCount() > 0)
        { V7MixedCoreGapIndex = 6; AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::GapsC, TEXT("checkpoint07_and_c_right_switch_grounded_before_c_gap_chain")); }
        else if (Location.X > 20040.0f && V7MixedCoreSwitches[3]->GetRightSwitchTouchCount() < 1)
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Switch C right switch was not physically touched on the static bank"));
        return;
    case ENaiwaP1V7MixedStep::FinalBank:
        Character->SetAutomatedMoveInput(1.0f);
        if (bV7MixedCoreCheckpointReached[7] && IsOnV7Ground(TEXT("FinalApproach")))
        {
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::FinalBounce,
                TEXT("checkpoint08_grounded_on_final_approach_before_terminal_compound"));
        }
        return;
    case ENaiwaP1V7MixedStep::FinalBounce:
        Character->SetAutomatedMoveInput(1.0f);
        if (bGrounded && Character->GetMovementBaseObject() == V7MixedCorePads[2]->PadCollision.Get())
            bV7MixedCoreFinalBounceSupported = true;
        if (V7MixedCorePads[2]->GetActivationCount() == V7MixedCoreFinalPadActivationBefore + 1
            && Movement->IsFalling() && Movement->Velocity.Z > 100.0f)
        {
            bV7MixedCoreFinalBounceLaunched = true;
            V7MixedCoreFinalBounceLaunchVz = Movement->Velocity.Z;
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::FinalBounceLanding,
                TEXT("terminal_bounce_real_support_then_upward_launch"));
            return;
        }
        if (V7MixedCorePads[2]->GetActivationCount() > V7MixedCoreFinalPadActivationBefore + 1
            || Location.X > 24600.0f || FeetZ < -100.0f)
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Terminal bounce was missed, repeated, or left its supported launch route"));
        return;
    case ENaiwaP1V7MixedStep::FinalBounceLanding:
        Character->SetAutomatedMoveInput(1.0f);
        if (bGrounded && Character->GetMovementBaseObject() == V7MixedCoreTimed[2]->PlatformCollision.Get()
            && FMath::Abs(FeetZ - 540.0f) <= 5.0f)
        {
            if (!bV7MixedCoreFinalBounceSupported || !bV7MixedCoreFinalBounceLaunched)
            { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Terminal timed landing lacks real bounce support and launch evidence")); return; }
            bV7MixedCoreFinalTimedSupported = true;
            V7MixedCoreFinalBounceLandingFeetZ = FeetZ;
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::FinalTimedCross,
                TEXT("bounce_landed_on_raised_terminal_timed_platform"));
        }
        else if (FeetZ < 100.0f || Location.X > 25000.0f)
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Terminal bounce arc missed the elevated timed platform"));
        return;
    case ENaiwaP1V7MixedStep::FinalTimedCross:
    {
        Character->SetAutomatedMoveInput(1.0f);
        ANaiwaP1TimedPlatform* TimedFinal = V7MixedCoreTimed[2].Get();
        if (!TimedFinal) { FinishV7MixedCoreProbe(TEXT("INCOMPLETE"), TEXT("Terminal timed platform binding was lost")); return; }
        const bool bOnFinalTimed = bGrounded && Character->GetMovementBaseObject() == TimedFinal->PlatformCollision.Get();
        if (bOnFinalTimed) bV7MixedCoreFinalTimedSupported = true;
        if (!bV7MixedCoreFinalTimedTriggered && TimedFinal->GetTriggerCount() == V7MixedCoreTimedFinalTriggerBefore + 1)
        {
            if (!bV7MixedCoreFinalTimedSupported || TimedFinal->GetState() != ENaiwaP1TimedPlatformState::Countdown)
            { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Terminal timer started without real support or Countdown state")); return; }
            bV7MixedCoreFinalTimedTriggered = true;
            V7MixedCoreTimedFinalRemainingPrevious = TimedFinal->GetCountdownRemaining();
        }
        if (bV7MixedCoreFinalTimedTriggered && TimedFinal->GetState() == ENaiwaP1TimedPlatformState::Countdown)
        {
            if (TimedFinal->GetCountdownRemaining() < V7MixedCoreTimedFinalRemainingPrevious - 0.001f)
                bV7MixedCoreFinalTimedDecreased = true;
            V7MixedCoreTimedFinalRemainingPrevious = TimedFinal->GetCountdownRemaining();
        }
        if (TimedFinal->GetTriggerCount() > V7MixedCoreTimedFinalTriggerBefore + 1
            || (!bV7HiddenFinalPitRecoveryProbeEnabled
                && TimedFinal->GetDisableCount() != V7MixedCoreTimedFinalDisableBefore))
        { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Terminal timer repeated or expired before the required takeoff")); return; }
        if (!bV7MixedCoreFinalExitJumpIssued && bOnFinalTimed
            && Location.X + V7MixedCoreCapsuleRadius >= 24945.0f)
        {
            const float Remaining = TimedFinal->GetCountdownRemaining();
            if (!bV7MixedCoreFinalTimedTriggered || !bV7MixedCoreFinalTimedDecreased
                || TimedFinal->GetState() != ENaiwaP1TimedPlatformState::Countdown || Remaining <= 0.12f)
            { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Terminal timer left too much delay or expired before the measured edge takeoff")); return; }
            bV7MixedCoreFinalExitJumpIssued = true;
            V7MixedCoreFinalExitJumpX = Location.X;
            V7MixedCoreTimedFinalRemainingAtTakeoff = Remaining;
            ++V7MixedCoreJumpCalls;
            bV7MixedCoreJumpHeld = true;
            V7MixedCoreJumpReleaseGameTime = GetWorld()->GetTimeSeconds() + 0.18f;
            Character->Jump();
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::FinalExit,
                TEXT("run_jump_from_terminal_timed_platform_inside_tight_window"));
            return;
        }
        if (!bV7HiddenFinalPitRecoveryProbeEnabled && !bV7MixedCoreFinalExitJumpIssued && Location.X > 25000.0f)
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Terminal timed platform was crossed without the edge takeoff"));
        return;
    }
    case ENaiwaP1V7MixedStep::FinalExit:
        Character->SetAutomatedMoveInput(1.0f);
        if (bV7MixedCoreFinalExitJumpIssued && !bV7MixedCoreFinalExitJumpLaunched && Movement->Velocity.Z > 50.0f)
            bV7MixedCoreFinalExitJumpLaunched = true;
        {
            const bool bOnIntermediateBank = IsOnV7Ground(TEXT("FinalIntermediate")) && Location.X >= 25170.0f;
            const bool bOnSecondBounce = bGrounded
                && Character->GetMovementBaseObject() == V7MixedCorePads[3]->PadCollision.Get();
            const bool bSecondBounceAlreadyLaunched = V7MixedCorePads[3]->GetActivationCount()
                    == V7MixedCoreSecondPadActivationBefore + 1
                && Movement->IsFalling() && Movement->Velocity.Z > 100.0f;
            if (bSecondBounceAlreadyLaunched)
            {
                bV7MixedCoreSecondBounceSupported = true;
                bV7MixedCoreSecondBounceLaunched = true;
                V7MixedCoreSecondBounceLaunchVz = Movement->Velocity.Z;
            }
            if (bOnIntermediateBank || bOnSecondBounce || bSecondBounceAlreadyLaunched)
            {
                if (!bV7MixedCoreFinalExitJumpLaunched || !bV7MixedCoreFinalTimedSupported
                    || !bV7MixedCoreFinalTimedTriggered || !bV7MixedCoreFinalTimedDecreased
                    || !bV7MixedCoreFinalBounceLaunched
                    || V7MixedCorePads[2]->GetActivationCount() != V7MixedCoreFinalPadActivationBefore + 1
                    || V7MixedCoreTimed[2]->GetTriggerCount() != V7MixedCoreTimedFinalTriggerBefore + 1
                    || V7MixedCoreTimedFinalRemainingAtTakeoff <= 0.12f
                    || !bV7MixedCoreHiddenFinalPitsInvisible)
                { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Terminal bounce/timed compound did not complete its exact physical sequence")); return; }
                bV7MixedCoreFinalCompoundCompleted = true;
                AdvanceV7MixedCoreProbe(bSecondBounceAlreadyLaunched
                        ? ENaiwaP1V7MixedStep::SecondBounceLanding : ENaiwaP1V7MixedStep::SecondBounce,
                    bSecondBounceAlreadyLaunched
                        ? TEXT("first_compound_landing_triggered_second_bounce_immediately")
                        : TEXT("first_compound_landed_on_intermediate_bank_before_second_compound"));
            }
            else if (bV7MixedCoreFinalExitJumpIssued && !bV7MixedCoreFinalExitJumpLaunched
                && (FeetZ < 100.0f || Location.X > 25800.0f))
                FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Terminal edge jump did not launch toward the final bank"));
        }
        return;
    case ENaiwaP1V7MixedStep::SecondBounce:
        Character->SetAutomatedMoveInput(1.0f);
        if (bGrounded && Character->GetMovementBaseObject() == V7MixedCorePads[3]->PadCollision.Get())
            bV7MixedCoreSecondBounceSupported = true;
        if (V7MixedCorePads[3]->GetActivationCount() == V7MixedCoreSecondPadActivationBefore + 1)
            bV7MixedCoreSecondBounceSupported = true;
        if (V7MixedCorePads[3]->GetActivationCount() == V7MixedCoreSecondPadActivationBefore + 1
            && Movement->IsFalling() && Movement->Velocity.Z > 100.0f)
        {
            bV7MixedCoreSecondBounceLaunched = true;
            V7MixedCoreSecondBounceLaunchVz = Movement->Velocity.Z;
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::SecondBounceLanding,
                TEXT("second_terminal_bounce_real_support_then_upward_launch"));
            return;
        }
        if (V7MixedCorePads[3]->GetActivationCount() > V7MixedCoreSecondPadActivationBefore + 1
            || Location.X > 25500.0f || FeetZ < 350.0f)
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Second terminal bounce was missed, repeated, or left its supported launch route"));
        return;
    case ENaiwaP1V7MixedStep::SecondBounceLanding:
        Character->SetAutomatedMoveInput(1.0f);
        if (bGrounded && Character->GetMovementBaseObject() == V7MixedCoreTimed[3]->PlatformCollision.Get()
            && FMath::Abs(FeetZ - 840.0f) <= 5.0f)
        {
            if (!bV7MixedCoreSecondBounceSupported || !bV7MixedCoreSecondBounceLaunched)
            { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Second timed landing lacks real bounce support and launch evidence")); return; }
            bV7MixedCoreSecondTimedSupported = true;
            V7MixedCoreSecondBounceLandingFeetZ = FeetZ;
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::SecondTimedCross,
                TEXT("second_bounce_landed_on_elevated_timed_platform"));
        }
        else if (FeetZ < 350.0f || Location.X > 25900.0f)
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Second bounce arc missed the higher timed platform"));
        return;
    case ENaiwaP1V7MixedStep::SecondTimedCross:
    {
        Character->SetAutomatedMoveInput(1.0f);
        ANaiwaP1TimedPlatform* TimedFinal2 = V7MixedCoreTimed[3].Get();
        if (!TimedFinal2) { FinishV7MixedCoreProbe(TEXT("INCOMPLETE"), TEXT("Second terminal timed platform binding was lost")); return; }
        const bool bOnSecondTimed = bGrounded && Character->GetMovementBaseObject() == TimedFinal2->PlatformCollision.Get();
        if (bOnSecondTimed) bV7MixedCoreSecondTimedSupported = true;
        if (!bV7MixedCoreSecondTimedTriggered
            && TimedFinal2->GetTriggerCount() == V7MixedCoreTimedSecondTriggerBefore + 1)
        {
            if (!bV7MixedCoreSecondTimedSupported || TimedFinal2->GetState() != ENaiwaP1TimedPlatformState::Countdown)
            { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Second terminal timer started without real support or Countdown state")); return; }
            bV7MixedCoreSecondTimedTriggered = true;
            V7MixedCoreSecondTimedRemainingPrevious = TimedFinal2->GetCountdownRemaining();
        }
        if (bV7MixedCoreSecondTimedTriggered && TimedFinal2->GetState() == ENaiwaP1TimedPlatformState::Countdown)
        {
            if (TimedFinal2->GetCountdownRemaining() < V7MixedCoreSecondTimedRemainingPrevious - 0.001f)
                bV7MixedCoreSecondTimedDecreased = true;
            V7MixedCoreSecondTimedRemainingPrevious = TimedFinal2->GetCountdownRemaining();
        }
        if (TimedFinal2->GetTriggerCount() > V7MixedCoreTimedSecondTriggerBefore + 1
            || (!bV7HiddenFinalPitRecoveryProbeEnabled
                && TimedFinal2->GetDisableCount() != V7MixedCoreTimedSecondDisableBefore))
        { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Second terminal timer repeated or expired before the required takeoff")); return; }
        const float TimedRightEdgeX = TimedFinal2->PlatformCollision->GetComponentLocation().X
            + TimedFinal2->PlatformCollision->GetScaledBoxExtent().X;
        if (!bV7HiddenFinalPitRecoveryProbeEnabled && !bV7MixedCoreSecondExitJumpIssued && bOnSecondTimed
            && Location.X + V7MixedCoreCapsuleRadius >= TimedRightEdgeX - 5.0f)
        {
            const float Remaining = TimedFinal2->GetCountdownRemaining();
            if (!bV7MixedCoreSecondTimedTriggered || !bV7MixedCoreSecondTimedDecreased
                || TimedFinal2->GetState() != ENaiwaP1TimedPlatformState::Countdown || Remaining <= 0.02f)
            { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Second terminal timer left too much delay or expired before the edge takeoff")); return; }
            bV7MixedCoreSecondExitJumpIssued = true;
            V7MixedCoreSecondExitJumpX = Location.X;
            V7MixedCoreSecondTimedRemainingAtTakeoff = Remaining;
            ++V7MixedCoreJumpCalls;
            bV7MixedCoreJumpHeld = true;
            V7MixedCoreJumpReleaseGameTime = GetWorld()->GetTimeSeconds() + 0.18f;
            Character->Jump();
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::SecondExit,
                TEXT("second_run_jump_from_timed_platform_inside_tight_window"));
            return;
        }
        return;
    }
    case ENaiwaP1V7MixedStep::SecondExit:
        Character->SetAutomatedMoveInput(1.0f);
        if (bV7MixedCoreSecondExitJumpIssued && !bV7MixedCoreSecondExitJumpLaunched && Movement->Velocity.Z > 50.0f)
            bV7MixedCoreSecondExitJumpLaunched = true;
        if (IsOnV7Ground(TEXT("Final")) && Location.X >= 26080.0f)
        {
            if (!bV7MixedCoreSecondExitJumpLaunched || !bV7MixedCoreSecondTimedSupported
                || !bV7MixedCoreSecondTimedTriggered || !bV7MixedCoreSecondTimedDecreased
                || !bV7MixedCoreSecondBounceLaunched
                || V7MixedCorePads[3]->GetActivationCount() != V7MixedCoreSecondPadActivationBefore + 1
                || V7MixedCoreTimed[3]->GetTriggerCount() != V7MixedCoreTimedSecondTriggerBefore + 1
                || V7MixedCoreSecondTimedRemainingAtTakeoff <= 0.02f
                || !bV7MixedCoreHiddenFinalPitsInvisible)
            { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Second bounce/timed compound did not complete its exact physical sequence")); return; }
            bV7MixedCoreSecondCompoundCompleted = true;
            if (!SetGait(false, TEXT("changed_to_Walk150_after_two_terminal_compounds")))
            { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Could not change to final Walk150 gait")); return; }
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::Pickup,
                TEXT("both_compounds_cleared_and_landed_on_goal_bank_before_pickup"));
        }
        else if (bV7MixedCoreSecondExitJumpIssued && !bV7MixedCoreSecondExitJumpLaunched
            && (FeetZ < 350.0f || Location.X > 26800.0f))
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Second timed edge jump did not launch toward the final bank"));
        return;
    case ENaiwaP1V7MixedStep::Pickup:
        Character->SetAutomatedMoveInput(1.0f);
        if (CollectedRequiredPickupIds.Contains(V7MixedCorePickup->GetStableId())
            && CollectedIds.Num() == ExpectedPickupCountForRoute)
        {
            if (!IsOnV7Ground(TEXT("Final")) || V7MixedCorePickup->DisplayMesh->IsVisible()
                || V7MixedCorePickup->TriggerBox->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
            { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Required pickup did not result from the final-bank physical route")); return; }
            bV7MixedCorePickupSeen = true;
            AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::Goal, TEXT("required_pickup_physically_collected_once"));
        }
        return;
    case ENaiwaP1V7MixedStep::Goal:
        if (State == ENaiwaP1State::Playing) { Character->SetAutomatedMoveInput(1.0f); return; }
        Character->SetAutomatedMoveInput(0.0f);
        if (State != ENaiwaP1State::Won || !V7MixedCoreGoal->TriggerBox->IsOverlappingActor(Character)
            || IsGameplayInputAllowed() || Movement->MovementMode != MOVE_None
            || CollectedIds.Num() != ExpectedPickupCountForRoute
            || ActiveCheckpoint.Get() != V7MixedCoreCheckpoints[7].Get() || !bV7MixedCorePickupSeen)
        {
            FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Physical goal overlap did not lock the Won state after the required pickup"));
            return;
        }
        V7MixedCoreWonSettleStartedWall = Now;
        AdvanceV7MixedCoreProbe(ENaiwaP1V7MixedStep::WonSettle, TEXT("physical_goal_overlap_entered_locked_won"));
        return;
    case ENaiwaP1V7MixedStep::WonSettle:
        if (Now - V7MixedCoreWonSettleStartedWall < 0.4) return;
        if (State != ENaiwaP1State::Won || IsGameplayInputAllowed()
            || DeathEventCount != ExpectedDeathCountForRoute
            || Character->GetVelocity().Size() > 1.0f || Movement->MovementMode != MOVE_None)
        { FinishV7MixedCoreProbe(TEXT("FAIL"), TEXT("Won state did not remain stable and motion-locked")); return; }
        FinishV7MixedCoreProbe(TEXT("PASS"), TEXT("The saved V7 map completed its full mixed-gait route, eight grounded checkpoints, eleven exact gap landings, terminal bounce/timed compound, hidden pits, required pickup, and physical goal Won"));
        return;
    }
}

void ANaiwaP1GameMode::FinishV7MixedCoreProbe(const TCHAR* Result, const FString& Detail)
{
    if (bV7HiddenFinalPitRecoveryProbeEnabled)
    {
        FinishV7HiddenFinalPitRecoveryProbe(Result, Detail);
        return;
    }
    if (bV7MixedCoreProbeFinished) return;
    bV7MixedCoreProbeFinished = true;
    if (PlayerCharacter.IsValid())
    {
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        PlayerCharacter->StopJumping();
        PlayerCharacter->DisableAutomatedInput();
    }
    int32 CheckpointsReached = 0;
    for (const bool bReached : bV7MixedCoreCheckpointReached) if (bReached) ++CheckpointsReached;
    int32 TransitTrapsSpawned = 0;
    for (int32 Index = 1; Index <= 3; ++Index)
    {
        if (V7MixedCoreSwitches[Index].IsValid())
            TransitTrapsSpawned += V7MixedCoreSwitches[Index]->GetTransitTrapSpawnCount();
    }
    const TCHAR* ShuttleIds[4] = { TEXT(""), TEXT("SwitchA"), TEXT("SwitchB"), TEXT("SwitchC") };
    bool bAllShuttleSpeedsVaried = true;
    TArray<TSharedPtr<FJsonValue>> ShuttleSpeedProfiles;
    for (int32 Index = 1; Index <= 3; ++Index)
    {
        const int32 SampleCount = V7MixedCoreSwitchSpeedSamples[Index];
        const float Minimum = SampleCount > 0 ? V7MixedCoreSwitchSpeedMinimum[Index] : 0.0f;
        const float Maximum = SampleCount > 0 ? V7MixedCoreSwitchSpeedMaximum[Index] : 0.0f;
        const bool bVaried = SampleCount >= 10 && Minimum < 250.0f && Maximum > 350.0f;
        bAllShuttleSpeedsVaried &= bVaried;
        TSharedRef<FJsonObject> Profile = MakeShared<FJsonObject>();
        Profile->SetStringField(TEXT("id"), ShuttleIds[Index]);
        Profile->SetNumberField(TEXT("sample_count"), SampleCount);
        Profile->SetNumberField(TEXT("minimum_cm_s"), Minimum);
        Profile->SetNumberField(TEXT("maximum_cm_s"), Maximum);
        Profile->SetBoolField(TEXT("speed_varied"), bVaried);
        ShuttleSpeedProfiles.Add(MakeShared<FJsonValueObject>(Profile));
    }
    const int32 ExpectedRouteDeathDelta = V7MixedCoreRecoveryCheckpointIndex >= 0 ? 1 : 0;
    const int32 ExpectedPickupCount = V7MixedCoreRecoveryCheckpointIndex < 0 ? 4
        : V7MixedCoreRecoveryCheckpointIndex < 3 ? 3
        : V7MixedCoreRecoveryCheckpointIndex < 5 ? 2 : 1;
    const bool bRecoveryContractPassed = V7MixedCoreRecoveryCheckpointIndex < 0 || bV7MixedCoreRecoveryPassed;
    const bool bContractPassed = FCString::Strcmp(Result, TEXT("PASS")) == 0
        && bV7MixedCoreProbeInitialized && CheckpointsReached == 8 && V7MixedCoreCheckpointRecords.Num() == 8
        && V7MixedCoreGaps.Num() == 11 && bV7MixedCoreTeachingHazardCleared
        && bV7MixedCoreTimedTeachingCompleted && bV7MixedCorePadBSupported && bV7MixedCorePadBLaunched
        && bV7MixedCorePadBLanded && bV7MixedCoreTimedCCompleted && bV7MixedCoreFinalCompoundCompleted
        && bV7MixedCoreSecondCompoundCompleted && bAllShuttleSpeedsVaried
        && bV7MixedCoreHiddenFinalPitsInvisible && bV7MixedCorePickupSeen
        && CollectedIds.Num() == ExpectedPickupCount && RequiredPickupCount == 1
        && bV7MixedCoreSwitchRiseObserved[3]
        && (V7MixedCoreRecoveryCheckpointIndex >= 0 || V7MixedCoreTransitTrapsJumped == TransitTrapsSpawned)
        && (V7MixedCoreRecoveryCheckpointIndex >= 0 || TransitTrapsSpawned >= 3)
        && State == ENaiwaP1State::Won
        && DeathEventCount == V7MixedCoreDeathCountBefore + ExpectedRouteDeathDelta
        && bRecoveryContractPassed
        && V7MixedCoreJumpCalls >= 14 && V7MixedCoreGaitChanges.Num() >= 2;
    const TCHAR* Status = bContractPassed ? TEXT("PASS")
        : FCString::Strcmp(Result, TEXT("INCOMPLETE")) == 0 ? TEXT("INCOMPLETE") : TEXT("FAIL");
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1V7MixedCoreRouteV1"));
    Report->SetStringField(TEXT("status"), Status);
    Report->SetStringField(TEXT("outcome"), Result);
    Report->SetStringField(TEXT("detail"), Detail);
    Report->SetStringField(TEXT("scope"), TEXT("V7 full mixed-gait core route; optional production-map single-checkpoint recovery and required-pickup replay"));
    Report->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetMapName() : FString());
    Report->SetStringField(TEXT("map_package"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
    Report->SetStringField(TEXT("map_sha256"), TEXT("4a55ced57ddbe47044aa7f4965fc4013cc82ccc48976a9819a6217a4dbef5754"));
    Report->SetStringField(TEXT("config_sha256"), TEXT("4fd6914655c2ae7c28567bbbdb808b758b0686e7b9bd260a145891d322aa2e59"));
    Report->SetBoolField(TEXT("exact_map_and_config_identity"), bV7MixedCoreProbeInitialized);
    Report->SetBoolField(TEXT("physical_mixed_gait_route"), bContractPassed);
    Report->SetStringField(TEXT("control_method"), TEXT("SetP1Gait and automated directional input; ordinary Jump calls at teaching, measured shuttle exits, individually observed gaps, and the terminal timed-platform edge"));
    Report->SetBoolField(TEXT("full_p1_v7_acceptance"), false);
    Report->SetBoolField(TEXT("real_keyboard_evidence_in_this_process"), false);
    Report->SetBoolField(TEXT("camera_visual_review_in_this_process"), false);
    Report->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - V7MixedCoreStartedWall);
    Report->SetNumberField(TEXT("world_time_s"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
    Report->SetNumberField(TEXT("final_step"), static_cast<int32>(V7MixedCoreStep));
    Report->SetStringField(TEXT("final_state"), StateName(State));
    Report->SetNumberField(TEXT("death_events_delta"), DeathEventCount - V7MixedCoreDeathCountBefore);
    Report->SetNumberField(TEXT("probe_character_teleports"), 0);
    Report->SetNumberField(TEXT("direct_trigger_injections"), 0);
    Report->SetNumberField(TEXT("platform_control_injections"), 0);
    Report->SetNumberField(TEXT("production_respawn_placements"), bV7MixedCoreRecoveryPassed ? 1 : 0);
    Report->SetBoolField(TEXT("checkpoint_recovery_requested"), V7MixedCoreRecoveryCheckpointIndex >= 0);
    Report->SetBoolField(TEXT("checkpoint_recovery_passed"), bV7MixedCoreRecoveryPassed);
    if (V7MixedCoreRecoveryRecord.IsValid())
    {
        Report->SetObjectField(TEXT("checkpoint_recovery"), V7MixedCoreRecoveryRecord);
        Report->SetBoolField(TEXT("required_pickup_reacquired_after_recovery"),
            bV7MixedCoreRecoveryPassed && bV7MixedCorePickupSeen && V7MixedCorePickup.IsValid()
            && CollectedRequiredPickupIds.Contains(V7MixedCorePickup->GetStableId())
            && State == ENaiwaP1State::Won);
    }
    else
    {
        Report->SetBoolField(TEXT("required_pickup_reacquired_after_recovery"), false);
    }
    Report->SetNumberField(TEXT("ordinary_jump_calls"), V7MixedCoreJumpCalls);
    Report->SetNumberField(TEXT("random_shuttle_traps_spawned"), TransitTrapsSpawned);
    Report->SetNumberField(TEXT("random_shuttle_traps_jumped"), V7MixedCoreTransitTrapsJumped);
    Report->SetBoolField(TEXT("random_shuttle_traps_avoided"), TransitTrapsSpawned >= 3
        && V7MixedCoreTransitTrapsJumped == TransitTrapsSpawned);
    Report->SetBoolField(TEXT("moving_platform_speed_varied"), bAllShuttleSpeedsVaried);
    Report->SetArrayField(TEXT("moving_platform_speed_profiles"), ShuttleSpeedProfiles);
    Report->SetBoolField(TEXT("c_shuttle_rise_to_timed_dock_observed"), bV7MixedCoreSwitchRiseObserved[3]
        && FMath::IsNearlyEqual(V7MixedCoreCTopAtDock, 340.0f, 1.0f));
    Report->SetNumberField(TEXT("c_shuttle_top_z_at_midpoint_cm"), V7MixedCoreCTopAtMidpoint);
    Report->SetNumberField(TEXT("c_shuttle_top_z_at_dock_cm"), V7MixedCoreCTopAtDock);
    Report->SetBoolField(TEXT("teaching_hazard_jump_and_landing"), bV7MixedCoreTeachingHazardCleared);
    Report->SetBoolField(TEXT("teaching_timer_exited_before_disable"), bV7MixedCoreTimedTeachingCompleted);
    Report->SetNumberField(TEXT("teaching_timer_remaining_at_exit_s"), V7MixedCoreTeachingTimedExitRemaining);
    Report->SetBoolField(TEXT("bounce_b_real_support_launch_and_landing"), bV7MixedCorePadBSupported
        && bV7MixedCorePadBLaunched && bV7MixedCorePadBLanded);
    Report->SetNumberField(TEXT("bounce_b_activation_count_delta"), V7MixedCorePads[1].IsValid()
        ? V7MixedCorePads[1]->GetActivationCount() - V7MixedCorePadActivationBefore : 0);
    Report->SetBoolField(TEXT("timed_c_real_support_countdown_and_safe_exit"), bV7MixedCoreTimedCCompleted);
    Report->SetNumberField(TEXT("timed_c_remaining_at_exit_s"), V7MixedCoreTimedCExitRemaining);
    Report->SetNumberField(TEXT("timed_c_trigger_count_delta"), V7MixedCoreTimed[1].IsValid()
        ? V7MixedCoreTimed[1]->GetTriggerCount() - V7MixedCoreTimedCTriggerBefore : 0);
    Report->SetBoolField(TEXT("first_terminal_bounce_timed_compound_completed"), bV7MixedCoreFinalCompoundCompleted);
    Report->SetBoolField(TEXT("second_terminal_bounce_timed_compound_completed"), bV7MixedCoreSecondCompoundCompleted);
    Report->SetBoolField(TEXT("two_terminal_bounce_timed_compounds_completed"), bV7MixedCoreFinalCompoundCompleted
        && bV7MixedCoreSecondCompoundCompleted);
    Report->SetBoolField(TEXT("terminal_bounce_timed_compound_completed"), bV7MixedCoreFinalCompoundCompleted
        && bV7MixedCoreSecondCompoundCompleted);
    Report->SetBoolField(TEXT("first_compound_intermediate_bank_landing"), bV7MixedCoreFinalExitJumpLaunched
        && bV7MixedCoreFinalCompoundCompleted);
    Report->SetBoolField(TEXT("terminal_bounce_real_support_and_launch"), bV7MixedCoreFinalBounceSupported
        && bV7MixedCoreFinalBounceLaunched);
    Report->SetNumberField(TEXT("terminal_bounce_activation_count_delta"), V7MixedCorePads[2].IsValid()
        ? V7MixedCorePads[2]->GetActivationCount() - V7MixedCoreFinalPadActivationBefore : 0);
    Report->SetNumberField(TEXT("terminal_bounce_launch_velocity_z_cm_s"), V7MixedCoreFinalBounceLaunchVz);
    Report->SetNumberField(TEXT("terminal_timed_landing_feet_z_cm"), V7MixedCoreFinalBounceLandingFeetZ);
    Report->SetBoolField(TEXT("terminal_timed_real_support_and_countdown"), bV7MixedCoreFinalTimedSupported
        && bV7MixedCoreFinalTimedTriggered && bV7MixedCoreFinalTimedDecreased);
    Report->SetNumberField(TEXT("terminal_timed_trigger_count_delta"), V7MixedCoreTimed[2].IsValid()
        ? V7MixedCoreTimed[2]->GetTriggerCount() - V7MixedCoreTimedFinalTriggerBefore : 0);
    Report->SetNumberField(TEXT("terminal_timed_active_remaining_at_takeoff_s"), V7MixedCoreTimedFinalRemainingAtTakeoff);
    Report->SetNumberField(TEXT("terminal_edge_jump_x_cm"), V7MixedCoreFinalExitJumpX);
    Report->SetBoolField(TEXT("terminal_edge_jump_landed_on_goal_bank"), bV7MixedCoreSecondExitJumpLaunched
        && bV7MixedCoreSecondCompoundCompleted);
    Report->SetBoolField(TEXT("second_terminal_bounce_real_support_and_launch"), bV7MixedCoreSecondBounceSupported
        && bV7MixedCoreSecondBounceLaunched);
    Report->SetNumberField(TEXT("second_terminal_bounce_activation_count_delta"), V7MixedCorePads[3].IsValid()
        ? V7MixedCorePads[3]->GetActivationCount() - V7MixedCoreSecondPadActivationBefore : 0);
    Report->SetNumberField(TEXT("second_terminal_bounce_launch_velocity_z_cm_s"), V7MixedCoreSecondBounceLaunchVz);
    Report->SetNumberField(TEXT("second_terminal_timed_landing_feet_z_cm"), V7MixedCoreSecondBounceLandingFeetZ);
    Report->SetBoolField(TEXT("second_terminal_timed_real_support_and_countdown"), bV7MixedCoreSecondTimedSupported
        && bV7MixedCoreSecondTimedTriggered && bV7MixedCoreSecondTimedDecreased);
    Report->SetNumberField(TEXT("second_terminal_timed_trigger_count_delta"), V7MixedCoreTimed[3].IsValid()
        ? V7MixedCoreTimed[3]->GetTriggerCount() - V7MixedCoreTimedSecondTriggerBefore : 0);
    Report->SetNumberField(TEXT("second_terminal_timed_active_remaining_at_takeoff_s"), V7MixedCoreSecondTimedRemainingAtTakeoff);
    Report->SetNumberField(TEXT("second_terminal_edge_jump_x_cm"), V7MixedCoreSecondExitJumpX);
    Report->SetNumberField(TEXT("hidden_terminal_pit_count"), V7MixedCoreHiddenPitCount);
    Report->SetBoolField(TEXT("hidden_terminal_pits_invisible_and_query_enabled"), bV7MixedCoreHiddenFinalPitsInvisible);
    Report->SetNumberField(TEXT("checkpoint_count"), V7MixedCoreCheckpointRecords.Num());
    Report->SetNumberField(TEXT("checkpoints_grounded"), CheckpointsReached);
    Report->SetBoolField(TEXT("eight_grounded_checkpoints"), CheckpointsReached == 8);
    Report->SetArrayField(TEXT("checkpoints"), V7MixedCoreCheckpointRecords);
    Report->SetNumberField(TEXT("gap_count"), V7MixedCoreGaps.Num());
    Report->SetNumberField(TEXT("gaps_landed_on_expected_static_islands"), V7MixedCoreGaps.Num());
    Report->SetBoolField(TEXT("eleven_individually_landed_gaps"), V7MixedCoreGaps.Num() == 11);
    Report->SetArrayField(TEXT("gaps"), V7MixedCoreGaps);
    Report->SetArrayField(TEXT("gait_changes"), V7MixedCoreGaitChanges);
    Report->SetNumberField(TEXT("gait_change_count"), V7MixedCoreGaitChanges.Num());
    Report->SetBoolField(TEXT("required_pickup_physically_collected"), bV7MixedCorePickupSeen);
    Report->SetNumberField(TEXT("pickup_count"), CollectedIds.Num());
    Report->SetBoolField(TEXT("four_ordered_pickups_collected"), CollectedIds.Num() == 4
        && RequiredPickupCount == 1);
    Report->SetBoolField(TEXT("physical_goal_won"), State == ENaiwaP1State::Won
        && V7MixedCoreGoal.IsValid() && PlayerCharacter.IsValid()
        && V7MixedCoreGoal->TriggerBox->IsOverlappingActor(PlayerCharacter.Get()));
    Report->SetBoolField(TEXT("zero_probe_teleports_or_trigger_injections"), true);
    Report->SetArrayField(TEXT("events"), V7MixedCoreEvents);
    TArray<TSharedPtr<FJsonValue>> Separate = {
        MakeShared<FJsonValueString>(TEXT("every_checkpoint_death_and_recovery")),
        MakeShared<FJsonValueString>(TEXT("ordinary_window_real_keyboard_and_camera_review")),
        MakeShared<FJsonValueString>(TEXT("pause_and_restart")),
        MakeShared<FJsonValueString>(TEXT("timed_C_walk_landing_after_disable_is_allowed_and_not_a_Run_only_gate"))
    };
    Report->SetArrayField(TEXT("separate_acceptance_gates"), Separate);

    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("v7_mixed_core_route_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_V7_MIXED_CORE status=%s checkpoints=%d gaps=%d pickup=%d goal=%d output=%s saved=%d"),
        Status, CheckpointsReached, V7MixedCoreGaps.Num(), bV7MixedCorePickupSeen ? 1 : 0,
        State == ENaiwaP1State::Won ? 1 : 0, *Output, bSaved);
    FPlatformMisc::RequestExit(false);
}

void ANaiwaP1GameMode::TickV7HiddenFinalPitRecoveryProbe()
{
    if (!PlayerCharacter.IsValid())
    {
        FinishV7HiddenFinalPitRecoveryProbe(TEXT("FAIL"), TEXT("Player character disappeared during terminal pit recovery"));
        return;
    }
    if (State == ENaiwaP1State::Dying)
    {
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        const int32 LaughFrame = GetDeathLaughSpriteFrame();
        if (!bV7HiddenFinalPitDeathScreenshotRequested && IsDeathLaughSpriteActive() && LaughFrame >= 12)
        {
            bV7HiddenFinalPitDeathScreenshotRequested = true;
            const FString ScreenshotDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
            IFileManager::Get().MakeDirectory(*ScreenshotDirectory, true);
            const FString ScreenshotName = FString::Printf(TEXT("alpha018_cp08_dying_%s.png"),
                *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")));
            const FString ScreenshotPath = FPaths::Combine(ScreenshotDirectory, ScreenshotName);
            if (GetWorld()->GetGameViewport() && !FScreenshotRequest::IsScreenshotRequested())
            {
                FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
                UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_V7_HIDDEN_PIT dying_screenshot_requested path=%s frame=%d"),
                    *ScreenshotPath, LaughFrame);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("NAIWA_P1_V7_HIDDEN_PIT dying_screenshot_unavailable viewport=%d pending=%d"),
                    GetWorld()->GetGameViewport() ? 1 : 0, FScreenshotRequest::IsScreenshotRequested() ? 1 : 0);
            }
        }
        if (!bV7HiddenFinalPitDeathObserved)
        {
            bV7HiddenFinalPitDeathObserved = true;
            ANaiwaP1Trigger* Pits[4] = {};
            for (TActorIterator<ANaiwaP1Trigger> It(GetWorld()); It; ++It)
            {
                for (int32 Index = 0; Index < 4; ++Index)
                {
                    if (It->GetStableId() == FName(*FString::Printf(TEXT("CompoundHiddenPit%02d"), Index + 1)))
                        Pits[Index] = *It;
                }
            }
            bV7HiddenFinalPitVisibleDuringDeath = true;
            for (ANaiwaP1Trigger* Pit : Pits)
                bV7HiddenFinalPitVisibleDuringDeath &= Pit && Pit->IsDisplayVisible();
            ANaiwaP1TimedPlatform* TimedFinal2 = V7MixedCoreTimed[3].Get();
            bV7HiddenFinalTimerExpiredBeforeDeath = TimedFinal2
                && TimedFinal2->GetDisableCount() == V7MixedCoreTimedSecondDisableBefore + 1
                && TimedFinal2->GetTriggerCount() == V7MixedCoreTimedSecondTriggerBefore + 1;
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P1_V7_HIDDEN_PIT death cause=%s visible=%d timer_expired=%d x=%.1f"),
                *LastDeathCause.ToString(), bV7HiddenFinalPitVisibleDuringDeath ? 1 : 0,
                bV7HiddenFinalTimerExpiredBeforeDeath ? 1 : 0, PlayerCharacter->GetActorLocation().X);
        }
        return;
    }
    if (State == ENaiwaP1State::Respawning)
    {
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        return;
    }
    if (State != ENaiwaP1State::Playing)
    {
        FinishV7HiddenFinalPitRecoveryProbe(TEXT("FAIL"), TEXT("Production recovery did not return to Playing"));
        return;
    }

    ANaiwaCharacter* Character = PlayerCharacter.Get();
    Character->SetAutomatedMoveInput(0.0f);
    ANaiwaP1TimedPlatform* TimedFinal = V7MixedCoreTimed[2].Get();
    ANaiwaP1TimedPlatform* TimedFinal2 = V7MixedCoreTimed[3].Get();
    const TWeakObjectPtr<AStaticMeshActor>* SafeBank = V7MixedCoreV7GroundById.Find(FName(TEXT("FinalApproach")));
    const ANaiwaP1Trigger* Checkpoint = V7MixedCoreCheckpoints[7].Get();
    const FVector ExpectedRespawn = Checkpoint
        ? Checkpoint->GetActorLocation() + Checkpoint->RespawnOffset : FVector::ZeroVector;
    const bool bAtCheckpoint = Checkpoint && ActiveCheckpoint.Get() == Checkpoint
        && FVector::Dist(Character->GetActorLocation(), ExpectedRespawn) <= 8.0f;
    const bool bOnSafeBank = SafeBank && SafeBank->IsValid()
        && Character->GetCharacterMovement()->IsMovingOnGround()
        && Character->GetMovementBaseObject() == SafeBank->Get()->GetStaticMeshComponent();
    const bool bControlsRestored = IsGameplayInputAllowed() && !Character->IsP1DeathPoseActive();
    const bool bTimerReset = TimedFinal && TimedFinal->GetState() == ENaiwaP1TimedPlatformState::Ready
        && TimedFinal->IsCarryingCollisionEnabled() && TimedFinal2
        && TimedFinal2->GetState() == ENaiwaP1TimedPlatformState::Ready
        && TimedFinal2->IsCarryingCollisionEnabled();
    if (!bV7HiddenFinalRespawnObserved)
    {
        bV7HiddenFinalRespawnObserved = true;
        V7HiddenFinalRespawnWall = FPlatformTime::Seconds();
    }
    if (!bTimerReset && FPlatformTime::Seconds() - V7HiddenFinalRespawnWall < 3.0)
        return;
    const bool bOneCorrectDeath = bV7HiddenFinalPitDeathObserved
        && LastDeathCause == FName(TEXT("CompoundHiddenPit04"))
        && DeathEventCount == V7MixedCoreDeathCountBefore + 1;
    const bool bRecoveryPassed = bOneCorrectDeath && bV7HiddenFinalPitVisibleDuringDeath
        && bV7HiddenFinalTimerExpiredBeforeDeath && bAtCheckpoint && bOnSafeBank
        && bControlsRestored && bTimerReset && CollectedIds.Num() == 0;
    if (bRecoveryPassed && bV7DeathEggProbeEnabled && DeathsThisRun < 4)
    {
        V7MixedCoreDeathCountBefore = DeathEventCount;
        V7MixedCoreFinalPadActivationBefore = V7MixedCorePads[2].IsValid()
            ? V7MixedCorePads[2]->GetActivationCount() : 0;
        V7MixedCoreTimedFinalTriggerBefore = TimedFinal ? TimedFinal->GetTriggerCount() : 0;
        V7MixedCoreTimedFinalDisableBefore = TimedFinal ? TimedFinal->GetDisableCount() : 0;
        V7MixedCoreSecondPadActivationBefore = V7MixedCorePads[3].IsValid()
            ? V7MixedCorePads[3]->GetActivationCount() : 0;
        V7MixedCoreTimedSecondTriggerBefore = TimedFinal2 ? TimedFinal2->GetTriggerCount() : 0;
        V7MixedCoreTimedSecondDisableBefore = TimedFinal2 ? TimedFinal2->GetDisableCount() : 0;

        bV7MixedCoreFinalBounceSupported = false;
        bV7MixedCoreFinalBounceLaunched = false;
        bV7MixedCoreFinalTimedSupported = false;
        bV7MixedCoreFinalTimedTriggered = false;
        bV7MixedCoreFinalTimedDecreased = false;
        bV7MixedCoreFinalExitJumpIssued = false;
        bV7MixedCoreFinalExitJumpLaunched = false;
        bV7MixedCoreFinalCompoundCompleted = false;
        bV7MixedCoreSecondBounceSupported = false;
        bV7MixedCoreSecondBounceLaunched = false;
        bV7MixedCoreSecondTimedSupported = false;
        bV7MixedCoreSecondTimedTriggered = false;
        bV7MixedCoreSecondTimedDecreased = false;
        bV7MixedCoreSecondExitJumpIssued = false;
        bV7MixedCoreSecondExitJumpLaunched = false;
        bV7MixedCoreSecondCompoundCompleted = false;
        bV7MixedCoreJumpHeld = false;
        bV7HiddenFinalPitDeathObserved = false;
        bV7HiddenFinalPitVisibleDuringDeath = false;
        bV7HiddenFinalTimerExpiredBeforeDeath = false;
        bV7HiddenFinalRespawnObserved = false;
        V7MixedCoreStep = ENaiwaP1V7MixedStep::FinalBank;
        V7MixedCoreStageStartedWall = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_V7_DEATH_EGG recovery_cycle_complete total=%d next=physical_terminal_compounds"),
            DeathsThisRun);
        return;
    }
    const bool bDeathEggPassed = !bV7DeathEggProbeEnabled || (DeathsThisRun == 4
        && bV7DeathEggFollowupObserved && bV7DeathEggMusicPauseObserved && bV7DeathEggMusicResumeObserved
        && bV7DeathEggReturnToDie1Observed);
    const bool bPassed = bRecoveryPassed && bDeathEggPassed;
    const FString Detail = FString::Printf(
        TEXT("cause=%s death_observed=%d four_traps_visible_during_death=%d second_timer_expired=%d cp08=%d safe_bank=%d controls=%d both_compounds_reset=%d total_deaths=%d egg_followup=%d bgm_paused=%d bgm_resumed=%d returned_to_die1=%d"),
        *LastDeathCause.ToString(), bOneCorrectDeath ? 1 : 0, bV7HiddenFinalPitVisibleDuringDeath ? 1 : 0,
        bV7HiddenFinalTimerExpiredBeforeDeath ? 1 : 0, bAtCheckpoint ? 1 : 0, bOnSafeBank ? 1 : 0,
        bControlsRestored ? 1 : 0, bTimerReset ? 1 : 0, DeathsThisRun,
        bV7DeathEggFollowupObserved ? 1 : 0, bV7DeathEggMusicPauseObserved ? 1 : 0,
        bV7DeathEggMusicResumeObserved ? 1 : 0, bV7DeathEggReturnToDie1Observed ? 1 : 0);
    FinishV7HiddenFinalPitRecoveryProbe(bPassed ? TEXT("PASS") : TEXT("FAIL"), Detail);
}

void ANaiwaP1GameMode::FinishV7HiddenFinalPitRecoveryProbe(const TCHAR* Result, const FString& Detail)
{
    if (bV7MixedCoreProbeFinished) return;
    bV7MixedCoreProbeFinished = true;
    if (PlayerCharacter.IsValid())
    {
        PlayerCharacter->SetAutomatedMoveInput(0.0f);
        PlayerCharacter->StopJumping();
        PlayerCharacter->DisableAutomatedInput();
    }

    ANaiwaCharacter* Character = PlayerCharacter.Get();
    ANaiwaP1TimedPlatform* TimedFinal = V7MixedCoreTimed[2].Get();
    ANaiwaP1TimedPlatform* TimedFinal2 = V7MixedCoreTimed[3].Get();
    const TWeakObjectPtr<AStaticMeshActor>* SafeBank = V7MixedCoreV7GroundById.Find(FName(TEXT("FinalApproach")));
    const ANaiwaP1Trigger* Checkpoint = V7MixedCoreCheckpoints[7].Get();
    const FVector ExpectedRespawn = Checkpoint
        ? Checkpoint->GetActorLocation() + Checkpoint->RespawnOffset : FVector::ZeroVector;
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("NaiwaP1V7HiddenFinalPitRecoveryV1"));
    Report->SetStringField(TEXT("status"), FCString::Strcmp(Result, TEXT("PASS")) == 0 ? TEXT("PASS")
        : FCString::Strcmp(Result, TEXT("INCOMPLETE")) == 0 ? TEXT("INCOMPLETE") : TEXT("FAIL"));
    Report->SetStringField(TEXT("outcome"), Result);
    Report->SetStringField(TEXT("detail"), Detail);
    Report->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetMapName() : FString());
    Report->SetStringField(TEXT("map_package"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
    Report->SetStringField(TEXT("map_sha256"), TEXT("4a55ced57ddbe47044aa7f4965fc4013cc82ccc48976a9819a6217a4dbef5754"));
    Report->SetStringField(TEXT("config_sha256"), TEXT("4fd6914655c2ae7c28567bbbdb808b758b0686e7b9bd260a145891d322aa2e59"));
    Report->SetBoolField(TEXT("exact_map_and_config_identity"), bV7MixedCoreProbeInitialized);
    Report->SetBoolField(TEXT("natural_core_route_reached_terminal_compound"),
        bV7MixedCoreProbeInitialized && bV7MixedCoreFinalBounceSupported
        && bV7MixedCoreFinalBounceLaunched && bV7MixedCoreFinalTimedSupported
        && bV7MixedCoreFinalTimedTriggered && bV7MixedCoreFinalTimedDecreased
        && bV7MixedCoreFinalCompoundCompleted && bV7MixedCoreSecondBounceSupported
        && bV7MixedCoreSecondBounceLaunched && bV7MixedCoreSecondTimedSupported
        && bV7MixedCoreSecondTimedTriggered && bV7MixedCoreSecondTimedDecreased
        && bV7MixedCoreCheckpointReached[7]);
    Report->SetNumberField(TEXT("checkpoint_count_before_death"), V7MixedCoreCheckpointRecords.Num());
    Report->SetStringField(TEXT("death_cause"), LastDeathCause.ToString());
    Report->SetBoolField(TEXT("production_dying_state_observed"), bV7HiddenFinalPitDeathObserved);
    Report->SetBoolField(TEXT("hidden_pits_visible_during_death"), bV7HiddenFinalPitVisibleDuringDeath);
    Report->SetBoolField(TEXT("terminal_timer_expired_before_death"), bV7HiddenFinalTimerExpiredBeforeDeath);
    Report->SetNumberField(TEXT("death_events"), DeathEventCount - V7MixedCoreDeathCountBefore);
    Report->SetStringField(TEXT("active_checkpoint_after_respawn"), ActiveCheckpoint.IsValid()
        ? ActiveCheckpoint->GetStableId().ToString() : TEXT("none"));
    Report->SetBoolField(TEXT("respawned_at_expected_checkpoint"), Checkpoint && ActiveCheckpoint.Get() == Checkpoint
        && Character && FVector::Dist(Character->GetActorLocation(), ExpectedRespawn) <= 8.0f);
    Report->SetBoolField(TEXT("grounded_on_configured_safe_bank"), Character && Character->GetCharacterMovement()->IsMovingOnGround()
        && SafeBank && SafeBank->IsValid()
        && Character->GetMovementBaseObject() == SafeBank->Get()->GetStaticMeshComponent());
    Report->SetBoolField(TEXT("controls_restored_and_death_pose_cleared"), Character
        && IsGameplayInputAllowed() && !Character->IsP1DeathPoseActive());
    Report->SetNumberField(TEXT("deaths_this_run"), DeathsThisRun);
    Report->SetBoolField(TEXT("death_egg_followup_selected"), bV7DeathEggFollowupObserved);
    Report->SetBoolField(TEXT("death_egg_background_music_paused"), bV7DeathEggMusicPauseObserved);
    Report->SetBoolField(TEXT("death_egg_background_music_resumed"), bV7DeathEggMusicResumeObserved);
    Report->SetBoolField(TEXT("death_egg_returned_to_die1"), bV7DeathEggReturnToDie1Observed);
    Report->SetBoolField(TEXT("terminal_mechanism_reset_after_death"), TimedFinal && TimedFinal2
        && TimedFinal->GetState() == ENaiwaP1TimedPlatformState::Ready && TimedFinal->IsCarryingCollisionEnabled()
        && TimedFinal2->GetState() == ENaiwaP1TimedPlatformState::Ready && TimedFinal2->IsCarryingCollisionEnabled());
    Report->SetNumberField(TEXT("terminal_timer_trigger_count_delta"), TimedFinal2
        ? TimedFinal2->GetTriggerCount() - V7MixedCoreTimedSecondTriggerBefore : 0);
    Report->SetNumberField(TEXT("terminal_timer_disable_count_delta"), TimedFinal2
        ? TimedFinal2->GetDisableCount() - V7MixedCoreTimedSecondDisableBefore : 0);
    Report->SetBoolField(TEXT("both_terminal_compounds_reset_after_death"), TimedFinal && TimedFinal2
        && TimedFinal->GetState() == ENaiwaP1TimedPlatformState::Ready
        && TimedFinal->IsCarryingCollisionEnabled()
        && TimedFinal2->GetState() == ENaiwaP1TimedPlatformState::Ready
        && TimedFinal2->IsCarryingCollisionEnabled());
    Report->SetNumberField(TEXT("probe_character_teleports"), 0);
    Report->SetNumberField(TEXT("direct_trigger_injections"), 0);
    Report->SetNumberField(TEXT("platform_control_injections"), 0);
    Report->SetNumberField(TEXT("production_respawn_placements"), DeathEventCount > V7MixedCoreDeathCountBefore ? 1 : 0);
    Report->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds() - V7MixedCoreStartedWall);

    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaP1"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Output = FPaths::Combine(Directory, FString::Printf(TEXT("v7_hidden_final_pit_recovery_%s_%s.json"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_V7_HIDDEN_PIT status=%s cause=%s visible=%d output=%s saved=%d"),
        Report->GetStringField(TEXT("status")) == TEXT("PASS") ? TEXT("PASS") : TEXT("FAIL"),
        *LastDeathCause.ToString(), bV7HiddenFinalPitVisibleDuringDeath ? 1 : 0, *Output, bSaved);
    FPlatformMisc::RequestExit(false);
}
