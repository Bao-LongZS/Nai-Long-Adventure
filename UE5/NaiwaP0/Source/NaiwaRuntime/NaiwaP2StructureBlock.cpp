#include "NaiwaP2StructureBlock.h"
#include "NaiwaP2ImpactEffectSubsystem.h"
#include "NaiwaP2PhysicsEgg.h"
#include "NaiwaP2PhysicsGoal.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    const TCHAR* const CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");

    /** 物理材质的组合模式：取两者中较大的一方，这样"蛋弹性 0.38"不会被木块 0.12 拉低。 */
    constexpr EFrictionCombineMode::Type CombineMode = EFrictionCombineMode::Max;

    const TCHAR* RoleToString(const NaiwaP2::Physics::EBlockRole Role)
    {
        switch (Role)
        {
        case NaiwaP2::Physics::EBlockRole::Foundation: return TEXT("Foundation");
        case NaiwaP2::Physics::EBlockRole::Column:     return TEXT("Column");
        case NaiwaP2::Physics::EBlockRole::Board:      return TEXT("Board");
        case NaiwaP2::Physics::EBlockRole::Goal:       return TEXT("Goal");
        default:                                       return TEXT("Unknown");
        }
    }
}

ANaiwaP2StructureBlock::ANaiwaP2StructureBlock()
{
    // 结构块自己不需要 Tick：刚体的位置由 Chaos 推，Actor 跟着组件走。
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    BlockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockMesh"));
    SetRootComponent(BlockMesh);

    // Movable 是硬要求：Static 组件上调用 SetSimulatePhysics 会直接失败并打警告。
    BlockMesh->SetMobility(EComponentMobility::Movable);

    // 网格在构造期就用硬引用填好，保证 /Engine/BasicShapes/Cube 进 cook。
    // InitializeFromBox 里还有一次运行时兜底（资产被裁掉的情况）。
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(CubeMeshPath);
    if (CubeFinder.Succeeded())
    {
        BlockMesh->SetStaticMesh(CubeFinder.Object);
    }

    // 默认不开碰撞：由 InitFromBox 按角色决定，避免生成到一半就参与求解。
    BlockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    BlockMesh->SetGenerateOverlapEvents(false);
    BlockMesh->bReturnMaterialOnMove = false;

    // 结构块要投影，但不需要参与距离场/光照贴图的额外计算。
    BlockMesh->SetCastShadow(true);
}

void ANaiwaP2StructureBlock::InitFromBox(const FString& InBlockId, const NaiwaP2::FBox2& Box,
                                         const NaiwaP2::Physics::EBlockRole InRole,
                                         UMaterialInterface* VisualMaterial,
                                         const double MassMultiplier)
{
    using namespace NaiwaP2::Physics;

    VerifyProjectPhysicsSettings();

    BlockId = InBlockId;
    Role = InRole;
    LastCollisionLogTimeByActor.Reset();

    const FBlockMaterial Material = GetBlockMaterial(InRole);

    // ---- 1. 尺寸：只读 Box，四边各内缩半个缝隙 ----
    //
    // 中心点不动（对称内缩），所以"坐标一个数不改"成立；改的只是每边让出 1 mm。
    // 退让量必须在 MinBlockDimensionMeters 之内，否则薄块会被缩成负尺寸。
    const double RawWidth = Box.MaxX - Box.MinX;
    const double RawHeight = Box.MaxZ - Box.MinZ;
    const double HalfGap = SpawnGapMeters * 0.5;
    const bool bCanApplyGap =
        RawWidth > MinBlockDimensionMeters + SpawnGapMeters * 2.0 &&
        RawHeight > MinBlockDimensionMeters + SpawnGapMeters * 2.0;

    const double EffectiveHalfGap = bCanApplyGap ? HalfGap : 0.0;
    const double Width = FMath::Max(MinBlockDimensionMeters, RawWidth - EffectiveHalfGap * 2.0);
    const double Height = FMath::Max(MinBlockDimensionMeters, RawHeight - EffectiveHalfGap * 2.0);

    const double CenterX = (Box.MinX + Box.MaxX) * 0.5;
    const double CenterZ = (Box.MinZ + Box.MaxZ) * 0.5;
    const double CenterY = 0.0;   // 与 ToWorld 一致：游戏平面在 Y = 0

    SpawnLocationCm = FVector(CenterX * CentimetersPerMeter,
                              CenterY * CentimetersPerMeter,
                              CenterZ * CentimetersPerMeter);

    // ---- 2. 网格与缩放 ----
    //
    // Cube 是 100 cm 边长，所以"以米为单位的尺寸"直接就是缩放值。
    if (BlockMesh->GetStaticMesh() == nullptr)
    {
        if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubeMeshPath))
        {
            BlockMesh->SetStaticMesh(Cube);
        }
    }

    BlockMesh->SetWorldScale3D(FVector(Width, DepthMeters, Height));
    SetActorLocation(SpawnLocationCm, false, nullptr, ETeleportType::TeleportPhysics);

    // 视觉材质与物理材质是两件事：前者决定看起来像灰盒，后者决定摩擦/恢复。
    if (VisualMaterial)
    {
        BlockMesh->SetMaterial(0, VisualMaterial);
    }

    // ---- 3. 物理材质（质量之外的表面属性） ----
    PhysMaterial = NewObject<UPhysicalMaterial>(this, TEXT("P2BlockPhysMat"));
    PhysMaterial->Friction = Material.Friction;
    PhysMaterial->StaticFriction = Material.StaticFriction;
    PhysMaterial->Restitution = Material.Restitution;
    PhysMaterial->FrictionCombineMode = CombineMode;
    PhysMaterial->bOverrideFrictionCombineMode = true;
    PhysMaterial->RestitutionCombineMode = CombineMode;
    PhysMaterial->bOverrideRestitutionCombineMode = true;
    BlockMesh->SetPhysMaterialOverride(PhysMaterial);

    // ---- 4. 碰撞：所有通道一律 Block ----
    //
    // 注意改造前的场景是 ECollisionEnabled::NoCollision —— 整个 P2 场景没有任何碰撞。
    // 所以这里不是"调整碰撞设置"，而是第一次真正给结构加碰撞体。
    BlockMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BlockMesh->SetCollisionObjectType(ECC_WorldDynamic);
    BlockMesh->SetCollisionResponseToAllChannels(ECR_Block);
    BlockMesh->SetNotifyRigidBodyCollision(bLogCollisionEvents);

    // ---- 5. 密度 → 质量 ----
    //
    // 质量按体积算，不给所有块同一个质量。柱 0.855 m³ 与板 1.44 m³ 的惯性差
    // 是"撞一根柱不会整排飞"的前提。
    const double VolumeM3 = Width * DepthMeters * Height;
    ExpectedMassKg = Material.DensityKgPerM3 * VolumeM3 * FMath::Max(0.1, MassMultiplier);
    bSimulating = Material.bSimulate;

    // P2 是正交侧视玩法：刚体只允许在 XZ 平面移动，并只绕 Y 轴旋转。
    // 不限制深度时，接触求解会把堆叠块推出画面平面，稳定性自检也会误报大漂移。
    BlockMesh->SetConstraintMode(EDOFMode::XZPlane);

    // SleepFamily 必须在刚体创建前落到 BodyInstance 上：它是建 proxy 时读一次的字段，
    // 建完再改不会推给 Chaos。
    if (FBodyInstance* Body = BlockMesh->GetBodyInstance())
    {
        Body->SleepFamily = BlockSleepFamily;
    }

    BlockMesh->SetSimulatePhysics(bSimulating);

    if (bSimulating)
    {
        BlockMesh->SetMassOverrideInKg(NAME_None, static_cast<float>(ExpectedMassKg), true);
        BlockMesh->SetLinearDamping(Material.LinearDamping);
        BlockMesh->SetAngularDamping(Material.AngularDamping);
        BlockMesh->SetUseCCD(Material.bUseCCD);

        if (FBodyInstance* Body = BlockMesh->GetBodyInstance())
        {
            // 去穿透速度上限：见 NaiwaP2PhysicsProfile.h。这是"塔被弹飞"的第二道保险。
            Body->SetMaxDepenetrationVelocity(MaxDepenetrationVelocityCmPerS);

            const double ActualMassKg = Body->GetBodyMass();
            UE_LOG(LogTemp, Display,
                TEXT("NAIWA_P2_BLOCK id=%s role=%s size_cm=(%.1f,%.1f,%.1f) vol_m3=%.4f "
                     "mass_kg=%.2f actual_kg=%.2f sleep=%d mass_scale=%.3f"),
                *BlockId, RoleToString(Role),
                Width * CentimetersPerMeter, DepthMeters * CentimetersPerMeter,
                Height * CentimetersPerMeter, VolumeM3, ExpectedMassKg, ActualMassKg,
                static_cast<int32>(BlockSleepFamily), FMath::Max(0.1, MassMultiplier));
        }
    }
    else
    {
        // 地基/墙：不求解，但必须有碰撞体，否则塔没有可以站的平面。
        UE_LOG(LogTemp, Verbose, TEXT("NAIWA_P2_BLOCK id=%s role=Foundation static_collider"),
            *BlockId);
    }

    if (bLogCollisionEvents)
    {
        BlockMesh->OnComponentHit.AddDynamic(this, &ANaiwaP2StructureBlock::HandleBlockHit);
    }
}

void ANaiwaP2StructureBlock::HandleBlockHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                            UPrimitiveComponent* OtherComp, FVector NormalImpulse,
                                            const FHitResult& Hit)
{
    using namespace NaiwaP2::Physics;
    if (!bLogCollisionEvents)
    {
        return;
    }

    if (Hit.bStartPenetrating)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("NAIWA_P2_PHYS_PENETRATION source=structure self=%s other=%s depth_cm=%.3f "
                 "point_cm=(%.1f,%.1f,%.1f)"),
            *BlockId, *GetNameSafe(OtherActor), Hit.PenetrationDepth,
            Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
    }

    // 冲量或相对速度太小的事件不记：塔自稳阶段接地/堆叠会持续产生低速回调。
    const double Impulse = NormalImpulse.Size();
    if (Impulse < CollisionLogMinImpulseKgCmPerS)
    {
        return;
    }

    const FVector SelfVelocity = BlockMesh ? BlockMesh->GetPhysicsLinearVelocity() : FVector::ZeroVector;
    const FVector OtherVelocity = OtherComp ? OtherComp->GetPhysicsLinearVelocity() : FVector::ZeroVector;
    const FVector RelativeVelocity = SelfVelocity - OtherVelocity;
    if (RelativeVelocity.Size() < CollisionLogMinRelativeSpeedCmPerS)
    {
        return;
    }

    // Chaos 子步可能在同一帧对持续接触重复派发 Hit；保留有意义的再次撞击，压掉接触刷屏。
    UWorld* World = GetWorld();
    if (!World || !OtherActor)
    {
        return;
    }
    const TWeakObjectPtr<AActor> OtherKey(OtherActor);
    const double Now = World->GetTimeSeconds();
    if (const double* LastLogTime = LastCollisionLogTimeByActor.Find(OtherKey))
    {
        if (Now - *LastLogTime < CollisionLogPairCooldownSeconds)
        {
            return;
        }
    }
    LastCollisionLogTimeByActor.Add(OtherKey, Now);

    const double SelfMassKg = GetMassKg();
    const double OtherMassKg = (OtherComp && OtherComp->GetBodyInstance())
        ? OtherComp->GetBodyInstance()->GetBodyMass() : -1.0;

    const FVector Impact = Hit.ImpactPoint;
    const FVector Normal = Hit.ImpactNormal;

    // 这一行是调参的主要依据：撞击位置 / 法线 / 相对速度 / 冲量 / 双方质量。
    // 字段顺序固定，脚本可以直接按 = 分割取值。
    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P2_HIT self=%s other=%s pos_cm=(%.1f,%.1f,%.1f) n=(%.3f,%.3f,%.3f) "
             "rel_cm_s=(%.1f,%.1f,%.1f) |rel|=%.1f impulse=%.2f m_self=%.2f m_other=%.2f "
             "mat_self=%s mat_other=%s start_penetrating=%d penetration_depth_cm=%.3f"),
        *BlockId,
        OtherActor ? *OtherActor->GetName() : TEXT("None"),
        Impact.X, Impact.Y, Impact.Z,
        Normal.X, Normal.Y, Normal.Z,
        RelativeVelocity.X, RelativeVelocity.Y, RelativeVelocity.Z,
        RelativeVelocity.Size(), Impulse,
        SelfMassKg, OtherMassKg,
        RoleToString(Role),
        (OtherComp && OtherComp->GetBodyInstance() && OtherComp->GetBodyInstance()->GetPhysMaterialOverride())
            ? TEXT("Override") : TEXT("Default"),
        Hit.bStartPenetrating ? 1 : 0,
        Hit.bStartPenetrating ? Hit.PenetrationDepth : 0.0f);

    // Eggs and goals have their own presentation callback. For block-to-block
    // contacts, only the lower unique id emits so one collision makes one burst.
    const ANaiwaP2StructureBlock* OtherBlock = Cast<ANaiwaP2StructureBlock>(OtherActor);
    const bool bHandledByOtherActor = Cast<ANaiwaP2PhysicsEgg>(OtherActor) != nullptr ||
        Cast<ANaiwaP2PhysicsGoal>(OtherActor) != nullptr;
    if (!bHandledByOtherActor && (!OtherBlock || GetUniqueID() < OtherActor->GetUniqueID()))
    {
        if (UNaiwaP2ImpactEffectSubsystem* ImpactEffects =
            GetWorld()->GetSubsystem<UNaiwaP2ImpactEffectSubsystem>())
        {
            ImpactEffects->SpawnImpact(this, OtherActor, Hit.ImpactPoint, Hit.ImpactNormal,
                Impulse, RelativeVelocity.Size());
        }
    }
}

void ANaiwaP2StructureBlock::WatchSupportJoint(UPhysicsConstraintComponent* Joint,
                                                const FString& OtherBlockId)
{
    if (Joint)
    {
        Joint->ComponentTags.AddUnique(FName(*FString::Printf(TEXT("P2SupportOther:%s"), *OtherBlockId)));
        WatchedSupportJoints.Add(Joint);
        Joint->OnConstraintBroken.AddDynamic(this, &ANaiwaP2StructureBlock::HandleSupportJointBroken);
    }
}

void ANaiwaP2StructureBlock::LogSupportJointStates()
{
    for (UPhysicsConstraintComponent* Joint : WatchedSupportJoints)
    {
        if (!IsValid(Joint))
        {
            continue;
        }
        const FName* OtherTag = Joint->ComponentTags.FindByPredicate([](const FName& Tag)
        {
            return Tag.ToString().StartsWith(TEXT("P2SupportOther:"));
        });
        const FString OtherId = OtherTag
            ? OtherTag->ToString().RightChop(15)
            : FString(TEXT("unknown"));
        FVector LinearForce = FVector::ZeroVector;
        FVector AngularForce = FVector::ZeroVector;
        Joint->GetConstraintForce(LinearForce, AngularForce);
        UPrimitiveComponent* ComponentA = nullptr;
        UPrimitiveComponent* ComponentB = nullptr;
        FName BoneA = NAME_None;
        FName BoneB = NAME_None;
        Joint->GetConstrainedComponents(ComponentA, BoneA, ComponentB, BoneB);
        const FTransform FrameA = Joint->ConstraintInstance.GetRefFrame(EConstraintFrame::Frame1);
        const FTransform FrameB = Joint->ConstraintInstance.GetRefFrame(EConstraintFrame::Frame2);
        // UpdateConstraintFrames divides local anchor positions by the constraint
        // component's minimum scale; CreateJoint restores that same scale. Rebuild
        // the world anchors in the unscaled body frame to match Chaos' convention.
        const float RefScale = FMath::Max(Joint->GetComponentScale().GetAbsMin(), 0.01f);
        FBodyInstance* BodyInstanceA = ComponentA ? ComponentA->GetBodyInstance(BoneA) : nullptr;
        FBodyInstance* BodyInstanceB = ComponentB ? ComponentB->GetBodyInstance(BoneB) : nullptr;
        FTransform BodyTransformA = BodyInstanceA
            ? BodyInstanceA->GetUnrealWorldTransform()
            : (ComponentA ? ComponentA->GetComponentTransform() : FTransform::Identity);
        FTransform BodyTransformB = BodyInstanceB
            ? BodyInstanceB->GetUnrealWorldTransform()
            : (ComponentB ? ComponentB->GetComponentTransform() : FTransform::Identity);
        BodyTransformA.RemoveScaling();
        BodyTransformB.RemoveScaling();
        const FVector AnchorA = ComponentA
            ? BodyTransformA.TransformPosition(FrameA.GetLocation() * RefScale)
            : FVector::ZeroVector;
        const FVector AnchorB = ComponentB
            ? BodyTransformB.TransformPosition(FrameB.GetLocation() * RefScale)
            : FVector::ZeroVector;
        const double AnchorErrorCm = ComponentA && ComponentB
            ? FVector::Distance(AnchorA, AnchorB)
            : -1.0;
        FTransform ComponentTransformA = ComponentA ? ComponentA->GetComponentTransform() : FTransform::Identity;
        FTransform ComponentTransformB = ComponentB ? ComponentB->GetComponentTransform() : FTransform::Identity;
        ComponentTransformA.RemoveScaling();
        ComponentTransformB.RemoveScaling();
        const FVector ComponentAnchorA = ComponentA
            ? ComponentTransformA.TransformPosition(FrameA.GetLocation() * RefScale)
            : FVector::ZeroVector;
        const FVector ComponentAnchorB = ComponentB
            ? ComponentTransformB.TransformPosition(FrameB.GetLocation() * RefScale)
            : FVector::ZeroVector;
        const double ComponentAnchorErrorCm = ComponentA && ComponentB
            ? FVector::Distance(ComponentAnchorA, ComponentAnchorB)
            : -1.0;
        const FVector JointLocation = Joint->ConstraintInstance.GetConstraintLocation();
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_SUPPORT_JOINT_STATE main=%s other=%s valid=%d broken=%d "
                 "linear_breakable=%d linear_threshold=%.1f angular_breakable=%d angular_threshold=%.1f "
                 "anchor_error_cm=%.2f component_anchor_error_cm=%.2f "
                 "anchor_a=(%.1f,%.1f,%.1f) anchor_b=(%.1f,%.1f,%.1f) "
                 "joint_cm=(%.1f,%.1f,%.1f) "
                 "force=(%.1f,%.1f,%.1f) torque=(%.1f,%.1f,%.1f)"),
            *BlockId, *OtherId, Joint->ConstraintInstance.IsValidConstraintInstance() ? 1 : 0,
            Joint->IsBroken() ? 1 : 0,
            Joint->ConstraintInstance.IsLinearBreakable() ? 1 : 0,
            Joint->ConstraintInstance.GetLinearBreakThreshold(),
            Joint->ConstraintInstance.IsAngularBreakable() ? 1 : 0,
            Joint->ConstraintInstance.GetAngularBreakThreshold(),
            AnchorErrorCm, ComponentAnchorErrorCm,
            AnchorA.X, AnchorA.Y, AnchorA.Z, AnchorB.X, AnchorB.Y, AnchorB.Z,
            JointLocation.X, JointLocation.Y, JointLocation.Z,
            LinearForce.X, LinearForce.Y, LinearForce.Z,
            AngularForce.X, AngularForce.Y, AngularForce.Z);
    }
}

void ANaiwaP2StructureBlock::HandleSupportJointBroken(const int32 ConstraintIndex)
{
    UE_LOG(LogTemp, Warning,
        TEXT("NAIWA_P2_SUPPORT_JOINT_BROKEN owner=%s constraint_index=%d"),
        *BlockId, ConstraintIndex);
}

double ANaiwaP2StructureBlock::GetDriftCm() const
{
    return GetDriftVectorCm().Size();
}

FVector ANaiwaP2StructureBlock::GetDriftVectorCm() const
{
    return BlockMesh
        ? BlockMesh->GetComponentLocation() - SpawnLocationCm
        : FVector::ZeroVector;
}

double ANaiwaP2StructureBlock::GetSpeedCmPerS() const
{
    return BlockMesh ? BlockMesh->GetPhysicsLinearVelocity().Size() : 0.0;
}

double ANaiwaP2StructureBlock::GetLeanDegrees() const
{
    if (BlockMesh == nullptr)
    {
        return 0.0;
    }
    // up 轴与世界 up 的夹角 = XZ 平面内的倾角。
    // XZPlane 约束只放开绕 Y 的旋转，而绕 Y 转正好就是把 up 轴在 XZ 平面内掰弯，
    // 所以这个指标在侧视玩法下是有意义的（不是恒 0，见头文件里的说明）。
    const double UpZ = static_cast<double>(BlockMesh->GetComponentQuat().GetUpVector().Z);
    return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(UpZ, -1.0, 1.0)));
}

double ANaiwaP2StructureBlock::GetMassKg() const
{
    if (!bSimulating || BlockMesh == nullptr)
    {
        return -1.0;
    }
    return BlockMesh->GetBodyInstance() ? BlockMesh->GetBodyInstance()->GetBodyMass() : -1.0;
}

void ANaiwaP2StructureBlock::SleepBlock()
{
    if (bSimulating && BlockMesh)
    {
        BlockMesh->PutRigidBodyToSleep();
    }
}

void ANaiwaP2StructureBlock::WakeBlock()
{
    if (bSimulating && BlockMesh)
    {
        BlockMesh->WakeRigidBody();
    }
}

void ANaiwaP2StructureBlock::MakeStaticCollider()
{
    if (!bSimulating || BlockMesh == nullptr)
    {
        return;
    }
    BlockMesh->SetSimulatePhysics(false);
    bSimulating = false;
    UE_LOG(LogTemp, Error,
        TEXT("NAIWA_P2_BLOCK_PINNED id=%s 该块在数据里悬空且无邻块可焊，已退化为静态碰撞体。"
             "这是数据缺陷，不是参数问题。"), *BlockId);
}

void ANaiwaP2StructureBlock::VerifyProjectPhysicsSettings()
{
    using namespace NaiwaP2::Physics;

    static bool bAlreadyChecked = false;
    if (bAlreadyChecked)
    {
        return;
    }
    bAlreadyChecked = true;

    const UPhysicsSettings* Settings = GetDefault<UPhysicsSettings>();
    if (Settings == nullptr)
    {
        return;
    }

    // 子步是 UPhysicsSettings 的 config 属性，初始化期读一次，运行期改无效。
    // 所以这里只比对、只报警，不"顺手修正"——否则会让人以为改成功了。
    if (!Settings->bSubstepping)
    {
        UE_LOG(LogTemp, Error,
            TEXT("NAIWA_P2_PHYS_CFG bSubstepping=false，期望 true。"
                 "请在 DefaultEngine.ini 的 [/Script/Engine.PhysicsSettings] 里开启，"
                 "否则细柱（高宽比 6.3）会抖或自倒。"));
    }
    if (!FMath::IsNearlyEqual(Settings->MaxSubstepDeltaTime, static_cast<float>(MaxSubstepDeltaTime), 1.0e-6f))
    {
        UE_LOG(LogTemp, Error,
            TEXT("NAIWA_P2_PHYS_CFG MaxSubstepDeltaTime=%.6f，期望 %.6f（1/120）。"),
            Settings->MaxSubstepDeltaTime, MaxSubstepDeltaTime);
    }
    if (Settings->MaxSubsteps != MaxSubsteps)
    {
        UE_LOG(LogTemp, Error,
            TEXT("NAIWA_P2_PHYS_CFG MaxSubsteps=%d，期望 %d。"),
            Settings->MaxSubsteps, MaxSubsteps);
    }
    if (Settings->bTickPhysicsAsync != bTickPhysicsAsync)
    {
        UE_LOG(LogTemp, Error,
            TEXT("NAIWA_P2_PHYS_CFG bTickPhysicsAsync=%d，期望 %d。"),
            Settings->bTickPhysicsAsync ? 1 : 0, bTickPhysicsAsync ? 1 : 0);
    }

    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P2_PHYS_CFG substep=%.6f max_substeps=%d async=%d gravity=%.2f"),
        Settings->MaxSubstepDeltaTime, Settings->MaxSubsteps,
        Settings->bTickPhysicsAsync ? 1 : 0, GravityMPerS2);
}
