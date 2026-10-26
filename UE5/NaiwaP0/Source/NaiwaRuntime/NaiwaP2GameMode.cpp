#include "NaiwaP2GameMode.h"
#include "NaiwaMenuProgress.h"
#include "NaiwaP2ImpactEffectSubsystem.h"
#include "NaiwaP2Archer.h"

#include "NaiwaP2Ballistics.h"
#include "NaiwaP2PlayerController.h"
#include "NaiwaP2HUD.h"
#include "NaiwaP2SceneLibrary.h"
#include "NaiwaP2PhysicsProfile.h"
#include "NaiwaP2PhysicsEgg.h"
#include "NaiwaP2PhysicsGoal.h"
#include "NaiwaP2StructureBlock.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundBase.h"
#include "UnrealClient.h"

namespace
{
    constexpr double CentimetersPerMeter = 100.0;
    constexpr double CameraOrthoWidthCm = 2310.0;
    constexpr double CameraCenterXCm = 600.0;
    // 2-1 Geo art reference in render_final.py: 21.5 m width, center (8.60, 4.235) m.
    constexpr double CameraOrthoWidth21Cm = 2150.0;
    constexpr double CameraCenterX21Cm = 860.0;
    constexpr double CameraCenterZ21Cm = 423.5;
    // Raise the camera center so the floor sits lower in the viewport and the
    // unused band below the playable area shrinks. Scene geometry stays put.
    constexpr double CameraCenterZCm = 390.0;

    // 奶蛋材质槽契约，与 NaiwaP2GameMode.h 里 EggVisuals 的注释一一对应。
    constexpr int32 EggBodySlotIndex = 0;
    constexpr int32 EggAccentSlotIndex = 4;
    constexpr int32 EggMaterialSlotCount = 5;

    FAutoConsoleCommandWithWorld StartP2PhysicsStabilityProbeCommand(
        TEXT("NaiwaP2.Stability.Start"),
        TEXT("Start a 5-second P2 structure stability sample in the active PIE or standalone game world."),
        FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
        {
            if (!World || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game))
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("NAIWA_P2_STABILITY_START rejected=not_game_world"));
                return;
            }

            ANaiwaP2GameMode* GameMode = Cast<ANaiwaP2GameMode>(
                UGameplayStatics::GetGameMode(World));
            if (!GameMode)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("NAIWA_P2_STABILITY_START rejected=game_mode_missing"));
                return;
            }

            GameMode->StartPhysicsStabilityProbe();
        }));

    // 资产表缺项时的兜底路径。字符串引用不进 cook 收集，打包版可能取不到资产，
    // 所以正式包必须靠 EggVisuals（构造函数里的硬引用）供货，而不是这条。
    constexpr const TCHAR* LegacyEggMeshPath =
        TEXT("/Game/Naiwa/P2/Art/Egg/SK_NaiwaEgg.SK_NaiwaEgg");

    /**
     * 豆奶的视觉缩放与判定倍率是**两个独立参数，故意不相等**。
     *
     * 判定：GetEggSpec 里 RadiusMultiplier = 1.90 → 球 r = 0.836，这个数不动。
     * 视觉：1.6106 —— 蛋是"高瘦"体（半高 0.575、最宽点三维距球心 0.4631），
     *       没有任何缩放能让蛋和球完全贴合，只能取"两端偏差相等"的 minimax 解：
     *       0.575s − 0.836 = 0.836 − 0.4631s  →  s = 0.88 / 1.0381 = 1.6106。
     *       此时蛋顶/底超出球 0.090 m、最宽点欠球 0.090 m，最大偏差 8 px，
     *       且两侧对称，整体读作"稍微碰到就算中"，偏宽容。
     *
     * 不要"顺手统一"成 1.90：1.90 时最宽点只超 3.7 px，但蛋顶/底尖超 21 px、
     * 围巾尾最外点超 25 px —— 而抛物线砸击最常见的接触点恰恰是蛋底，
     * 玩家会稳定看到"蛋底已经插进目标了，但没判中"。
     */
    constexpr double SoyVisualScale = 1.6106;

    /** 只有豆奶做视觉放大；其余四个变体判定与视觉都是 1.0，不缩放。 */
    double GetEggVisualScale(const NaiwaP2::EEggKind Kind)
    {
        return Kind == NaiwaP2::EEggKind::Soy ? SoyVisualScale : 1.0;
    }

    /** 只在构造 CDO 时使用：已导入资产建立硬引用；未交付的变体先跳过，避免每次启动报错。 */
    template <typename TAssetType>
    TAssetType* LoadEggVisualAsset(const TCHAR* Path)
    {
        const FString PackageName = FPackageName::ObjectPathToPackageName(FString(Path));
        if (PackageName.IsEmpty() || !FPackageName::DoesPackageExist(PackageName))
        {
            return nullptr;
        }
        ConstructorHelpers::FObjectFinder<TAssetType> Finder(Path);
        return Finder.Succeeded() ? Finder.Object.Get() : nullptr;
    }

    FVector ToWorld(const double X, const double Z)
    {
        return FVector(X * CentimetersPerMeter, 0.0, Z * CentimetersPerMeter);
    }

    UPrimitiveComponent* GetPhysicsGoalBody(AActor* Actor)
    {
        if (ANaiwaP2PhysicsGoal* PhysicsGoal = Cast<ANaiwaP2PhysicsGoal>(Actor))
        {
            return PhysicsGoal->GetCollisionCapsule();
        }
        return IsValid(Actor) ? Actor->FindComponentByClass<UPrimitiveComponent>() : nullptr;
    }

    UStaticMesh* LoadMesh(const TCHAR* Path)
    {
        return LoadObject<UStaticMesh>(nullptr, Path);
    }

    // 仅供资产表缺项时兜底（美术交付中间态、或打包丢资产）。
    // 正常路径走 EggVisuals 里的硬引用；这里的字符串路径不进 cook 收集。
    const TCHAR* GetEggMaterialPath(const NaiwaP2::EEggKind Kind)
    {
        switch (Kind)
        {
        case NaiwaP2::EEggKind::Chick:
            return TEXT("/Game/Naiwa/P2/Art/Egg/Materials/MI_NaiwaEgg_Chick.MI_NaiwaEgg_Chick");
        case NaiwaP2::EEggKind::Mouse:
            return TEXT("/Game/Naiwa/P2/Art/Egg/Materials/MI_NaiwaEgg_Mouse.MI_NaiwaEgg_Mouse");
        case NaiwaP2::EEggKind::Soy:
            return TEXT("/Game/Naiwa/P2/Art/Egg/Materials/MI_NaiwaEgg_Soy.MI_NaiwaEgg_Soy");
        case NaiwaP2::EEggKind::Tea:
            return TEXT("/Game/Naiwa/P2/Art/Egg/Materials/MI_NaiwaEgg_Tea.MI_NaiwaEgg_Tea");
        case NaiwaP2::EEggKind::Base:
        default:
            return TEXT("/Game/Naiwa/P2/Art/Egg/Materials/MI_NaiwaEgg_Base.MI_NaiwaEgg_Base");
        }
    }

    UMaterialInterface* LoadMaterial(const TCHAR* Path)
    {
        return LoadObject<UMaterialInterface>(nullptr, Path);
    }

    // ---- P2-ART-INTEG：Geo 风格背景数据 ----
    //
    // 2-1 折线逐点抄自 render_final.py；2-2/2-3 折线按 P2 美术计划 §3.3
    // 的构图草案定稿。单位米，折线 (X, Z) 从左到右；厚度沿用相同层级间距。
    constexpr const TCHAR* GeoMaterialDir =
        TEXT("/Game/Naiwa/P2/Art/Geo/Materials/");

    FString GeoMaterialPath(const TCHAR* Name)
    {
        return FString(GeoMaterialDir) + Name + TEXT(".") + Name;
    }

    FColor GetGeoEggBodyColor(const NaiwaP2::EEggKind Kind)
    {
        // Keep the canonical sRGB colors from奶蛋参数.json; Geo mode renders them
        // through the same Unlit material contract as the surrounding level art.
        switch (Kind)
        {
        case NaiwaP2::EEggKind::Chick: return FColor(0xF5, 0xC5, 0x42);
        case NaiwaP2::EEggKind::Mouse: return FColor(0xE6, 0xD9, 0xB8);
        case NaiwaP2::EEggKind::Soy: return FColor(0xCF, 0xE0, 0xA8);
        case NaiwaP2::EEggKind::Tea: return FColor(0xC8, 0x9A, 0x6A);
        case NaiwaP2::EEggKind::Base:
        default: return FColor(0xEF, 0x9F, 0x27);
        }
    }

    // 折线数据（render_final.py 逐点抄录，(X, Z) 米，从左到右）。
    TArray<FVector2D> MakeGeoPolylineL4(const FString& LevelId)
    {
        if (LevelId == TEXT("2-2"))
        {
            return { FVector2D(-4.5, 5.0), FVector2D(-1.0, 5.4), FVector2D(2.5, 4.9),
                     FVector2D(5.5, 5.5), FVector2D(8.5, 5.1), FVector2D(11.0, 5.4),
                     FVector2D(13.6, 5.8), FVector2D(15.3, 8.0), FVector2D(17.0, 6.5),
                     FVector2D(20.2, 5.3), FVector2D(24.0, 4.8) };
        }
        if (LevelId == TEXT("2-3"))
        {
            return { FVector2D(-4.5, 5.0), FVector2D(-1.0, 5.7), FVector2D(2.5, 4.9),
                     FVector2D(6.0, 5.4), FVector2D(9.0, 5.0), FVector2D(12.5, 6.1),
                     FVector2D(16.3, 8.3), FVector2D(18.5, 6.8), FVector2D(21.0, 5.8),
                     FVector2D(24.0, 5.2) };
        }
        return { FVector2D(-4.5, 5.2), FVector2D(-1.2, 6.1), FVector2D(2.4, 5.5),
                 FVector2D(6.0, 6.4), FVector2D(9.2, 5.8), FVector2D(12.0, 6.9),
                 FVector2D(15.5, 8.3), FVector2D(18.6, 6.6), FVector2D(21.2, 5.9),
                 FVector2D(24.0, 5.4) };
    }

    TArray<FVector2D> MakeGeoPolylineL3(const FString& LevelId)
    {
        if (LevelId == TEXT("2-2"))
        {
            return { FVector2D(-4.5, 4.0), FVector2D(-1.5, 4.6), FVector2D(2.0, 4.2),
                     FVector2D(5.5, 4.8), FVector2D(8.5, 4.3), FVector2D(11.5, 4.9),
                     FVector2D(14.0, 5.2), FVector2D(15.3, 5.5), FVector2D(17.8, 5.0),
                     FVector2D(20.8, 4.4), FVector2D(24.0, 4.0) };
        }
        if (LevelId == TEXT("2-3"))
        {
            return { FVector2D(-4.5, 4.0), FVector2D(-1.0, 4.8), FVector2D(2.5, 4.3),
                     FVector2D(6.0, 4.8), FVector2D(9.2, 4.4), FVector2D(12.5, 5.5),
                     FVector2D(16.3, 5.8), FVector2D(18.5, 5.1), FVector2D(21.0, 4.7),
                     FVector2D(24.0, 4.1) };
        }
        return { FVector2D(-4.5, 4.1), FVector2D(-1.8, 4.8), FVector2D(1.5, 4.3),
                 FVector2D(4.2, 5.0), FVector2D(7.4, 4.4), FVector2D(10.4, 5.0),
                 FVector2D(13.4, 5.4), FVector2D(15.5, 5.9), FVector2D(17.8, 5.2),
                 FVector2D(20.6, 4.6), FVector2D(24.0, 4.2) };
    }

    TArray<FVector2D> MakeGeoPolylineL2(const FString& LevelId)
    {
        if (LevelId == TEXT("2-2"))
        {
            return { FVector2D(-4.5, 2.1), FVector2D(0.0, 2.3), FVector2D(4.0, 2.0),
                     FVector2D(8.0, 2.25), FVector2D(12.0, 2.35), FVector2D(15.3, 2.2),
                     FVector2D(19.0, 2.1), FVector2D(24.0, 1.9) };
        }
        if (LevelId == TEXT("2-3"))
        {
            return { FVector2D(-4.5, 2.0), FVector2D(0.0, 2.25), FVector2D(4.0, 2.0),
                     FVector2D(8.0, 2.3), FVector2D(12.5, 2.25), FVector2D(16.0, 2.15),
                     FVector2D(20.0, 2.0), FVector2D(24.0, 1.85) };
        }
        return { FVector2D(-4.5, 2.05), FVector2D(2.0, 2.35), FVector2D(6.4, 1.95),
                 FVector2D(10.6, 2.20), FVector2D(14.0, 2.35), FVector2D(18.0, 2.05),
                 FVector2D(24.0, 1.85) };
    }


    /**
     * 判定一个 FBox2 在物理化之后是"布景"还是"结构"。
     *
     * 这是**唯一**决定哪些块参与求解的地方。改关卡时要同步这张表。
     *
     * 判为布景（静态碰撞体，不倒）的三类：
     *   1. 地基 `.base`、挡墙 `.wall` —— 它们就是地面和边界，倒了整个关卡就没有站立面。
     *   2. 高台 `A.col` / `A.plate` —— 2-2 的 7.2 m 立柱 + 台面。0.60 m 宽 7.2 m 高，
     *      高宽比 12，做成动态刚体即使不撞也会自己晃；而且 A1 目标立在台面上，
     *      台面塌了 A1 就必须跟着掉（那是 M3 的目标物理化，本轮不做）。
     *   3. 吊桥 `S.rope1` / `S.rope2` / `S.deck` —— 2-3 里悬在 z = 4.25~7.10 的一段，
     *      下面没有任何支撑。这是原设计里的"空中平台"，物理化之后它必须要么被钉住、
     *      要么直接自由落体掉下去。钉住才是保持关卡的唯一选择。
     */
    bool IsSceneryBox(const FString& Id)
    {
        if (Id.EndsWith(TEXT(".base")) || Id.EndsWith(TEXT(".wall")))
        {
            return true;
        }

        static const TCHAR* const ExplicitScenery[] =
        {
            TEXT("A.col"), TEXT("A.plate"),
            TEXT("S.rope1"), TEXT("S.rope2"), TEXT("S.deck")
        };
        for (const TCHAR* Candidate : ExplicitScenery)
        {
            if (Id.Equals(Candidate, ESearchCase::CaseSensitive))
            {
                return true;
            }
        }
        return false;
    }

    /**
     * 结构块角色：布景 → Foundation；其余按高宽比分 Column / Board。
     *
     * 分 Column / Board 只影响密度与阻尼两档参数，**不改变几何**。
     * 用高宽比而不是写死 ID，是为了关卡数据里加一根柱/一块板时不用回来改代码。
     */
    NaiwaP2::Physics::EBlockRole ClassifyBlockRole(const NaiwaP2::FBox2& Box)
    {
        if (IsSceneryBox(Box.Id))
        {
            return NaiwaP2::Physics::EBlockRole::Foundation;
        }
        const double Width = Box.MaxX - Box.MinX;
        const double Height = Box.MaxZ - Box.MinZ;
        return Width >= Height
            ? NaiwaP2::Physics::EBlockRole::Board
            : NaiwaP2::Physics::EBlockRole::Column;
    }
}

ANaiwaP2GameMode::ANaiwaP2GameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = nullptr;
    PlayerControllerClass = ANaiwaP2PlayerController::StaticClass();
    HUDClass = ANaiwaP2HUD::StaticClass();

    static ConstructorHelpers::FObjectFinder<USoundBase> Level21Music(
        TEXT("/Game/Naiwa/P2/Audio/SW_P2_21_TitleTheme.SW_P2_21_TitleTheme"));
    if (Level21Music.Succeeded()) Music21 = Level21Music.Object;
    static ConstructorHelpers::FObjectFinder<USoundBase> Level22Music(
        TEXT("/Game/Naiwa/P2/Audio/SW_P2_22_RainLove.SW_P2_22_RainLove"));
    if (Level22Music.Succeeded()) Music22 = Level22Music.Object;
    static ConstructorHelpers::FObjectFinder<USoundBase> Level23Music(
        TEXT("/Game/Naiwa/P2/Audio/SW_P2_23_BoonieBears.SW_P2_23_BoonieBears"));
    if (Level23Music.Succeeded()) Music23 = Level23Music.Object;
    static ConstructorHelpers::FObjectFinder<USoundBase> GoalHitCue(
        TEXT("/Game/Naiwa/P2/Audio/SFX_P2_GoalHit.SFX_P2_GoalHit"));
    if (GoalHitCue.Succeeded()) GoalHitSound = GoalHitCue.Object;
    static ConstructorHelpers::FObjectFinder<USoundBase> MissCue(
        TEXT("/Game/Naiwa/P2/Audio/SFX_P2_Miss.SFX_P2_Miss"));
    if (MissCue.Succeeded()) MissSound = MissCue.Object;
    static ConstructorHelpers::FObjectFinder<USoundBase> CollectionCue(
        TEXT("/Game/Naiwa/P2/Audio/SFX_P2_Collection.SFX_P2_Collection"));
    if (CollectionCue.Succeeded()) CollectionSound = CollectionCue.Object;
    static ConstructorHelpers::FObjectFinder<USoundBase> FailureTrumpetCue(
        TEXT("/Game/Naiwa/P2/Audio/SFX_P2_FailureTrumpet.SFX_P2_FailureTrumpet"));
    if (FailureTrumpetCue.Succeeded()) FailureTrumpetSound = FailureTrumpetCue.Object;

    // ---- 五变体视觉资产表 ----
    // Base 故意沿用既有的 SK_NaiwaEgg / MI_NaiwaEgg_Base，不重命名，
    // 这样现有资产、现有 validation 与现有 96px 构图全部不受影响；
    // 其余四个变体是新增资产，路径即美术交付契约。
    // 美术交付前这些路径取不到，FObjectFinder 会打 "Failed to find" 日志并返回空，
    // 代码随后回落到旧的字符串路径，行为与改动前完全一致。
    struct FDefaultEggVisualRow
    {
        NaiwaP2::EEggKind Kind;
        const TCHAR* MeshPath;
        const TCHAR* BodyPath;
        const TCHAR* AccentPath;   // slot 4，可空
    };
    static const FDefaultEggVisualRow DefaultRows[] =
    {
        {
            NaiwaP2::EEggKind::Base,
            TEXT("/Game/Naiwa/P2/Art/Egg/SK_NaiwaEgg.SK_NaiwaEgg"),
            TEXT("/Game/Naiwa/P2/Art/Egg/Materials/MI_NaiwaEgg_Base.MI_NaiwaEgg_Base"),
            nullptr
        },
        {
            NaiwaP2::EEggKind::Chick,
            TEXT("/Game/Naiwa/P2/Art/Egg/SK_NaiwaEgg_Chick.SK_NaiwaEgg_Chick"),
            TEXT("/Game/Naiwa/P2/Art/Egg/Materials/MI_NaiwaEgg_Chick.MI_NaiwaEgg_Chick"),
            TEXT("/Game/Naiwa/P2/Art/Egg/Materials/MI_Accent_Crest.MI_Accent_Crest")
        },
        {
            NaiwaP2::EEggKind::Mouse,
            TEXT("/Game/Naiwa/P2/Art/Egg/SK_NaiwaEgg_Mouse.SK_NaiwaEgg_Mouse"),
            TEXT("/Game/Naiwa/P2/Art/Egg/Materials/MI_NaiwaEgg_Mouse.MI_NaiwaEgg_Mouse"),
            nullptr
        },
        {
            NaiwaP2::EEggKind::Soy,
            TEXT("/Game/Naiwa/P2/Art/Egg/SK_NaiwaEgg_Soy.SK_NaiwaEgg_Soy"),
            TEXT("/Game/Naiwa/P2/Art/Egg/Materials/MI_NaiwaEgg_Soy.MI_NaiwaEgg_Soy"),
            TEXT("/Game/Naiwa/P2/Art/Egg/Materials/MI_Accent_Scarf.MI_Accent_Scarf")
        },
        {
            NaiwaP2::EEggKind::Tea,
            TEXT("/Game/Naiwa/P2/Art/Egg/SK_NaiwaEgg_Tea.SK_NaiwaEgg_Tea"),
            TEXT("/Game/Naiwa/P2/Art/Egg/Materials/MI_NaiwaEgg_Tea.MI_NaiwaEgg_Tea"),
            TEXT("/Game/Naiwa/P2/Art/Egg/Materials/MI_Accent_Straw.MI_Accent_Straw")
        },
    };

    for (const FDefaultEggVisualRow& Row : DefaultRows)
    {
        FNaiwaP2EggVisual Visual;
        Visual.Mesh = LoadEggVisualAsset<USkeletalMesh>(Row.MeshPath);
        Visual.BodyMaterial = LoadEggVisualAsset<UMaterialInterface>(Row.BodyPath);
        if (Row.AccentPath)
        {
            Visual.AccentMaterial = LoadEggVisualAsset<UMaterialInterface>(Row.AccentPath);
        }
        EggVisuals.Add(static_cast<uint8>(Row.Kind), Visual);
    }
}

void ANaiwaP2GameMode::BeginPlay()
{
    Super::BeginPlay();

    InitializeLevelRules();
    bCampaignRun = FParse::Param(FCommandLine::Get(), TEXT("NaiwaLinkRun")) ||
        (GetWorld() && GetWorld()->URL.HasOption(TEXT("NaiwaLinkRun")));
    PlayLevelMusic();
    LogEggVisualTableDiagnostics();
    bRunPlayTest = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP2PlayTest"));
    bRunStructurePlayTest = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP2StructurePlayTest"));
    bRunRefundPlayTest = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP2RefundPlayTest"));
    bRun22PlayTest = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP2_22PlayTest"));
    bRun22RefundPlayTest = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP2_22RefundPlayTest"));
    const FString RuntimeMapName = GetWorld() ? GetWorld()->GetMapName() : FString();
    const bool bPhysicsPrototypeMap = RuntimeMapName.Contains(TEXT("L_NaiwaP2_21_Prototype"));
    const bool bStandalonePhysicsMap = RuntimeMapName.EndsWith(TEXT("L_NaiwaP2_21"), ESearchCase::IgnoreCase) ||
        RuntimeMapName.EndsWith(TEXT("L_NaiwaP2_22"), ESearchCase::IgnoreCase) ||
        RuntimeMapName.EndsWith(TEXT("L_NaiwaP2_23"), ESearchCase::IgnoreCase);
    bRunPhysicsLevelResetTest = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP2PhysicsLevelResetTest"));
    bRunPhysicsLevelRefundTest = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP2PhysicsLevelRefundTest"));
    bRunPhysicsLevelMultiShotTest = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP2PhysicsLevelMultiShotTest"));
    FString SkillProbeArg;
    bRunPhysicsLevelSkillProbe = FParse::Value(FCommandLine::Get(),
        TEXT("NaiwaP2PhysicsLevelSkillProbe="), SkillProbeArg);
    if (bRunPhysicsLevelSkillProbe)
    {
        PhysicsLevelSkillProbeLabel = SkillProbeArg.ToUpper();
        if (PhysicsLevelSkillProbeLabel == TEXT("BASE"))
        {
            PhysicsLevelSkillProbeEgg = NaiwaP2::EEggKind::Base;
        }
        else if (PhysicsLevelSkillProbeLabel == TEXT("CHICK"))
        {
            PhysicsLevelSkillProbeEgg = NaiwaP2::EEggKind::Chick;
        }
        else if (PhysicsLevelSkillProbeLabel == TEXT("MOUSE"))
        {
            PhysicsLevelSkillProbeEgg = NaiwaP2::EEggKind::Mouse;
        }
        else if (PhysicsLevelSkillProbeLabel == TEXT("SOY"))
        {
            PhysicsLevelSkillProbeEgg = NaiwaP2::EEggKind::Soy;
        }
        else if (PhysicsLevelSkillProbeLabel == TEXT("TEA"))
        {
            PhysicsLevelSkillProbeEgg = NaiwaP2::EEggKind::Tea;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("NAIWA_P2_PHYS_SKILL_PROBE pass=0 reason=unknown_egg egg=%s"),
                *PhysicsLevelSkillProbeLabel);
            FPlatformMisc::RequestExitWithStatus(false, 1,
                TEXT("Naiwa P2 physics skill probe egg is unknown"));
            return;
        }
    }
    bRunPhysicsLevelAutoTest = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP2PhysicsLevelAutoTest")) ||
        bRunPhysicsLevelResetTest || bRunPhysicsLevelRefundTest || bRunPhysicsLevelMultiShotTest ||
        bRunPhysicsLevelSkillProbe;
    bRunPhysicsFrameTimeProbe = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP2PhysicsFrameTimeProbe"));
    if (bRunPhysicsFrameTimeProbe)
    {
        bRunPhysicsLevelAutoTest = true;
        PhysicsFrameIntervalMs.Reserve(4096);
        PhysicsFrameWorldDeltaMs.Reserve(4096);
        PhysicsFrameTimeLastSampleSeconds = 0.0;
        bPhysicsFrameTimeWasSampling = false;
        const IConsoleVariable* VSyncCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
        const IConsoleVariable* ScreenPercentageCVar =
            IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_FRAME_TIME_PROBE_START target=1920x1080 threshold_p95_ms=16.7 "
                 "max_fps=%.1f vsync=%d screen_percentage=%.1f"),
            GEngine ? GEngine->GetMaxFPS() : -1.0f,
            VSyncCVar ? VSyncCVar->GetInt() : -1,
            ScreenPercentageCVar ? ScreenPercentageCVar->GetFloat() : -1.0f);
    }
    bRunPhysicsFullLevel = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP2PhysicsLevel")) ||
        bRunPhysicsLevelAutoTest || bStandalonePhysicsMap;
    if (bRunPhysicsLevelSkillProbe)
    {
        if (!AmmoRule.EggList.Contains(PhysicsLevelSkillProbeEgg))
        {
            UE_LOG(LogTemp, Error,
                TEXT("NAIWA_P2_PHYS_SKILL_PROBE pass=0 reason=egg_unavailable level=%s egg=%s"),
                *CurrentLevelId, *PhysicsLevelSkillProbeLabel);
            FPlatformMisc::RequestExitWithStatus(false, 1,
                TEXT("Naiwa P2 physics skill probe egg is unavailable in this level"));
            return;
        }
        SelectedEgg = PhysicsLevelSkillProbeEgg;
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_PHYS_SKILL_PROBE_START level=%s egg=%s"),
            *CurrentLevelId, *PhysicsLevelSkillProbeLabel);
    }
    bRunPhysicsImpactProbe = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP2PhysicsImpactProbe"));
    FParse::Value(FCommandLine::Get(), TEXT("NaiwaP2ImpactAngle="), PhysicsImpactProbeAngleDegrees);
    FParse::Value(FCommandLine::Get(), TEXT("NaiwaP2ImpactPower="), PhysicsImpactProbePower);
    FParse::Value(FCommandLine::Get(), TEXT("NaiwaP2ImpactTarget="), PhysicsImpactProbeTargetBlockId);
    FParse::Value(FCommandLine::Get(), TEXT("NaiwaP2StructureMassMultiplier="), PhysicsStructureMassMultiplier);
    PhysicsStructureMassMultiplier = FMath::Clamp(PhysicsStructureMassMultiplier, 0.1, 2.0);
    FString MultiShotAnglesArg;
    if (FParse::Value(FCommandLine::Get(), TEXT("NaiwaP2PhysicsLevelAngles="), MultiShotAnglesArg))
    {
        TArray<FString> AngleParts;
        MultiShotAnglesArg.ParseIntoArray(AngleParts, TEXT("+"), true);
        bool bAnglesValid = AngleParts.Num() == AmmoRule.TotalShots;
        TArray<double> ParsedAngles;
        for (const FString& Part : AngleParts)
        {
            const double Angle = FCString::Atod(*Part);
            bAnglesValid &= Angle >= 1.0 && Angle <= 89.0;
            ParsedAngles.Add(Angle);
        }
        if (bAnglesValid)
        {
            PhysicsLevelMultiShotAngles = MoveTemp(ParsedAngles);
        }
        else if (bRunPhysicsLevelMultiShotTest)
        {
            UE_LOG(LogTemp, Error,
                TEXT("NAIWA_P2_PHYS_LEVEL_MULTI_SHOT_TEST pass=0 reason=invalid_angles expected=%d value=%s"),
                AmmoRule.TotalShots, *MultiShotAnglesArg);
            FPlatformMisc::RequestExitWithStatus(false, 1,
                TEXT("Naiwa P2 multi-shot angle sequence is invalid"));
            return;
        }
    }
    PhysicsImpactProbeAngleDegrees = FMath::Clamp(PhysicsImpactProbeAngleDegrees, 1.0, 89.0);
    PhysicsImpactProbePower = FMath::Clamp(PhysicsImpactProbePower, 0.05, 1.0);
    bRunPhysicsSandbox = !bRunPhysicsFullLevel &&
        (FParse::Param(FCommandLine::Get(), TEXT("NaiwaP2PhysicsSandbox")) ||
         bPhysicsPrototypeMap || bRunPhysicsImpactProbe);
    if (bRunPhysicsSandbox)
    {
        CurrentLevelId = TEXT("2-1");
        CurrentStage = 1;
        CurrentScene = NaiwaP2::FSceneLibrary::Make(CurrentLevelId, CurrentStage);
        AmmoRule = NaiwaP2::FSceneLibrary::GetAmmoRule(CurrentLevelId);
        AvailableEggs = AmmoRule.EggList;
        SelectedEgg = AvailableEggs.IsEmpty() ? NaiwaP2::EEggKind::Base : AvailableEggs[0];
        bFreeRetryAvailable = AmmoRule.bHasFreeRetry;
        bRunPlayTest = false;
        bRunStructurePlayTest = false;
        bRunRefundPlayTest = false;
        bRun22PlayTest = false;
        bRun22RefundPlayTest = false;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_PHYS_SANDBOX level=2-1 mode=manual target=T1"));
    }

    // 全物理改造开关：默认走真实物理；-NaiwaP2LegacyCollapse 退回旧的查表倒塌，用于对照。
    bPhysicsStructureEnabled = !FParse::Param(FCommandLine::Get(), TEXT("NaiwaP2LegacyCollapse"));

    // P2-ART-INTEG：默认显示无光照几何美术（Unlit 色板材质 + 程序化背景）；
    // 显式传入 -NaiwaP2ArtStyle=Graybox 时才回退灰盒，便于诊断对照。
    FString NaiwaP2ArtStyle;
    const bool bHasArtStyleOverride = FParse::Value(
        FCommandLine::Get(), TEXT("NaiwaP2ArtStyle="), NaiwaP2ArtStyle);
    bGeoArtStyle = !bHasArtStyleOverride ||
        NaiwaP2ArtStyle.Equals(TEXT("Geo"), ESearchCase::IgnoreCase);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_ART_STYLE style=%s level=%s"),
        bGeoArtStyle ? TEXT("Geo") : TEXT("Graybox"), *CurrentLevelId);

    // 截图探针：-NaiwaP2ScreenshotSeconds=N -NaiwaP2ScreenshotPath=X
    FString ScreenshotSecondsArg;
    if (FParse::Value(FCommandLine::Get(), TEXT("NaiwaP2ScreenshotSeconds="), ScreenshotSecondsArg) &&
        FParse::Value(FCommandLine::Get(), TEXT("NaiwaP2ScreenshotPath="), ScreenshotProbePath))
    {
        ScreenshotProbeDelaySeconds = FMath::Max(0.5, FCString::Atod(*ScreenshotSecondsArg));
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_SCREENSHOT_PROBE delay=%.1fs path=%s"),
            ScreenshotProbeDelaySeconds, *ScreenshotProbePath);
    }
    if (bRunPhysicsFullLevel && CurrentLevelId != TEXT("2-1") && CurrentLevelId != TEXT("2-2") &&
        CurrentLevelId != TEXT("2-3"))
    {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_P2_PHYS_LEVEL_REJECT reason=level_not_supported level=%s expected=2-1_or_2-2_or_2-3"),
            *CurrentLevelId);
        bRunPhysicsFullLevel = false;
        if (bRunPhysicsLevelAutoTest)
        {
            FPlatformMisc::RequestExitWithStatus(false, 1,
                TEXT("Naiwa P2 full physics test currently supports 2-1, 2-2, and 2-3"));
        }
    }
    // 塔自稳自检：跑满 StabilityTestSeconds 后打印最大位移并退出，退出码 = 是否通过。
    bRunPhysicsStabilityTest = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP2PhysicsStability"));

    // 重力必须在生成刚体之前接管，否则第一帧的刚体用的是 9.8 m/s²。
    ApplyP2WorldGravity();

    SpawnPersistentScene();
    SpawnAttemptScene();
    bDisableVisualGoalCollisionAfterBeginPlay =
        !bRunPhysicsSandbox && !bRunPhysicsFullLevel && !GoalActors.IsEmpty();
    SetPhase(ENaiwaP2PlayState::Aiming);

    const bool bLevel22 = CurrentLevelId == TEXT("2-2");
    const bool bLevel23 = CurrentLevelId == TEXT("2-3");
    const bool bLevel21 = CurrentLevelId == TEXT("2-1");
    const bool bGeoLevel21 = bLevel21 && bGeoArtStyle;
    const double CameraWidthCm = (bLevel22 || bGeoLevel21) ? 2150.0 : CameraOrthoWidthCm;
    const double CameraCenterX = bLevel22 ? 840.0 :
        (bLevel23 ? 1080.0 : (bGeoLevel21 ? CameraCenterX21Cm : CameraCenterXCm));
    const double CameraCenterZ = bLevel22 ? 425.0 :
        (bGeoLevel21 ? CameraCenterZ21Cm : CameraCenterZCm);
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(
                ACameraActor::StaticClass(), FVector(CameraCenterX, 2500.0, CameraCenterZ),
                FRotator(0.0, -90.0, 0.0)))
        {
            UCameraComponent* CameraComponent = Camera->GetCameraComponent();
            CameraComponent->ProjectionMode = ECameraProjectionMode::Orthographic;
            CameraComponent->OrthoWidth = CameraWidthCm;
            if (bGeoArtStyle)
            {
                // Keep the Geo palette close to the Blender Standard preview.
                FPostProcessSettings& GeoPostProcess = CameraComponent->PostProcessSettings;
                GeoPostProcess.bOverride_AutoExposureMethod = true;
                GeoPostProcess.AutoExposureMethod = AEM_Histogram;
                GeoPostProcess.bOverride_AutoExposureBias = true;
                GeoPostProcess.AutoExposureBias = 1.5f;
                // Match Blender's Standard display transform for the authored
                // sRGB palette: UE 5.8 exposes the tone-curve reduction per view.
                GeoPostProcess.bOverride_ToneCurveAmount = true;
                GeoPostProcess.ToneCurveAmount = 0.0f;
                GeoPostProcess.bOverride_ExpandGamut = true;
                GeoPostProcess.ExpandGamut = 0.0f;
            }
            PC->SetViewTarget(Camera);
            PersistentActors.Add(Camera);
        }
    }
    const double ArcherX = FMath::Max(-135.0, CameraCenterX - CameraWidthCm * 0.5 + 95.0);
    const double ArcherY = bLevel23 ? -30.0 : 0.0;
    ArcherActor = GetWorld()->SpawnActor<ANaiwaP2Archer>(
        FVector(ArcherX, ArcherY, 0.0), FRotator::ZeroRotator);
    if (ArcherActor)
    {
        ArcherActor->Tags.Add(FName(TEXT("P2_NaiwaArcher")));
        PersistentActors.Add(ArcherActor);
        FString ArcherDrawProbeArg;
        if (FParse::Value(FCommandLine::Get(), TEXT("NaiwaP2ArcherDrawProbe="), ArcherDrawProbeArg))
        {
            const float ProbeProgress = FMath::Clamp(FCString::Atof(*ArcherDrawProbeArg), 0.0f, 1.0f);
            ArcherActor->BeginDraw();
            ArcherActor->SetDrawProgress(ProbeProgress);
            UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_ARCHER_DRAW_PROBE progress=%.3f"), ProbeProgress);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_ARCHER_SPAWN level=%s actor=%s location=(%.1f,%.1f,0)"),
        *CurrentLevelId, *GetNameSafe(ArcherActor), ArcherX, ArcherY);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_PLAY_READY level=%s ortho_width_cm=%.0f"),
        *CurrentLevelId, CameraWidthCm);
    if (bRunPhysicsStabilityTest)
    {
        StartPhysicsStabilityProbe();
    }

}

void ANaiwaP2GameMode::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateScreenshotProbe(DeltaSeconds);

    if (bRunPhysicsFrameTimeProbe)
    {
        const double NowSeconds = FPlatformTime::Seconds();
        const bool bInPhysicsShot = PlayState == ENaiwaP2PlayState::Flying ||
            PlayState == ENaiwaP2PlayState::Resolving;
        if (bInPhysicsShot && bPhysicsFrameTimeWasSampling && PhysicsFrameTimeLastSampleSeconds > 0.0)
        {
            const double IntervalMs = (NowSeconds - PhysicsFrameTimeLastSampleSeconds) * 1000.0;
            if (FMath::IsFinite(IntervalMs) && IntervalMs > 0.0)
            {
                PhysicsFrameIntervalMs.Add(IntervalMs);
                const double WorldDeltaMs = static_cast<double>(DeltaSeconds) * 1000.0;
                if (FMath::IsFinite(WorldDeltaMs) && WorldDeltaMs > 0.0)
                {
                    PhysicsFrameWorldDeltaMs.Add(WorldDeltaMs);
                }
            }
        }
        PhysicsFrameTimeLastSampleSeconds = bInPhysicsShot ? NowSeconds : 0.0;
        bPhysicsFrameTimeWasSampling = bInPhysicsShot;
    }

    if (bDisableVisualGoalCollisionAfterBeginPlay)
    {
        bDisableVisualGoalCollisionAfterBeginPlay = false;
        for (const TPair<FString, TObjectPtr<AActor>>& Pair : GoalActors)
        {
            if (UPrimitiveComponent* GoalBody = GetPhysicsGoalBody(Pair.Value.Get()))
            {
                GoalBody->SetSimulatePhysics(false);
                GoalBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
        }
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_GOAL_VISUAL_COLLISION_RESET goals=%d"), GoalActors.Num());
    }

    const double SafeDeltaSeconds = FMath::Max(0.0, static_cast<double>(DeltaSeconds));
    UpdateFeedbackTimers(SafeDeltaSeconds);

    if (ActionNoticeRemaining > 0.0)
    {
        ActionNoticeRemaining = FMath::Max(0.0, ActionNoticeRemaining - SafeDeltaSeconds);
        if (ActionNoticeRemaining <= 0.0)
        {
            ActionNotice.Reset();
        }
    }

    if (bPouchReturning && PouchActor)
    {
        PouchReturnRemaining = FMath::Max(0.0, PouchReturnRemaining - SafeDeltaSeconds);
        const double Progress = 1.0 - (PouchReturnRemaining / PouchReturnDuration);
        const double EasedProgress = FMath::InterpEaseOut(0.0, 1.0, Progress, 2.0);
        const FVector LauncherLocation = ToWorld(0.0, NaiwaP2::PouchHeightM);
        PouchActor->SetActorLocation(FMath::Lerp(PouchReturnStart, LauncherLocation, EasedProgress));
        if (PouchReturnRemaining <= 0.0)
        {
            PouchActor->SetActorLocation(LauncherLocation);
            bPouchReturning = false;
        }
    }

    if (bRunPhysicsSandbox)
    {
        if (bRunPhysicsImpactProbe && PlayState == ENaiwaP2PlayState::Aiming && ShotsFired == 0)
        {
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_PHYS_IMPACT_PROBE_START angle=%.2f power=%.3f target=%s"),
                PhysicsImpactProbeAngleDegrees, PhysicsImpactProbePower, *PhysicsImpactProbeTargetBlockId);
            if (!TryFire(PhysicsImpactProbeAngleDegrees, PhysicsImpactProbePower))
            {
                UE_LOG(LogTemp, Error, TEXT("NAIWA_P2_PHYS_IMPACT_PROBE pass=0 reason=shot_rejected"));
                FPlatformMisc::RequestExitWithStatus(false, 1,
                    TEXT("Naiwa P2 physics impact probe shot rejected"));
                return;
            }
        }
        if (PlayState == ENaiwaP2PlayState::Flying)
        {
            UpdatePhysicsSandboxShot(SafeDeltaSeconds);
        }
        UpdatePhysicsStabilityProbe(SafeDeltaSeconds);
        return;
    }

    if (bRunPhysicsFullLevel)
    {
        const bool bAutoTestReadyToFire = bRunPhysicsLevelMultiShotTest
            ? ShotsFired < AmmoRule.TotalShots
            : ShotsFired == 0;
        if (bRunPhysicsLevelAutoTest && PlayState == ENaiwaP2PlayState::Aiming && bAutoTestReadyToFire)
        {
            const double TestAngle = bRunPhysicsLevelMultiShotTest
                ? PhysicsLevelMultiShotAngles[FMath::Clamp(ShotsFired, 0, PhysicsLevelMultiShotAngles.Num() - 1)]
                : PhysicsImpactProbeAngleDegrees;
            const double TestPower = bRunPhysicsLevelMultiShotTest ? 1.0 : PhysicsImpactProbePower;
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_PHYS_LEVEL_TEST_START level=%s shot=%d angle=%.2f power=%.3f sequence=%d"),
                *CurrentLevelId, ShotsFired + 1, TestAngle, TestPower,
                bRunPhysicsLevelMultiShotTest ? 1 : 0);
            if (!TryFire(TestAngle, TestPower))
            {
                UE_LOG(LogTemp, Error, TEXT("NAIWA_P2_PHYS_LEVEL_TEST pass=0 reason=shot_rejected"));
                FPlatformMisc::RequestExitWithStatus(false, 1,
                    TEXT("Naiwa P2 full physics test shot rejected"));
                return;
            }
        }
        if (bRunPhysicsLevelSkillProbe && PlayState == ENaiwaP2PlayState::Flying &&
            !CurrentShot.bSkillTriggered && PhysicsLevelShotElapsed >= 0.12 &&
            (ShotEgg == NaiwaP2::EEggKind::Chick || ShotEgg == NaiwaP2::EEggKind::Mouse))
        {
            ActivateFlightSkill();
        }
        if (PlayState == ENaiwaP2PlayState::Flying || PlayState == ENaiwaP2PlayState::Resolving)
        {
            UpdatePhysicsLevelShot(SafeDeltaSeconds);
        }
        UpdatePhysicsStabilityProbe(SafeDeltaSeconds);
        return;
    }

    if (bRun22PlayTest && !b22PlayTestCompleted)
    {
        Advance22PlayTest();
    }
    if (bRun22RefundPlayTest && !b22RefundPlayTestCompleted)
    {
        Advance22RefundPlayTest();
    }

    if (bRunPlayTest && !bPlayTestCompleted)
    {
        if (PlayState == ENaiwaP2PlayState::Aiming && ShotsFired < AmmoRule.TotalShots)
        {
            TryFire(89.0, 0.05);
        }
        else if (PlayState == ENaiwaP2PlayState::Failed)
        {
            Retry();
            bPlayTestCompleted = PlayState == ENaiwaP2PlayState::Aiming &&
                ShotsFired == 0 && GoalActors.Num() == 4;
            UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_PLAYTEST_%s retry_shots=%d goals=%d"),
                bPlayTestCompleted ? TEXT("PASS") : TEXT("FAIL"), ShotsFired, GoalActors.Num());
            FPlatformMisc::RequestExitWithStatus(false, bPlayTestCompleted ? 0 : 1,
                TEXT("Naiwa P2 fail/retry playtest completed"));
        }
    }

    if (bRunStructurePlayTest && !bStructurePlayTestCompleted)
    {
        if (PlayState == ENaiwaP2PlayState::Aiming && ShotsFired == 0)
        {
            TryFire(42.0, 0.92);
        }
        else if (PlayState == ENaiwaP2PlayState::Aiming && ShotsFired == 1)
        {
            const NaiwaP2::FBox2* UpperWall = CurrentScene.Boxes.FindByPredicate([](const NaiwaP2::FBox2& Box)
            {
                return Box.Id == TEXT("T.c5");
            });
            const NaiwaP2::FBox2* UpperDeck = CurrentScene.Boxes.FindByPredicate([](const NaiwaP2::FBox2& Box)
            {
                return Box.Id == TEXT("T.d3");
            });
            const bool bUpperWallCollapsed = UpperWall && UpperWall->MaxZ - UpperWall->MinZ < 0.35 &&
                !UpperWall->bBreakable && UpperWall->Kills.IsEmpty();
            const bool bUpperStackDropped = UpperDeck && UpperDeck->MaxZ < 3.2;
            const bool bStandardShotOneLedger = GoalActors.Num() == 1;
            UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_STRUCTURE_CHECK support_debris=%d upper_stack_dropped=%d remaining_goals=%d deck_max_z=%.3f"),
                bUpperWallCollapsed, bUpperStackDropped, GoalActors.Num(), UpperDeck ? UpperDeck->MaxZ : -1.0);
            if (!bUpperWallCollapsed || !bUpperStackDropped || !bStandardShotOneLedger)
            {
                bStructurePlayTestCompleted = true;
                FPlatformMisc::RequestExitWithStatus(false, 1,
                    TEXT("Naiwa P2 structure collapse playtest failed at shot 1"));
            }
            else
            {
                TryFire(9.0, 1.0);
            }
        }
        else if (PlayState == ENaiwaP2PlayState::Won)
        {
            const NaiwaP2::FBox2* Pillar = CurrentScene.Boxes.FindByPredicate([](const NaiwaP2::FBox2& Box)
            {
                return Box.Id == TEXT("P.col");
            });
            const bool bPillarDebrisRetained = Pillar && Pillar->MaxZ - Pillar->MinZ < 0.5 &&
                !Pillar->bBreakable && Pillar->Kills.IsEmpty();
            const bool bChainPassed = bPillarDebrisRetained && ShotsFired == 2 && GoalActors.IsEmpty();
            bStructurePlayTestCompleted = true;
            if (!bChainPassed)
            {
                UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_STRUCTURE_CHAIN_FAIL shots=%d goals=%d pillar_debris=%d"),
                    ShotsFired, GoalActors.Num(), bPillarDebrisRetained);
                FPlatformMisc::RequestExitWithStatus(false, 1,
                    TEXT("Naiwa P2 structure collapse playtest failed after shot 2"));
                return;
            }

            Retry();
            const auto FindSceneBox = [this](const FString& Id) -> const NaiwaP2::FBox2*
            {
                return CurrentScene.Boxes.FindByPredicate([&Id](const NaiwaP2::FBox2& Box)
                {
                    return Box.Id == Id;
                });
            };
            const NaiwaP2::FBox2* ResetUpperWall = FindSceneBox(TEXT("T.c5"));
            const NaiwaP2::FBox2* ResetUpperDeck = FindSceneBox(TEXT("T.d3"));
            const NaiwaP2::FBox2* ResetPillar = FindSceneBox(TEXT("P.col"));
            const bool bStructureResetPassed = ResetUpperWall && ResetUpperDeck && ResetPillar &&
                FMath::IsNearlyEqual(ResetUpperWall->MaxZ - ResetUpperWall->MinZ, 1.7, 1.0e-6) &&
                FMath::IsNearlyEqual(ResetUpperDeck->MaxZ, 4.5, 1.0e-6) &&
                FMath::IsNearlyEqual(ResetPillar->MaxZ - ResetPillar->MinZ, 3.6, 1.0e-6) &&
                ResetUpperWall->bBreakable && ResetUpperWall->Kills.Num() == 3 &&
                ResetPillar->bBreakable && ResetPillar->Kills.Num() == 1 &&
                ShotsFired == 0 && GoalActors.Num() == 4 && PlayState == ENaiwaP2PlayState::Aiming &&
                !bStructureCollapseAnimating;
            const bool bPassed = bChainPassed && bStructureResetPassed;
            UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_STRUCTURE_RESET_CHECK pass=%d shots=%d goals=%d upper_wall_height=%.3f upper_deck_max_z=%.3f pillar_height=%.3f"),
                bStructureResetPassed, ShotsFired, GoalActors.Num(),
                ResetUpperWall ? ResetUpperWall->MaxZ - ResetUpperWall->MinZ : -1.0,
                ResetUpperDeck ? ResetUpperDeck->MaxZ : -1.0,
                ResetPillar ? ResetPillar->MaxZ - ResetPillar->MinZ : -1.0);
            UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_STRUCTURE_PLAYTEST_%s shots=2 goals=0 pillar_debris=%d structure_reset=%d"),
                bPassed ? TEXT("PASS") : TEXT("FAIL"), bPillarDebrisRetained, bStructureResetPassed);
            FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1,
                TEXT("Naiwa P2 structure collapse playtest completed"));
        }
        else if (PlayState == ENaiwaP2PlayState::Failed)
        {
            bStructurePlayTestCompleted = true;
            UE_LOG(LogTemp, Error, TEXT("NAIWA_P2_STRUCTURE_PLAYTEST_FAIL shots=%d goals=%d"),
                ShotsFired, GoalActors.Num());
            FPlatformMisc::RequestExitWithStatus(false, 1,
                TEXT("Naiwa P2 structure collapse playtest failed to clear level"));
        }
    }

    if (bRunRefundPlayTest && !bRefundPlayTestCompleted)
    {
        if (PlayState == ENaiwaP2PlayState::Aiming && ShotsFired == 0 && bFreeRetryAvailable)
        {
            if (!TryFire(36.0, 1.0))
            {
                bRefundPlayTestCompleted = true;
                UE_LOG(LogTemp, Error, TEXT("NAIWA_P2_REFUND_PLAYTEST_FAIL could_not_fire=1"));
                FPlatformMisc::RequestExitWithStatus(false, 1,
                    TEXT("Naiwa P2 zero-kill refund playtest could not fire its vector"));
            }
        }
        else if (PlayState == ENaiwaP2PlayState::Aiming && ShotsFired == 0 && !bFreeRetryAvailable)
        {
            const NaiwaP2::FBox2* Deck = CurrentScene.Boxes.FindByPredicate([](const NaiwaP2::FBox2& Box)
            {
                return Box.Id == TEXT("T.d3");
            });
            const TObjectPtr<AActor>* DeckActor = StructureActors.Find(TEXT("T.d3"));
            const bool bSceneRestored = Deck && Deck->bBreakable && Deck->Kills.IsEmpty() &&
                FMath::IsNearlyEqual(Deck->MaxZ, 4.5, 1.0e-6);
            const bool bActorRestored = DeckActor && IsValid(DeckActor->Get()) &&
                FVector::Dist(DeckActor->Get()->GetActorLocation(), ToWorld(15.5, 4.35)) < 0.01 &&
                FVector::Dist(DeckActor->Get()->GetActorScale3D(), FVector(2.2, 1.5, 0.3)) < 0.01;
            const bool bRefundPassed = bSceneRestored && bActorRestored && !ProjectileActor &&
                ShotsFired == 0 && GoalActors.Num() == 4 && !bStructureCollapseAnimating &&
                !bFreeRetryAvailable && PlayState == ENaiwaP2PlayState::Aiming;
            bRefundPlayTestCompleted = true;
            UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_REFUND_CHECK pass=%d scene_restored=%d actor_restored=%d shots=%d goals=%d entitlement=%d"),
                bRefundPassed, bSceneRestored, bActorRestored, ShotsFired, GoalActors.Num(), bFreeRetryAvailable);
            UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_REFUND_PLAYTEST_%s"),
                bRefundPassed ? TEXT("PASS") : TEXT("FAIL"));
            FPlatformMisc::RequestExitWithStatus(false, bRefundPassed ? 0 : 1,
                TEXT("Naiwa P2 zero-kill refund playtest completed"));
        }
        else if (PlayState == ENaiwaP2PlayState::Failed || PlayState == ENaiwaP2PlayState::Won)
        {
            bRefundPlayTestCompleted = true;
            UE_LOG(LogTemp, Error, TEXT("NAIWA_P2_REFUND_PLAYTEST_FAIL state=%s shots=%d goals=%d entitlement=%d"),
                *GetPlayStateLabel(), ShotsFired, GoalActors.Num(), bFreeRetryAvailable);
            FPlatformMisc::RequestExitWithStatus(false, 1,
                TEXT("Naiwa P2 zero-kill refund playtest reached an unexpected terminal state"));
        }
    }

    if (PlayState == ENaiwaP2PlayState::Flying)
    {
        StepAccumulator += SafeDeltaSeconds;
        while (StepAccumulator >= NaiwaP2::FixedStepSeconds && StateIndex < CurrentShot.TotalTicks)
        {
            const int32 NextTick = StateIndex + 1;
            if (bRun22PlayTest && !CurrentShot.bSkillTriggered &&
                ((ShotEgg == NaiwaP2::EEggKind::Mouse && NextTick == 72) ||
                 (ShotEgg == NaiwaP2::EEggKind::Chick && NextTick == 46)))
            {
                ActivateFlightSkill();
            }
            StepAccumulator -= NaiwaP2::FixedStepSeconds;
            ++StateIndex;
            UpdateProjectileActorsAtCurrentTick();
        }

        if (CurrentShot.TotalTicks > 0 && StateIndex >= CurrentShot.TotalTicks)
        {
            ResolveDelay = 0.35;
            SetPhase(ENaiwaP2PlayState::Resolving);
        }
    }
    else if (PlayState == ENaiwaP2PlayState::Resolving)
    {
        if (bStructureCollapseAnimating)
        {
            UpdateStructureCollapse(SafeDeltaSeconds);
            if (!bStructureCollapseAnimating)
            {
                FinishCurrentShot();
            }
        }
        else
        {
            ResolveDelay -= SafeDeltaSeconds;
            if (ResolveDelay <= 0.0)
            {
                FinishCurrentShot();
            }
        }
    }

    UpdatePhysicsStabilityProbe(SafeDeltaSeconds);
}

bool ANaiwaP2GameMode::TryFire(const double AngleDegrees, const double Power, const int32 ClickTick)
{
    if (PlayState != ENaiwaP2PlayState::Aiming || ShotsFired >= AmmoRule.TotalShots)
    {
        return false;
    }
    if (!AmmoRule.bRepeatAllowed && !AvailableEggs.Contains(SelectedEgg))
    {
        ShowActionNotice(TEXT("SELECT AN AVAILABLE EGG"));
        return false;
    }

    AimAngleDegrees = FMath::Clamp(AngleDegrees, 1.0, 89.0);
    AimPower = FMath::Clamp(Power, 0.05, 1.0);
    CurrentShotAngle = AimAngleDegrees;
    CurrentShotPower = AimPower;
    ShotEgg = SelectedEgg;
    CurrentShot = NaiwaP2::FBallistics::Shoot(CurrentScene, AimAngleDegrees, AimPower,
        NaiwaP2::GetEggSpec(ShotEgg), 10000, ClickTick);
    if (!CurrentShot.bCompleted || CurrentShot.States.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("NAIWA_P2_SHOT_REJECT reason=%s"), *CurrentShot.FailureReason);
        return false;
    }
    if (bRunPhysicsFullLevel)
    {
        // Keep the predicted states for the existing HUD/debug display, but never let its hit table
        // decide collision, target kills, or the physical shot result.
        CurrentShot.Hits.Reset();
        CurrentShot.RawKilled.Reset();
    }

    AActor* NewProjectile = nullptr;
    if (bRunPhysicsSandbox || bRunPhysicsFullLevel)
    {
        const NaiwaP2::FShotState Launch = NaiwaP2::FBallistics::MakeLaunchState(
            AimAngleDegrees, AimPower, NaiwaP2::GetEggSpec(ShotEgg));
        NewProjectile = SpawnEggActor(TEXT("P2_PhysicsProjectile"), ToWorld(Launch.X, Launch.Z),
            ShotEgg, 1.0, false, true,
            FVector(Launch.VelocityX * CentimetersPerMeter, 0.0,
                    Launch.VelocityZ * CentimetersPerMeter));
        if (!Cast<ANaiwaP2PhysicsEgg>(NewProjectile))
        {
            UE_LOG(LogTemp, Error, TEXT("NAIWA_P2_PHYS_SANDBOX_SHOT_REJECT reason=physics_egg_spawn_failed"));
            if (IsValid(NewProjectile))
            {
                NewProjectile->Destroy();
            }
            return false;
        }
    }

    ShotStartScene = CurrentScene;
    ShotStartAvailableEggs = AvailableEggs;
    ShotStartSelectedEgg = SelectedEgg;
    ShotStartStructureTransforms.Reset();
    for (const TPair<FString, TObjectPtr<AActor>>& Pair : StructureActors)
    {
        if (IsValid(Pair.Value.Get()))
        {
            ShotStartStructureTransforms.Add(Pair.Key, Pair.Value->GetActorTransform());
        }
    }
    ShotStartGoalTransforms.Reset();
    for (const TPair<FString, TObjectPtr<AActor>>& Pair : GoalActors)
    {
        if (IsValid(Pair.Value.Get()))
        {
            ShotStartGoalTransforms.Add(Pair.Key, Pair.Value->GetActorTransform());
        }
    }
    bHasShotStartSnapshot = true;

    ++ShotsFired;
    if (!AmmoRule.bRepeatAllowed)
    {
        AvailableEggs.RemoveSingle(ShotEgg);
        SelectNextAvailableEgg();
    }
    StateIndex = 0;
    StepAccumulator = 0.0;
    CurrentShot.States[0].X = 0.0;
    CurrentShot.States[0].Z = NaiwaP2::PouchHeightM;

    const FVector Start = ToWorld(CurrentShot.States[0].X, CurrentShot.States[0].Z);
    PhysicsLevelActiveProjectiles.Reset();
    PhysicsLevelShotProjectiles.Reset();
    PhysicsLevelProjectileSlowElapsedByEgg.Reset();
    SplitProjectileActors.Reset();
    ProjectileActor = (bRunPhysicsSandbox || bRunPhysicsFullLevel)
        ? NewProjectile
        : SpawnEggActor(TEXT("P2_Projectile"), Start, ShotEgg);
    if (ProjectileActor)
    {
        AttemptActors.Add(ProjectileActor);
        if (bRunPhysicsFullLevel)
        {
            if (ANaiwaP2PhysicsEgg* PhysicsEgg = Cast<ANaiwaP2PhysicsEgg>(ProjectileActor))
            {
                PhysicsLevelActiveProjectiles.Add(PhysicsEgg);
                PhysicsLevelShotProjectiles.Add(PhysicsEgg);
            }
        }
    }
    if ((bRunPhysicsSandbox || bRunPhysicsFullLevel) && PouchActor)
    {
        PouchActor->SetActorHiddenInGame(true);
    }
    if (bRunPhysicsSandbox)
    {
        PhysicsSandboxShotElapsed = 0.0;
        PhysicsSandboxSlowElapsed = 0.0;
    }
    if (bRunPhysicsFullLevel)
    {
        PhysicsLevelShotElapsed = 0.0;
        PhysicsLevelProjectileSlowElapsed = 0.0;
        PhysicsLevelSceneQuietElapsed = 0.0;
        PhysicsLevelMaxSceneSpeedCmPerS = 0.0;
        PhysicsLevelShotCollisionCount = 0;
        PhysicsLevelResolvedProjectileImpactCount = 0;
        bPhysicsLevelTeaBlastApplied = false;
        bPhysicsLevelChickImpulseApplied = false;
        bPhysicsLevelSkillProbeEffectObserved = false;
        PhysicsLevelMouseSplitSpawnedCount = 0;
        PhysicsLevelChickDeltaVelocityZCmPerS = 0.0;
        PhysicsLevelSkillProbeRadiusCm = 0.0;
        PhysicsLevelSkillProbeMassKg = 0.0;
        PhysicsLevelSkillProbeVisualScale = GetEggVisualScale(ShotEgg);
        if (const ANaiwaP2PhysicsEgg* PhysicsEgg = Cast<ANaiwaP2PhysicsEgg>(ProjectileActor))
        {
            if (const USphereComponent* Collider = PhysicsEgg->GetCollisionSphere())
            {
                PhysicsLevelSkillProbeRadiusCm = Collider->GetUnscaledSphereRadius();
                PhysicsLevelSkillProbeMassKg = Collider->GetMass();
            }
        }
        if (bRunPhysicsLevelSkillProbe && ShotEgg == NaiwaP2::EEggKind::Soy)
        {
            const bool bSoyShapePassed = FMath::IsNearlyEqual(
                PhysicsLevelSkillProbeRadiusCm,
                NaiwaP2::BaseProjectileRadiusM * NaiwaP2::GetEggSpec(ShotEgg).RadiusMultiplier * 100.0,
                0.1) &&
                FMath::IsNearlyEqual(PhysicsLevelSkillProbeMassKg,
                    NaiwaP2::Physics::GetEggMassKg(NaiwaP2::GetEggSpec(ShotEgg)), 0.1) &&
                PhysicsLevelSkillProbeVisualScale > 1.5;
            bPhysicsLevelSkillProbeEffectObserved = bSoyShapePassed;
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_PHYS_SKILL_SOY radius_cm=%.2f mass_kg=%.2f visual_scale=%.4f pass=%d"),
                PhysicsLevelSkillProbeRadiusCm, PhysicsLevelSkillProbeMassKg,
                PhysicsLevelSkillProbeVisualScale, bSoyShapePassed ? 1 : 0);
        }
        else if (bRunPhysicsLevelSkillProbe && ShotEgg == NaiwaP2::EEggKind::Base)
        {
            const bool bBaseShapePassed = FMath::IsNearlyEqual(
                PhysicsLevelSkillProbeRadiusCm,
                NaiwaP2::BaseProjectileRadiusM * CentimetersPerMeter, 0.1) &&
                FMath::IsNearlyEqual(PhysicsLevelSkillProbeMassKg,
                    NaiwaP2::Physics::BaseEggMassKg, 0.1);
            bPhysicsLevelSkillProbeEffectObserved = bBaseShapePassed;
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_PHYS_SKILL_BASE radius_cm=%.2f mass_kg=%.2f pass=%d"),
                PhysicsLevelSkillProbeRadiusCm, PhysicsLevelSkillProbeMassKg,
                bBaseShapePassed ? 1 : 0);
        }
    }
    SetPhase(ENaiwaP2PlayState::Flying);

    // 开火时把所有结构块唤醒。
    //
    // 为什么要这么粗暴：休眠的刚体不会因为"脚下的支撑被别人抽走"而自己醒 ——
    // 抽走支撑的是另一块刚体，睡眠岛之间的唤醒不是可靠的因果关系。结果就是
    // 下层被打飞、上层靠休眠冻在半空（验收第 4 条明确不允许的"悬空"）。
    // 单关最多 26 块，全唤醒的代价可以忽略。
    WakeAllStructureBlocks();
    ShowActionNotice(FString::Printf(TEXT("FIRED  -  SHOT %d / %d"), ShotsFired, AmmoRule.TotalShots), 1.8);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_SHOT_FIRE shot=%d angle=%.2f power=%.3f states=%d chaos=%d"),
        ShotsFired, AimAngleDegrees, AimPower, CurrentShot.States.Num(),
        (bRunPhysicsSandbox || bRunPhysicsFullLevel) ? 1 : 0);
    return true;
}

bool ANaiwaP2GameMode::SelectEggByIndex(const int32 Index)
{
    if (PlayState != ENaiwaP2PlayState::Aiming || !AmmoRule.EggList.IsValidIndex(Index))
    {
        return false;
    }
    const NaiwaP2::EEggKind Candidate = AmmoRule.EggList[Index];
    if (!AmmoRule.bRepeatAllowed && !AvailableEggs.Contains(Candidate))
    {
        ShowActionNotice(TEXT("THIS EGG WAS ALREADY USED"), 1.1);
        return false;
    }
    SelectedEgg = Candidate;
    ApplyEggActorAppearance(PouchActor, SelectedEgg);
    ShowActionNotice(FString::Printf(TEXT("SELECTED  %s"), NaiwaP2::GetEggKindLabel(SelectedEgg)), 0.9);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_EGG_SELECT level=%s egg=%s tokens=%d"),
        *CurrentLevelId, NaiwaP2::GetEggKindLabel(SelectedEgg), AvailableEggs.Num());
    return true;
}

bool ANaiwaP2GameMode::ActivateFlightSkill()
{
    if (PlayState != ENaiwaP2PlayState::Flying || CurrentShot.bSkillTriggered ||
        (ShotEgg != NaiwaP2::EEggKind::Chick && ShotEgg != NaiwaP2::EEggKind::Mouse))
    {
        return false;
    }

    if (bRunPhysicsFullLevel)
    {
        ANaiwaP2PhysicsEgg* PhysicsEgg = Cast<ANaiwaP2PhysicsEgg>(ProjectileActor);
        USphereComponent* Collider = PhysicsEgg ? PhysicsEgg->GetCollisionSphere() : nullptr;
        if (!PhysicsEgg || !Collider || !Collider->IsSimulatingPhysics())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("NAIWA_P2_PHYS_SKILL_IGNORED egg=%s reason=physics_projectile_unavailable"),
                NaiwaP2::GetEggKindLabel(ShotEgg));
            return false;
        }

        const FVector Position = Collider->GetComponentLocation();
        const FVector VelocityBefore = Collider->GetPhysicsLinearVelocity();
        const int32 EventTick = FMath::Max(1,
            FMath::RoundToInt(PhysicsLevelShotElapsed / NaiwaP2::FixedStepSeconds));
        CurrentShot.bSkillTriggered = true;
        CurrentShot.SkillTick = EventTick;

        if (ShotEgg == NaiwaP2::EEggKind::Chick)
        {
            const double DeltaVelocityZ = NaiwaP2::ChickVerticalImpulseMPerS * CentimetersPerMeter;
            FVector VelocityAfter = VelocityBefore + FVector(0.0, 0.0, DeltaVelocityZ);
            VelocityAfter.Y = 0.0;
            Collider->SetPhysicsLinearVelocity(VelocityAfter, false);
            ApplyEggActorAppearance(PhysicsEgg, ShotEgg, 1.0, true);
            bPhysicsLevelChickImpulseApplied = true;
            PhysicsLevelChickDeltaVelocityZCmPerS = VelocityAfter.Z - VelocityBefore.Z;
            if (bRunPhysicsLevelSkillProbe && PhysicsLevelSkillProbeEgg == ShotEgg)
            {
                bPhysicsLevelSkillProbeEffectObserved = FMath::IsNearlyEqual(
                    PhysicsLevelChickDeltaVelocityZCmPerS, DeltaVelocityZ, 1.0);
            }
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_PHYS_SKILL_TRIGGER egg=CHICK type=vertical_impulse tick=%d elapsed_s=%.3f "
                     "position_cm=(%.1f,%.1f,%.1f) velocity_before_cm_s=(%.1f,%.1f,%.1f) "
                     "velocity_after_cm_s=(%.1f,%.1f,%.1f) delta_vz_cm_s=%.1f"),
                EventTick, PhysicsLevelShotElapsed, Position.X, Position.Y, Position.Z,
                VelocityBefore.X, VelocityBefore.Y, VelocityBefore.Z,
                VelocityAfter.X, VelocityAfter.Y, VelocityAfter.Z,
                PhysicsLevelChickDeltaVelocityZCmPerS);
        }
        else
        {
            FVector Direction(VelocityBefore.X, 0.0, VelocityBefore.Z);
            Direction = Direction.GetSafeNormal();
            if (Direction.IsNearlyZero())
            {
                Direction = FVector::ForwardVector;
            }
            Direction.Y = 0.0;
            Direction.Normalize();
            const FVector Perpendicular(-Direction.Z, 0.0, Direction.X);
            const double SplitSpeed = VelocityBefore.Size() * NaiwaP2::MouseSplitSpeedMultiplier;
            const double BaseAngle = FMath::Atan2(VelocityBefore.Z, VelocityBefore.X);
            constexpr double DegreesToRadians = 0.0174532925199432957692;
            const double ChildRadiusCm = NaiwaP2::BaseProjectileRadiusM *
                NaiwaP2::GetEggSpec(ShotEgg).RadiusMultiplier *
                NaiwaP2::MouseSplitRadiusMultiplier * CentimetersPerMeter;

            Collider->SetSimulatePhysics(false);
            Collider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            TArray<TObjectPtr<AActor>> NewSplitActors;
            for (int32 Index = 0; Index < 3; ++Index)
            {
                const double OffsetDegrees = (Index - 1) * NaiwaP2::MouseSplitHalfAngleDegrees;
                const double ChildAngle = BaseAngle + OffsetDegrees * DegreesToRadians;
                const FVector ChildVelocity(SplitSpeed * FMath::Cos(ChildAngle), 0.0,
                    SplitSpeed * FMath::Sin(ChildAngle));
                const double LateralOffset = (Index - 1) * ChildRadiusCm * 2.05;
                const FVector ChildLocation = Position + Perpendicular * LateralOffset;
                AActor* SplitActor = SpawnEggActor(
                    FString::Printf(TEXT("P2_MouseSplit_%d"), Index + 1), ChildLocation,
                    ShotEgg, NaiwaP2::MouseSplitRadiusMultiplier, true, true, ChildVelocity);
                if (!Cast<ANaiwaP2PhysicsEgg>(SplitActor))
                {
                    if (IsValid(SplitActor))
                    {
                        SplitActor->Destroy();
                    }
                    for (AActor* SpawnedActor : NewSplitActors)
                    {
                        if (IsValid(SpawnedActor))
                        {
                            SpawnedActor->Destroy();
                        }
                    }
                    Collider->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                    Collider->SetSimulatePhysics(true);
                    Collider->SetPhysicsLinearVelocity(VelocityBefore, false);
                    CurrentShot.bSkillTriggered = false;
                    CurrentShot.SkillTick = INDEX_NONE;
                    UE_LOG(LogTemp, Error,
                        TEXT("NAIWA_P2_PHYS_SKILL_FAIL egg=MOUSE reason=split_spawn_failed"));
                    return false;
                }
                NewSplitActors.Add(SplitActor);
            }

            AttemptActors.Remove(ProjectileActor);
            PhysicsLevelResolvedProjectileImpactCount += PhysicsEgg->GetCollisionCount();
            PhysicsLevelActiveProjectiles.Remove(PhysicsEgg);
            PhysicsLevelShotProjectiles.Remove(PhysicsEgg);
            PhysicsLevelProjectileSlowElapsedByEgg.Remove(PhysicsEgg);
            PhysicsEgg->Destroy();
            SplitProjectileActors = NewSplitActors;
            for (AActor* SplitActor : NewSplitActors)
            {
                AttemptActors.Add(SplitActor);
                if (ANaiwaP2PhysicsEgg* SplitEgg = Cast<ANaiwaP2PhysicsEgg>(SplitActor))
                {
                    PhysicsLevelActiveProjectiles.Add(SplitEgg);
                    PhysicsLevelShotProjectiles.Add(SplitEgg);
                }
            }
            ProjectileActor = NewSplitActors.IsValidIndex(1) ? NewSplitActors[1].Get() : nullptr;
            PhysicsLevelMouseSplitSpawnedCount = NewSplitActors.Num();

            if (const ANaiwaP2PhysicsEgg* FirstSplitEgg =
                Cast<ANaiwaP2PhysicsEgg>(NewSplitActors[0].Get()))
            {
                const USphereComponent* SplitCollider = FirstSplitEgg->GetCollisionSphere();
                PhysicsLevelSkillProbeRadiusCm = SplitCollider->GetUnscaledSphereRadius();
                PhysicsLevelSkillProbeMassKg = SplitCollider->GetMass();
            }
            if (bRunPhysicsLevelSkillProbe && PhysicsLevelSkillProbeEgg == ShotEgg)
            {
                const double ExpectedRadiusCm = NaiwaP2::BaseProjectileRadiusM *
                    NaiwaP2::GetEggSpec(ShotEgg).RadiusMultiplier *
                    NaiwaP2::MouseSplitRadiusMultiplier * CentimetersPerMeter;
                const double ExpectedMassKg = NaiwaP2::Physics::BaseEggMassKg *
                    FMath::Pow(NaiwaP2::MouseSplitRadiusMultiplier, 3.0);
                bPhysicsLevelSkillProbeEffectObserved = NewSplitActors.Num() == 3 &&
                    FMath::IsNearlyEqual(PhysicsLevelSkillProbeRadiusCm, ExpectedRadiusCm, 0.1) &&
                    FMath::IsNearlyEqual(PhysicsLevelSkillProbeMassKg, ExpectedMassKg, 0.1);
            }
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_PHYS_SKILL_TRIGGER egg=MOUSE type=split tick=%d elapsed_s=%.3f "
                     "children=%d speed_before_cm_s=%.1f speed_child_cm_s=%.1f radius_cm=%.2f "
                     "mass_kg=%.2f spacing_cm=%.2f"),
                EventTick, PhysicsLevelShotElapsed, PhysicsLevelMouseSplitSpawnedCount,
                VelocityBefore.Size(), SplitSpeed, PhysicsLevelSkillProbeRadiusCm,
                PhysicsLevelSkillProbeMassKg, ChildRadiusCm * 2.05);
        }

        ShowActionNotice(ShotEgg == NaiwaP2::EEggKind::Chick
            ? TEXT("CHICK FLAP!  +7.5 M/S") : TEXT("MOUSE SPLIT!  THREE PHYSICS EGGS"), 1.4);
        return true;
    }

    const int32 EventTick = StateIndex + 1;
    NaiwaP2::FShotResult SkillShot = NaiwaP2::FBallistics::Shoot(CurrentScene,
        CurrentShotAngle, CurrentShotPower, NaiwaP2::GetEggSpec(ShotEgg), 10000, EventTick);
    if (!SkillShot.bSkillTriggered || !SkillShot.bCompleted)
    {
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_SKILL_IGNORED egg=%s tick=%d reason=collision_or_landing"),
            NaiwaP2::GetEggKindLabel(ShotEgg), EventTick);
        return false;
    }

    CurrentShot = MoveTemp(SkillShot);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_SKILL_TRIGGER egg=%s tick=%d split_paths=%d"),
        NaiwaP2::GetEggKindLabel(ShotEgg), CurrentShot.SkillTick, CurrentShot.SplitPaths.Num());
    if (ShotEgg == NaiwaP2::EEggKind::Mouse)
    {
        if (ProjectileActor)
        {
            ProjectileActor->SetActorHiddenInGame(true);
        }
        SplitProjectileActors.Reset();
        for (int32 Index = 0; Index < CurrentShot.SplitPaths.Num(); ++Index)
        {
            AActor* SplitActor = SpawnEggActor(
                FString::Printf(TEXT("P2_MouseSplit_%d"), Index + 1),
                ToWorld(0.0, NaiwaP2::PouchHeightM), ShotEgg,
                NaiwaP2::MouseSplitRadiusMultiplier, true);
            SplitProjectileActors.Add(SplitActor);
            if (SplitActor)
            {
                AttemptActors.Add(SplitActor);
                SplitActor->SetActorHiddenInGame(true);
            }
        }
    }
    else
    {
        ApplyEggActorAppearance(ProjectileActor, ShotEgg, 1.0, true);
    }
    ShowActionNotice(ShotEgg == NaiwaP2::EEggKind::Chick
        ? TEXT("CHICK FLAP!  +7.5 M/S") : TEXT("MOUSE SPLIT!  THREE EGGS"), 1.4);
    return true;
}

void ANaiwaP2GameMode::SetAimPreview(const double AngleDegrees, const double Power)
{
    if (PlayState != ENaiwaP2PlayState::Aiming || Power < 0.05)
    {
        AimAngleDegrees = FMath::Clamp(AngleDegrees, 1.0, 89.0);
        AimPower = 0.0;
        bHasAimPreview = false;
        return;
    }
    AimAngleDegrees = FMath::Clamp(AngleDegrees, 1.0, 89.0);
    AimPower = FMath::Clamp(Power, 0.05, 1.0);
    AimPreview = NaiwaP2::FBallistics::Shoot(CurrentScene, AimAngleDegrees, AimPower,
        NaiwaP2::GetEggSpec(SelectedEgg));
    bHasAimPreview = AimPreview.bCompleted && AimPreview.States.Num() > 1;
}

void ANaiwaP2GameMode::Retry()
{
    ++LevelResetCount;
    const bool bShowFifthResetCue = LevelResetCount % 5 == 0;
    ActiveFeedbackImage = ENaiwaP2FeedbackImage::None;
    FeedbackImageRemaining = 0.0;

    ResetArcherDraw();
    for (AActor* Actor : AttemptActors)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
    AttemptActors.Reset();
    SplitProjectileActors.Reset();
    PhysicsLevelActiveProjectiles.Reset();
    PhysicsLevelShotProjectiles.Reset();
    PhysicsLevelProjectileSlowElapsedByEgg.Reset();
    GoalActors.Reset();
    ProjectileActor = nullptr;
    PouchActor = nullptr;
    bPouchReturning = false;
    PouchReturnRemaining = 0.0;
    CurrentScene = NaiwaP2::FSceneLibrary::Make(CurrentLevelId, CurrentStage);
    AvailableEggs = AmmoRule.EggList;
    SelectedEgg = AvailableEggs.IsEmpty() ? NaiwaP2::EEggKind::Base : AvailableEggs[0];
    StructureActors.Reset();
    CollapseActorIds.Reset();
    CollapseStartLocations.Reset();
    CollapseTargetLocations.Reset();
    CollapseStartScales.Reset();
    CollapseTargetScales.Reset();
    CollapseStartBoxes.Reset();
    CollapseTargetBoxes.Reset();
    StructureCollapseElapsed = 0.0;
    bStructureCollapseAnimating = false;
    bShotStructureEffectsApplied = false;
    bFreeRetryAvailable = AmmoRule.bHasFreeRetry;
    bHasShotStartSnapshot = false;
    ShotStartScene = NaiwaP2::FScene();
    ShotStartAvailableEggs.Reset();
    ShotStartStructureTransforms.Reset();
    ShotStartGoalTransforms.Reset();
    ShotsFired = 0;
    CurrentShot = NaiwaP2::FShotResult();
    AimPreview = NaiwaP2::FShotResult();
    bHasAimPreview = false;
    AimAngleDegrees = 45.0;
    AimPower = 0.5;
    PhysicsLevelShotElapsed = 0.0;
    PhysicsLevelProjectileSlowElapsed = 0.0;
    PhysicsLevelSceneQuietElapsed = 0.0;
    PhysicsLevelMaxSceneSpeedCmPerS = 0.0;
    PhysicsLevelShotCollisionCount = 0;
    SpawnAttemptScene();
    ResetPouchLocation();
    SetPhase(ENaiwaP2PlayState::Aiming);
    ShowActionNotice(FString::Printf(TEXT("LEVEL RESET  -  %d EGGS / %d TARGETS"),
        AmmoRule.TotalShots, GoalActors.Num()), 2.0);
    if (bShowFifthResetCue)
    {
        ShowFeedbackCue(ENaiwaP2FeedbackImage::FifthReset, CollectionSound);
    }
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_RETRY_RESET level=%s shots=0 goals=%d tokens=%d"),
        *CurrentLevelId, GoalActors.Num(), AvailableEggs.Num());
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_RESET_COUNT level=%s count=%d"),
        *CurrentLevelId, LevelResetCount);
}

void ANaiwaP2GameMode::SetPouchDragWorldLocation(const FVector& Location)
{
    bPouchReturning = false;
    PouchReturnRemaining = 0.0;
    if (PouchActor)
    {
        PouchActor->SetActorLocation(FVector(Location.X, 0.0, Location.Z));
    }
}

void ANaiwaP2GameMode::ReleasePouchToLauncher()
{
    if (!PouchActor)
    {
        return;
    }
    PouchReturnStart = PouchActor->GetActorLocation();
    PouchReturnRemaining = PouchReturnDuration;
    bPouchReturning = true;
}

void ANaiwaP2GameMode::ResetPouchLocation()
{
    bPouchReturning = false;
    PouchReturnRemaining = 0.0;
    if (PouchActor)
    {
        PouchActor->SetActorLocation(ToWorld(0.0, NaiwaP2::PouchHeightM));
    }
}

void ANaiwaP2GameMode::BeginArcherDraw()
{
    if (ArcherActor) ArcherActor->BeginDraw();
}

void ANaiwaP2GameMode::SetArcherDrawProgress(const float Progress)
{
    if (ArcherActor) ArcherActor->SetDrawProgress(Progress);
}

void ANaiwaP2GameMode::ReleaseArcherDraw()
{
    if (ArcherActor) ArcherActor->ReleaseDraw();
}

void ANaiwaP2GameMode::CancelArcherDraw()
{
    if (ArcherActor) ArcherActor->CancelDraw();
}

void ANaiwaP2GameMode::ResetArcherDraw()
{
    if (ArcherActor) ArcherActor->ResetDrawPose();
}

void ANaiwaP2GameMode::ShowActionNotice(const FString& Text, const double Duration)
{
    ActionNotice = Text;
    ActionNoticeRemaining = FMath::Max(0.0, Duration);
}

FString ANaiwaP2GameMode::GetPlayStateLabel() const
{
    switch (PlayState)
    {
    case ENaiwaP2PlayState::Aiming: return TEXT("AIMING");
    case ENaiwaP2PlayState::Flying: return TEXT("FLYING");
    case ENaiwaP2PlayState::Resolving: return TEXT("RESOLVING");
    case ENaiwaP2PlayState::Failed: return TEXT("FAILED");
    case ENaiwaP2PlayState::Won: return TEXT("CLEARED");
    default: return TEXT("UNKNOWN");
    }
}

void ANaiwaP2GameMode::InitializeLevelRules()
{
    FString RequestedLevel;
    if (FParse::Value(FCommandLine::Get(), TEXT("NaiwaP2Level="), RequestedLevel))
    {
        CurrentLevelId = RequestedLevel;
    }
    else if (GetWorld())
    {
        const FString MapName = GetWorld()->GetMapName();
        if (MapName.Contains(TEXT("L_NaiwaP2_22")))
        {
            CurrentLevelId = TEXT("2-2");
        }
        else if (MapName.Contains(TEXT("L_NaiwaP2_23")))
        {
            CurrentLevelId = TEXT("2-3");
        }
        else
        {
            CurrentLevelId = TEXT("2-1");
        }
    }

    if (CurrentLevelId != TEXT("2-1") && CurrentLevelId != TEXT("2-2") &&
        CurrentLevelId != TEXT("2-3"))
    {
        UE_LOG(LogTemp, Warning, TEXT("NAIWA_P2_LEVEL_INVALID value=%s fallback=2-1"), *CurrentLevelId);
        CurrentLevelId = TEXT("2-1");
    }

    FString RequestedStage;
    if (!FParse::Value(FCommandLine::Get(), TEXT("NaiwaP2Stage="), RequestedStage) && GetWorld())
    {
        const FString Url = GetWorld()->URL.ToString();
        FParse::Value(*Url, TEXT("NaiwaP2Stage="), RequestedStage);
    }
    CurrentStage = CurrentLevelId == TEXT("2-3") && FCString::Atoi(*RequestedStage) == 2 ? 2 : 1;

    CurrentScene = NaiwaP2::FSceneLibrary::Make(CurrentLevelId, CurrentStage);
    AmmoRule = NaiwaP2::FSceneLibrary::GetAmmoRule(CurrentLevelId);
    AvailableEggs = AmmoRule.EggList;
    SelectedEgg = AvailableEggs.IsEmpty() ? NaiwaP2::EEggKind::Base : AvailableEggs[0];
    bFreeRetryAvailable = AmmoRule.bHasFreeRetry;
}

void ANaiwaP2GameMode::PlayLevelMusic()
{
    USoundBase* Music = CurrentLevelId == TEXT("2-2") ? Music22.Get() :
        (CurrentLevelId == TEXT("2-3") ? Music23.Get() : Music21.Get());
    if (!Music)
    {
        UE_LOG(LogTemp, Warning, TEXT("NAIWA_P2_AUDIO_MUSIC_MISSING level=%s"), *CurrentLevelId);
        return;
    }

    if (IsValid(LevelMusicComponent))
    {
        LevelMusicComponent->Stop();
    }
    LevelMusicComponent = UGameplayStatics::SpawnSound2D(
        this, Music, 1.0f, 1.0f, 0.0f, nullptr, false, false);
    ApplyMusicDucking();
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_AUDIO_MUSIC level=%s asset=%s loop=1 started=%d"),
        *CurrentLevelId, *GetNameSafe(Music), IsValid(LevelMusicComponent) ? 1 : 0);
}

void ANaiwaP2GameMode::PlayGoalHitSound()
{
    if (GoalHitSound)
    {
        // Spawn a separate voice for every target so simultaneous hits stack.
        PlayDuckedSound(GoalHitSound);
    }
}

void ANaiwaP2GameMode::PlayMissSound()
{
    if (MissSound)
    {
        PlayDuckedSound(MissSound);
    }
}

void ANaiwaP2GameMode::PlayDuckedSound(USoundBase* Sound)
{
    if (!Sound)
    {
        return;
    }

    UAudioComponent* Component = UGameplayStatics::SpawnSound2D(
        this, Sound, 1.0f, 1.0f, 0.0f, nullptr, false, true);
    if (!IsValid(Component))
    {
        UE_LOG(LogTemp, Warning, TEXT("NAIWA_P2_AUDIO_SFX_FAILED asset=%s"), *GetNameSafe(Sound));
        return;
    }

    Component->OnAudioFinished.AddDynamic(this, &ANaiwaP2GameMode::HandleDuckedSoundFinished);
    ++ActiveDuckedSoundCount;
    ApplyMusicDucking();
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_AUDIO_SFX asset=%s active=%d bgm_ducked=1"),
        *GetNameSafe(Sound), ActiveDuckedSoundCount);
}

void ANaiwaP2GameMode::ShowFeedbackCue(const ENaiwaP2FeedbackImage Image, USoundBase* Sound)
{
    ActiveFeedbackImage = Image;
    FeedbackCueAgeSeconds = 0.0;
    ++FeedbackCueSerial;
    if (Image == ENaiwaP2FeedbackImage::OutOfEggs)
    {
        // Keep the final message visible while the failed state is on screen.
        FeedbackImageRemaining = 0.0;
    }
    else
    {
        const double SoundDuration = Sound ? FMath::Max(0.0, static_cast<double>(Sound->GetDuration())) : 0.0;
        FeedbackImageRemaining = FMath::Max(3.0, SoundDuration + 1.25);
    }
    PlayDuckedSound(Sound);
}

void ANaiwaP2GameMode::UpdateFeedbackTimers(const double DeltaSeconds)
{
    FeedbackCueAgeSeconds += DeltaSeconds;
    if (PlayState != ENaiwaP2PlayState::Won && PlayState != ENaiwaP2PlayState::Failed)
    {
        LevelElapsedSeconds += DeltaSeconds;
        const int32 CompletedMinutes = FMath::FloorToInt(LevelElapsedSeconds / 60.0);
        while (LastMinuteCueNumber < CompletedMinutes)
        {
            ++LastMinuteCueNumber;
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_FEEDBACK_TRIGGER level=%s event=one_minute minute=%d elapsed=%.2f"),
                *CurrentLevelId, LastMinuteCueNumber, LevelElapsedSeconds);
            ShowFeedbackCue(ENaiwaP2FeedbackImage::OneMinute, CollectionSound);
        }
    }

    if (ActiveFeedbackImage != ENaiwaP2FeedbackImage::None &&
        !(ActiveFeedbackImage == ENaiwaP2FeedbackImage::OutOfEggs &&
          PlayState == ENaiwaP2PlayState::Failed))
    {
        FeedbackImageRemaining = FMath::Max(0.0, FeedbackImageRemaining - DeltaSeconds);
        if (FeedbackImageRemaining <= 0.0)
        {
            ActiveFeedbackImage = ENaiwaP2FeedbackImage::None;
        }
    }
}

bool ANaiwaP2GameMode::ShouldShowFeedbackImage() const
{
    if (ActiveFeedbackImage == ENaiwaP2FeedbackImage::None)
    {
        return false;
    }
    return (ActiveFeedbackImage == ENaiwaP2FeedbackImage::OutOfEggs &&
            PlayState == ENaiwaP2PlayState::Failed) || FeedbackImageRemaining > 0.0;
}

void ANaiwaP2GameMode::ApplyMusicDucking()
{
    if (IsValid(LevelMusicComponent))
    {
        LevelMusicComponent->SetVolumeMultiplier(ActiveDuckedSoundCount <= 0 ? 1.0f : 0.25f);
    }
}

void ANaiwaP2GameMode::HandleDuckedSoundFinished()
{
    ActiveDuckedSoundCount = FMath::Max(0, ActiveDuckedSoundCount - 1);
    ApplyMusicDucking();
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_AUDIO_DUCK_RESTORE active=%d bgm_ducked=%d"),
        ActiveDuckedSoundCount, ActiveDuckedSoundCount > 0 ? 1 : 0);
}

void ANaiwaP2GameMode::ScheduleCampaignAdvance()
{
    // 2-3 is always a two-stage route and returns to level select on its final clear,
    // including when the map was launched outside the linked campaign flow.
    if ((!bCampaignRun && CurrentLevelId != TEXT("2-3")) || !GetWorld())
    {
        return;
    }

    GetWorldTimerManager().SetTimer(CampaignAdvanceTimer, this,
        &ANaiwaP2GameMode::AdvanceCampaign, 1.5f, false);
}

void ANaiwaP2GameMode::AdvanceCampaign()
{
    FString MapPath;
    FString Options;
    if (CurrentLevelId == TEXT("2-1"))
    {
        MapPath = TEXT("/Game/Naiwa/P2/Maps/L_NaiwaP2_22");
    }
    else if (CurrentLevelId == TEXT("2-2"))
    {
        MapPath = TEXT("/Game/Naiwa/P2/Maps/L_NaiwaP2_23");
        Options = TEXT("NaiwaP2Stage=1");
    }
    else if (CurrentLevelId == TEXT("2-3") && CurrentStage == 1)
    {
        MapPath = TEXT("/Game/Naiwa/P2/Maps/L_NaiwaP2_23");
        Options = TEXT("NaiwaP2Stage=2");
    }
    else if (CurrentLevelId == TEXT("2-3") && CurrentStage == 2)
    {
        MapPath = TEXT("/Game/Naiwa/UI/L_NaiwaMainMenu");
        Options = TEXT("NaiwaMenuPage=levels");
    }
    else
    {
        return;
    }

    if (bCampaignRun)
    {
        if (!Options.IsEmpty()) Options += TEXT("?");
        Options += TEXT("NaiwaLinkRun=1");
    }
    UE_LOG(LogTemp, Display, TEXT("NAIWA_CAMPAIGN transition=%s stage=%d map=%s"),
        *CurrentLevelId, CurrentStage, *MapPath);
    UGameplayStatics::OpenLevel(this, FName(*MapPath), true, Options);
}

void ANaiwaP2GameMode::SelectNextAvailableEgg()
{
    if (AmmoRule.bRepeatAllowed)
    {
        SelectedEgg = AmmoRule.EggList.IsEmpty() ? NaiwaP2::EEggKind::Base : AmmoRule.EggList[0];
        ApplyEggActorAppearance(PouchActor, SelectedEgg);
        return;
    }
    if (AvailableEggs.Contains(SelectedEgg))
    {
        ApplyEggActorAppearance(PouchActor, SelectedEgg);
        return;
    }
    SelectedEgg = AvailableEggs.IsEmpty() ? NaiwaP2::EEggKind::Base : AvailableEggs[0];
    ApplyEggActorAppearance(PouchActor, SelectedEgg);
}

void ANaiwaP2GameMode::UpdateProjectileActorsAtCurrentTick()
{
    const bool bBaseProjectileActive = CurrentShot.SplitPaths.IsEmpty() ||
        StateIndex < CurrentShot.SkillTick;
    if (ProjectileActor)
    {
        if (bBaseProjectileActive && CurrentShot.States.IsValidIndex(StateIndex))
        {
            const NaiwaP2::FTrajectoryState& State = CurrentShot.States[StateIndex];
            ProjectileActor->SetActorHiddenInGame(false);
            ProjectileActor->SetActorLocation(ToWorld(State.X, State.Z));
        }
        else
        {
            ProjectileActor->SetActorHiddenInGame(true);
        }
    }

    if (ShotEgg == NaiwaP2::EEggKind::Tea && ProjectileActor &&
        CurrentShot.Hits.ContainsByPredicate([this](const NaiwaP2::FShotHit& Hit)
        {
            return Hit.Tick == StateIndex;
        }))
    {
        ApplyEggActorAppearance(ProjectileActor, ShotEgg, 1.0, true);
    }

    for (int32 Index = 0; Index < SplitProjectileActors.Num(); ++Index)
    {
        AActor* Actor = SplitProjectileActors[Index].Get();
        if (!IsValid(Actor) || !CurrentShot.SplitPaths.IsValidIndex(Index))
        {
            continue;
        }
        const NaiwaP2::FProjectilePath& Path = CurrentShot.SplitPaths[Index];
        const int32 LocalStateIndex = StateIndex - Path.StartTick;
        if (LocalStateIndex >= 0 && !Path.States.IsEmpty())
        {
            // Child paths stop independently. Keep an earlier child at its impact/landing
            // position while the remaining split eggs finish their trajectories.
            const int32 VisibleStateIndex = FMath::Min(LocalStateIndex, Path.States.Num() - 1);
            const NaiwaP2::FTrajectoryState& State = Path.States[VisibleStateIndex];
            Actor->SetActorHiddenInGame(false);
            Actor->SetActorLocation(ToWorld(State.X, State.Z));
        }
        else
        {
            Actor->SetActorHiddenInGame(true);
        }
    }
}

void ANaiwaP2GameMode::SpawnPersistentScene()
{
    UStaticMesh* Cube = LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Sphere = LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UMaterialInterface* FloorMaterial = LoadMaterial(TEXT("/Game/Naiwa/Materials/M_GrayboxFloor.M_GrayboxFloor"));
    // Geo 风格：地面带换 Unlit 色板材质；资产缺失时回落灰盒，保证开关永远可回退。
    UMaterialInterface* GroundVisual = FloorMaterial;
    if (bGeoArtStyle)
    {
        if (UMaterialInterface* GeoGround = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_Ground"))))
        {
            GroundVisual = GeoGround;
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("NAIWA_P2_ART_STYLE geo_material_missing=MI_P2Geo_Ground fallback=Graybox"));
        }
    }
    const bool bLevel23 = CurrentLevelId == TEXT("2-3");
    const double GroundCenterXcm = bLevel23 ? CurrentScene.MaxX * CentimetersPerMeter * 0.5 : 650.0;
    const double GroundWidthScale = bLevel23 ? CurrentScene.MaxX + 4.0 : 24.0;
    if (AStaticMeshActor* Ground = SpawnMeshActor(TEXT("P2_Ground"), FVector(GroundCenterXcm, 0.0, -25.0),
            FVector(GroundWidthScale, 4.0, 0.5), Cube, FloorMaterial))
    {
        // 改造前整个 P2 场景是零碰撞（SpawnMeshActor 里统一 NoCollision）。
        // 物理化之后这块地面是**唯一**的地板：顶面在 z = 0，正好对得上 SceneLibrary 里
        // 所有 MinZ = 0 的方块。不补这条，所有刚体会直接穿过地面掉下去。
        if (UStaticMeshComponent* GroundMesh = Ground->GetStaticMeshComponent())
        {
            GroundMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            GroundMesh->SetCollisionObjectType(ECC_WorldStatic);
            GroundMesh->SetCollisionResponseToAllChannels(ECR_Block);
            // 碰撞口径不变，只换外观（Geo 风格的地面带色板见接入计划书 §2.1）。
            GroundMesh->SetMaterial(0, GroundVisual);
        }
        PersistentActors.Add(Ground);
    }

    // 弹弓路 a：几何不动，只换木色 Unlit 材质（接入计划书 §6）。
    UMaterialInterface* SlingshotMaterial = LoadMaterial(TEXT("/Game/Naiwa/Materials/M_GrayboxPlatform.M_GrayboxPlatform"));
    if (bGeoArtStyle)
    {
        if (UMaterialInterface* GeoWood = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_Wood"))))
        {
            SlingshotMaterial = GeoWood;
        }
    }
    if (AStaticMeshActor* Grip = SpawnMeshActor(TEXT("P2_SlingshotGrip"), ToWorld(0.68, 0.72),
            FVector(0.48, 0.34, 1.20), Cube, SlingshotMaterial))
    {
        PersistentActors.Add(Grip);
    }
    if (AStaticMeshActor* Stem = SpawnMeshActor(TEXT("P2_SlingshotStem"), ToWorld(0.68, 1.55),
            FVector(0.38, 0.32, 0.78), Cube, SlingshotMaterial))
    {
        PersistentActors.Add(Stem);
    }
    if (AStaticMeshActor* UpperFork = SpawnMeshActor(TEXT("P2_SlingshotUpperFork"), ToWorld(1.18, 2.22),
            FVector(0.88, 0.30, 0.30), Cube, SlingshotMaterial, FRotator(38.0, 0.0, 0.0)))
    {
        PersistentActors.Add(UpperFork);
    }
    if (AStaticMeshActor* LowerFork = SpawnMeshActor(TEXT("P2_SlingshotLowerFork"), ToWorld(1.18, 1.16),
            FVector(0.88, 0.30, 0.30), Cube, SlingshotMaterial, FRotator(-38.0, 0.0, 0.0)))
    {
        PersistentActors.Add(LowerFork);
    }

    if (ADirectionalLight* Light = GetWorld()->SpawnActor<ADirectionalLight>(
            FVector(-500.0, -900.0, 1400.0), FRotator(-35.0, -28.0, 0.0)))
    {
        Light->GetLightComponent()->SetIntensity(3.0f);
        PersistentActors.Add(Light);
    }
    (void)Sphere;

    if (bGeoArtStyle)
    {
        SpawnGeoBackdrop();
    }
}

void ANaiwaP2GameMode::SpawnGeoBackdrop()
{
    // 2-1 逐点复用 render_final.py；2-2/2-3 使用已写回计划书的定稿折点。
    const bool bLevel22 = CurrentLevelId == TEXT("2-2");
    const bool bLevel23 = CurrentLevelId == TEXT("2-3");
    if (CurrentLevelId != TEXT("2-1") && !bLevel22 && !bLevel23)
    {
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_GEO_BACKDROP skipped level=%s reason=unsupported_level"),
            *CurrentLevelId);
        return;
    }
    const int32 ExistingPersistentActorCount = PersistentActors.Num();

    UStaticMesh* Cube = LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Sphere = LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UMaterialInterface* Cloud = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_Cloud")));
    UMaterialInterface* Sky1 = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_Sky1")));
    UMaterialInterface* Sky2 = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_Sky2")));
    UMaterialInterface* SkyTop = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_SkyTop")));
    UMaterialInterface* SkyMid = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_SkyMid")));
    UMaterialInterface* SkyBot = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_SkyBot")));
    UMaterialInterface* Ground = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_Ground")));
    UMaterialInterface* L1 = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_L1")));
    UMaterialInterface* L2 = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_L2")));
    UMaterialInterface* L3 = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_L3")));
    UMaterialInterface* L4 = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_L4")));
    UMaterialInterface* Light = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_Light")));
    UMaterialInterface* Leaf = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_Leaf")));
    UMaterialInterface* Trunk = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_Trunk")));
    UMaterialInterface* Rock = LoadMaterial(*GeoMaterialPath(TEXT("MI_P2Geo_Rock")));

    auto SpawnSlab = [&](const FString& Label, const double X1, const double X2,
                         const double Z1, const double Z2, const double Y1, const double Y2,
                         UMaterialInterface* Material)
    {
        if (!Material)
        {
            return;
        }
        // render_final.py 的 Blender 场景里"Y 越大越远"，而 UE 相机在 +Y 看向 -Y
        // （Y 越大越近）。背景数据的 Y 区间必须取负，否则天空板会盖在所有游戏对象前面。
        AStaticMeshActor* Actor = SpawnMeshActor(Label,
            FVector((X1 + X2) * 0.5 * CentimetersPerMeter,
                    -((Y1 + Y2) * 0.5) * CentimetersPerMeter,
                    (Z1 + Z2) * 0.5 * CentimetersPerMeter),
            FVector(X2 - X1, Y2 - Y1, Z2 - Z1), Cube, Material);
        if (Actor)
        {
            if (UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent())
            {
                Mesh->SetCastShadow(false);
            }
            PersistentActors.Add(Actor);
        }
    };

    // 天空五带（render_final.py sky_1/sky_2/sky_top/sky_mid/sky_bot，x -4..23）。
    SpawnSlab(TEXT("P2Geo_Sky1"),   -4.0, 23.0, 11.5, 14.0, 9.0, 9.2, Sky1);
    SpawnSlab(TEXT("P2Geo_Sky2"),   -4.0, 23.0,  9.2, 11.5, 9.0, 9.2, Sky2);
    SpawnSlab(TEXT("P2Geo_SkyTop"), -4.0, 23.0,  8.2,  9.2, 9.0, 9.2, SkyTop);
    SpawnSlab(TEXT("P2Geo_SkyMid"), -4.0, 23.0,  4.4,  8.2, 9.0, 9.2, SkyMid);
    SpawnSlab(TEXT("P2Geo_SkyBot"), -4.0, 23.0, -2.5,  4.4, 9.0, 9.2, SkyBot);

    // Render the seven flat Blender cloud forms as thin, unlit ellipsoids.
    struct FGeoCloud { double X; double Z; double HalfWidth; double HalfHeight; };
    const FGeoCloud Clouds[] = {
        {5.2, 9.4, 1.5, 0.28}, {16.8, 10.2, 1.1, 0.22}, {2.4, 11.3, 0.9, 0.18},
        {9.6, 10.6, 1.3, 0.24}, {13.4, 11.8, 0.8, 0.16}, {20.4, 9.6, 1.0, 0.20},
        {7.0, 12.4, 0.7, 0.14}
    };
    if (Sphere && Cloud)
    {
        for (int32 i = 0; i < UE_ARRAY_COUNT(Clouds); ++i)
        {
            const FGeoCloud& Shape = Clouds[i];
            if (AStaticMeshActor* Actor = SpawnMeshActor(
                    FString::Printf(TEXT("P2Geo_Cloud%d"), i),
                    FVector(Shape.X * CentimetersPerMeter, -860.0,
                            Shape.Z * CentimetersPerMeter),
                    FVector(Shape.HalfWidth * 2.0, 0.06, Shape.HalfHeight * 2.0),
                    Sphere, Cloud))
            {
                if (UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent())
                {
                    Mesh->SetCastShadow(false);
                }
                PersistentActors.Add(Actor);
            }
        }
    }

    // 四层山：L4/L3/L2 折线剪影（PMC 三角带），L1 近层实心带（render_final.py 同名 slab）。
    auto SpawnLayer = [&](const TCHAR* Name, TArray<FVector2D>&& Points,
                          const double YCenter, const double BottomZ, UMaterialInterface* Material)
    {
        if (!Material)
        {
            return;
        }
        if (AActor* Actor = SpawnGeoSilhouette(FString::Printf(TEXT("P2Geo_%s"), Name),
                MoveTemp(Points), YCenter, BottomZ, Material))
        {
            PersistentActors.Add(Actor);
        }
    };
    SpawnLayer(TEXT("L4"), MakeGeoPolylineL4(CurrentLevelId), -7.5, -2.5, L4);
    SpawnLayer(TEXT("L3"), MakeGeoPolylineL3(CurrentLevelId), -6.1, -2.5, L3);
    SpawnLayer(TEXT("L2"), MakeGeoPolylineL2(CurrentLevelId), -4.1, -2.5, L2);
    const double NearLayerX1 = bLevel23 ? 10.0 : 9.2;
    SpawnSlab(TEXT("P2Geo_L1"), NearLayerX1, 23.0, -2.5, 1.10, 2.4, 2.6, L1);
    // The structural floor is only 0.5m deep; this wider background band fills
    // the lower frame like render_final.py without changing collision geometry.
    SpawnSlab(TEXT("P2Geo_GroundBand"), -4.0, 23.0, -2.6, 0.0, 1.2, 1.4, Ground);

    // Z0 亮线：render_final 里压在地面上沿（z -0.022..0），但 UE 的碰撞地面箱顶面
    // 也在 z=0 且更靠前，会共面闪烁；抬 2.2mm 放在地面上方，视觉等效。
    SpawnSlab(TEXT("P2Geo_LineZ0"), -4.0, 23.0, 0.0, 0.022, 0.70, 0.80, Light);

    // 中景装饰：各关调整摆位，避开主塔背后的山峰轮廓（纯装饰零碰撞）。
    struct FGeoTree { double X; double Height; };
    TArray<FGeoTree> Trees;
    if (bLevel22)
    {
        Trees = { {5.8, 1.20}, {8.2, 0.95}, {10.7, 1.40}, {13.0, 0.90} };
    }
    else if (bLevel23)
    {
        Trees = { {4.2, 1.15}, {7.0, 0.95}, {9.8, 1.35}, {12.0, 0.85} };
    }
    else
    {
        Trees = { {7.6, 1.30}, {8.9, 0.95}, {10.2, 1.55}, {12.2, 0.85} };
    }
    for (int32 i = 0; i < Trees.Num(); ++i)
    {
        const double Tx = Trees[i].X;
        const double Th = Trees[i].Height;
        SpawnSlab(FString::Printf(TEXT("P2Geo_Trunk%d"), i),
            Tx - 0.045, Tx + 0.045, 0.0, Th * 0.45, 1.55, 1.62, Trunk);
        SpawnSlab(FString::Printf(TEXT("P2Geo_Leaf%d"), i),
            Tx - 0.34, Tx + 0.34, Th * 0.42, Th, 1.56, 1.61, Leaf);
    }
    struct FGeoRock { double X; double W; double H; };
    TArray<FGeoRock> Rocks;
    if (bLevel22)
    {
        Rocks = { {6.7, 0.34, 0.18}, {11.6, 0.30, 0.16}, {13.9, 0.24, 0.12} };
    }
    else if (bLevel23)
    {
        Rocks = { {5.8, 0.34, 0.18}, {9.1, 0.30, 0.16}, {13.1, 0.24, 0.12} };
    }
    else
    {
        Rocks = { {9.6, 0.42, 0.22}, {11.4, 0.30, 0.16}, {6.9, 0.26, 0.13} };
    }
    for (int32 i = 0; i < Rocks.Num(); ++i)
    {
        SpawnSlab(FString::Printf(TEXT("P2Geo_Rock%d"), i),
            Rocks[i].X - Rocks[i].W, Rocks[i].X + Rocks[i].W, 0.0, Rocks[i].H, 1.60, 1.66, Rock);
    }

    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P2_GEO_BACKDROP spawned level=%s background_actors=%d silhouettes=3 clouds=7"),
        *CurrentLevelId, PersistentActors.Num() - ExistingPersistentActorCount);
}

AActor* ANaiwaP2GameMode::SpawnGeoSilhouette(const FString& Label,
    const TArray<FVector2D>& PointsMeters, const double YCenterMeters,
    const double BottomZMeters, UMaterialInterface* Material)
{
    if (PointsMeters.Num() < 2 || !Material)
    {
        return nullptr;
    }

    const double YCm = YCenterMeters * CentimetersPerMeter;
    const double BottomZCm = BottomZMeters * CentimetersPerMeter;
    TArray<FVector> Vertices;
    Vertices.Reserve(PointsMeters.Num() * 2);
    for (const FVector2D& Point : PointsMeters)
    {
        // 每个折点两列顶点：折线顶边 + 平底；相机在 +Y 侧。
        Vertices.Add(FVector(Point.X * CentimetersPerMeter, YCm, Point.Y * CentimetersPerMeter));
        Vertices.Add(FVector(Point.X * CentimetersPerMeter, YCm, BottomZCm));
    }

    TArray<int32> Triangles;
    Triangles.Reserve((PointsMeters.Num() - 1) * 6);
    for (int32 i = 0; i < PointsMeters.Num() - 1; ++i)
    {
        const int32 TopA = i * 2;
        const int32 BotA = i * 2 + 1;
        const int32 TopB = i * 2 + 2;
        const int32 BotB = i * 2 + 3;
        // 折线从左到右（dx>0）且底边低于顶边时，该绕序的法线朝 +Y。
        Triangles.Append({ TopA, TopB, BotB, TopA, BotB, BotA });
    }

    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FProcMeshTangent> Tangents;
    Normals.Init(FVector::YAxisVector, Vertices.Num());
    UV0.Init(FVector2D::ZeroVector, Vertices.Num());
    Tangents.Init(FProcMeshTangent(1.0f, 0.0f, 0.0f), Vertices.Num());

    AActor* Actor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
    if (!Actor)
    {
        return nullptr;
    }
    UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Actor, *Label);
    Actor->SetRootComponent(Mesh);
    Mesh->RegisterComponent();
    Mesh->CreateMeshSection(0, Vertices, Triangles, Normals, UV0, TArray<FColor>(),
        Tangents, false);
    Mesh->SetMaterial(0, Material);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCastShadow(false);
    Actor->Tags.Add(FName(*Label));
    return Actor;
}

UMaterialInterface* ANaiwaP2GameMode::GetGeoMaterialForBlock(const NaiwaP2::FBox2& Box) const
{
    // 按 Id 后缀映射（接入计划书 §7.4），三关通用：
    // .wall = 矮墙米色（W.wall / R.wall），.col = 石柱（P.col / A.col），其余 = 木塔。
    const TCHAR* Name = TEXT("MI_P2Geo_Wood");
    if (Box.Id.EndsWith(TEXT(".wall")))
    {
        Name = TEXT("MI_P2Geo_Plast");
    }
    else if (Box.Id.EndsWith(TEXT(".col")))
    {
        Name = TEXT("MI_P2Geo_Stone");
    }
    return LoadMaterial(*GeoMaterialPath(Name));
}

void ANaiwaP2GameMode::UpdateScreenshotProbe(const double DeltaSeconds)
{
    if (ScreenshotProbeDelaySeconds <= 0.0 || ScreenshotProbeState >= 2)
    {
        return;
    }
    ScreenshotProbeElapsed += DeltaSeconds;
    if (ScreenshotProbeState == 0 && ScreenshotProbeElapsed >= ScreenshotProbeDelaySeconds)
    {
        // 高分辨率截图走 GameViewport 的异步捕获，下一两帧落盘。
        FScreenshotRequest::RequestScreenshot(ScreenshotProbePath, false, false);
        ScreenshotProbeState = 1;
        ScreenshotProbeElapsed = 0.0;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_SCREENSHOT_PROBE requested path=%s"),
            *ScreenshotProbePath);
    }
    else if (ScreenshotProbeState == 1 && ScreenshotProbeElapsed >= 1.5)
    {
        ScreenshotProbeState = 2;
        const bool bSaved = ScreenshotProbePath.IsEmpty() ||
            IFileManager::Get().FileSize(*ScreenshotProbePath) > 0;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_SCREENSHOT_PROBE done saved=%d path=%s"),
            bSaved ? 1 : 0, *ScreenshotProbePath);
        FPlatformMisc::RequestExit(false);
    }
}

void ANaiwaP2GameMode::SpawnAttemptScene()
{
    StructureBlocks.Reset();

    UStaticMesh* Cube = LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Sphere = LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UMaterialInterface* StructureMaterial = LoadMaterial(TEXT("/Game/Naiwa/Materials/M_GrayboxPlatform.M_GrayboxPlatform"));
    UMaterialInterface* GoalMaterial = LoadMaterial(TEXT("/Game/Naiwa/P1/Materials/M_P1_V7_goal.M_P1_V7_goal"));
    UMaterialInstanceDynamic* GeoGoalMaterial = nullptr;
    if (bGeoArtStyle)
    {
        if (UMaterialInterface* GeoSolid = LoadMaterial(*GeoMaterialPath(TEXT("M_P2Geo_Solid"))))
        {
            GeoGoalMaterial = UMaterialInstanceDynamic::Create(GeoSolid, this);
            if (GeoGoalMaterial)
            {
                GeoGoalMaterial->SetVectorParameterValue(TEXT("BaseColor"),
                    FLinearColor::FromSRGBColor(FColor(0xF5, 0xC5, 0x42)));
                GeoGoalMaterial->SetScalarParameterValue(TEXT("Grain"), 0.0f);
            }
        }
    }

    for (const NaiwaP2::FBox2& Box : CurrentScene.Boxes)
    {
        // F1 isolates one static wall, its unchanged T.base support, and one movable complete
        // block so collisions and momentum can be observed without the rest of the tower masking
        // the result. Keeping T.base prevents the dynamic T.c1 from starting 30 cm above ground.
        // SceneLibrary remains the source of these unchanged shapes and coordinates.
        if (bRunPhysicsSandbox && !Box.bGoal &&
            Box.Id != TEXT("W.wall") && Box.Id != TEXT("T.base") && Box.Id != TEXT("T.c1"))
        {
            continue;
        }
        if (bRunPhysicsSandbox && Box.bGoal && Box.Id != TEXT("T1"))
        {
            continue;
        }
        const double Width = Box.MaxX - Box.MinX;
        const double Height = Box.MaxZ - Box.MinZ;
        const double CenterX = (Box.MinX + Box.MaxX) * 0.5;
        const double CenterZ = (Box.MinZ + Box.MaxZ) * 0.5;
        const FString Label = FString::Printf(TEXT("P2_%s"), *Box.Id);

        if (!Box.bGoal && bPhysicsStructureEnabled)
        {
            // 结构块：用 FBox2 的原始 X/Z 生成一个刚体。坐标不改，只换存在形式。
            ANaiwaP2StructureBlock* Block = GetWorld()->SpawnActor<ANaiwaP2StructureBlock>(
                ANaiwaP2StructureBlock::StaticClass(), FTransform::Identity);
            if (!Block)
            {
                UE_LOG(LogTemp, Error, TEXT("NAIWA_P2_BLOCK_SPAWN_FAIL id=%s"), *Box.Id);
                continue;
            }
            // Geo 风格：按块选 Unlit 色板材质（.wall=Plast .col=Stone 其余=Wood）。
            // 物理不受影响：InitFromBox 里碰撞/质量只依赖 FBox2 与角色，材质仅 SetMaterial。
            UMaterialInterface* BlockVisual = StructureMaterial;
            if (bGeoArtStyle)
            {
                BlockVisual = GetGeoMaterialForBlock(Box);
                if (!BlockVisual)
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("NAIWA_P2_ART_STYLE geo_material_missing block=%s fallback=Graybox"),
                        *Box.Id);
                    BlockVisual = StructureMaterial;
                }
            }
            Block->InitFromBox(Box.Id, Box, ClassifyBlockRole(Box), BlockVisual,
                PhysicsStructureMassMultiplier);
            AttemptActors.Add(Block);
            StructureActors.Add(Box.Id, Block);
            StructureBlocks.Add(Block);
            continue;
        }

        if (Box.bGoal && bRunPhysicsFullLevel)
        {
            double GoalSupportTopZ = 0.0;
            for (const NaiwaP2::FBox2& Candidate : CurrentScene.Boxes)
            {
                if (Candidate.bGoal || Candidate.Id == Box.Id ||
                    Candidate.MinX >= Box.MaxX || Candidate.MaxX <= Box.MinX)
                {
                    continue;
                }
                if (FMath::Abs(Candidate.MaxZ - Box.MinZ) <= 0.03)
                {
                    GoalSupportTopZ = FMath::Max(GoalSupportTopZ, Candidate.MaxZ);
                }
            }
            // Align each capsule bottom with its authored support top. A goal with no
            // support beneath it starts on the generated ground instead of falling into it.
            const double GoalSpawnCenterZ = GoalSupportTopZ + Height * 0.5;
            const bool bCompactGoalCollider = CurrentLevelId == TEXT("2-2") ||
                CurrentLevelId == TEXT("2-3");
            const double GoalCollisionRadiusCm = CurrentLevelId == TEXT("2-3")
                ? 24.0 : (bCompactGoalCollider ? 25.0 : Width * CentimetersPerMeter * 0.5);
            const double GoalCollisionWidthM = GoalCollisionRadiusCm * 2.0 / CentimetersPerMeter;
            ANaiwaP2PhysicsGoal* GoalActor = GetWorld()->SpawnActor<ANaiwaP2PhysicsGoal>(
                ANaiwaP2PhysicsGoal::StaticClass(), ToWorld(CenterX, GoalSpawnCenterZ), FRotator::ZeroRotator);
            if (!GoalActor)
            {
                UE_LOG(LogTemp, Error, TEXT("NAIWA_P2_PHYS_GOAL_SPAWN_FAIL id=%s"), *Box.Id);
                continue;
            }

            GoalActor->Tags.AddUnique(FName(*Label));
            GoalActor->Tags.AddUnique(FName(TEXT("NaiwaP2PhysicsGoal")));
            GoalActor->InitializeVisual(Sphere, GoalMaterial, FVector(Width, 0.68, Height),
                static_cast<float>(GoalCollisionRadiusCm),
                static_cast<float>(Height * CentimetersPerMeter * 0.5));
            if (GeoGoalMaterial)
            {
                GoalActor->InitializeGeoEggVisual(GeoGoalMaterial, FVector(Width, 0.68, Height));
            }

            const NaiwaP2::Physics::FBlockMaterial GoalPhysics =
                NaiwaP2::Physics::GetBlockMaterial(NaiwaP2::Physics::EBlockRole::Goal);
            UPhysicalMaterial* GoalMaterialOverride = NewObject<UPhysicalMaterial>(
                GoalActor, TEXT("P2PhysicsGoalPhysMat"));
            GoalMaterialOverride->Friction = GoalPhysics.Friction;
            GoalMaterialOverride->StaticFriction = GoalPhysics.StaticFriction;
            GoalMaterialOverride->Restitution = GoalPhysics.Restitution;
            GoalMaterialOverride->FrictionCombineMode = EFrictionCombineMode::Max;
            GoalMaterialOverride->bOverrideFrictionCombineMode = true;
            GoalMaterialOverride->RestitutionCombineMode = EFrictionCombineMode::Max;
            GoalMaterialOverride->bOverrideRestitutionCombineMode = true;

            UCapsuleComponent* GoalBody = GoalActor->GetCollisionCapsule();
            GoalBody->SetPhysMaterialOverride(GoalMaterialOverride);
            GoalBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            GoalBody->SetCollisionObjectType(ECC_WorldDynamic);
            GoalBody->SetCollisionResponseToAllChannels(ECR_Block);
            GoalBody->SetNotifyRigidBodyCollision(true);
            GoalBody->SetConstraintMode(EDOFMode::XZPlane);
            GoalBody->SetMassOverrideInKg(NAME_None, static_cast<float>(
                GoalPhysics.DensityKgPerM3 * (4.0 / 3.0) * PI *
                (GoalCollisionWidthM * 0.5) * (0.68 * 0.5) * (Height * 0.5)), true);
            GoalBody->SetLinearDamping(GoalPhysics.LinearDamping);
            GoalBody->SetAngularDamping(GoalPhysics.AngularDamping);
            GoalBody->SetUseCCD(true);
            GoalBody->OnComponentHit.AddDynamic(this, &ANaiwaP2GameMode::HandlePhysicsGoalHit);
            GoalBody->SetSimulatePhysics(true);

            AttemptActors.Add(GoalActor);
            GoalActors.Add(Box.Id, GoalActor);
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_PHYS_GOAL id=%s mass_kg=%.1f collider_radius_cm=%.1f collider_half_height_cm=%.1f spawn_z_cm=%.1f full_level=1"),
                *Box.Id, GoalBody->GetMass(), GoalBody->GetUnscaledCapsuleRadius(),
                GoalBody->GetUnscaledCapsuleHalfHeight(), GoalSpawnCenterZ * CentimetersPerMeter);
            continue;
        }

        AStaticMeshActor* Actor = SpawnMeshActor(Label, ToWorld(CenterX, CenterZ),
            Box.bGoal ? FVector(Width, 0.68, Height) : FVector(Width, 1.5, Height),
            Box.bGoal ? Sphere : Cube, Box.bGoal ? GoalMaterial : StructureMaterial);
        if (!Actor)
        {
            continue;
        }
        AttemptActors.Add(Actor);
        if (Box.bGoal)
        {
            if (bRunPhysicsSandbox)
            {
                Actor->Tags.AddUnique(FName(TEXT("NaiwaP2PhysicsGoal")));
                if (bRunPhysicsSandbox)
                {
                    Actor->Tags.AddUnique(FName(TEXT("NaiwaP2PhysicsSandboxGoal")));
                }
                if (UStaticMeshComponent* GoalMesh = Actor->GetStaticMeshComponent())
                {
                    const NaiwaP2::Physics::FBlockMaterial GoalPhysics =
                        NaiwaP2::Physics::GetBlockMaterial(NaiwaP2::Physics::EBlockRole::Goal);
                    UPhysicalMaterial* GoalMaterialOverride = NewObject<UPhysicalMaterial>(Actor, TEXT("P2SandboxGoalPhysMat"));
                    GoalMaterialOverride->Friction = GoalPhysics.Friction;
                    GoalMaterialOverride->StaticFriction = GoalPhysics.StaticFriction;
                    GoalMaterialOverride->Restitution = GoalPhysics.Restitution;
                    GoalMaterialOverride->FrictionCombineMode = EFrictionCombineMode::Max;
                    GoalMaterialOverride->bOverrideFrictionCombineMode = true;
                    GoalMaterialOverride->RestitutionCombineMode = EFrictionCombineMode::Max;
                    GoalMaterialOverride->bOverrideRestitutionCombineMode = true;
                    GoalMesh->SetPhysMaterialOverride(GoalMaterialOverride);
                    GoalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                    GoalMesh->SetCollisionObjectType(ECC_WorldDynamic);
                    GoalMesh->SetCollisionResponseToAllChannels(ECR_Block);
                    GoalMesh->SetNotifyRigidBodyCollision(true);
                    GoalMesh->SetConstraintMode(EDOFMode::XZPlane);
                    GoalMesh->SetSimulatePhysics(true);
                    const double GoalMassKg = bRunPhysicsSandbox
                        ? NaiwaP2::Physics::SandboxGoalMassKg
                        : GoalPhysics.DensityKgPerM3 * (4.0 / 3.0) * PI *
                            (Width * 0.5) * (0.68 * 0.5) * (Height * 0.5);
                    GoalMesh->SetMassOverrideInKg(NAME_None, static_cast<float>(GoalMassKg), true);
                    GoalMesh->SetLinearDamping(GoalPhysics.LinearDamping);
                    GoalMesh->SetAngularDamping(GoalPhysics.AngularDamping);
                    GoalMesh->SetUseCCD(true);
                }
                const double GoalMassKg = NaiwaP2::Physics::SandboxGoalMassKg;
                UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_PHYS_GOAL id=%s mass_kg=%.1f full_level=%d"),
                    *Box.Id, GoalMassKg, 0);
            }
            GoalActors.Add(Box.Id, Actor);
            if (!bRunPhysicsSandbox && !bRunPhysicsFullLevel)
            {
                if (UPrimitiveComponent* GoalVisualBody = GetPhysicsGoalBody(Actor))
                {
                    GoalVisualBody->SetSimulatePhysics(false);
                    GoalVisualBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    UE_LOG(LogTemp, Display,
                        TEXT("NAIWA_P2_GOAL_VISUAL_ONLY id=%s actor=%s collision=%d sim=%d"),
                        *Box.Id, *Actor->GetName(),
                        static_cast<int32>(GoalVisualBody->GetCollisionEnabled()),
                        GoalVisualBody->IsSimulatingPhysics() ? 1 : 0);
                }
            }
        }
        else
        {
            StructureActors.Add(Box.Id, Actor);
        }
    }

    // 所有块生成完之后再做支撑关系分析：悬空的块要么焊到邻块上、要么退化为静态。
    // 必须在全部块都存进 StructureBlocks 之后跑，否则找不到可以焊的对象。
    ResolveUnsupportedBlocks();

    PouchActor = SpawnEggActor(TEXT("P2_LauncherPouch"),
        ToWorld(0.0, NaiwaP2::PouchHeightM), SelectedEgg);
    if (PouchActor)
    {
        AttemptActors.Add(PouchActor);
    }
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_SCENE_SPAWN structures_and_goals=%d"),
        CurrentScene.Boxes.Num());
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_PHYS_SPAWN level=%s blocks=%d goals=%d physics=%d"),
        *CurrentLevelId, StructureBlocks.Num(), GoalActors.Num(), bPhysicsStructureEnabled ? 1 : 0);
    if (bRunPhysicsSandbox)
    {
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_PHYS_SANDBOX_LAYOUT static_wall=W.wall static_support=T.base "
                 "movable_block=T.c1 goal=T1"));
    }
}

AStaticMeshActor* ANaiwaP2GameMode::SpawnMeshActor(const FString& Label, const FVector& Location,
    const FVector& Scale, UStaticMesh* Mesh, UMaterialInterface* Material, const FRotator& Rotation)
{
    if (!Mesh)
    {
        return nullptr;
    }
    AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Location, Rotation);
    if (!Actor)
    {
        return nullptr;
    }
    Actor->Tags.Add(FName(*Label));
    Actor->SetActorScale3D(Scale);
    UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetStaticMesh(Mesh);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (Material)
    {
        Component->SetMaterial(0, Material);
    }
    return Actor;
}

const FNaiwaP2EggVisual* ANaiwaP2GameMode::FindEggVisual(const NaiwaP2::EEggKind Kind) const
{
    return EggVisuals.Find(static_cast<uint8>(Kind));
}

void ANaiwaP2GameMode::LogEggVisualTableDiagnostics() const
{
    // 打包版一旦出现"五个蛋都是灰球"，先看这条日志：
    // mesh=ASSIGNED 表示硬引用成立（资产已进 cook）；UNASSIGNED 表示资产没进包或路径不对。
    // Only report variants that this level actually offers. Unimported special
    // eggs are expected on 2-1, which currently uses Base exclusively.
    for (const NaiwaP2::EEggKind Kind : AvailableEggs)
    {
        const FNaiwaP2EggVisual* Visual = FindEggVisual(Kind);
        if (!Visual || !Visual->Mesh)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("NAIWA_P2_EGG_VISUAL kind=%s mesh=UNASSIGNED body=%s source=legacy_string_path"),
                NaiwaP2::GetEggKindLabel(Kind),
                (Visual && Visual->BodyMaterial) ? TEXT("ASSIGNED") : TEXT("UNASSIGNED"));
            continue;
        }

        const int32 SlotCount = Visual->Mesh->GetMaterials().Num();
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_EGG_VISUAL kind=%s mesh=%s slots=%d morphs=%d accent=%s"),
            NaiwaP2::GetEggKindLabel(Kind),
            *Visual->Mesh->GetName(),
            SlotCount,
            Visual->Mesh->GetMorphTargets().Num(),
            Visual->AccentMaterial ? TEXT("ASSIGNED") : TEXT("EMPTY"));

        const bool bAccentSlotExpected = Kind != NaiwaP2::EEggKind::Base &&
            Kind != NaiwaP2::EEggKind::Mouse;
        const int32 ExpectedSlotCount = bAccentSlotExpected ? EggMaterialSlotCount : EggAccentSlotIndex;
        if (SlotCount < ExpectedSlotCount)
        {
            // Base and Mouse intentionally use four slots; accent variants require slot 4.
            UE_LOG(LogTemp, Warning,
                TEXT("NAIWA_P2_EGG_VISUAL_SLOTS kind=%s slots=%d expected=%d"),
                NaiwaP2::GetEggKindLabel(Kind), SlotCount, ExpectedSlotCount);
        }
    }
}

AActor* ANaiwaP2GameMode::SpawnEggActor(const FString& Label, const FVector& Location,
    const NaiwaP2::EEggKind Kind, const double ScaleMultiplier, const bool bSkillActive,
    const bool bPhysicsBody, const FVector& InitialVelocityCmPerS)
{
    USkeletalMesh* Mesh = nullptr;
    if (const FNaiwaP2EggVisual* Visual = FindEggVisual(Kind))
    {
        Mesh = Visual->Mesh.Get();
    }
    if (!Mesh)
    {
        // 兜底：资产表缺项时沿用旧路径，保证美术交付前的行为与改动前完全一致。
        // 注意这条路径不进 cook，正式包不能依赖它。
        Mesh = LoadObject<USkeletalMesh>(nullptr, LegacyEggMeshPath);
    }
    if (!Mesh)
    {
        UE_LOG(LogTemp, Error,
            TEXT("NAIWA_P2_EGG_MESH_MISSING kind=%s table=%s fallback=%s"),
            NaiwaP2::GetEggKindLabel(Kind),
            FindEggVisual(Kind) ? TEXT("configured_but_unloaded") : TEXT("not_configured"),
            LegacyEggMeshPath);
        UStaticMesh* Sphere = LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        // 灰球兜底也要尽量保留变体色，否则"打包丢资产"会退化成五个同色灰球，更难定位。
        UMaterialInterface* FallbackMaterial = nullptr;
        if (const FNaiwaP2EggVisual* Visual = FindEggVisual(Kind))
        {
            FallbackMaterial = Visual->BodyMaterial.Get();
        }
        if (!FallbackMaterial)
        {
            FallbackMaterial = LoadMaterial(GetEggMaterialPath(Kind));
        }
        AStaticMeshActor* Fallback = SpawnMeshActor(Label, Location,
            FVector(1.0, 1.0, 1.0) * (ScaleMultiplier * GetEggVisualScale(Kind)),
            Sphere, FallbackMaterial);
        return Fallback;
    }

    if (bPhysicsBody)
    {
        ANaiwaP2PhysicsEgg* PhysicsEgg = GetWorld()->SpawnActor<ANaiwaP2PhysicsEgg>(
            ANaiwaP2PhysicsEgg::StaticClass(), FTransform(FRotator::ZeroRotator, Location));
        if (!PhysicsEgg)
        {
            return nullptr;
        }
        PhysicsEgg->Tags.Add(FName(*Label));
        PhysicsEgg->GetVisualMesh()->SetSkeletalMesh(Mesh);
        NaiwaP2::FEggSpec PhysicsEggSpec = NaiwaP2::GetEggSpec(Kind);
        PhysicsEggSpec.RadiusMultiplier *= ScaleMultiplier;
        PhysicsEgg->InitializePhysics(
            NaiwaP2::BaseProjectileRadiusM * PhysicsEggSpec.RadiusMultiplier *
                NaiwaP2::Physics::CentimetersPerMeter,
            NaiwaP2::Physics::GetEggMassKg(PhysicsEggSpec), InitialVelocityCmPerS,
            NaiwaP2::Physics::EggRestitution, NaiwaP2::Physics::EggFriction,
            NaiwaP2::Physics::bEggUseCCD);
        ApplyEggActorAppearance(PhysicsEgg, Kind, ScaleMultiplier, bSkillActive);
        return PhysicsEgg;
    }

    ASkeletalMeshActor* Actor = GetWorld()->SpawnActor<ASkeletalMeshActor>(Location, FRotator::ZeroRotator);
    if (!Actor)
    {
        return nullptr;
    }
    Actor->Tags.Add(FName(*Label));
    Actor->SetActorScale3D(FVector(ScaleMultiplier, ScaleMultiplier, ScaleMultiplier));
    USkeletalMeshComponent* Component = Actor->GetSkeletalMeshComponent();
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetSkeletalMesh(Mesh);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    ApplyEggActorAppearance(Actor, Kind, ScaleMultiplier, bSkillActive);
    UE_LOG(LogTemp, Verbose, TEXT("NAIWA_P2_EGG_ACTOR label=%s kind=%s mesh=%s scale=%.2f"),
        *Label, NaiwaP2::GetEggKindLabel(Kind), *GetNameSafe(Mesh), ScaleMultiplier);
    return Actor;
}

void ANaiwaP2GameMode::ApplyEggActorAppearance(AActor* Actor, const NaiwaP2::EEggKind Kind,
    const double ScaleMultiplier, const bool bSkillActive)
{
    if (!IsValid(Actor))
    {
        return;
    }
    // ScaleMultiplier 是调用方的相对倍率（鼠分裂 0.72、其余 1.0），这里再叠加按变体的视觉缩放。
    // 放在这里而不是各个调用点，是因为皮兜刷新那三处传默认 1.0，若在外面加会被它们抹回去。
    const double EffectiveScale = ScaleMultiplier * GetEggVisualScale(Kind);
    if (ANaiwaP2PhysicsEgg* PhysicsEgg = Cast<ANaiwaP2PhysicsEgg>(Actor))
    {
        PhysicsEgg->SetVisualScale(EffectiveScale);
    }
    else
    {
        Actor->SetActorScale3D(FVector(EffectiveScale, EffectiveScale, EffectiveScale));
    }
    USkeletalMeshComponent* Component = Actor->FindComponentByClass<USkeletalMeshComponent>();
    if (!Component)
    {
        return;
    }
    const FNaiwaP2EggVisual* Visual = FindEggVisual(Kind);
    if (Visual && Visual->Mesh && Component->GetSkeletalMeshAsset() != Visual->Mesh.Get())
    {
        Component->SetSkinnedAssetAndUpdate(Visual->Mesh.Get());
        if (Actor->ActorHasTag(FName(TEXT("P2_LauncherPouch"))))
        {
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_POUCH_VISUAL kind=%s mesh=%s"),
                NaiwaP2::GetEggKindLabel(Kind), *Visual->Mesh->GetName());
        }
    }
    UMaterialInterface* BodyMaterial = Visual ? Visual->BodyMaterial.Get() : nullptr;
    if (!BodyMaterial)
    {
        // 兜底同上：仅用于资产表未配置的中间态。
        BodyMaterial = LoadMaterial(GetEggMaterialPath(Kind));
    }

    // 按槽索引写入。用 GetNumMaterials 挡一层：网格还没迁到 5 槽时自动跳过，
    // 不会因为越界写槽而静默丢掉材质覆盖。
    const int32 SlotCount = Component->GetNumMaterials();
    if (BodyMaterial && SlotCount > EggBodySlotIndex)
    {
        UMaterialInterface* GeoSolid = bGeoArtStyle
            ? LoadMaterial(*GeoMaterialPath(TEXT("M_P2Geo_Solid")))
            : nullptr;
        UMaterialInstanceDynamic* GeoBodyMaterial = GeoSolid
            ? UMaterialInstanceDynamic::Create(GeoSolid, Component)
            : nullptr;
        if (GeoBodyMaterial)
        {
            GeoBodyMaterial->SetVectorParameterValue(TEXT("BaseColor"),
                FLinearColor::FromSRGBColor(GetGeoEggBodyColor(Kind)));
            GeoBodyMaterial->SetScalarParameterValue(TEXT("Grain"), 0.0f);
            Component->SetMaterial(EggBodySlotIndex, GeoBodyMaterial);
        }
        else
        {
            Component->SetMaterial(EggBodySlotIndex, BodyMaterial);
        }
    }
    if (Visual && Visual->AccentMaterial && SlotCount > EggAccentSlotIndex)
    {
        Component->SetMaterial(EggAccentSlotIndex, Visual->AccentMaterial.Get());
    }
    Component->SetMorphTarget(TEXT("EyeWide"), 0.0f);
    Component->SetMorphTarget(TEXT("Scream"), 0.0f);
    Component->SetMorphTarget(TEXT("Squash"), 0.0f);
    if (bSkillActive)
    {
        if (Kind == NaiwaP2::EEggKind::Chick)
        {
            Component->SetMorphTarget(TEXT("EyeWide"), 1.0f);
        }
        else if (Kind == NaiwaP2::EEggKind::Mouse || Kind == NaiwaP2::EEggKind::Tea)
        {
            Component->SetMorphTarget(TEXT("Scream"), 1.0f);
        }
    }
}

void ANaiwaP2GameMode::FinishCurrentShot()
{
    if (!bShotStructureEffectsApplied)
    {
        bShotStructureEffectsApplied = true;

        if (bPhysicsStructureEnabled)
        {
            // 全物理模式：倒塌由 Chaos 算，不再调 StartStructureCollapse。
            //
            // 必须真正跳过而不是"跑了但不管结果"：旧倒塌会直接改 Actor 的 Transform 和
            // Mesh 的 Scale，而结构块现在是参与求解的刚体 —— 对一个正在模拟的刚体
            // teleport，会把速度清零、把整个接触岛唤醒并推出一个极大的去穿透冲量，
            // 表现就是"结算瞬间整座塔炸开"。
            //
            // 目标判定（哪几个目标被打飞）在 M3 接物理接触事件，本轮不接。
        }
        else
        {
            StartStructureCollapse();
            if (bStructureCollapseAnimating)
            {
                return;
            }
        }
    }

    if (CurrentShot.RawKilled.IsEmpty() && RefundZeroKillShot())
    {
        return;
    }

    ApplyShotGoalsAndTransition();
}

bool ANaiwaP2GameMode::RefundZeroKillShot()
{
    if (!bFreeRetryAvailable || !bHasShotStartSnapshot || !CurrentShot.RawKilled.IsEmpty())
    {
        return false;
    }

    if (bRunPhysicsFullLevel)
    {
        const int32 ResolvedShotNumber = ShotsFired;
        const bool bTDeckWasHit = CurrentShot.Hits.ContainsByPredicate([](const NaiwaP2::FShotHit& Hit)
        {
            return Hit.BoxId == TEXT("T.d3");
        });
        Retry();
        bFreeRetryAvailable = false;
        ShowActionNotice(TEXT("FREE RETRY  -  PHYSICS SCENE RESTORED"), 2.2);
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_PHYS_REFUND shot=%d no_new_goals=1 t_d3_hit=%d scene_rebuilt=1 shots=%d entitlement=%d"),
            ResolvedShotNumber, bTDeckWasHit, ShotsFired, bFreeRetryAvailable);
        return true;
    }

    CurrentScene = ShotStartScene;
    AvailableEggs = ShotStartAvailableEggs;
    SelectedEgg = ShotStartSelectedEgg;
    bool bAllStructureActorsRestored = true;
    for (const TPair<FString, FTransform>& Pair : ShotStartStructureTransforms)
    {
        TObjectPtr<AActor>* ActorEntry = StructureActors.Find(Pair.Key);
        if (!ActorEntry || !IsValid(ActorEntry->Get()))
        {
            bAllStructureActorsRestored = false;
            continue;
        }
        ActorEntry->Get()->SetActorTransform(Pair.Value);
    }

    const int32 ResolvedShotNumber = ShotsFired;
    const bool bTDeckWasHit = CurrentShot.Hits.ContainsByPredicate([](const NaiwaP2::FShotHit& Hit)
    {
        return Hit.BoxId == TEXT("T.d3");
    });
    if (ProjectileActor)
    {
        AttemptActors.Remove(ProjectileActor);
        ProjectileActor->Destroy();
        ProjectileActor = nullptr;
    }
    ShotsFired = FMath::Max(0, ShotsFired - 1);
    bFreeRetryAvailable = false;
    bHasShotStartSnapshot = false;
    ShotStartScene = NaiwaP2::FScene();
    ShotStartAvailableEggs.Reset();
    ShotStartStructureTransforms.Reset();
    ShotStartGoalTransforms.Reset();
    CurrentShot = NaiwaP2::FShotResult();
    StateIndex = 0;
    StepAccumulator = 0.0;
    ResolveDelay = 0.0;
    CollapseActorIds.Reset();
    CollapseStartLocations.Reset();
    CollapseTargetLocations.Reset();
    CollapseStartScales.Reset();
    CollapseTargetScales.Reset();
    CollapseStartBoxes.Reset();
    CollapseTargetBoxes.Reset();
    StructureCollapseElapsed = 0.0;
    bStructureCollapseAnimating = false;
    bShotStructureEffectsApplied = false;
    bHasAimPreview = false;
    ResetPouchLocation();
    SetPhase(ENaiwaP2PlayState::Aiming);
    ShowActionNotice(TEXT("FREE RETRY  -  NO NEW TARGETS"), 2.2);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_REFUND no_new_goals=1 shot=%d t_d3_hit=%d structure_restored=%d shots=%d entitlement=%d"),
        ResolvedShotNumber, bTDeckWasHit, bAllStructureActorsRestored, ShotsFired, bFreeRetryAvailable);
    return true;
}

void ANaiwaP2GameMode::StartStructureCollapse()
{
    CollapseActorIds.Reset();
    CollapseStartLocations.Reset();
    CollapseTargetLocations.Reset();
    CollapseStartScales.Reset();
    CollapseTargetScales.Reset();
    CollapseStartBoxes.Reset();
    CollapseTargetBoxes.Reset();

    TSet<FString> CompactIds;
    TMap<FString, FVector2D> TranslateById;
    TMap<FString, double> CompactHeightById;

    const auto AddCompact = [&CompactIds, &CompactHeightById](const FString& Id, const double Height)
    {
        CompactIds.Add(Id);
        CompactHeightById.Add(Id, Height);
    };
    const auto AddTranslate = [&TranslateById](const FString& Id, const double DeltaX, const double DeltaZ)
    {
        FVector2D& Delta = TranslateById.FindOrAdd(Id);
        Delta.X += DeltaX;
        Delta.Y += DeltaZ;
    };

    for (const NaiwaP2::FShotHit& Hit : CurrentShot.Hits)
    {
        if (Hit.bGoal)
        {
            continue;
        }

        const NaiwaP2::FBox2* HitBox = CurrentScene.Boxes.FindByPredicate([&Hit](const NaiwaP2::FBox2& Box)
        {
            return Box.Id == Hit.BoxId;
        });
        if (!HitBox || !HitBox->bBreakable)
        {
            continue;
        }

        const FString& Id = Hit.BoxId;
        if (Id == TEXT("P.col"))
        {
            AddCompact(Id, 0.32);
        }
        else if (Id == TEXT("T.c5") || Id == TEXT("T.c6"))
        {
            const double FallSide = Id == TEXT("T.c5") ? -1.0 : 1.0;
            AddCompact(Id, 0.30);
            AddTranslate(Id, FallSide * 0.36, 0.0);
            AddTranslate(TEXT("T.d3"), FallSide * 0.34, -1.48);
            AddTranslate(TEXT("T.post"), FallSide * 0.34, -1.48);
            AddTranslate(TEXT("T.cap"), FallSide * 0.34, -1.48);
        }
        else if (Id == TEXT("T.c2") || Id == TEXT("T.c3"))
        {
            const double FallSide = Id == TEXT("T.c2") ? -1.0 : 1.0;
            AddCompact(Id, 0.32);
            AddTranslate(Id, FallSide * 0.28, 0.0);
            AddTranslate(TEXT("T.d2"), FallSide * 0.24, -0.82);
            AddTranslate(TEXT("T.c5"), FallSide * 0.24, -0.82);
            AddTranslate(TEXT("T.c6"), FallSide * 0.24, -0.82);
            AddTranslate(TEXT("T.d3"), FallSide * 0.24, -0.82);
            AddTranslate(TEXT("T.post"), FallSide * 0.24, -0.82);
            AddTranslate(TEXT("T.cap"), FallSide * 0.24, -0.82);
        }
        else if (Id == TEXT("T.d2"))
        {
            AddCompact(Id, 0.24);
            AddTranslate(TEXT("T.c5"), -0.16, -0.58);
            AddTranslate(TEXT("T.c6"), 0.16, -0.58);
            AddTranslate(TEXT("T.d3"), 0.0, -0.58);
            AddTranslate(TEXT("T.post"), 0.0, -0.58);
            AddTranslate(TEXT("T.cap"), 0.0, -0.58);
        }
        else if (Id == TEXT("T.d3"))
        {
            AddCompact(Id, 0.22);
            AddTranslate(TEXT("T.post"), 0.0, -0.55);
            AddTranslate(TEXT("T.cap"), 0.0, -0.55);
        }
        else if (Id == TEXT("T.post"))
        {
            AddCompact(Id, 0.32);
            AddTranslate(TEXT("T.cap"), 0.0, -0.50);
        }
        else
        {
            AddCompact(Id, 0.34);
        }
    }

    TSet<FString> AffectedIds = CompactIds;
    for (const TPair<FString, FVector2D>& Pair : TranslateById)
    {
        AffectedIds.Add(Pair.Key);
    }

    for (const FString& Id : AffectedIds)
    {
        TObjectPtr<AActor>* ActorEntry = StructureActors.Find(Id);
        NaiwaP2::FBox2* Box = CurrentScene.Boxes.FindByPredicate([&Id](NaiwaP2::FBox2& Candidate)
        {
            return Candidate.Id == Id;
        });
        if (!ActorEntry || !IsValid(ActorEntry->Get()) || !Box)
        {
            continue;
        }

        NaiwaP2::FBox2 TargetBox = *Box;
        if (const double* CompactHeight = CompactHeightById.Find(Id))
        {
            TargetBox.MaxZ = TargetBox.MinZ + *CompactHeight;
            TargetBox.bBreakable = false;
            TargetBox.Kills.Reset();
        }
        FVector2D Translation = FVector2D::ZeroVector;
        if (const FVector2D* FoundTranslation = TranslateById.Find(Id))
        {
            Translation = *FoundTranslation;
        }
        TargetBox.MinX += Translation.X;
        TargetBox.MaxX += Translation.X;
        TargetBox.MinZ += Translation.Y;
        TargetBox.MaxZ += Translation.Y;

        AActor* Actor = ActorEntry->Get();
        const FVector StartLocation = Actor->GetActorLocation();
        const FVector TargetLocation = ToWorld((TargetBox.MinX + TargetBox.MaxX) * 0.5,
            (TargetBox.MinZ + TargetBox.MaxZ) * 0.5);
        const FVector StartScale = Actor->GetActorScale3D();
        FVector TargetScale = StartScale;
        TargetScale.X = TargetBox.MaxX - TargetBox.MinX;
        TargetScale.Z = TargetBox.MaxZ - TargetBox.MinZ;

        CollapseActorIds.Add(Id);
        CollapseStartLocations.Add(Id, StartLocation);
        CollapseTargetLocations.Add(Id, TargetLocation);
        CollapseStartScales.Add(Id, StartScale);
        CollapseTargetScales.Add(Id, TargetScale);
        CollapseStartBoxes.Add(Id, *Box);
        CollapseTargetBoxes.Add(Id, TargetBox);
    }

    if (CollapseActorIds.IsEmpty())
    {
        return;
    }

    StructureCollapseElapsed = 0.0;
    bStructureCollapseAnimating = true;
    ShowActionNotice(TEXT("STRUCTURE COLLAPSING  -  DEBRIS REMAINS"), StructureCollapseDuration + 0.8);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_STRUCTURE_COLLAPSE_START shot=%d modules=%d"),
        ShotsFired, CollapseActorIds.Num());
}

void ANaiwaP2GameMode::UpdateStructureCollapse(const double DeltaSeconds)
{
    StructureCollapseElapsed = FMath::Min(StructureCollapseDuration,
        StructureCollapseElapsed + FMath::Max(0.0, DeltaSeconds));
    const double LinearProgress = StructureCollapseDuration > 0.0
        ? StructureCollapseElapsed / StructureCollapseDuration : 1.0;
    const double Progress = 1.0 - FMath::Pow(1.0 - LinearProgress, 2.4);

    for (const FString& Id : CollapseActorIds)
    {
        TObjectPtr<AActor>* ActorEntry = StructureActors.Find(Id);
        const FVector* StartLocation = CollapseStartLocations.Find(Id);
        const FVector* TargetLocation = CollapseTargetLocations.Find(Id);
        const FVector* StartScale = CollapseStartScales.Find(Id);
        const FVector* TargetScale = CollapseTargetScales.Find(Id);
        const NaiwaP2::FBox2* StartBox = CollapseStartBoxes.Find(Id);
        const NaiwaP2::FBox2* TargetBox = CollapseTargetBoxes.Find(Id);
        if (!ActorEntry || !IsValid(ActorEntry->Get()) || !StartLocation || !TargetLocation ||
            !StartScale || !TargetScale || !StartBox || !TargetBox)
        {
            continue;
        }

        AActor* Actor = ActorEntry->Get();
        Actor->SetActorLocation(FMath::Lerp(*StartLocation, *TargetLocation, Progress));
        Actor->SetActorScale3D(FMath::Lerp(*StartScale, *TargetScale, Progress));
        if (NaiwaP2::FBox2* Box = CurrentScene.Boxes.FindByPredicate([&Id](NaiwaP2::FBox2& Candidate)
            { return Candidate.Id == Id; }))
        {
            Box->MinX = FMath::Lerp(StartBox->MinX, TargetBox->MinX, Progress);
            Box->MaxX = FMath::Lerp(StartBox->MaxX, TargetBox->MaxX, Progress);
            Box->MinZ = FMath::Lerp(StartBox->MinZ, TargetBox->MinZ, Progress);
            Box->MaxZ = FMath::Lerp(StartBox->MaxZ, TargetBox->MaxZ, Progress);
            if (Progress >= 1.0)
            {
                Box->bBreakable = false;
                Box->Kills.Reset();
            }
        }
    }

    if (LinearProgress >= 1.0)
    {
        bStructureCollapseAnimating = false;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_STRUCTURE_COLLAPSE_DONE shot=%d modules=%d debris_collision=retained"),
            ShotsFired, CollapseActorIds.Num());
    }
}

void ANaiwaP2GameMode::ApplyP2WorldGravity()
{
    // 这里的 CentimetersPerMeter 不写 using namespace：本文件的匿名命名空间里也有同名常量，
    // 两者数值相同（100.0），同时可见会触发 C2872 二义性。下面直接用 NaiwaP2::Physics 全限定。
    if (!NaiwaP2::Physics::bApplyWorldGravity)
    {
        return;
    }
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }
    AWorldSettings* WorldSettings = World->GetWorldSettings();
    if (WorldSettings == nullptr)
    {
        return;
    }

    // Chaos 每帧都会重新取一次重力（PhysLevel.cpp 里注释就是 "Update gravity in case it changed"），
    // 所以运行期改这两个字段从下一帧开始生效，不需要重启世界。
    //
    // 为什么不用 per-body gravity scale：UE 5.8 的 FBodyInstance 只有
    // SetEnableGravity / SetGravityGroupIndex，没有逐刚体重力倍率。
    // 而弹丸是手写积分、用的是 15.68 m/s²，刚体走 Chaos、用的是世界重力 ——
    // 两边必须是同一个数，否则蛋和建筑块的下落速度肉眼可见地对不上。
    //
    // P2 独占一张关卡，P1/P0 各自的世界不受影响。
    WorldSettings->bGlobalGravitySet = true;
    WorldSettings->GlobalGravityZ =
        static_cast<float>(-NaiwaP2::Physics::GravityMPerS2 * CentimetersPerMeter);

    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_GRAVITY z_cm_s2=%.1f gravity_m_s2=%.2f"),
        WorldSettings->GlobalGravityZ, NaiwaP2::Physics::GravityMPerS2);
}

void ANaiwaP2GameMode::WakeAllStructureBlocks()
{
    for (ANaiwaP2StructureBlock* Block : StructureBlocks)
    {
        if (IsValid(Block))
        {
            Block->WakeBlock();
        }
    }
}

void ANaiwaP2GameMode::UpdatePhysicsStabilityProbe(const double DeltaSeconds)
{
    using namespace NaiwaP2::Physics;

    if (!bRunPhysicsStabilityTest || bPhysicsStabilityTestDone)
    {
        return;
    }

    PhysicsStabilityElapsed += DeltaSeconds;

    for (ANaiwaP2StructureBlock* Block : StructureBlocks)
    {
        if (!IsValid(Block) || !Block->IsSimulating())
        {
            continue;
        }
        const double Drift = Block->GetDriftCm();
        if (Drift > PhysicsStabilityMaxDriftCm)
        {
            PhysicsStabilityMaxDriftCm = Drift;
            PhysicsStabilityWorstBlockId = Block->GetBlockId();
        }
        PhysicsStabilityMaxSpeedCmPerS =
            FMath::Max(PhysicsStabilityMaxSpeedCmPerS, Block->GetSpeedCmPerS());
        PhysicsStabilityMaxLeanDeg =
            FMath::Max(PhysicsStabilityMaxLeanDeg, Block->GetLeanDegrees());
    }
    for (const TPair<FString, TObjectPtr<AActor>>& Pair : GoalActors)
    {
        AActor* GoalActor = Pair.Value.Get();
        UPrimitiveComponent* GoalBody = GetPhysicsGoalBody(GoalActor);
        const FTransform* StartTransform = PhysicsStabilityStartGoalTransforms.Find(Pair.Key);
        if (!GoalBody || !StartTransform)
        {
            continue;
        }
        const double Drift = FVector::Dist(GoalActor->GetActorLocation(), StartTransform->GetLocation());
        if (Drift > PhysicsStabilityMaxDriftCm)
        {
            PhysicsStabilityMaxDriftCm = Drift;
            PhysicsStabilityWorstBlockId = FString::Printf(TEXT("goal:%s"), *Pair.Key);
        }
        PhysicsStabilityMaxSpeedCmPerS = FMath::Max(PhysicsStabilityMaxSpeedCmPerS,
            GoalBody->GetPhysicsLinearVelocity().Size());
        const double RotationDeltaDeg = FMath::RadiansToDegrees(
            GoalActor->GetActorQuat().AngularDistance(StartTransform->GetRotation()));
        PhysicsStabilityMaxLeanDeg = FMath::Max(PhysicsStabilityMaxLeanDeg, RotationDeltaDeg);
    }

    if (PhysicsStabilityElapsed < StabilityTestSeconds)
    {
        return;
    }

    bPhysicsStabilityTestDone = true;

    // 逐块明细：卡住的时候要能一眼看出是哪一块在漂，而不是只知道"最大位移 3 cm"。
    for (ANaiwaP2StructureBlock* Block : StructureBlocks)
    {
        if (!IsValid(Block))
        {
            continue;
        }
        const FVector Drift = Block->GetDriftVectorCm();
        // 倾角：tilt_deg = up 轴与世界 up 的夹角；lean_deg = 绕 Y 的偏航角。
        // 只报位移无法区分"被挤开（平移）"与"被压弯（旋转）"，而两者修法相反。
        // 下面两个角度必须分开看，而且 tilt_deg 在 EDOFMode::XZPlane 下**恒为 0**：
        //   该约束锁掉了绕 X/Z 的旋转，只允许绕 Y 转，而绕 Y 转不改变 up 轴。
        //   所以 tilt_deg 只能发现「约束失效」，看不见「柱子歪了」。
        //   上轮判断 2-2 时读到的 0.00 就是这种假阴性。
        // lean_deg（绕 Y 的偏航角绝对值）才是侧视平面内的「歪」：
        //   柱被压歪、板被撑斜，全都落在这个数上。
        const double LeanDegrees = Block->GetLeanDegrees();
        const double TiltDegrees = Block->GetBlockMesh()
            ? static_cast<double>(FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
                static_cast<double>(Block->GetBlockMesh()->GetComponentQuat().GetUpVector().Z),
                -1.0, 1.0))))
            : 0.0;
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_STABILITY_BLOCK id=%s sim=%d drift_cm=%.3f dx=%.3f dy=%.3f dz=%.3f "
                 "lean_deg=%.3f tilt_deg=%.3f speed_cm_s=%.2f mass_kg=%.2f"),
            *Block->GetBlockId(), Block->IsSimulating() ? 1 : 0,
            Drift.Size(), Drift.X, Drift.Y, Drift.Z, LeanDegrees, TiltDegrees,
            Block->GetSpeedCmPerS(), Block->GetMassKg());
    }
    for (const TPair<FString, TObjectPtr<AActor>>& Pair : GoalActors)
    {
        AActor* GoalActor = Pair.Value.Get();
        UPrimitiveComponent* GoalBody = GetPhysicsGoalBody(GoalActor);
        const FTransform* StartTransform = PhysicsStabilityStartGoalTransforms.Find(Pair.Key);
        if (!GoalBody || !StartTransform)
        {
            continue;
        }
        const double Drift = FVector::Dist(GoalActor->GetActorLocation(), StartTransform->GetLocation());
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_STABILITY_GOAL id=%s actor=%s collision=%d sim=%d drift_cm=%.3f speed_cm_s=%.2f mass_kg=%.2f"),
            *Pair.Key, *GoalActor->GetName(), static_cast<int32>(GoalBody->GetCollisionEnabled()),
            GoalBody->IsSimulatingPhysics() ? 1 : 0, Drift,
            GoalBody->GetPhysicsLinearVelocity().Size(),
            GoalBody->GetBodyInstance() ? GoalBody->GetBodyInstance()->GetBodyMass() : -1.0);
    }

    const bool bPass = PhysicsStabilityMaxDriftCm <= StabilityMaxDriftCm;
    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P2_STABILITY pass=%d drift_max_cm=%.3f limit_cm=%.1f speed_max_cm_s=%.2f "
             "lean_max_deg=%.3f worst=%s blocks=%d goals=%d seconds=%.2f gravity_m_s2=%.2f"),
        bPass ? 1 : 0, PhysicsStabilityMaxDriftCm, StabilityMaxDriftCm,
        PhysicsStabilityMaxSpeedCmPerS, PhysicsStabilityMaxLeanDeg,
        PhysicsStabilityWorstBlockId.IsEmpty() ? TEXT("None") : *PhysicsStabilityWorstBlockId,
        StructureBlocks.Num(), PhysicsStabilityStartGoalTransforms.Num(), PhysicsStabilityElapsed, GravityMPerS2);

    // A stability probe launched inside PIE must leave the editor alive so the
    // result can be inspected and another run can be started. Standalone game
    // processes exit with a status for the test runner.
    const UWorld* World = GetWorld();
    if (World && World->WorldType == EWorldType::PIE)
    {
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_STABILITY_EDITOR_PIE complete=1 editor_remains_open=1"));
        return;
    }

    FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1,
        TEXT("Naiwa P2 physics stability completed"));
}

void ANaiwaP2GameMode::StartPhysicsStabilityProbe()
{
    bRunPhysicsStabilityTest = true;
    bPhysicsStabilityTestDone = false;
    PhysicsStabilityElapsed = 0.0;
    PhysicsStabilityMaxDriftCm = 0.0;
    PhysicsStabilityMaxSpeedCmPerS = 0.0;
    PhysicsStabilityMaxLeanDeg = 0.0;
    PhysicsStabilityWorstBlockId.Reset();
    PhysicsStabilityStartGoalTransforms.Reset();
    for (const TPair<FString, TObjectPtr<AActor>>& Pair : GoalActors)
    {
        if (IsValid(Pair.Value.Get()))
        {
            PhysicsStabilityStartGoalTransforms.Add(Pair.Key, Pair.Value->GetActorTransform());
        }
    }

    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P2_STABILITY_START accepted=1 seconds=%.2f blocks=%d"),
        NaiwaP2::Physics::StabilityTestSeconds, StructureBlocks.Num());
}

void ANaiwaP2GameMode::UpdatePhysicsSandboxShot(const double DeltaSeconds)
{
    using namespace NaiwaP2::Physics;

    PhysicsSandboxShotElapsed += FMath::Max(0.0, DeltaSeconds);
    ANaiwaP2PhysicsEgg* Egg = Cast<ANaiwaP2PhysicsEgg>(ProjectileActor);
    const int32 ProbeTargetImpactCount = IsValid(Egg) && bRunPhysicsImpactProbe
        ? Egg->GetStructureImpactCount(PhysicsImpactProbeTargetBlockId)
        : 0;
    const double ProbeTargetMaxOutwardReboundSpeed = IsValid(Egg) && bRunPhysicsImpactProbe
        ? Egg->GetMaxOutwardReboundSpeedCmPerS(PhysicsImpactProbeTargetBlockId)
        : 0.0;
    double ProbeTargetDisplacementCm = -1.0;
    double ProbeTargetYawDegrees = -1.0;
    USphereComponent* Collider = Egg ? Egg->GetCollisionSphere() : nullptr;
    if (!IsValid(Egg) || !Collider || !Collider->IsSimulatingPhysics())
    {
        UE_LOG(LogTemp, Error, TEXT("NAIWA_P2_PHYS_SANDBOX_END reason=projectile_missing"));
        PhysicsSandboxSlowElapsed = SandboxShotSettleSeconds;
    }
    else
    {
        const FVector Position = Collider->GetComponentLocation();
        const FVector Velocity = Collider->GetPhysicsLinearVelocity();
        const double Speed = Velocity.Size();
        const double RadiusCm = NaiwaP2::BaseProjectileRadiusM * NaiwaP2::Physics::CentimetersPerMeter;
        if (Egg->GetCollisionCount() > 0 && Speed <= SandboxShotSettleSpeedCmPerS)
        {
            PhysicsSandboxSlowElapsed += FMath::Max(0.0, DeltaSeconds);
        }
        else
        {
            PhysicsSandboxSlowElapsed = 0.0;
        }

        const double MaxXcm = (CurrentScene.MaxX + SandboxWorldMarginMeters) *
            NaiwaP2::Physics::CentimetersPerMeter;
        const bool bOutOfBounds = Position.X < -SandboxWorldMarginMeters *
                NaiwaP2::Physics::CentimetersPerMeter ||
            Position.X > MaxXcm || Position.Z < -SandboxWorldMarginMeters *
                NaiwaP2::Physics::CentimetersPerMeter;
        const bool bGroundedAndSlow = Position.Z <= RadiusCm + 20.0 &&
            Speed <= SandboxShotSettleSpeedCmPerS && Egg->GetCollisionCount() > 0;
        if (bGroundedAndSlow)
        {
            PhysicsSandboxSlowElapsed = FMath::Max(PhysicsSandboxSlowElapsed, SandboxShotSettleSeconds);
        }

        if (!bOutOfBounds && PhysicsSandboxShotElapsed < SandboxShotMaxSeconds &&
            PhysicsSandboxSlowElapsed < SandboxShotSettleSeconds)
        {
            return;
        }

        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_PHYS_SANDBOX_END reason=%s elapsed_s=%.2f impacts=%d pos_cm=(%.1f,%.1f,%.1f) "
                 "speed_cm_s=%.1f"),
            bOutOfBounds ? TEXT("out_of_bounds") :
                (PhysicsSandboxShotElapsed >= SandboxShotMaxSeconds ? TEXT("time_limit") : TEXT("settled")),
            PhysicsSandboxShotElapsed, Egg->GetCollisionCount(), Position.X, Position.Y, Position.Z, Speed);
    }

    for (const TPair<FString, TObjectPtr<AActor>>& Pair : StructureActors)
    {
        AActor* StructureActor = Pair.Value.Get();
        if (!IsValid(StructureActor))
        {
            continue;
        }

        const FTransform* StartTransform = ShotStartStructureTransforms.Find(Pair.Key);
        const FVector Displacement = StartTransform
            ? StructureActor->GetActorLocation() - StartTransform->GetLocation()
            : FVector::ZeroVector;
        const FRotator RotationDelta = StartTransform
            ? (StructureActor->GetActorRotation() - StartTransform->GetRotation().Rotator()).GetNormalized()
            : FRotator::ZeroRotator;
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_PHYS_SANDBOX_BLOCK_FINAL id=%s displacement_cm=(%.1f,%.1f,%.1f) "
                 "rotation_deg=(%.1f,%.1f,%.1f)"),
            *Pair.Key, Displacement.X, Displacement.Y, Displacement.Z,
            RotationDelta.Pitch, RotationDelta.Yaw, RotationDelta.Roll);
        if (bRunPhysicsImpactProbe && Pair.Key == PhysicsImpactProbeTargetBlockId)
        {
            ProbeTargetDisplacementCm = Displacement.Size();
            ProbeTargetYawDegrees = FMath::Abs(RotationDelta.Yaw);
        }
    }

    if (ProjectileActor)
    {
        AttemptActors.Remove(ProjectileActor);
        ProjectileActor->Destroy();
        ProjectileActor = nullptr;
    }
    CurrentShot = NaiwaP2::FShotResult();
    PhysicsSandboxShotElapsed = 0.0;
    PhysicsSandboxSlowElapsed = 0.0;
    if (PouchActor)
    {
        PouchActor->SetActorHiddenInGame(false);
    }
    ResetPouchLocation();

    if (ShotsFired >= AmmoRule.TotalShots)
    {
        SetPhase(ENaiwaP2PlayState::Failed);
        ShowActionNotice(TEXT("TEST SHOTS USED  -  PRESS R TO RESET"), 3.0);
    }
    else
    {
        SetPhase(ENaiwaP2PlayState::Aiming);
        ShowActionNotice(TEXT("PHYSICS SETTLED  -  DRAG TO FIRE AGAIN"), 2.0);
    }

    if (bRunPhysicsImpactProbe)
    {
        const ANaiwaP2StructureBlock* TargetBlock = FindStructureBlock(PhysicsImpactProbeTargetBlockId);
        const bool bTargetNeedsMotion = IsValid(TargetBlock) && TargetBlock->IsSimulating();
        const bool bTargetMoved = ProbeTargetDisplacementCm >= 5.0 || ProbeTargetYawDegrees >= 5.0;
        const bool bMotionPassed = !bTargetNeedsMotion || bTargetMoved;
        const bool bPassed = ProbeTargetImpactCount > 0 &&
            ProbeTargetMaxOutwardReboundSpeed > 0.0 && bMotionPassed;
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_PHYS_IMPACT_PROBE pass=%d target=%s impacts=%d outward_rebound_cm_s=%.1f "
                 "displacement_cm=%.2f yaw_deg=%.2f motion_required=%d move_limit_cm=5.0 yaw_limit_deg=5.0"),
            bPassed ? 1 : 0, *PhysicsImpactProbeTargetBlockId, ProbeTargetImpactCount,
            ProbeTargetMaxOutwardReboundSpeed, ProbeTargetDisplacementCm, ProbeTargetYawDegrees,
            bTargetNeedsMotion ? 1 : 0);
        FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1,
            TEXT("Naiwa P2 physics impact probe completed"));
    }
}

void ANaiwaP2GameMode::HandlePhysicsGoalHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, const FVector NormalImpulse, const FHitResult& Hit)
{
    using namespace NaiwaP2::Physics;
    if (!bRunPhysicsFullLevel || !HitComponent ||
        (PlayState != ENaiwaP2PlayState::Flying && PlayState != ENaiwaP2PlayState::Resolving))
    {
        return;
    }

    AActor* GoalActor = HitComponent->GetOwner();
    FString GoalId;
    for (const TPair<FString, TObjectPtr<AActor>>& Pair : GoalActors)
    {
        if (Pair.Value.Get() == GoalActor)
        {
            GoalId = Pair.Key;
            break;
        }
    }
    if (GoalId.IsEmpty() || CurrentShot.RawKilled.Contains(GoalId))
    {
        return;
    }

    const FVector GoalVelocity = HitComponent->GetPhysicsLinearVelocity();
    const FVector OtherVelocity = OtherComp ? OtherComp->GetPhysicsLinearVelocity() : FVector::ZeroVector;
    const double RelativeSpeed = (GoalVelocity - OtherVelocity).Size();
    const double Impulse = NormalImpulse.Size();
    if (Impulse < GoalHitMinImpulseKgCmPerS || RelativeSpeed < GoalHitMinRelativeSpeedCmPerS)
    {
        return;
    }

    NaiwaP2::FShotHit GoalHit;
    GoalHit.BoxId = GoalId;
    GoalHit.X = Hit.ImpactPoint.X / NaiwaP2::Physics::CentimetersPerMeter;
    GoalHit.Z = Hit.ImpactPoint.Z / NaiwaP2::Physics::CentimetersPerMeter;
    GoalHit.Tick = StateIndex;
    GoalHit.bGoal = true;
    CurrentShot.Hits.Add(GoalHit);
    CurrentShot.RawKilled.Add(GoalId);
    PlayGoalHitSound();

    if (UWorld* World = GetWorld())
    {
        if (UNaiwaP2ImpactEffectSubsystem* ImpactEffects =
            World->GetSubsystem<UNaiwaP2ImpactEffectSubsystem>())
        {
            ImpactEffects->SpawnImpact(GoalActor, OtherActor, Hit.ImpactPoint, Hit.ImpactNormal,
                Impulse, RelativeSpeed);
        }
    }

    GoalActors.Remove(GoalId);
    CurrentScene.GoalIds.Remove(GoalId);
    CurrentScene.Boxes.RemoveAll([&GoalId](const NaiwaP2::FBox2& Box)
    {
        return Box.bGoal && Box.Id == GoalId;
    });
    GoalActor->Destroy();

    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P2_PHYS_GOAL_HIT shot=%d id=%s other=%s point_cm=(%.1f,%.1f,%.1f) "
             "relative_speed_cm_s=%.1f impulse_kg_cm_s=%.1f remaining=%d"),
        ShotsFired, *GoalId, *GetNameSafe(OtherActor), Hit.ImpactPoint.X, Hit.ImpactPoint.Y,
        Hit.ImpactPoint.Z, RelativeSpeed, Impulse, GoalActors.Num());
    ShowActionNotice(FString::Printf(TEXT("TARGET HIT  -  %d LEFT"), GoalActors.Num()), 1.0);
}

void ANaiwaP2GameMode::ApplyPhysicsTeaBlast(ANaiwaP2PhysicsEgg* Egg)
{
    if (bPhysicsLevelTeaBlastApplied || !IsValid(Egg) || ShotEgg != NaiwaP2::EEggKind::Tea ||
        !Egg->GetCollisionSphere())
    {
        return;
    }

    bPhysicsLevelTeaBlastApplied = true;
    ApplyEggActorAppearance(Egg, NaiwaP2::EEggKind::Tea, 1.0, true);
    const FVector BlastCenter = Egg->GetCollisionSphere()->GetComponentLocation();
    const double BlastRadiusCm = NaiwaP2::TeaBlastRadiusM * CentimetersPerMeter;
    constexpr double CenterVelocityChangeCmPerS = 500.0;
    int32 AffectedStructureCount = 0;

    const auto DistanceInPhysicsPlane = [&BlastCenter](const FVector& Point)
    {
        const double DX = Point.X - BlastCenter.X;
        const double DZ = Point.Z - BlastCenter.Z;
        return FMath::Sqrt(DX * DX + DZ * DZ);
    };
    const auto ApplyRadialImpulse = [&](UPrimitiveComponent* Body,
        const double DistanceCm)
    {
        if (!Body || !Body->IsSimulatingPhysics() || Body->GetMass() <= 0.0)
        {
            return;
        }
        FVector Direction = Body->GetComponentLocation() - BlastCenter;
        Direction.Y = 0.0;
        if (Direction.IsNearlyZero())
        {
            Direction = FVector::UpVector;
        }
        else
        {
            Direction.Normalize();
        }
        const double Falloff = FMath::Clamp(1.0 - DistanceCm / BlastRadiusCm, 0.0, 1.0);
        const FVector Impulse = Direction * Body->GetMass() * CenterVelocityChangeCmPerS * Falloff;
        Body->AddImpulse(Impulse, NAME_None, false);
    };

    for (ANaiwaP2StructureBlock* Block : StructureBlocks)
    {
        if (!IsValid(Block) || !Block->IsSimulating() || !Block->GetBlockMesh())
        {
            continue;
        }
        const double DistanceCm = DistanceInPhysicsPlane(Block->GetBlockMesh()->GetComponentLocation());
        if (DistanceCm <= BlastRadiusCm)
        {
            ApplyRadialImpulse(Block->GetBlockMesh(), DistanceCm);
            ++AffectedStructureCount;
        }
    }

    TArray<FString> GoalsInBlast;
    for (const TPair<FString, TObjectPtr<AActor>>& Pair : GoalActors)
    {
        UPrimitiveComponent* GoalBody = GetPhysicsGoalBody(Pair.Value.Get());
        if (!IsValid(Pair.Value.Get()) || !GoalBody)
        {
            continue;
        }
        const double DistanceCm = DistanceInPhysicsPlane(GoalBody->GetComponentLocation());
        if (DistanceCm <= BlastRadiusCm)
        {
            ApplyRadialImpulse(GoalBody, DistanceCm);
            GoalsInBlast.Add(Pair.Key);
        }
    }

    for (const FString& GoalId : GoalsInBlast)
    {
        TObjectPtr<AActor>* GoalActor = GoalActors.Find(GoalId);
        if (!GoalActor || !IsValid(GoalActor->Get()))
        {
            continue;
        }
        const FVector GoalLocation = GetPhysicsGoalBody(GoalActor->Get())->GetComponentLocation();
        NaiwaP2::FShotHit GoalHit;
        GoalHit.BoxId = GoalId;
        GoalHit.X = GoalLocation.X / CentimetersPerMeter;
        GoalHit.Z = GoalLocation.Z / CentimetersPerMeter;
        GoalHit.Tick = StateIndex;
        GoalHit.bGoal = true;
        CurrentShot.Hits.Add(GoalHit);
        CurrentShot.RawKilled.AddUnique(GoalId);
        PlayGoalHitSound();
        GoalActor->Get()->Destroy();
        GoalActors.Remove(GoalId);
        CurrentScene.GoalIds.Remove(GoalId);
        CurrentScene.Boxes.RemoveAll([&GoalId](const NaiwaP2::FBox2& Box)
        {
            return Box.bGoal && Box.Id == GoalId;
        });
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_PHYS_TEA_BLAST_GOAL shot=%d id=%s radius_cm=%.1f"),
            ShotsFired, *GoalId, BlastRadiusCm);
    }

    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P2_PHYS_TEA_BLAST shot=%d radius_cm=%.1f structures_impulsed=%d "
             "goals_removed=%d center_cm=(%.1f,%.1f,%.1f) delta_v_center_cm_s=%.1f"),
        ShotsFired, BlastRadiusCm, AffectedStructureCount, GoalsInBlast.Num(),
        BlastCenter.X, BlastCenter.Y, BlastCenter.Z, CenterVelocityChangeCmPerS);
    if (bRunPhysicsLevelSkillProbe && PhysicsLevelSkillProbeEgg == NaiwaP2::EEggKind::Tea)
    {
        bPhysicsLevelSkillProbeEffectObserved = true;
    }
}

void ANaiwaP2GameMode::UpdatePhysicsLevelShot(const double DeltaSeconds)
{
    using namespace NaiwaP2::Physics;
    PhysicsLevelShotElapsed += FMath::Max(0.0, DeltaSeconds);

    double SceneMaxZ = 0.0;
    for (const NaiwaP2::FBox2& Box : CurrentScene.Boxes)
    {
        SceneMaxZ = FMath::Max(SceneMaxZ, Box.MaxZ);
    }
    const double WorldMarginCm = SandboxWorldMarginMeters * NaiwaP2::Physics::CentimetersPerMeter;
    const double MinXcm = -WorldMarginCm;
    const double MaxXcm = (CurrentScene.MaxX + SandboxWorldMarginMeters) *
        NaiwaP2::Physics::CentimetersPerMeter;
    const double MinZcm = -WorldMarginCm;
    const double MaxZcm = (SceneMaxZ + SandboxWorldMarginMeters) *
        NaiwaP2::Physics::CentimetersPerMeter;

    // Debris can leave the playable bounds after a real collision. Remove it once it is well
    // outside the scene so an off-screen fall cannot keep the shot resolving forever.
    TSet<FString> EscapedBlockIds;
    for (ANaiwaP2StructureBlock* Block : StructureBlocks)
    {
        if (!IsValid(Block) || !Block->IsSimulating())
        {
            continue;
        }
        const FVector Position = Block->GetActorLocation();
        if (Position.X < MinXcm || Position.X > MaxXcm ||
            Position.Z < MinZcm || Position.Z > MaxZcm)
        {
            const FVector Velocity = Block->GetBlockMesh()
                ? Block->GetBlockMesh()->GetPhysicsLinearVelocity() : FVector::ZeroVector;
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_PHYS_BLOCK_OUT_OF_BOUNDS shot=%d id=%s pos_cm=(%.1f,%.1f,%.1f) "
                     "speed_cm_s=%.1f removed=1"),
                ShotsFired, *Block->GetBlockId(), Position.X, Position.Y, Position.Z, Velocity.Size());
            EscapedBlockIds.Add(Block->GetBlockId());
            Block->Destroy();
            StructureActors.Remove(Block->GetBlockId());
            CurrentScene.Boxes.RemoveAll([Block](const NaiwaP2::FBox2& Box)
            {
                return !Box.bGoal && Box.Id == Block->GetBlockId();
            });
        }
    }
    if (!EscapedBlockIds.IsEmpty())
    {
        StructureBlocks.RemoveAll([&EscapedBlockIds](const TObjectPtr<ANaiwaP2StructureBlock>& Block)
        {
            return !IsValid(Block.Get()) || EscapedBlockIds.Contains(Block->GetBlockId());
        });
        AttemptActors.RemoveAll([&EscapedBlockIds](const TObjectPtr<AActor>& Actor)
        {
            const ANaiwaP2StructureBlock* Block = Cast<ANaiwaP2StructureBlock>(Actor.Get());
            return !IsValid(Actor.Get()) || (Block && EscapedBlockIds.Contains(Block->GetBlockId()));
        });
    }

    TArray<FString> EscapedGoalIds;
    for (const TPair<FString, TObjectPtr<AActor>>& Pair : GoalActors)
    {
        AActor* GoalActor = Pair.Value.Get();
        UPrimitiveComponent* GoalBody = GetPhysicsGoalBody(GoalActor);
        if (!IsValid(GoalActor) || !GoalBody)
        {
            continue;
        }
        const FVector Position = GoalBody->GetComponentLocation();
        if (Position.X < MinXcm || Position.X > MaxXcm ||
            Position.Z < MinZcm || Position.Z > MaxZcm)
        {
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_PHYS_GOAL_OUT_OF_BOUNDS shot=%d id=%s pos_cm=(%.1f,%.1f,%.1f) removed=1"),
                ShotsFired, *Pair.Key, Position.X, Position.Y, Position.Z);
            EscapedGoalIds.Add(Pair.Key);
        }
    }
    for (const FString& GoalId : EscapedGoalIds)
    {
        if (TObjectPtr<AActor>* GoalActor = GoalActors.Find(GoalId))
        {
            if (IsValid(GoalActor->Get()))
            {
                GoalActor->Get()->Destroy();
            }
        }
        GoalActors.Remove(GoalId);
        CurrentScene.GoalIds.Remove(GoalId);
        CurrentScene.Boxes.RemoveAll([&GoalId](const NaiwaP2::FBox2& Box)
        {
            return Box.bGoal && Box.Id == GoalId;
        });
        CurrentShot.RawKilled.Add(GoalId);
    }

    const bool bHadActiveProjectiles = !PhysicsLevelActiveProjectiles.IsEmpty();
    TArray<TObjectPtr<ANaiwaP2PhysicsEgg>> RemainingProjectiles;
    const TCHAR* LastProjectileResolveReason = TEXT("projectile_missing");
    for (ANaiwaP2PhysicsEgg* Egg : PhysicsLevelActiveProjectiles)
    {
        if (!IsValid(Egg))
        {
            continue;
        }

        USphereComponent* Collider = Egg->GetCollisionSphere();
        bool bEggResolved = false;
        bool bKeepLandedEgg = false;
        const TCHAR* EggResolveReason = TEXT("pending");
        if (!Collider || !Collider->IsSimulatingPhysics())
        {
            EggResolveReason = TEXT("projectile_not_simulating");
            bEggResolved = true;
        }
        else
        {
            if (ShotEgg == NaiwaP2::EEggKind::Tea && Egg->GetCollisionCount() > 0)
            {
                ApplyPhysicsTeaBlast(Egg);
            }

            const FVector Position = Collider->GetComponentLocation();
            const FVector Velocity = Collider->GetPhysicsLinearVelocity();
            const double Speed = Velocity.Size();
            // Leaving the camera at the top is not leaving the level: a high arc
            // must remain alive and come back down under gravity. Only horizontal
            // escapes and falling below the world resolve as out of bounds.
            const bool bOutOfBounds = Position.X < MinXcm || Position.X > MaxXcm ||
                Position.Z < MinZcm;
            if (bOutOfBounds)
            {
                EggResolveReason = TEXT("out_of_bounds");
                bEggResolved = true;
            }
            else if (Egg->GetCollisionCount() > 0 && Speed <= PhysicsLevelProjectileSettleSpeedCmPerS)
            {
                double& SlowElapsed = PhysicsLevelProjectileSlowElapsedByEgg.FindOrAdd(Egg);
                SlowElapsed += FMath::Max(0.0, DeltaSeconds);
                if (SlowElapsed >= SandboxShotSettleSeconds)
                {
                    EggResolveReason = TEXT("projectile_settled");
                    bEggResolved = true;
                    bKeepLandedEgg = true;
                }
            }
            else
            {
                PhysicsLevelProjectileSlowElapsedByEgg.Remove(Egg);
            }

            if (!bEggResolved && PhysicsLevelShotElapsed >= PhysicsLevelShotMaxSeconds)
            {
                EggResolveReason = TEXT("projectile_time_limit");
                bEggResolved = true;
            }
        }

        if (bEggResolved)
        {
            LastProjectileResolveReason = EggResolveReason;
            PhysicsLevelResolvedProjectileImpactCount += Egg->GetCollisionCount();
            PhysicsLevelProjectileSlowElapsedByEgg.Remove(Egg);
            if (!bKeepLandedEgg)
            {
                AttemptActors.Remove(Egg);
                PhysicsLevelShotProjectiles.Remove(Egg);
                Egg->Destroy();
            }
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_PHYS_PROJECTILE_LIFETIME actor=%s kept_in_attempt=%d reason=%s impacts=%d"),
                *GetNameSafe(Egg), bKeepLandedEgg ? 1 : 0, EggResolveReason, Egg->GetCollisionCount());
        }
        else
        {
            RemainingProjectiles.Add(Egg);
        }
    }
    PhysicsLevelActiveProjectiles = MoveTemp(RemainingProjectiles);
    const bool bProjectileResolved = PhysicsLevelActiveProjectiles.IsEmpty();
    const TCHAR* ProjectileResolveReason = bProjectileResolved
        ? (bHadActiveProjectiles ? LastProjectileResolveReason : TEXT("projectile_missing"))
        : TEXT("pending");
    PhysicsLevelShotCollisionCount = PhysicsLevelResolvedProjectileImpactCount;
    for (const ANaiwaP2PhysicsEgg* Egg : PhysicsLevelShotProjectiles)
    {
        if (IsValid(Egg))
        {
            PhysicsLevelShotCollisionCount += Egg->GetCollisionCount();
        }
    }
    ProjectileActor = PhysicsLevelActiveProjectiles.IsEmpty()
        ? nullptr : PhysicsLevelActiveProjectiles[0].Get();
    if (bProjectileResolved && PlayState == ENaiwaP2PlayState::Flying)
    {
        SetPhase(ENaiwaP2PlayState::Resolving);
        ShowActionNotice(TEXT("IMPACT COMPLETE  -  WAITING FOR STRUCTURE"), 1.2);
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_PHYS_PROJECTILE_END reason=%s elapsed_s=%.2f impacts=%d"),
            ProjectileResolveReason, PhysicsLevelShotElapsed, PhysicsLevelShotCollisionCount);
    }

    double MaxLinearSpeed = 0.0;
    double MaxAngularSpeed = 0.0;
    for (ANaiwaP2StructureBlock* Block : StructureBlocks)
    {
        if (!IsValid(Block) || !Block->IsSimulating() || !Block->GetBlockMesh())
        {
            continue;
        }
        MaxLinearSpeed = FMath::Max(MaxLinearSpeed, Block->GetBlockMesh()->GetPhysicsLinearVelocity().Size());
        MaxAngularSpeed = FMath::Max(MaxAngularSpeed,
            Block->GetBlockMesh()->GetPhysicsAngularVelocityInDegrees().Size());
    }
    for (const TPair<FString, TObjectPtr<AActor>>& Pair : GoalActors)
    {
        AActor* GoalActor = Pair.Value.Get();
        UPrimitiveComponent* GoalBody = GetPhysicsGoalBody(GoalActor);
        if (!GoalBody || !GoalBody->IsSimulatingPhysics())
        {
            continue;
        }
        MaxLinearSpeed = FMath::Max(MaxLinearSpeed, GoalBody->GetPhysicsLinearVelocity().Size());
        MaxAngularSpeed = FMath::Max(MaxAngularSpeed,
            GoalBody->GetPhysicsAngularVelocityInDegrees().Size());
    }
    PhysicsLevelMaxSceneSpeedCmPerS = FMath::Max(PhysicsLevelMaxSceneSpeedCmPerS, MaxLinearSpeed);

    const bool bSceneQuiet = MaxLinearSpeed <= PhysicsLevelSceneSettleSpeedCmPerS &&
        MaxAngularSpeed <= PhysicsLevelSceneSettleAngularSpeedDegPerS;
    if (PlayState == ENaiwaP2PlayState::Resolving && bSceneQuiet)
    {
        PhysicsLevelSceneQuietElapsed += FMath::Max(0.0, DeltaSeconds);
    }
    else
    {
        PhysicsLevelSceneQuietElapsed = 0.0;
    }

    if (bProjectileResolved && PlayState == ENaiwaP2PlayState::Resolving &&
        (PhysicsLevelSceneQuietElapsed >= PhysicsLevelSettleSeconds ||
         PhysicsLevelShotElapsed >= PhysicsLevelShotMaxSeconds))
    {
        FinishPhysicsLevelShot(PhysicsLevelShotElapsed >= PhysicsLevelShotMaxSeconds
            ? TEXT("scene_time_limit") : TEXT("scene_settled"));
    }
}

void ANaiwaP2GameMode::FinishPhysicsLevelShot(const TCHAR* ResolveReason)
{
    const int32 TargetsHit = CurrentShot.RawKilled.Num();
    const int32 RemainingTargets = GoalActors.Num();
    const int32 ShotNumber = ShotsFired;
    double MaxStructureDriftCm = 0.0;
    for (ANaiwaP2StructureBlock* Block : StructureBlocks)
    {
        if (!IsValid(Block) || !Block->IsSimulating() || !Block->GetBlockMesh())
        {
            continue;
        }
        const FVector Drift = Block->GetDriftVectorCm();
        MaxStructureDriftCm = FMath::Max(MaxStructureDriftCm, Drift.Size());
        Block->LogSupportJointStates();
        if (Drift.Size() > 1.0)
        {
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_PHYS_LEVEL_BLOCK shot=%d id=%s drift_cm=%.2f dx=%.2f dz=%.2f "
                     "lean_deg=%.2f speed_cm_s=%.1f angular_deg_s=%.1f"),
                ShotNumber, *Block->GetBlockId(), Drift.Size(), Drift.X, Drift.Z,
                Block->GetLeanDegrees(), Block->GetBlockMesh()->GetPhysicsLinearVelocity().Size(),
                Block->GetBlockMesh()->GetPhysicsAngularVelocityInDegrees().Size());
        }
    }

    // A settled physical egg remains visible until Retry() destroys all actors
    // from the current attempt. Keep it in AttemptActors for that reset cleanup.
    ProjectileActor = nullptr;
    AttemptActors.RemoveAll([](const TObjectPtr<AActor>& Actor)
    {
        return !IsValid(Actor);
    });

    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P2_PHYS_LEVEL_SHOT_RESOLVE shot=%d reason=%s impacts=%d targets_hit=%d "
             "remaining_targets=%d max_scene_speed_cm_s=%.1f max_structure_drift_cm=%.2f elapsed_s=%.2f"),
        ShotNumber, ResolveReason, PhysicsLevelShotCollisionCount, TargetsHit, RemainingTargets,
        PhysicsLevelMaxSceneSpeedCmPerS, MaxStructureDriftCm, PhysicsLevelShotElapsed);

    if (TargetsHit == 0)
    {
        PlayMissSound();
    }

    if (bRunPhysicsLevelRefundTest)
    {
        const int32 PreRefundShot = ShotsFired;
        const int32 PreRefundImpacts = PhysicsLevelShotCollisionCount;
        const int32 ExpectedGoalCount = CurrentScene.GoalIds.Num();
        const bool bSettledWithinLimit = FCString::Strcmp(ResolveReason, TEXT("scene_time_limit")) != 0;
        const bool bRefunded = TargetsHit == 0 && PreRefundImpacts > 0 && bSettledWithinLimit &&
            bFreeRetryAvailable && bHasShotStartSnapshot && IsPhysicsLevelAtShotStart() &&
            RefundZeroKillShot();
        const bool bRefundPassed = bRefunded && ShotsFired == 0 && GoalActors.Num() == ExpectedGoalCount &&
            AvailableEggs.Num() == AmmoRule.EggList.Num() && PlayState == ENaiwaP2PlayState::Aiming &&
            !bFreeRetryAvailable && !ProjectileActor && CurrentScene.GoalIds.Num() == ExpectedGoalCount;
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_PHYS_LEVEL_REFUND_TEST level=%s pass=%d shot_before=%d impacts=%d shots=%d goals=%d "
                 "tokens=%d retry=%d state=%s"),
            *CurrentLevelId, bRefundPassed ? 1 : 0, PreRefundShot, PreRefundImpacts, ShotsFired, GoalActors.Num(),
            AvailableEggs.Num(), bFreeRetryAvailable ? 1 : 0, *GetPlayStateLabel());
        FPlatformMisc::RequestExitWithStatus(false, bRefundPassed ? 0 : 1,
            TEXT("Naiwa P2 full physics refund test completed"));
        return;
    }

    if (!bRunPhysicsLevelAutoTest && TargetsHit == 0 && bFreeRetryAvailable &&
        bHasShotStartSnapshot && IsPhysicsLevelAtShotStart())
    {
        RefundZeroKillShot();
        return;
    }

    if (RemainingTargets == 0)
    {
        if (GetGameInstance() && (CurrentLevelId != TEXT("2-3") || CurrentStage == 2))
        {
            if (UNaiwaProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UNaiwaProgressSubsystem>())
            {
                Progress->RecordLevelClear(CurrentLevelId, ShotsFired,
                    static_cast<float>(LevelElapsedSeconds), LevelResetCount, 0);
            }
        }
        SetPhase(ENaiwaP2PlayState::Won);
        ScheduleCampaignAdvance();
    }
    else if (ShotsFired >= AmmoRule.TotalShots)
    {
        SetPhase(ENaiwaP2PlayState::Failed);
    }
    else
    {
        SetPhase(ENaiwaP2PlayState::Aiming);
        if (PouchActor)
        {
            PouchActor->SetActorHiddenInGame(false);
        }
        ResetPouchLocation();
    }

    bHasAimPreview = false;
    bShotStructureEffectsApplied = false;
    bHasShotStartSnapshot = false;
    ShotStartScene = NaiwaP2::FScene();
    ShotStartStructureTransforms.Reset();
    ShotStartGoalTransforms.Reset();

    if (bRunPhysicsLevelSkillProbe)
    {
        const bool bSettledWithinLimit = FCString::Strcmp(ResolveReason, TEXT("scene_time_limit")) != 0;
        const bool bImpactObserved = PhysicsLevelShotCollisionCount > 0 || TargetsHit > 0;
        const bool bPassed = bPhysicsLevelSkillProbeEffectObserved && bImpactObserved && bSettledWithinLimit;
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_PHYS_SKILL_PROBE pass=%d level=%s egg=%s impacts=%d targets_hit=%d "
                 "remaining=%d effect=%d chick_delta_vz_cm_s=%.1f mouse_children=%d tea_blast=%d "
                 "radius_cm=%.2f mass_kg=%.2f visual_scale=%.4f state=%s"),
            bPassed ? 1 : 0, *CurrentLevelId, *PhysicsLevelSkillProbeLabel,
            PhysicsLevelShotCollisionCount, TargetsHit, RemainingTargets,
            bPhysicsLevelSkillProbeEffectObserved ? 1 : 0, PhysicsLevelChickDeltaVelocityZCmPerS,
            PhysicsLevelMouseSplitSpawnedCount, bPhysicsLevelTeaBlastApplied ? 1 : 0,
            PhysicsLevelSkillProbeRadiusCm, PhysicsLevelSkillProbeMassKg,
            PhysicsLevelSkillProbeVisualScale, *GetPlayStateLabel());
        FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1,
            TEXT("Naiwa P2 full physics skill probe completed"));
        return;
    }

    if (bRunPhysicsLevelAutoTest)
    {
        const bool bSettledWithinLimit = FCString::Strcmp(ResolveReason, TEXT("scene_time_limit")) != 0;
        const bool bFrameTimePassed = LogPhysicsFrameTimeProbe();
        const bool bPhysicsShotPassed = (PhysicsLevelShotCollisionCount > 0 || TargetsHit > 0) &&
            bSettledWithinLimit;
        const bool bPassed = bPhysicsShotPassed && (!bRunPhysicsFrameTimeProbe || bFrameTimePassed);
        if (bRunPhysicsLevelResetTest)
        {
            const int32 PreResetTargetsHit = TargetsHit;
            const int32 PreResetCollisions = PhysicsLevelShotCollisionCount;
            Retry();
            const FString ResetGoalId = CurrentLevelId == TEXT("2-2") ? TEXT("E1") :
                (CurrentLevelId == TEXT("2-3") ? TEXT("T1") : TEXT("P1"));
            const FString ResetStructureId = CurrentLevelId == TEXT("2-2") ? TEXT("E.d3") : TEXT("T.cap");
            const TObjectPtr<AActor>* ResetGoal = GoalActors.Find(ResetGoalId);
            const TObjectPtr<AActor>* ResetStructure = StructureActors.Find(ResetStructureId);
            const NaiwaP2::FBox2* ResetGoalBox = CurrentScene.Boxes.FindByPredicate(
                [&ResetGoalId](const NaiwaP2::FBox2& Box) { return Box.bGoal && Box.Id == ResetGoalId; });
            const NaiwaP2::FBox2* ResetStructureBox = CurrentScene.Boxes.FindByPredicate(
                [&ResetStructureId](const NaiwaP2::FBox2& Box) { return Box.Id == ResetStructureId; });
            const int32 ExpectedGoalCount = CurrentScene.GoalIds.Num();
            const FVector ExpectedGoalLocation = ResetGoalBox
                ? ToWorld((ResetGoalBox->MinX + ResetGoalBox->MaxX) * 0.5,
                    (ResetGoalBox->MinZ + ResetGoalBox->MaxZ) * 0.5)
                : FVector::ZeroVector;
            const FVector ExpectedStructureLocation = ResetStructureBox
                ? ToWorld((ResetStructureBox->MinX + ResetStructureBox->MaxX) * 0.5,
                    (ResetStructureBox->MinZ + ResetStructureBox->MaxZ) * 0.5)
                : FVector::ZeroVector;
            const bool bResetPassed = bPassed && ShotsFired == 0 && GoalActors.Num() == ExpectedGoalCount &&
                CurrentScene.GoalIds.Num() == ExpectedGoalCount && AvailableEggs.Num() == AmmoRule.EggList.Num() &&
                PlayState == ENaiwaP2PlayState::Aiming && bFreeRetryAvailable == AmmoRule.bHasFreeRetry &&
                !ProjectileActor && ResetGoal && IsValid(ResetGoal->Get()) && ResetStructure &&
                IsValid(ResetStructure->Get()) && ResetGoalBox && ResetStructureBox &&
                FVector::Dist(ResetGoal->Get()->GetActorLocation(), ExpectedGoalLocation) < 1.0 &&
                FVector::Dist(ResetStructure->Get()->GetActorLocation(), ExpectedStructureLocation) < 1.0;
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_PHYS_LEVEL_RESET_TEST level=%s pass=%d shots=%d goals=%d tokens=%d "
                     "targets_hit_before_reset=%d impacts_before_reset=%d state=%s"),
                *CurrentLevelId, bResetPassed ? 1 : 0, ShotsFired, GoalActors.Num(), AvailableEggs.Num(),
                PreResetTargetsHit, PreResetCollisions, *GetPlayStateLabel());
            FPlatformMisc::RequestExitWithStatus(false, bResetPassed ? 0 : 1,
                TEXT("Naiwa P2 full physics reset test completed"));
            return;
        }
        if (bRunPhysicsLevelMultiShotTest)
        {
            ++PhysicsLevelMultiShotResolvedCount;
            PhysicsLevelMultiShotTotalImpacts += PhysicsLevelShotCollisionCount;
            PhysicsLevelMultiShotTotalTargetsHit += TargetsHit;
            bPhysicsLevelMultiShotPassed &= bPassed;

            if (!bPassed)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("NAIWA_P2_PHYS_LEVEL_MULTI_SHOT_TEST pass=0 resolved=%d shots=%d "
                         "targets_hit=%d remaining=%d state=%s"),
                    PhysicsLevelMultiShotResolvedCount, ShotsFired, PhysicsLevelMultiShotTotalTargetsHit,
                    GoalActors.Num(), *GetPlayStateLabel());
                FPlatformMisc::RequestExitWithStatus(false, 1,
                    TEXT("Naiwa P2 multi-shot sequence did not settle"));
                return;
            }

            if (PlayState == ENaiwaP2PlayState::Aiming && ShotsFired < AmmoRule.TotalShots)
            {
                return;
            }

            const bool bTerminalState = PlayState == ENaiwaP2PlayState::Won ||
                PlayState == ENaiwaP2PlayState::Failed;
            const bool bSequencePassed = bPhysicsLevelMultiShotPassed && bTerminalState &&
                PhysicsLevelMultiShotResolvedCount == ShotsFired &&
                (PlayState == ENaiwaP2PlayState::Won ? GoalActors.IsEmpty()
                    : ShotsFired == AmmoRule.TotalShots);
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_PHYS_LEVEL_MULTI_SHOT_TEST pass=%d resolved=%d shots=%d impacts=%d "
                     "targets_hit=%d remaining=%d state=%s"),
                bSequencePassed ? 1 : 0, PhysicsLevelMultiShotResolvedCount, ShotsFired,
                PhysicsLevelMultiShotTotalImpacts, PhysicsLevelMultiShotTotalTargetsHit,
                GoalActors.Num(), *GetPlayStateLabel());
            FPlatformMisc::RequestExitWithStatus(false, bSequencePassed ? 0 : 1,
                TEXT("Naiwa P2 multi-shot sequence completed"));
            return;
        }
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_PHYS_LEVEL_TEST pass=%d physics_pass=%d frame_time_probe=%d frame_time_pass=%d "
                 "shots=%d impacts=%d targets_hit=%d remaining_targets=%d state=%s"),
            bPassed ? 1 : 0, bPhysicsShotPassed ? 1 : 0,
            bRunPhysicsFrameTimeProbe ? 1 : 0, bFrameTimePassed ? 1 : 0,
            ShotsFired, PhysicsLevelShotCollisionCount, TargetsHit, GoalActors.Num(), *GetPlayStateLabel());
        FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1,
            TEXT("Naiwa P2 full physics level test completed"));
    }
}

bool ANaiwaP2GameMode::LogPhysicsFrameTimeProbe() const
{
    if (!bRunPhysicsFrameTimeProbe)
    {
        return true;
    }

    TArray<double> SortedIntervals = PhysicsFrameIntervalMs;
    SortedIntervals.Sort();
    TArray<double> SortedWorldDeltas = PhysicsFrameWorldDeltaMs;
    SortedWorldDeltas.Sort();

    const auto GetPercentileMs = [](const TArray<double>& SortedSamples, const double Percentile)
    {
        if (SortedSamples.IsEmpty())
        {
            return 0.0;
        }
        const int32 Index = FMath::Clamp(
            FMath::CeilToInt(Percentile * SortedSamples.Num()) - 1,
            0, SortedSamples.Num() - 1);
        return SortedSamples[Index];
    };

    int32 ViewportWidth = 0;
    int32 ViewportHeight = 0;
    if (GEngine && GEngine->GameViewport)
    {
        FVector2D ViewportSize = FVector2D::ZeroVector;
        GEngine->GameViewport->GetViewportSize(ViewportSize);
        ViewportWidth = FMath::RoundToInt(ViewportSize.X);
        ViewportHeight = FMath::RoundToInt(ViewportSize.Y);
    }

    const double P50Ms = GetPercentileMs(SortedIntervals, 0.50);
    const double P95Ms = GetPercentileMs(SortedIntervals, 0.95);
    const double P99Ms = GetPercentileMs(SortedIntervals, 0.99);
    const double P95WorldDeltaMs = GetPercentileMs(SortedWorldDeltas, 0.95);
    const double MaxMs = SortedIntervals.IsEmpty() ? 0.0 : SortedIntervals.Last();
    const IConsoleVariable* VSyncCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
    const IConsoleVariable* ScreenPercentageCVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
    const float MaxFps = GEngine ? GEngine->GetMaxFPS() : -1.0f;
    const float MaxTickRate = GEngine ? GEngine->GetMaxTickRate(0.0f, true) : -1.0f;
    const int32 VSync = VSyncCVar ? VSyncCVar->GetInt() : -1;
    const float ScreenPercentage = ScreenPercentageCVar ? ScreenPercentageCVar->GetFloat() : -1.0f;
    const bool bPassed = SortedIntervals.Num() >= 120 &&
        ViewportWidth == 1920 && ViewportHeight == 1080 && ScreenPercentage >= 99.0f &&
        P95Ms <= 16.7;

    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P2_FRAME_TIME_PROBE pass=%d samples=%d window=physics_flight_and_resolution viewport=%dx%d "
             "max_fps=%.1f max_tick_rate=%.1f vsync=%d screen_percentage=%.1f "
             "p50_tick_ms=%.3f p95_tick_ms=%.3f p99_tick_ms=%.3f max_tick_ms=%.3f "
             "p95_world_delta_ms=%.3f threshold_p95_ms=16.7"),
        bPassed ? 1 : 0, SortedIntervals.Num(), ViewportWidth, ViewportHeight,
        MaxFps, MaxTickRate, VSync, ScreenPercentage, P50Ms, P95Ms, P99Ms, MaxMs, P95WorldDeltaMs);
    return bPassed;
}

bool ANaiwaP2GameMode::IsPhysicsLevelAtShotStart() const
{
    if (!bHasShotStartSnapshot || ShotStartStructureTransforms.Num() != StructureActors.Num() ||
        ShotStartGoalTransforms.Num() != GoalActors.Num())
    {
        return false;
    }

    const auto IsAtStart = [](const AActor* Actor, const FTransform& StartTransform)
    {
        if (!IsValid(Actor) || FVector::Dist(Actor->GetActorLocation(), StartTransform.GetLocation()) > 1.0)
        {
            return false;
        }
        const FRotator Delta = (Actor->GetActorRotation() - StartTransform.GetRotation().Rotator()).GetNormalized();
        if (FMath::Abs(Delta.Pitch) > 1.0 || FMath::Abs(Delta.Yaw) > 1.0 || FMath::Abs(Delta.Roll) > 1.0)
        {
            return false;
        }
        const UPrimitiveComponent* Body = GetPhysicsGoalBody(const_cast<AActor*>(Actor));
        return !Body || !Body->IsSimulatingPhysics() ||
            (Body->GetPhysicsLinearVelocity().Size() <= 5.0 &&
             Body->GetPhysicsAngularVelocityInDegrees().Size() <= 5.0);
    };

    for (const TPair<FString, FTransform>& Pair : ShotStartStructureTransforms)
    {
        const TObjectPtr<AActor>* Actor = StructureActors.Find(Pair.Key);
        if (!Actor || !IsAtStart(Actor->Get(), Pair.Value))
        {
            return false;
        }
    }
    for (const TPair<FString, FTransform>& Pair : ShotStartGoalTransforms)
    {
        const TObjectPtr<AActor>* Actor = GoalActors.Find(Pair.Key);
        if (!Actor || !IsAtStart(Actor->Get(), Pair.Value))
        {
            return false;
        }
    }
    return true;
}

ANaiwaP2StructureBlock* ANaiwaP2GameMode::FindStructureBlock(const FString& BlockId) const
{
    for (ANaiwaP2StructureBlock* Block : StructureBlocks)
    {
        if (IsValid(Block) && Block->GetBlockId() == BlockId)
        {
            return Block;
        }
    }
    return nullptr;
}

void ANaiwaP2GameMode::ResolveUnsupportedBlocks()
{
    using namespace NaiwaP2::Physics;

    const TArray<NaiwaP2::FBox2>& Boxes = CurrentScene.Boxes;

    for (const NaiwaP2::FBox2& Box : Boxes)
    {
        if (Box.bGoal)
        {
            continue;
        }
        ANaiwaP2StructureBlock* Block = FindStructureBlock(Box.Id);
        if (Block == nullptr || !Block->IsSimulating())
        {
            continue;   // 地基不参与，也就无所谓悬空
        }
        if (Box.MinZ <= GroundSupportZ)
        {
            continue;   // 已经坐在世界地面上
        }

        // ---- 1. 有没有东西托着它 ----
        bool bSupported = false;
        for (const NaiwaP2::FBox2& Other : Boxes)
        {
            if (Other.Id == Box.Id || Other.bGoal)
            {
                continue;
            }
            if (FMath::Abs(Other.MaxZ - Box.MinZ) > SupportZToleranceMeters)
            {
                continue;
            }
            if (Other.MinX > Box.MaxX || Other.MaxX < Box.MinX)
            {
                continue;
            }
            bSupported = true;
            break;
        }
        if (bSupported)
        {
            continue;
        }
        // Keep the authored rigid body dynamic. Missing support must be corrected
        // in scene geometry; never hide it by welding pieces together or freezing it.
        UE_LOG(LogTemp, Warning,
            TEXT("NAIWA_P2_UNSUPPORTED_BLOCK id=%s x=(%.2f,%.2f) z=(%.2f,%.2f) "
                 "no physical support; left as an independent dynamic body."),
            *Box.Id, Box.MinX, Box.MaxX, Box.MinZ, Box.MaxZ);
    }
}

void ANaiwaP2GameMode::ApplyShotGoalsAndTransition()
{
    for (const FString& GoalId : CurrentShot.RawKilled)
    {
        if (TObjectPtr<AActor>* Actor = GoalActors.Find(GoalId))
        {
            if (IsValid(Actor->Get()))
            {
                Actor->Get()->Destroy();
            }
            GoalActors.Remove(GoalId);
            CurrentScene.GoalIds.Remove(GoalId);
            CurrentScene.Boxes.RemoveAll([&GoalId](const NaiwaP2::FBox2& Box)
            {
                return Box.bGoal && Box.Id == GoalId;
            });
        }
    }

    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_SHOT_RESOLVE shot=%d hits=%d killed=%d remaining_goals=%d"),
        ShotsFired, CurrentShot.Hits.Num(), CurrentShot.RawKilled.Num(), GoalActors.Num());
    if (GoalActors.IsEmpty())
    {
        SetPhase(ENaiwaP2PlayState::Won);
    }
    else if (ShotsFired >= AmmoRule.TotalShots)
    {
        SetPhase(ENaiwaP2PlayState::Failed);
    }
    else
    {
        SetPhase(ENaiwaP2PlayState::Aiming);
    }
    bHasAimPreview = false;
    bShotStructureEffectsApplied = false;
    bHasShotStartSnapshot = false;
    ShotStartScene = NaiwaP2::FScene();
    ShotStartStructureTransforms.Reset();
    ShotStartGoalTransforms.Reset();
    ShotStartAvailableEggs.Reset();
}

void ANaiwaP2GameMode::Advance22PlayTest()
{
    const auto Fail = [this](const TCHAR* Reason)
    {
        b22PlayTestCompleted = true;
        UE_LOG(LogTemp, Error, TEXT("NAIWA_P2_22_PLAYTEST_FAIL reason=%s step=%d state=%s shots=%d goals=%d tokens=%d"),
            Reason, M2bPlayTestNextShot, *GetPlayStateLabel(), ShotsFired, GoalActors.Num(), AvailableEggs.Num());
        FPlatformMisc::RequestExitWithStatus(false, 1, TEXT("Naiwa P2 M2b standard playtest failed"));
    };

    if (CurrentLevelId != TEXT("2-2"))
    {
        Fail(TEXT("wrong_level"));
        return;
    }
    if (PlayState == ENaiwaP2PlayState::Failed)
    {
        Fail(TEXT("unexpected_failed_state"));
        return;
    }
    if (PlayState == ENaiwaP2PlayState::Won)
    {
        const bool bPassed = M2bPlayTestNextShot == 3 && ShotsFired == 3 &&
            GoalActors.IsEmpty() && AvailableEggs.Num() == 2 && bFreeRetryAvailable;
        b22PlayTestCompleted = true;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_22_PLAYTEST_%s shots=%d goals=%d tokens=%d retry=%d"),
            bPassed ? TEXT("PASS") : TEXT("FAIL"), ShotsFired, GoalActors.Num(), AvailableEggs.Num(), bFreeRetryAvailable);
        FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1,
            TEXT("Naiwa P2 M2b standard playtest completed"));
        return;
    }
    if (PlayState != ENaiwaP2PlayState::Aiming)
    {
        return;
    }

    if (M2bPlayTestNextShot == 0)
    {
        if (!SelectEggByIndex(2) || !TryFire(14.0, 0.90))
        {
            Fail(TEXT("mouse_shot_rejected"));
            return;
        }
        M2bPlayTestNextShot = 1;
        return;
    }
    if (M2bPlayTestNextShot == 1)
    {
        bool bSplitActorsAtTerminalStates = CurrentShot.SplitPaths.Num() == 3 &&
            SplitProjectileActors.Num() == CurrentShot.SplitPaths.Num();
        bool bHasEarlyTerminalPath = false;
        for (int32 Index = 0; bSplitActorsAtTerminalStates && Index < CurrentShot.SplitPaths.Num(); ++Index)
        {
            const NaiwaP2::FProjectilePath& Path = CurrentShot.SplitPaths[Index];
            AActor* Actor = SplitProjectileActors[Index].Get();
            if (Path.States.IsEmpty())
            {
                bSplitActorsAtTerminalStates = false;
                break;
            }
            bHasEarlyTerminalPath |= Path.StartTick + Path.States.Num() - 1 < CurrentShot.TotalTicks;
            const NaiwaP2::FTrajectoryState& LastState = Path.States.Last();
            bSplitActorsAtTerminalStates &= IsValid(Actor) && !Actor->IsHidden() &&
                FVector::Dist(Actor->GetActorLocation(), ToWorld(LastState.X, LastState.Z)) < 0.01;
        }
        bSplitActorsAtTerminalStates &= bHasEarlyTerminalPath;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_SPLIT_VISUAL_CHECK pass=%d early_path=%d children=%d"),
            bSplitActorsAtTerminalStates, bHasEarlyTerminalPath, SplitProjectileActors.Num());

        const bool bFirstShotPassed = ShotsFired == 1 && GoalActors.Num() == 5 &&
            CurrentShot.RawKilled.Num() == 5 && CurrentShot.bSkillTriggered &&
            CurrentShot.SkillTick == 72 && AvailableEggs.Num() == 4 && bSplitActorsAtTerminalStates;
        if (!bFirstShotPassed || !SelectEggByIndex(1) || !TryFire(74.0, 0.94))
        {
            Fail(TEXT("mouse_split_or_ledger_mismatch"));
            return;
        }
        M2bPlayTestNextShot = 2;
        return;
    }
    if (M2bPlayTestNextShot == 2)
    {
        const bool bSecondShotPassed = ShotsFired == 2 && GoalActors.Num() == 1 &&
            CurrentShot.RawKilled.Num() == 4 && CurrentShot.bSkillTriggered &&
            CurrentShot.SkillTick == 46 && AvailableEggs.Num() == 3;
        if (!bSecondShotPassed || !SelectEggByIndex(4) || !TryFire(53.0, 1.0))
        {
            Fail(TEXT("chick_flap_or_ledger_mismatch"));
            return;
        }
        M2bPlayTestNextShot = 3;
        return;
    }
    if (M2bPlayTestNextShot == 3)
    {
        Fail(TEXT("third_shot_did_not_win"));
    }
}

void ANaiwaP2GameMode::Advance22RefundPlayTest()
{
    const auto Fail = [this](const TCHAR* Reason)
    {
        b22RefundPlayTestCompleted = true;
        UE_LOG(LogTemp, Error, TEXT("NAIWA_P2_22_REFUND_PLAYTEST_FAIL reason=%s step=%d state=%s shots=%d goals=%d tokens=%d retry=%d"),
            Reason, M2bRefundPlayTestStep, *GetPlayStateLabel(), ShotsFired, GoalActors.Num(),
            AvailableEggs.Num(), bFreeRetryAvailable);
        FPlatformMisc::RequestExitWithStatus(false, 1, TEXT("Naiwa P2 M2b refund playtest failed"));
    };

    if (CurrentLevelId != TEXT("2-2"))
    {
        Fail(TEXT("wrong_level"));
        return;
    }
    if (M2bRefundPlayTestStep == 0 && PlayState == ENaiwaP2PlayState::Aiming)
    {
        if (!SelectEggByIndex(0) || !TryFire(89.0, 0.05))
        {
            Fail(TEXT("miss_shot_rejected"));
            return;
        }
        M2bRefundPlayTestStep = 1;
        return;
    }
    if (M2bRefundPlayTestStep == 1 && PlayState == ENaiwaP2PlayState::Aiming)
    {
        const bool bRefundPassed = ShotsFired == 0 && GoalActors.Num() == 10 &&
            AvailableEggs.Num() == 5 && !bFreeRetryAvailable;
        if (!bRefundPassed)
        {
            Fail(TEXT("zero_kill_refund_did_not_restore_token"));
            return;
        }
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_22_REFUND_CHECK pass=1 shots=0 goals=10 tokens=5 retry=0"));
        Retry();
        M2bRefundPlayTestStep = 2;
        return;
    }
    if (M2bRefundPlayTestStep == 2 && PlayState == ENaiwaP2PlayState::Aiming)
    {
        const bool bResetPassed = ShotsFired == 0 && GoalActors.Num() == 10 &&
            AvailableEggs.Num() == 5 && bFreeRetryAvailable;
        b22RefundPlayTestCompleted = true;
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_22_REFUND_PLAYTEST_%s shots=%d goals=%d tokens=%d retry=%d"),
            bResetPassed ? TEXT("PASS") : TEXT("FAIL"), ShotsFired, GoalActors.Num(),
            AvailableEggs.Num(), bFreeRetryAvailable);
        FPlatformMisc::RequestExitWithStatus(false, bResetPassed ? 0 : 1,
            TEXT("Naiwa P2 M2b refund playtest completed"));
    }
}

void ANaiwaP2GameMode::SetPhase(const ENaiwaP2PlayState NewState)
{
    const bool bEnteringFailedState = NewState == ENaiwaP2PlayState::Failed &&
        PlayState != ENaiwaP2PlayState::Failed;
    PlayState = NewState;
    if (NewState == ENaiwaP2PlayState::Won)
    {
        ActiveFeedbackImage = ENaiwaP2FeedbackImage::None;
        FeedbackImageRemaining = 0.0;
    }
    else if (bEnteringFailedState && !GoalActors.IsEmpty() &&
        (AvailableEggs.IsEmpty() || ShotsFired >= AmmoRule.TotalShots))
    {
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_FEEDBACK_TRIGGER level=%s event=out_of_eggs shots=%d tokens=%d"),
            *CurrentLevelId, ShotsFired, AvailableEggs.Num());
        ShowFeedbackCue(ENaiwaP2FeedbackImage::OutOfEggs, FailureTrumpetSound);
    }
}
