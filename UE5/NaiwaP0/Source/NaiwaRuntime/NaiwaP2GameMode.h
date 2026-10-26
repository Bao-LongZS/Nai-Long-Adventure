#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "NaiwaP2Types.h"
#include "NaiwaP2GameMode.generated.h"

class AStaticMeshActor;
class ANaiwaP2Archer;
class ANaiwaP2PhysicsEgg;
class ANaiwaP2StructureBlock;
class USkeletalMesh;
class UMaterialInterface;
class USoundBase;
class UAudioComponent;

UENUM(BlueprintType)
enum class ENaiwaP2PlayState : uint8
{
    Aiming,
    Flying,
    Resolving,
    Failed,
    Won
};

UENUM()
enum class ENaiwaP2FeedbackImage : uint8
{
    None,
    OneMinute,
    FifthReset,
    OutOfEggs
};

/**
 * 单个奶蛋变体的视觉资产。
 *
 * 五种变体（Base / Chick / Mouse / Soy / Tea）共用同一个 Root 骨架；各自保留同名的
 * EyeWide / Scream / Squash 三个 morph target；变体之间的几何差异（鸡冠、耳、围巾、吸管）
 * 已经烘进各自的 body 网格里，运行时不存在"隐藏多余特征"的机制。
 *
 * 为什么不做运行时隐藏（顶点 alpha / WPO 塌陷 / material section 剔除）：
 *   1. WPO 塌陷会让被隐藏的顶点继续参与包围盒与阴影，实测尺寸类验收会虚报；
 *   2. section 剔除需要各变体槽位数不一致，而代码按槽索引 SetMaterial，槽序一变就静默套错材质；
 *   3. 资产级隔离天然没有上述问题，代价只是 5 个网格资产而不是 1 个。
 */
USTRUCT()
struct FNaiwaP2EggVisual
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "Naiwa|Egg")
    TObjectPtr<USkeletalMesh> Mesh = nullptr;

    /** slot 0，变体主色 */
    UPROPERTY(EditDefaultsOnly, Category = "Naiwa|Egg")
    TObjectPtr<UMaterialInterface> BodyMaterial = nullptr;

    /** slot 4，特征主色（鸡冠红橙 / 围巾红 / 吸管米白） */
    UPROPERTY(EditDefaultsOnly, Category = "Naiwa|Egg")
    TObjectPtr<UMaterialInterface> AccentMaterial = nullptr;
};

/** P2 game mode; the canonical 2-1 map selects the full Chaos level automatically. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP2GameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ANaiwaP2GameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    bool TryFire(double AngleDegrees, double Power, int32 ClickTick = INDEX_NONE);
    bool ActivateFlightSkill();
    bool SelectEggByIndex(int32 Index);
    void SetAimPreview(double AngleDegrees, double Power);
    void Retry();
    void SetPouchDragWorldLocation(const FVector& Location);
    void ReleasePouchToLauncher();
    void ResetPouchLocation();
    void BeginArcherDraw();
    void SetArcherDrawProgress(float Progress);
    void ReleaseArcherDraw();
    void CancelArcherDraw();
    void ResetArcherDraw();
    void ShowActionNotice(const FString& Text, double Duration = 1.5);
    /** Start a 5-second stability sample from an active PIE or standalone game console. */
    void StartPhysicsStabilityProbe();

    ENaiwaP2PlayState GetPlayState() const { return PlayState; }
    const NaiwaP2::FShotResult* GetAimPreview() const { return bHasAimPreview ? &AimPreview : nullptr; }
    const NaiwaP2::FShotResult* GetCurrentShot() const { return CurrentShot.States.IsEmpty() ? nullptr : &CurrentShot; }
    const FString& GetActionNotice() const { return ActionNotice; }
    double GetActionNoticeRemaining() const { return ActionNoticeRemaining; }
    int32 GetShotsRemaining() const { return FMath::Max(0, AmmoRule.TotalShots - ShotsFired); }
    int32 GetShotsFired() const { return ShotsFired; }
    int32 GetTotalShots() const { return AmmoRule.TotalShots; }
    int32 GetEggTokensRemaining() const { return AvailableEggs.Num(); }
    ENaiwaP2FeedbackImage GetFeedbackImage() const { return ActiveFeedbackImage; }
    double GetFeedbackCueAgeSeconds() const { return FeedbackCueAgeSeconds; }
    int32 GetFeedbackCueSerial() const { return FeedbackCueSerial; }
    bool ShouldShowFeedbackImage() const;
    const TArray<NaiwaP2::EEggKind>& GetAvailableEggs() const { return AvailableEggs; }
    NaiwaP2::EEggKind GetSelectedEgg() const { return SelectedEgg; }
    NaiwaP2::EEggKind GetCurrentShotEgg() const { return ShotEgg; }
    FString GetLevelId() const { return CurrentLevelId; }
    int32 GetRemainingGoalCount() const { return GoalActors.Num(); }
    double GetAimAngleDegrees() const { return AimAngleDegrees; }
    double GetAimPower() const { return AimPower; }
    FString GetPlayStateLabel() const;

private:
    void SpawnPersistentScene();
    void SpawnAttemptScene();

    // ---- P2-ART-INTEG：Geo 美术风格（默认开启；-NaiwaP2ArtStyle=Graybox 回退） ----
    // 纯视觉替换：结构块碰撞/质量仍由 InitFromBox 决定，与材质无关。
    // 默认显示美术背景；需要诊断原型时可显式切回灰盒路径。
    /** 按效果图（render_final.py）生成天空/四层山/地面带/Z0 亮线/装饰树石。 */
    void SpawnGeoBackdrop();
    /** 用 ProceduralMeshComponent 把一条 (X,Z) 米制折线生成剪影三角带。 */
    AActor* SpawnGeoSilhouette(const FString& Label, const TArray<FVector2D>& PointsMeters,
        double YCenterMeters, double BottomZMeters, UMaterialInterface* Material);
    /** 结构块 → Geo 材质映射（按 Id 后缀：.wall=Plast .col=Stone 其余=Wood）。 */
    UMaterialInterface* GetGeoMaterialForBlock(const NaiwaP2::FBox2& Box) const;
    /** Geo 风格总开关；默认开启，false 时全部走灰盒材质。 */
    bool bGeoArtStyle = true;

    // ---- 截图探针（诊断用） ----
    // -NaiwaP2ScreenshotSeconds=N -NaiwaP2ScreenshotPath=X：N 秒后存一张截图再退出，
    // 供命令行回归拿实机画面（HighResShot 的 ExecCmds 在第一帧就触发，相机还没接管）。
    void UpdateScreenshotProbe(double DeltaSeconds);
    double ScreenshotProbeDelaySeconds = 0.0;
    double ScreenshotProbeElapsed = 0.0;
    int32 ScreenshotProbeState = 0;   // 0=等待 1=已请求截图 2=完成/退出
    FString ScreenshotProbePath;

    AActor* SpawnEggActor(const FString& Label, const FVector& Location,
        NaiwaP2::EEggKind Kind, double ScaleMultiplier = 1.0, bool bSkillActive = false,
        bool bPhysicsBody = false, const FVector& InitialVelocityCmPerS = FVector::ZeroVector);
    void ApplyEggActorAppearance(AActor* Actor, NaiwaP2::EEggKind Kind,
        double ScaleMultiplier = 1.0, bool bSkillActive = false);
    const FNaiwaP2EggVisual* FindEggVisual(NaiwaP2::EEggKind Kind) const;
    void LogEggVisualTableDiagnostics() const;
    void UpdateProjectileActorsAtCurrentTick();
    void InitializeLevelRules();
    void SelectNextAvailableEgg();
    AStaticMeshActor* SpawnMeshActor(const FString& Label, const FVector& Location,
        const FVector& Scale, UStaticMesh* Mesh, UMaterialInterface* Material,
        const FRotator& Rotation = FRotator::ZeroRotator);
    void FinishCurrentShot();
    bool RefundZeroKillShot();
    void StartStructureCollapse();
    void UpdateStructureCollapse(double DeltaSeconds);
    void ApplyShotGoalsAndTransition();
    void SetPhase(ENaiwaP2PlayState NewState);
    void Advance22PlayTest();
    void Advance22RefundPlayTest();

    // ---- P2 全物理改造（M1） ----
    /** 接管世界重力（-15.68 m/s²）。P2 独占一张关卡，不影响 P1/P0。 */
    void ApplyP2WorldGravity();
    /** 把所有结构刚体唤醒。开火时调用，避免"支撑被抽走、上层还在睡"导致悬空。 */
    void WakeAllStructureBlocks();
    /** 塔自稳自检：每帧记录最大位移，到时间点出报告。由 -NaiwaP2PhysicsStability 开启。 */
    void UpdatePhysicsStabilityProbe(double DeltaSeconds);
    /** F1 沙盒里仅由 Chaos 推进弹丸，按静止、出界或时限结束本次试射。 */
    void UpdatePhysicsSandboxShot(double DeltaSeconds);
    /** 完整 P2 物理回合：等待弹丸结束及所有受影响刚体稳定。 */
    void UpdatePhysicsLevelShot(double DeltaSeconds);
    bool LogPhysicsFrameTimeProbe() const;
    void ApplyPhysicsTeaBlast(ANaiwaP2PhysicsEgg* Egg);
    void FinishPhysicsLevelShot(const TCHAR* ResolveReason);
    void PlayLevelMusic();
    void PlayGoalHitSound();
    void PlayMissSound();
    void PlayDuckedSound(USoundBase* Sound);
    void ShowFeedbackCue(ENaiwaP2FeedbackImage Image, USoundBase* Sound);
    void UpdateFeedbackTimers(double DeltaSeconds);
    void ApplyMusicDucking();
    UFUNCTION()
    void HandleDuckedSoundFinished();
    void ScheduleCampaignAdvance();
    void AdvanceCampaign();
    bool IsPhysicsLevelAtShotStart() const;
    UFUNCTION()
    void HandlePhysicsGoalHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                              UPrimitiveComponent* OtherComp, FVector NormalImpulse,
                              const FHitResult& Hit);
    /** 检查悬空结构块；保持独立刚体，不用焊接约束或静态化兜底。 */
    void ResolveUnsupportedBlocks();
    ANaiwaP2StructureBlock* FindStructureBlock(const FString& BlockId) const;

    NaiwaP2::FScene CurrentScene;
    NaiwaP2::FAmmoRule AmmoRule;
    TArray<NaiwaP2::EEggKind> AvailableEggs;
    NaiwaP2::FShotResult CurrentShot;
    NaiwaP2::FShotResult AimPreview;
    ENaiwaP2PlayState PlayState = ENaiwaP2PlayState::Aiming;
    double AimAngleDegrees = 45.0;
    double AimPower = 0.5;
    double StepAccumulator = 0.0;
    double ResolveDelay = 0.0;
    double ActionNoticeRemaining = 0.0;
    double LevelElapsedSeconds = 0.0;
    double FeedbackImageRemaining = 0.0;
    double FeedbackCueAgeSeconds = 0.0;
    double PouchReturnRemaining = 0.0;
    double PouchReturnDuration = 0.16;
    double CurrentShotAngle = 45.0;
    double CurrentShotPower = 0.5;
    double StructureCollapseElapsed = 0.0;
    double StructureCollapseDuration = 0.58;
    int32 ShotsFired = 0;
    int32 LevelResetCount = 0;
    int32 LastMinuteCueNumber = 0;
    int32 FeedbackCueSerial = 0;
    int32 StateIndex = 0;
    bool bHasAimPreview = false;
    bool bRunPlayTest = false;
    bool bRunStructurePlayTest = false;
    bool bRunRefundPlayTest = false;
    bool bRun22PlayTest = false;
    bool bRun22RefundPlayTest = false;
    bool b22PlayTestCompleted = false;
    bool b22RefundPlayTestCompleted = false;
    int32 M2bPlayTestNextShot = 0;
    int32 M2bRefundPlayTestStep = 0;
    bool bPlayTestCompleted = false;
    bool bStructurePlayTestCompleted = false;
    bool bRefundPlayTestCompleted = false;
    bool bPouchReturning = false;
    bool bStructureCollapseAnimating = false;
    bool bShotStructureEffectsApplied = false;
    bool bFreeRetryAvailable = true;
    bool bHasShotStartSnapshot = false;
    ENaiwaP2FeedbackImage ActiveFeedbackImage = ENaiwaP2FeedbackImage::None;
    NaiwaP2::EEggKind SelectedEgg = NaiwaP2::EEggKind::Base;
    NaiwaP2::EEggKind ShotEgg = NaiwaP2::EEggKind::Base;
    NaiwaP2::EEggKind ShotStartSelectedEgg = NaiwaP2::EEggKind::Base;
    FString CurrentLevelId = TEXT("2-1");
    int32 CurrentStage = 1;
    bool bCampaignRun = false;
    FString ActionNotice;
    FVector PouchReturnStart = FVector::ZeroVector;
    FTimerHandle CampaignAdvanceTimer;

    UPROPERTY()
    TObjectPtr<USoundBase> Music21 = nullptr;

    UPROPERTY()
    TObjectPtr<USoundBase> Music22 = nullptr;

    UPROPERTY()
    TObjectPtr<USoundBase> Music23 = nullptr;

    UPROPERTY()
    TObjectPtr<USoundBase> GoalHitSound = nullptr;

    UPROPERTY()
    TObjectPtr<USoundBase> MissSound = nullptr;

    UPROPERTY()
    TObjectPtr<USoundBase> CollectionSound = nullptr;

    UPROPERTY()
    TObjectPtr<USoundBase> FailureTrumpetSound = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> LevelMusicComponent = nullptr;

    int32 ActiveDuckedSoundCount = 0;

    /**
     * 五变体视觉资产表：按 EEggKind 索引。
     *
     * 材质槽契约（0-3 顺序锁死，slot 4 可为空）：
     *   slot 0 Body / 1 EyeWhite / 2 Pupil / 3 Mouth / 4 Accent
     * 0-3 由网格资产自身的材质指派决定，代码只在换变体色时覆盖 slot 0；
     * slot 4 承载鸡冠、围巾或吸管颜色。Base 与 Mouse 的 slot 4 留空是预期状态 ——
     * 空槽没有 face 指派就没有 section，也就不产生 draw call，编辑器里只有"空槽"警告。
     *
     * 表内容由构造函数里的 ConstructorHelpers 填好：本工程没有 BP_GameMode 子类
     * （关卡直接指定 C++ 类），编辑器无法赋值；同时硬引用能保证这些资产被 cook 收集。
     * 旧的字符串 LoadObject 路径不进 cook，打包版会静默降级成灰球，保留它只是美术交付前的兜底。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Naiwa|Egg")
    TMap<uint8, FNaiwaP2EggVisual> EggVisuals;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> PersistentActors;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> AttemptActors;

    TMap<FString, TObjectPtr<AActor>> StructureActors;

    /**
     * 结构刚体。与 StructureActors 同键。
     *
     * 保留 StructureActors 是因为重试/快照那套仍按 ID 迭代；两者指向同一批 Actor，
     * 只是这里的类型更强，可以调刚体接口。
     */
    UPROPERTY(Transient)
    TArray<TObjectPtr<ANaiwaP2StructureBlock>> StructureBlocks;

    /**
     * 是否为 true 时由真实物理驱动结构。
     *
     * 置 false 会退回旧的"查表倒塌"（StartStructureCollapse 把 FBox2 缩小 + 移动 Actor）。
     * 保留这个开关是为了能一条命令做新旧对照，不要在没有对照数据的情况下删掉旧路径。
     */
    bool bPhysicsStructureEnabled = true;

    /** 隔离的 F1 手动 Chaos 试射入口，由 -NaiwaP2PhysicsSandbox 开启。 */
    bool bRunPhysicsSandbox = false;
    /** 无屏幕人工输入的单发碰撞诊断，由 -NaiwaP2PhysicsImpactProbe 开启。 */
    bool bRunPhysicsImpactProbe = false;
    /** 完整 P2 刚体路径；正式 2-1/2-2/2-3 地图自动开启，探针也可用命令行开启。 */
    bool bRunPhysicsFullLevel = false;
    /** 隐藏自动单发回归，由 -NaiwaP2PhysicsLevelAutoTest 开启。 */
    bool bRunPhysicsLevelAutoTest = false;
    /** 单发后执行 Retry 并验证全关卡状态重建。 */
    bool bRunPhysicsLevelResetTest = false;
    /** 零击杀且场景恢复初态后自动验证补发与整局重建。 */
    bool bRunPhysicsLevelRefundTest = false;
    /** 固定三发 2-1 回归，验证连续发射、每发结算与终局状态。 */
    bool bRunPhysicsLevelMultiShotTest = false;
    /** Hidden single-egg skill probe: BASE/CHICK/MOUSE/SOY/TEA. */
    bool bRunPhysicsLevelSkillProbe = false;
    /** Record real game-loop intervals and validate a 1920x1080 frame-time probe. */
    bool bRunPhysicsFrameTimeProbe = false;
    double PhysicsFrameTimeLastSampleSeconds = 0.0;
    bool bPhysicsFrameTimeWasSampling = false;
    TArray<double> PhysicsFrameIntervalMs;
    TArray<double> PhysicsFrameWorldDeltaMs;
    NaiwaP2::EEggKind PhysicsLevelSkillProbeEgg = NaiwaP2::EEggKind::Chick;
    FString PhysicsLevelSkillProbeLabel;
    bool bPhysicsLevelSkillProbeEffectObserved = false;
    bool bPhysicsLevelTeaBlastApplied = false;
    bool bPhysicsLevelChickImpulseApplied = false;
    int32 PhysicsLevelMouseSplitSpawnedCount = 0;
    double PhysicsLevelChickDeltaVelocityZCmPerS = 0.0;
    double PhysicsLevelSkillProbeRadiusCm = 0.0;
    double PhysicsLevelSkillProbeMassKg = 0.0;
    double PhysicsLevelSkillProbeVisualScale = 0.0;
    /** Diagnostic-only: let the synthetic bridges across authored gaps yield under impact. */
    double PhysicsImpactProbeAngleDegrees = 19.37;
    double PhysicsImpactProbePower = 1.0;
    FString PhysicsImpactProbeTargetBlockId = TEXT("T.c1");
    /** Hidden physics tuning override; 1.0 preserves the baseline density profile. */
    double PhysicsStructureMassMultiplier = 1.0;
    TArray<double> PhysicsLevelMultiShotAngles = {34.0, 19.37, 26.0};
    double PhysicsSandboxShotElapsed = 0.0;
    double PhysicsSandboxSlowElapsed = 0.0;
    double PhysicsLevelShotElapsed = 0.0;
    double PhysicsLevelProjectileSlowElapsed = 0.0;
    double PhysicsLevelSceneQuietElapsed = 0.0;
    double PhysicsLevelMaxSceneSpeedCmPerS = 0.0;
    int32 PhysicsLevelShotCollisionCount = 0;
    int32 PhysicsLevelResolvedProjectileImpactCount = 0;
    int32 PhysicsLevelMultiShotResolvedCount = 0;
    int32 PhysicsLevelMultiShotTotalImpacts = 0;
    int32 PhysicsLevelMultiShotTotalTargetsHit = 0;
    bool bPhysicsLevelMultiShotPassed = true;

    // ---- 塔自稳自检（-NaiwaP2PhysicsStability） ----
    bool bRunPhysicsStabilityTest = false;
    bool bPhysicsStabilityTestDone = false;
    bool bDisableVisualGoalCollisionAfterBeginPlay = false;
    double PhysicsStabilityElapsed = 0.0;
    double PhysicsStabilityMaxDriftCm = 0.0;
    double PhysicsStabilityMaxSpeedCmPerS = 0.0;
    /** 5 秒内出现过的最大倾角（绕 Y）。与位移一起判断「被挤开」还是「被压歪」。 */
    double PhysicsStabilityMaxLeanDeg = 0.0;
    FString PhysicsStabilityWorstBlockId;
    TMap<FString, FTransform> PhysicsStabilityStartGoalTransforms;

    TArray<FString> CollapseActorIds;

    TMap<FString, FVector> CollapseStartLocations;
    TMap<FString, FVector> CollapseTargetLocations;
    TMap<FString, FVector> CollapseStartScales;
    TMap<FString, FVector> CollapseTargetScales;
    TMap<FString, NaiwaP2::FBox2> CollapseStartBoxes;
    TMap<FString, NaiwaP2::FBox2> CollapseTargetBoxes;
    NaiwaP2::FScene ShotStartScene;
    TArray<NaiwaP2::EEggKind> ShotStartAvailableEggs;
    TMap<FString, FTransform> ShotStartStructureTransforms;
    TMap<FString, FTransform> ShotStartGoalTransforms;

    UPROPERTY(Transient)
    TMap<FString, TObjectPtr<AActor>> GoalActors;

    UPROPERTY(Transient)
    TObjectPtr<AActor> ProjectileActor;

    /** Every live Chaos egg in the current shot; Mouse can replace one with three. */
    UPROPERTY(Transient)
    TArray<TObjectPtr<ANaiwaP2PhysicsEgg>> PhysicsLevelActiveProjectiles;

    /** All projectiles from this shot, including settled eggs retained in the scene. */
    UPROPERTY(Transient)
    TArray<TObjectPtr<ANaiwaP2PhysicsEgg>> PhysicsLevelShotProjectiles;

    TMap<TWeakObjectPtr<ANaiwaP2PhysicsEgg>, double> PhysicsLevelProjectileSlowElapsedByEgg;

    UPROPERTY(Transient)
    TObjectPtr<AActor> PouchActor;

    UPROPERTY(Transient)
    TObjectPtr<ANaiwaP2Archer> ArcherActor;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> SplitProjectileActors;
};
