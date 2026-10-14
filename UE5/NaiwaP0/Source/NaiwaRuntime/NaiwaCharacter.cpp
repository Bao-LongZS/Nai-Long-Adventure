#include "NaiwaCharacter.h"
#include "NaiwaAnimInstance.h"
#include "NaiwaAutoTestComponent.h"
#include "NaiwaVisualReviewComponent.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

ANaiwaCharacter::ANaiwaCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
    JumpMaxCount = 1;
    JumpMaxHoldTime = 0.2f;

    GetCapsuleComponent()->InitCapsuleSize(CollisionRadiusCm, CollisionHalfHeightCm);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetRelativeLocation(MeshOffset);
    GetMesh()->SetRelativeRotation(FRotator(0.0f, MeshRightYaw, 0.0f));
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    AnimationClass = UNaiwaAnimInstance::StaticClass();

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->SetPlaneConstraintNormal(FVector(0.0f, 1.0f, 0.0f));
    Movement->SetPlaneConstraintOrigin(FVector::ZeroVector);
    Movement->bConstrainToPlane = true;
    Movement->bSnapToPlaneAtStart = true;
    Movement->bOrientRotationToMovement = false;
    Movement->bUseControllerDesiredRotation = false;
    Movement->MaxWalkSpeed = 300.0f;
    Movement->MaxAcceleration = 1800.0f;
    Movement->BrakingDecelerationWalking = 2200.0f;
    Movement->GroundFriction = 8.0f;
    Movement->JumpZVelocity = 650.0f;
    // Consume the bounded jump-hold force before applying gravity. The default
    // gravity-during-force path loses height when a frame spans most of the hold.
    Movement->bApplyGravityWhileJumping = false;
    Movement->GravityScale = 1.6f;
    Movement->AirControl = 0.4f;
    Movement->MaxStepHeight = 45.0f;
    Movement->bEnablePhysicsInteraction = false;

    SideCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("SideCamera"));
    SideCamera->SetupAttachment(RootComponent);
    SideCamera->SetRelativeLocation(FVector(0.0f, 900.0f, 40.0f));
    SideCamera->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    SideCamera->bUsePawnControlRotation = false;
    SideCamera->FieldOfView = 40.0f;
    AutoTest = CreateDefaultSubobject<UNaiwaAutoTestComponent>(TEXT("P0AutoTest"));
    CreateDefaultSubobject<UNaiwaVisualReviewComponent>(TEXT("P0VisualReview"));
}

void ANaiwaCharacter::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyCharacterAssets();
}

void ANaiwaCharacter::ApplyCharacterAssets()
{
    GetCharacterMovement()->bApplyGravityWhileJumping = false;
    GetCharacterMovement()->MaxWalkSpeed = bRunningGait ? 300.0f : 150.0f;
    GetCapsuleComponent()->SetCapsuleSize(CollisionRadiusCm, FMath::Max(CollisionHalfHeightCm, CollisionRadiusCm));
    if (SkeletalMeshAsset && GetMesh()->GetSkeletalMeshAsset() != SkeletalMeshAsset)
    {
        GetMesh()->SetSkeletalMesh(SkeletalMeshAsset);
    }
    GetMesh()->SetRelativeScale3D(MeshScale);
    GetMesh()->SetRelativeLocation(MeshOffset);
    ApplyFacing();
    if (AnimationClass && GetMesh()->GetAnimClass() != AnimationClass)
    {
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        GetMesh()->SetAnimInstanceClass(AnimationClass);
    }
}

void ANaiwaCharacter::BeginPlay()
{
    Super::BeginPlay();
    ApplyCharacterAssets();
    FVector Initial = GetActorLocation();
    Initial.Y = 0.0f;
    SetActorLocation(Initial, false, nullptr, ETeleportType::TeleportPhysics);
    SpawnLocation = Initial;
    AddInputMapping();
    if (!SkeletalMeshAsset || !IdleSequence || !WalkSequence || !JumpSequence)
    {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_ASSETS_INCOMPLETE: mesh and Idle/Walk/Jump must be assigned to BP_NaiwaCharacter."));
    }
}

void ANaiwaCharacter::PawnClientRestart()
{
    Super::PawnClientRestart();
    AddInputMapping();
}

void ANaiwaCharacter::AddInputMapping()
{
    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                if (InputMapping)
                {
                    Subsystem->AddMappingContext(InputMapping, 0);
                }
            }
        }
    }
}

void ANaiwaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (MoveAction)
        {
            EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ANaiwaCharacter::HandleMove);
            EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &ANaiwaCharacter::HandleMoveCompleted);
            EnhancedInput->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ANaiwaCharacter::HandleMoveCompleted);
        }
        if (JumpAction)
        {
            EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ANaiwaCharacter::HandleJumpStarted);
            EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ANaiwaCharacter::HandleJumpCompleted);
            EnhancedInput->BindAction(JumpAction, ETriggerEvent::Canceled, this, &ANaiwaCharacter::HandleJumpCompleted);
        }
        if (PauseAction)
        {
            EnhancedInput->BindAction(PauseAction, ETriggerEvent::Started, this, &ANaiwaCharacter::HandlePause);
        }
        if (ResetAction)
        {
            EnhancedInput->BindAction(ResetAction, ETriggerEvent::Started, this, &ANaiwaCharacter::HandleReset);
        }
        if (GaitAction)
        {
            EnhancedInput->BindAction(GaitAction, ETriggerEvent::Started, this, &ANaiwaCharacter::HandleGaitToggle);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_INPUT_ERROR: DefaultInputComponentClass must be EnhancedInputComponent."));
    }
}

void ANaiwaCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    SampleInputEvidence();
    if (bAutomatedInput)
    {
        AddMovementInput(FVector(1.0f, 0.0f, 0.0f), AutomatedMoveInput);
    }
    const float HorizontalSpeed = GetVelocity().X;
    if (FMath::Abs(HorizontalSpeed) > 3.0f)
    {
        bFacingRight = HorizontalSpeed > 0.0f;
        ApplyFacing();
    }
}

void ANaiwaCharacter::ApplyFacing()
{
    GetMesh()->SetRelativeRotation(FRotator(0.0f, MeshRightYaw + (bFacingRight ? 0.0f : 180.0f), 0.0f));
}

void ANaiwaCharacter::HandleMove(const FInputActionValue& Value)
{
    ++MoveInputEvents;
    const float Axis = Value.Get<float>();
    if (FMath::Abs(Axis - LastLoggedMoveInput) > 0.1f)
    {
        LastLoggedMoveInput = Axis;
        WriteInputEvidence(TEXT("move_started_or_changed"));
    }
    if (!bAutomatedInput)
    {
        AddMovementInput(FVector(1.0f, 0.0f, 0.0f), Value.Get<float>());
    }
}

void ANaiwaCharacter::HandleMoveCompleted(const FInputActionValue& Value)
{
    LastLoggedMoveInput = 0.0f;
    WriteInputEvidence(TEXT("move_released"));
    // Input values are consumed every movement tick; no persistent input remains after release.
}

void ANaiwaCharacter::HandleJumpStarted(const FInputActionValue& Value)
{
    ++JumpInputEvents;
    WriteInputEvidence(TEXT("jump_started"));
    if (!bAutomatedInput)
    {
        Jump();
    }
}

void ANaiwaCharacter::HandleJumpCompleted(const FInputActionValue& Value)
{
    StopJumping();
    WriteInputEvidence(TEXT("jump_released"));
}

void ANaiwaCharacter::HandlePause(const FInputActionValue& Value)
{
    ++PauseInputEvents;
    TogglePause();
    WriteInputEvidence(TEXT("pause_toggled"));
}

void ANaiwaCharacter::HandleReset(const FInputActionValue& Value)
{
    ++ResetInputEvents;
    ResetToSpawn();
    WriteInputEvidence(TEXT("reset_requested"));
}

void ANaiwaCharacter::HandleGaitToggle(const FInputActionValue& Value)
{
    ++GaitInputEvents;
    bRunningGait = !bRunningGait;
    GetCharacterMovement()->MaxWalkSpeed = bRunningGait ? 300.0f : 150.0f;
    WriteInputEvidence(TEXT("gait_toggled"));
}

void ANaiwaCharacter::TogglePause()
{
    UGameplayStatics::SetGamePaused(this, !UGameplayStatics::IsGamePaused(this));
}

void ANaiwaCharacter::ResetToSpawn()
{
    UGameplayStatics::SetGamePaused(this, false);
    StopJumping();
    GetCharacterMovement()->StopMovementImmediately();
    ConsumeMovementInputVector();
    AutomatedMoveInput = 0.0f;
    bFacingRight = true;
    SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
    GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    if (UNaiwaAnimInstance* Anim = Cast<UNaiwaAnimInstance>(GetMesh()->GetAnimInstance()))
    {
        Anim->ResetLocomotionClock();
    }
    ApplyFacing();
    LastResetLocation = GetActorLocation();
    LastResetSpeed = GetVelocity().Size();
    UE_LOG(LogTemp, Display, TEXT("NAIWA_RESET X=%.3f Y=%.3f Z=%.3f"), SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);
}

void ANaiwaCharacter::SetAutomatedMoveInput(float Value)
{
    bAutomatedInput = true;
    AutomatedMoveInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void ANaiwaCharacter::DisableAutomatedInput()
{
    bAutomatedInput = false;
    AutomatedMoveInput = 0.0f;
}

FString ANaiwaCharacter::GetLocomotionState() const
{
    if (const UNaiwaAnimInstance* Instance = Cast<UNaiwaAnimInstance>(GetMesh()->GetAnimInstance()))
    {
        switch (Instance->LocomotionState)
        {
        case ENaiwaLocomotionState::Walk: return TEXT("Walk");
        case ENaiwaLocomotionState::Jump: return TEXT("Jump");
        default: return TEXT("Idle");
        }
    }
    return TEXT("NoAnimInstance");
}

float ANaiwaCharacter::GetMeshBoundsBottom() const
{
    return GetMesh()->Bounds.Origin.Z - GetMesh()->Bounds.BoxExtent.Z;
}

float ANaiwaCharacter::GetCapsuleBottom() const
{
    return GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

void ANaiwaCharacter::SampleInputEvidence()
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("NaiwaInputEvidence"))) return;
    const double Now = FPlatformTime::Seconds();
    if (Now < NextInputEvidenceSampleSeconds) return;
    NextInputEvidenceSampleSeconds = Now + 0.1;
    WriteInputEvidence(TEXT("sample"));
}

void ANaiwaCharacter::WriteInputEvidence(const FString& Event)
{
    if (Event != TEXT("sample"))
    {
        UE_LOG(LogTemp, Display, TEXT("NAIWA_ENHANCED_INPUT event=%s move=%d jump=%d pause=%d reset=%d gait=%d running=%d max_walk_speed=%.1f paused=%d X=%.3f Y=%.3f Z=%.3f"),
            *Event, MoveInputEvents, JumpInputEvents, PauseInputEvents, ResetInputEvents,
            GaitInputEvents, bRunningGait, GetCharacterMovement()->MaxWalkSpeed,
            UGameplayStatics::IsGamePaused(this), GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
    }
    if (!FParse::Param(FCommandLine::Get(), TEXT("NaiwaInputEvidence"))) return;
    const double Now = FPlatformTime::Seconds();
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaInputEvidence"));
    if (InputEvidenceJsonlPath.IsEmpty())
    {
        InputEvidenceStartSeconds = Now;
        IFileManager::Get().MakeDirectory(*Directory, true);
        // A unique file keeps successive keyboard/focus sessions reviewable.
        InputEvidenceJsonlPath = FPaths::Combine(Directory, FString::Printf(TEXT("input_%s_%s_%s.jsonl"),
            *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), *GetName(), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        UE_LOG(LogTemp, Display, TEXT("NAIWA_INPUT_EVIDENCE output=%s"), *InputEvidenceJsonlPath);
    }
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("event"), Event);
    Object->SetStringField(TEXT("timestamp_utc"), FDateTime::UtcNow().ToIso8601());
    Object->SetNumberField(TEXT("wall_time_s"), Now - InputEvidenceStartSeconds);
    Object->SetNumberField(TEXT("world_time_s"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
    Object->SetNumberField(TEXT("move_events"), MoveInputEvents);
    Object->SetNumberField(TEXT("jump_events"), JumpInputEvents);
    Object->SetNumberField(TEXT("pause_events"), PauseInputEvents);
    Object->SetNumberField(TEXT("reset_events"), ResetInputEvents);
    Object->SetNumberField(TEXT("gait_events"), GaitInputEvents);
    Object->SetBoolField(TEXT("running_gait"), bRunningGait);
    Object->SetNumberField(TEXT("max_walk_speed_cm_s"), GetCharacterMovement()->MaxWalkSpeed);
    Object->SetBoolField(TEXT("paused"), UGameplayStatics::IsGamePaused(this));
    Object->SetBoolField(TEXT("auto_test_mode"), FParse::Param(FCommandLine::Get(), TEXT("NaiwaAutoTest")));
    if (const UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
    {
        if (ViewportClient->Viewport)
        {
            Object->SetBoolField(TEXT("viewport_focused"), ViewportClient->Viewport->HasFocus());
            Object->SetBoolField(TEXT("viewport_foreground"), ViewportClient->Viewport->IsForegroundWindow());
        }
    }
    Object->SetStringField(TEXT("animation_state"), GetLocomotionState());
    const UNaiwaAnimInstance* Anim = Cast<UNaiwaAnimInstance>(GetMesh()->GetAnimInstance());
    const UAnimSequence* MovingSequence = Anim ? Anim->GetProxyMovingSequence() : nullptr;
    Object->SetStringField(TEXT("moving_sequence"), MovingSequence ? MovingSequence->GetPathName() : FString());
    Object->SetNumberField(TEXT("x_cm"), GetActorLocation().X);
    Object->SetNumberField(TEXT("y_cm"), GetActorLocation().Y);
    Object->SetNumberField(TEXT("z_cm"), GetActorLocation().Z);
    Object->SetNumberField(TEXT("speed_cm_s"), GetVelocity().Size());
    Object->SetNumberField(TEXT("velocity_x_cm_s"), GetVelocity().X);
    Object->SetNumberField(TEXT("velocity_y_cm_s"), GetVelocity().Y);
    Object->SetNumberField(TEXT("velocity_z_cm_s"), GetVelocity().Z);
    Object->SetBoolField(TEXT("is_falling"), GetCharacterMovement()->IsFalling());
    Object->SetBoolField(TEXT("gravity_during_jump_hold"), GetCharacterMovement()->bApplyGravityWhileJumping);
    Object->SetNumberField(TEXT("mesh_yaw_deg"), GetMesh()->GetRelativeRotation().Yaw);
    FString Json;
    FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json));
    FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Directory, TEXT("latest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FFileHelper::SaveStringToFile(Json + TEXT("\n"), *InputEvidenceJsonlPath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}
