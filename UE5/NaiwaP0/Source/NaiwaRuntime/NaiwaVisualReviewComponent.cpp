#include "NaiwaVisualReviewComponent.h"
#include "NaiwaCharacter.h"
#include "Animation/AnimSequence.h"
#include "Animation/MorphTarget.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SkeletalRenderPublic.h"
#include "UnrealClient.h"

namespace {
TSharedPtr<FJsonObject> ReviewVector(const FVector& V) {
    TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
    J->SetNumberField(TEXT("x"), V.X); J->SetNumberField(TEXT("y"), V.Y); J->SetNumberField(TEXT("z"), V.Z);
    return J;
}
}

UNaiwaVisualReviewComponent::UNaiwaVisualReviewComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UNaiwaVisualReviewComponent::BeginPlay()
{
    Super::BeginPlay();
    bPlayback = FParse::Param(FCommandLine::Get(), TEXT("NaiwaReviewPlayback"));
    bEnabled = bPlayback || FParse::Param(FCommandLine::Get(), TEXT("NaiwaVisualReview"));
    bQuit = FParse::Param(FCommandLine::Get(), TEXT("NaiwaReviewQuit"));
    Character = Cast<ANaiwaCharacter>(GetOwner());
    if (!bEnabled || !Character.IsValid()) { SetComponentTickEnabled(false); return; }
    if (FParse::Param(FCommandLine::Get(), TEXT("NaiwaAutoTest"))) {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_VISUAL_REVIEW cannot run with NaiwaAutoTest"));
        Finish(false); return;
    }
    Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NaiwaVisualReviews"),
        FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")), FPlatformProcess::GetCurrentProcessId())));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const TCHAR* Names[] = {TEXT("Idle"), TEXT("Walk"), TEXT("Jump"), TEXT("Laugh"), TEXT("Run")};
    USkeletalMeshComponent* Mesh = Character->GetMesh();
    if (!Mesh || !Mesh->GetSkeletalMeshAsset()) { Finish(false); return; }
    for (const TCHAR* Name : Names) {
        const FString Path = FString::Printf(TEXT("/Game/Naiwa/%sAS_Naiwa_%s.AS_Naiwa_%s"),
            FString(Name) == TEXT("Run") ? TEXT("Gameplay/") : TEXT(""), Name, Name);
        UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *Path);
        if (!Sequence || Sequence->GetPlayLength() <= 0.0f || Sequence->GetSkeleton() != Mesh->GetSkeletalMeshAsset()->GetSkeleton()) {
            UE_LOG(LogTemp, Error, TEXT("NAIWA_VISUAL_REVIEW invalid clip/skeleton: %s"), *Path); Finish(false); return;
        }
        Clips.Add(Sequence);
    }
    Character->GetCharacterMovement()->DisableMovement();
    if (APlayerController* PC = Cast<APlayerController>(Character->GetController())) {
        PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
        Character->DisableInput(PC);
        if (AHUD* HUD = PC->GetHUD()) HUD->bShowHUD = false;
    }
    AddTickPrerequisiteComponent(Mesh);
    Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Mesh->bEnableUpdateRateOptimizations = false;
    FModuleManager::Get().LoadModuleChecked<IModuleInterface>(TEXT("ImageWrapper"));
    ScreenshotHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &UNaiwaVisualReviewComponent::OnScreenshot);
    NextWallTime = FPlatformTime::Seconds() + 20.0;
    UE_LOG(LogTemp, Display, TEXT("NAIWA_VISUAL_REVIEW_START mode=%s directory=%s"), bPlayback ? TEXT("continuous_playback") : TEXT("held_phase"), *Directory);
}

void UNaiwaVisualReviewComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
{
    Super::TickComponent(DeltaTime, TickType, Function);
    if (!bEnabled || !Character.IsValid()) return;
    const double Now = FPlatformTime::Seconds();
    if (bPlayback) { TickPlayback(Now); return; }
    if (bPending) {
        if (Now - PendingWallTime > 15.0) { UE_LOG(LogTemp, Error, TEXT("NAIWA_VISUAL_REVIEW screenshot timed out")); Finish(false); }
        return;
    }
    if (Now < NextWallTime) return;
    if (ClipIndex >= Clips.Num()) { Finish(true); return; }
    USkeletalMeshComponent* Mesh = Character->GetMesh();
    UAnimSequence* Clip = Clips[ClipIndex];
    if (!bPrepared) {
        if (PhaseIndex == 0) { Mesh->PlayAnimation(Clip, false); Mesh->SetPlayRate(0.0f); }
        SampleTime = PhaseIndex < 12 ? Clip->GetPlayLength() * PhaseIndex / 12.0f :
            (PhaseIndex % 2 == 0 ? FMath::Max(0.0f, Clip->GetPlayLength() - 0.00001f) : 0.0f);
        Mesh->SetPosition(SampleTime, false);
        Mesh->TickAnimation(0.0f, false);
        Mesh->RefreshBoneTransforms();
        bPrepared = true; SettledTicks = 0; NextWallTime = Now + 0.12;
        return;
    }
    if (++SettledTicks < 2) return;
    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("clip"), Clip->GetName());
    Entry->SetStringField(TEXT("asset"), Clip->GetPathName());
    Entry->SetNumberField(TEXT("phase_index"), PhaseIndex);
    Entry->SetNumberField(TEXT("sample_time_s"), SampleTime);
    Entry->SetNumberField(TEXT("clip_length_s"), Clip->GetPlayLength());
    Entry->SetNumberField(TEXT("actual_single_node_time_s"), Mesh->GetSingleNodeInstance()->GetCurrentTime());
    Entry->SetBoolField(TEXT("loop_seam_sample"), PhaseIndex >= 12);
    Entry->SetObjectField(TEXT("actor_location_cm"), ReviewVector(Character->GetActorLocation()));
    Entry->SetObjectField(TEXT("camera_world_location_cm"), ReviewVector(Character->SideCamera->GetComponentLocation()));
    const FRotator CameraRotation = Character->SideCamera->GetComponentRotation();
    Entry->SetObjectField(TEXT("camera_rotation_pitch_yaw_roll_deg"), ReviewVector(FVector(CameraRotation.Pitch, CameraRotation.Yaw, CameraRotation.Roll)));
    Entry->SetNumberField(TEXT("camera_fov_deg"), Character->SideCamera->FieldOfView);
    Entry->SetNumberField(TEXT("capsule_radius_cm"), Character->GetCapsuleComponent()->GetScaledCapsuleRadius());
    Entry->SetNumberField(TEXT("capsule_half_height_cm"), Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    TArray<FFinalSkinVertex> Vertices;
    Mesh->GetCPUSkinnedVertices(Vertices, 0);
    FBox Surface(ForceInit);
    for (const FFinalSkinVertex& Vertex : Vertices) Surface += Mesh->GetComponentTransform().TransformPosition(FVector(Vertex.Position));
    Entry->SetNumberField(TEXT("actual_cpu_skinned_vertex_count"), Vertices.Num());
    TArray<TSharedPtr<FJsonValue>> Bones;
    const FReferenceSkeleton& Skeleton = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    const TArray<FTransform>& Pose = Mesh->GetComponentSpaceTransforms();
    for (int32 BoneIndex = 0; BoneIndex < Pose.Num(); ++BoneIndex) {
        TSharedPtr<FJsonObject> Bone = MakeShared<FJsonObject>();
        Bone->SetStringField(TEXT("name"), Skeleton.GetBoneName(BoneIndex).ToString());
        Bone->SetObjectField(TEXT("position_cm"), ReviewVector(Pose[BoneIndex].GetLocation()));
        const FQuat Q = Pose[BoneIndex].GetRotation();
        TArray<TSharedPtr<FJsonValue>> Quaternion;
        for (double Value : {Q.X, Q.Y, Q.Z, Q.W}) Quaternion.Add(MakeShared<FJsonValueNumber>(Value));
        Bone->SetArrayField(TEXT("quaternion_xyzw"), Quaternion);
        Bones.Add(MakeShared<FJsonValueObject>(Bone));
    }
    Entry->SetArrayField(TEXT("component_space_bones"), Bones);
    TSharedPtr<FJsonObject> Morphs = MakeShared<FJsonObject>();
    for (const UMorphTarget* Morph : Mesh->GetSkeletalMeshAsset()->GetMorphTargets()) {
        const int32* Index = Mesh->GetSkeletalMeshAsset()->GetMorphTargetIndexMap().Find(Morph->GetFName());
        if (Index && Mesh->MorphTargetWeights.IsValidIndex(*Index)) Morphs->SetNumberField(Morph->GetName(), Mesh->MorphTargetWeights[*Index]);
    }
    Entry->SetObjectField(TEXT("final_component_morph_weights"), Morphs);
    if (Surface.IsValid) {
        Entry->SetObjectField(TEXT("surface_min_world_cm"), ReviewVector(Surface.Min));
        Entry->SetObjectField(TEXT("surface_max_world_cm"), ReviewVector(Surface.Max));
        Entry->SetNumberField(TEXT("front_extent_from_actor_cm"), Surface.Max.X - Character->GetActorLocation().X);
        Entry->SetNumberField(TEXT("rear_extent_from_actor_cm"), Character->GetActorLocation().X - Surface.Min.X);
    }
    PendingPath = FPaths::Combine(Directory, FString::Printf(TEXT("%s_phase_%02d.png"), *Clip->GetName(), PhaseIndex));
    Entry->SetStringField(TEXT("file"), PendingPath);
    Entries.Add(MakeShared<FJsonValueObject>(Entry));
    bPending = true;
    PendingWallTime = Now;
    FScreenshotRequest::RequestScreenshot(PendingPath, false, false);
}

TSharedPtr<FJsonObject> UNaiwaVisualReviewComponent::PlaybackPose(bool bMeasureSurface) const
{
    USkeletalMeshComponent* Mesh = Character->GetMesh();
    TSharedPtr<FJsonObject> Frame = MakeShared<FJsonObject>();
    Frame->SetStringField(TEXT("clip"), Clips[ClipIndex]->GetName());
    Frame->SetStringField(TEXT("asset"), Clips[ClipIndex]->GetPathName());
    Frame->SetNumberField(TEXT("clip_index"), ClipIndex);
    Frame->SetNumberField(TEXT("frame_index"), PlaybackFrameCount);
    Frame->SetNumberField(TEXT("clip_elapsed_game_s"), PlaybackElapsed);
    Frame->SetNumberField(TEXT("world_game_time_s"), GetWorld()->GetTimeSeconds());
    Frame->SetNumberField(TEXT("wall_time_s"), FPlatformTime::Seconds());
    Frame->SetNumberField(TEXT("actual_single_node_time_s"), Mesh->GetSingleNodeInstance()->GetCurrentTime());
    Frame->SetNumberField(TEXT("clip_length_s"), Clips[ClipIndex]->GetPlayLength());
    Frame->SetNumberField(TEXT("play_rate"), Mesh->GetSingleNodeInstance()->GetPlayRate());
    Frame->SetNumberField(TEXT("asset_rate_scale"), Clips[ClipIndex]->RateScale);
    Frame->SetNumberField(TEXT("observed_position_wraps"), PlaybackWraps);
    Frame->SetNumberField(TEXT("bone_transform_revision"), Mesh->GetBoneTransformRevisionNumber());
    Frame->SetObjectField(TEXT("actor_location_cm"), ReviewVector(Character->GetActorLocation()));
    Frame->SetObjectField(TEXT("camera_world_location_cm"), ReviewVector(Character->SideCamera->GetComponentLocation()));
    const FRotator CameraRotation = Character->SideCamera->GetComponentRotation();
    Frame->SetObjectField(TEXT("camera_rotation_pitch_yaw_roll_deg"), ReviewVector(FVector(CameraRotation.Pitch, CameraRotation.Yaw, CameraRotation.Roll)));
    Frame->SetNumberField(TEXT("camera_fov_deg"), Character->SideCamera->FieldOfView);
    Frame->SetNumberField(TEXT("capsule_radius_cm"), Character->GetCapsuleComponent()->GetScaledCapsuleRadius());
    Frame->SetNumberField(TEXT("capsule_half_height_cm"), Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    TArray<TSharedPtr<FJsonValue>> Bones;
    const FReferenceSkeleton& Skeleton = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    const TArray<FTransform>& Pose = Mesh->GetComponentSpaceTransforms();
    for (int32 BoneIndex = 0; BoneIndex < FMath::Min(Pose.Num(), Skeleton.GetNum()); ++BoneIndex) {
        TSharedPtr<FJsonObject> Bone = MakeShared<FJsonObject>();
        Bone->SetStringField(TEXT("name"), Skeleton.GetBoneName(BoneIndex).ToString());
        Bone->SetObjectField(TEXT("position_cm"), ReviewVector(Pose[BoneIndex].GetLocation()));
        const FQuat Q = Pose[BoneIndex].GetRotation();
        TArray<TSharedPtr<FJsonValue>> Quaternion;
        for (double Value : {Q.X, Q.Y, Q.Z, Q.W}) Quaternion.Add(MakeShared<FJsonValueNumber>(Value));
        Bone->SetArrayField(TEXT("quaternion_xyzw"), Quaternion);
        Bones.Add(MakeShared<FJsonValueObject>(Bone));
    }
    Frame->SetArrayField(TEXT("component_space_bones"), Bones);
    TSharedPtr<FJsonObject> Morphs = MakeShared<FJsonObject>();
    for (const UMorphTarget* Morph : Mesh->GetSkeletalMeshAsset()->GetMorphTargets()) {
        const int32* Index = Mesh->GetSkeletalMeshAsset()->GetMorphTargetIndexMap().Find(Morph->GetFName());
        if (Index && Mesh->MorphTargetWeights.IsValidIndex(*Index)) Morphs->SetNumberField(Morph->GetName(), Mesh->MorphTargetWeights[*Index]);
    }
    Frame->SetObjectField(TEXT("final_component_morph_weights"), Morphs);
    if (bMeasureSurface) {
        TArray<FFinalSkinVertex> Vertices;
        Mesh->GetCPUSkinnedVertices(Vertices, 0);
        FBox Surface(ForceInit);
        for (const FFinalSkinVertex& Vertex : Vertices) Surface += Mesh->GetComponentTransform().TransformPosition(FVector(Vertex.Position));
        Frame->SetNumberField(TEXT("actual_cpu_skinned_vertex_count"), Vertices.Num());
        if (Surface.IsValid) {
            Frame->SetObjectField(TEXT("surface_min_world_cm"), ReviewVector(Surface.Min));
            Frame->SetObjectField(TEXT("surface_max_world_cm"), ReviewVector(Surface.Max));
        }
    }
    return Frame;
}

void UNaiwaVisualReviewComponent::TickPlayback(double Now)
{
    if (bPending && Now - PendingWallTime > 15.0) {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_VISUAL_REVIEW playback screenshot timed out")); Finish(false); return;
    }
    if (Now < NextWallTime) return;
    if (ClipIndex >= Clips.Num()) { Finish(true); return; }
    USkeletalMeshComponent* Mesh = Character->GetMesh();
    UAnimSequence* Clip = Clips[ClipIndex];
    const double GameNow = GetWorld()->GetTimeSeconds();
    if (!bPrepared) {
        if (ClipIndex == 0) {
            // GameMode can possess the pawn after component BeginPlay. Reapply isolation
            // once that controller exists, before the first continuously played clip.
            Character->GetCharacterMovement()->DisableMovement();
            if (APlayerController* PC = Cast<APlayerController>(Character->GetController())) {
                PC->FlushPressedKeys();
                PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
                Character->DisableInput(PC);
                if (AHUD* HUD = PC->GetHUD()) HUD->bShowHUD = false;
            }
        }
        Mesh->PlayAnimation(Clip, true);
        Mesh->SetPlayRate(1.0f);
        Mesh->TickAnimation(0.0f, false);
        Mesh->RefreshBoneTransforms();
        if (!Mesh->GetSingleNodeInstance()) { Finish(false); return; }
        bPrepared = true;
        bPlaybackClipClosing = false;
        PlaybackStartGameTime = PreviousPlaybackGameTime = GameNow;
        PlaybackElapsed = NextPlaybackScreenshotTime = 0.0;
        PreviousPlaybackPosition = Mesh->GetSingleNodeInstance()->GetCurrentTime();
        PlaybackWraps = PlaybackFrameCount = PlaybackClipShotCount = 0;
        MaximumPlaybackTimeError = 0.0;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_PLAYBACK_CLIP_START clip=%s length=%.4f game_time=%.4f"), *Clip->GetName(), Clip->GetPlayLength(), GameNow);
    }
    if (bPlaybackClipClosing) return;
    const float Position = Mesh->GetSingleNodeInstance()->GetCurrentTime();
    const double GameDelta = GameNow - PreviousPlaybackGameTime;
    PlaybackElapsed = GameNow - PlaybackStartGameTime;
    if (PlaybackFrameCount > 0) {
        if (Position + 0.00001f < PreviousPlaybackPosition) ++PlaybackWraps;
        const double ExpectedPosition = FMath::Fmod(static_cast<double>(PreviousPlaybackPosition) + GameDelta * Clip->RateScale, static_cast<double>(Clip->GetPlayLength()));
        double Error = FMath::Abs(ExpectedPosition - Position);
        Error = FMath::Min(Error, static_cast<double>(Clip->GetPlayLength()) - Error);
        MaximumPlaybackTimeError = FMath::Max(MaximumPlaybackTimeError, Error);
    }
    PreviousPlaybackPosition = Position;
    PreviousPlaybackGameTime = GameNow;
    TSharedPtr<FJsonObject> Frame = PlaybackPose(PlaybackFrameCount == 0);
    Frame->SetNumberField(TEXT("delta_game_s"), GameDelta);
    Frame->SetNumberField(TEXT("maximum_time_continuity_error_s"), MaximumPlaybackTimeError);
    PlaybackFrames.Add(MakeShared<FJsonValueObject>(Frame));
    ++PlaybackFrameCount;
    const bool bTwoCycles = PlaybackElapsed + 0.00001 >= 2.0 * Clip->GetPlayLength() && PlaybackWraps >= 2;
    if (PlaybackElapsed > 4.0 * Clip->GetPlayLength() + 2.0 && !bTwoCycles) {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_PLAYBACK_CLIP_FAILED no two observed wraps clip=%s"), *Clip->GetName()); Finish(false); return;
    }
    // The mesh keeps advancing even when a previous screenshot is pending. No held poses
    // or SetPosition calls are used during playback; the last naturally reached pose is
    // stopped only after two complete cycles, so the terminal PNG cannot drift.
    if (!bPending && (bTwoCycles || PlaybackElapsed + 0.00001 >= NextPlaybackScreenshotTime)) {
        if (FScreenshotRequest::IsScreenshotRequested()) return;
        PendingPath = FPaths::Combine(Directory, FString::Printf(TEXT("%s_playback_%04d.png"), *Clip->GetName(), PlaybackClipShotCount));
        Frame->SetStringField(TEXT("file"), PendingPath);
        Frame->SetBoolField(TEXT("terminal_after_two_cycles"), bTwoCycles);
        Entries.Add(MakeShared<FJsonValueObject>(Frame));
        bPlaybackClipClosing = bTwoCycles;
        if (bTwoCycles) Mesh->GetSingleNodeInstance()->SetPlaying(false);
        NextPlaybackScreenshotTime = (FMath::FloorToDouble(PlaybackElapsed * 8.0) + 1.0) / 8.0;
        bPending = true;
        PendingWallTime = Now;
        FScreenshotRequest::RequestScreenshot(PendingPath, false, false);
    }
}

void UNaiwaVisualReviewComponent::CompletePlaybackClip()
{
    TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
    UAnimSequence* Clip = Clips[ClipIndex];
    Summary->SetStringField(TEXT("clip"), Clip->GetName());
    Summary->SetStringField(TEXT("asset"), Clip->GetPathName());
    Summary->SetNumberField(TEXT("duration_game_s"), PlaybackElapsed);
    Summary->SetNumberField(TEXT("clip_length_s"), Clip->GetPlayLength());
    Summary->SetNumberField(TEXT("observed_position_wraps"), PlaybackWraps);
    Summary->SetNumberField(TEXT("game_tick_frames"), PlaybackFrameCount);
    Summary->SetNumberField(TEXT("saved_screenshots"), PlaybackClipShotCount);
    Summary->SetNumberField(TEXT("maximum_time_continuity_error_s"), MaximumPlaybackTimeError);
    const bool bComplete = PlaybackElapsed + 0.00001 >= 2.0 * Clip->GetPlayLength() && PlaybackWraps >= 2 && PlaybackClipShotCount >= 3;
    Summary->SetBoolField(TEXT("two_cycles_complete"), bComplete);
    PlaybackClips.Add(MakeShared<FJsonValueObject>(Summary));
    UE_LOG(LogTemp, Display, TEXT("NAIWA_PLAYBACK_CLIP_COMPLETE clip=%s cycles=%d duration=%.4f ticks=%d screenshots=%d"), *Clip->GetName(), PlaybackWraps, PlaybackElapsed, PlaybackFrameCount, PlaybackClipShotCount);
    if (!bComplete) { Finish(false); return; }
    ++ClipIndex;
    bPrepared = bPlaybackClipClosing = false;
    NextWallTime = FPlatformTime::Seconds() + 0.04;
}

void UNaiwaVisualReviewComponent::OnScreenshot(int32 Width, int32 Height, const TArray<FColor>& Pixels)
{
    if (!bPending) return;
    TArray64<uint8> Png;
    FImageUtils::PNGCompressImageArray(Width, Height, MakeArrayView64(Pixels), Png);
    const bool Saved = !Png.IsEmpty() && FFileHelper::SaveArrayToFile(Png, *PendingPath);
    Entries.Last()->AsObject()->SetBoolField(TEXT("saved"), Saved);
    Entries.Last()->AsObject()->SetNumberField(TEXT("width"), Width);
    Entries.Last()->AsObject()->SetNumberField(TEXT("height"), Height);
    if (!Saved) { Finish(false); return; }
    bPending = false;
    if (bPlayback) {
        ++PlaybackClipShotCount;
        if (bPlaybackClipClosing) CompletePlaybackClip();
        return;
    }
    bPrepared = false;
    if (++PhaseIndex >= 16) { PhaseIndex = 0; ++ClipIndex; }
    NextWallTime = FPlatformTime::Seconds() + 0.04;
}

void UNaiwaVisualReviewComponent::Finish(bool bSuccess)
{
    if (bFinished) return;
    bFinished = true;
    bEnabled = false; SetComponentTickEnabled(false);
    if (ScreenshotHandle.IsValid()) UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
    ScreenshotHandle.Reset();
    TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
    bool bComplete = bSuccess && (bPlayback ? PlaybackClips.Num() == 5 : Entries.Num() == 80);
    for (const TSharedPtr<FJsonValue>& Entry : Entries) {
        bool bSaved = false;
        bComplete &= Entry->AsObject()->TryGetBoolField(TEXT("saved"), bSaved) && bSaved;
    }
    Report->SetBoolField(TEXT("capture_complete"), bComplete);
    Report->SetStringField(TEXT("mode"), bPlayback ? TEXT("continuous_playback") : TEXT("held_phase"));
    Report->SetStringField(TEXT("method"), bPlayback ?
        TEXT("Saved UE animations, naturally looping single-node PlayRate 1; each clip at least two game-time cycles and two observed position wraps; every game tick records actual animation time and component-space bones; approximate 8 fps game-time screenshots; one CPU-skinned surface measurement per clip; fixed saved gameplay camera") :
        TEXT("Saved UE animations, single-node held phases; 12 equal phases and two end/start pairs per clip; actual CPU-skinned surface; fixed saved gameplay camera"));
    Report->SetStringField(TEXT("limits"), bPlayback ?
        TEXT("Continuous saved-asset playback on a stationary pawn, not player locomotion or physical input. Original Jump and Laugh are demonstration clips and are intentionally looped for review. Screenshot intervals follow measured game time and can vary with frame rate/encoding cost; frame records preserve the actual cadence. CPU extraction and PNG encoding are not performance measurements. An early EndPlay writes capture_complete=false.") :
        TEXT("Phase screenshots are asset/loop and geometry evidence, not live player locomotion or physical input. CPU extraction/screenshot encoding costs are excluded from performance. Jump and Laugh are original demonstration clips."));
    Report->SetArrayField(TEXT("entries"), Entries);
    if (bPlayback) {
        Report->SetArrayField(TEXT("playback_frames"), PlaybackFrames);
        Report->SetArrayField(TEXT("playback_clips"), PlaybackClips);
        Report->SetNumberField(TEXT("target_capture_rate_game_fps"), 8.0);
        Report->SetNumberField(TEXT("required_cycles_per_clip"), 2.0);
    }
    FString Json;
    FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
    if (!Directory.IsEmpty()) FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Directory, TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_VISUAL_REVIEW_COMPLETE success=%d screenshots=%d playback_frames=%d directory=%s"), bComplete, Entries.Num(), PlaybackFrames.Num(), *Directory);
    if (bQuit) FPlatformMisc::RequestExit(false);
}

void UNaiwaVisualReviewComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bEnabled && !bFinished) {
        // A user/editor close must preserve partial evidence, without requesting another exit.
        bQuit = false;
        Finish(false);
    }
    if (ScreenshotHandle.IsValid()) UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
    ScreenshotHandle.Reset();
    Super::EndPlay(Reason);
}
