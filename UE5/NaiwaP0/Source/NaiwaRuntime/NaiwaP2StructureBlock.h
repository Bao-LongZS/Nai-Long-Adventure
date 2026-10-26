#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NaiwaP2Types.h"
#include "NaiwaP2PhysicsProfile.h"
#include "NaiwaP2StructureBlock.generated.h"

class UStaticMeshComponent;
class UPhysicalMaterial;
class UMaterialInterface;
class UPhysicsConstraintComponent;

/**
 * 一块结构 = 一个刚体。
 *
 * 存在形式：一个立方体网格按 FBox2 的宽高缩放 → 一个静态网格体组件开 Simulate Physics。
 * 生成时**只读** FBox2 的 MinX/MaxX/MinZ/MaxZ，不写回、不修正任何坐标；唯一的形状处理是
 * 四边各内缩半个 SpawnGapMeters 以留出初始缝隙（见 NaiwaP2PhysicsProfile.h 里的说明）。
 *
 * 为什么每个块是一个独立 Actor 而不是一个组件挂在 GameMode 上：
 *   故障隔离。一块的 BodyInstance 配错只会坏一块，整个塔一起坏的时候没法二分定位。
 *   代价是每块一个 AActor 开销 —— 单关最多 26 块，可以接受。
 *
 * 为什么不用 KINEMATIC 假刚体：
 *   KINEMATIC 不参与物理求解。撞倒柱子之后上面的板不会自己压下来，等于退回查表倒塌；
 *   而且 SetSimulatePhysics(true) 生效到第一个物理步之间至少有一帧窗口期，那一帧里
 *   蛋会先"陷进"柱子里，下一帧才被弹开 —— 手感读作"延迟爆炸"。
 *   这里全部用纯动态刚体，靠参数和初始缝隙调稳。
 */
UCLASS()
class NAIWARUNTIME_API ANaiwaP2StructureBlock : public AActor
{
    GENERATED_BODY()

public:
    ANaiwaP2StructureBlock();

    /**
     * 按 FBox2 生成刚体。可重复调用（重试时会重建），重复调用会重置该块的状态。
     *
     * @param InBlockId SceneLibrary 里的 Box.Id，日志与自检都用它做键。
     * @param Box       原始几何。只读。
     * @param InRole    角色分档，决定是否求解与质量/摩擦取哪一档。
     * @param VisualMaterial 视觉材质。只影响外观，不影响物理；传空则保持网格默认材质。
     */
    void InitFromBox(const FString& InBlockId, const NaiwaP2::FBox2& Box,
                     NaiwaP2::Physics::EBlockRole InRole,
                     UMaterialInterface* VisualMaterial = nullptr,
                     double MassMultiplier = 1.0);

    const FString& GetBlockId() const { return BlockId; }
    NaiwaP2::Physics::EBlockRole GetRole() const { return Role; }
    UStaticMeshComponent* GetBlockMesh() const { return BlockMesh; }

    /** 该块是否参与物理求解（地基/墙为 false）。 */
    bool IsSimulating() const { return bSimulating; }

    /** 生成时的世界中心（cm）。自检算位移量用。 */
    const FVector& GetSpawnLocationCm() const { return SpawnLocationCm; }

    /** 相对生成位置的位移（cm）。验收第 1 条就看这个数。 */
    double GetDriftCm() const;

    /**
     * 相对生成位置的分量位移（cm）。
     *
     * 只给诊断用：只看总位移无法区分"落进缝隙"（Z 向下）和"被挤开"（X 横向），
     * 而这两种情况的修法完全相反 —— 前者调缝隙，后者调摩擦或支撑。
     */
    FVector GetDriftVectorCm() const;

    /** 当前线速度（cm/s），用于静止判定。 */
    double GetSpeedCmPerS() const;

    /**
     * 侧视平面内的倾角（度）：块自身 up 轴与世界 up 的夹角。
     *
     * 结构块被 EDOFMode::XZPlane 约束：位移只剩 X/Z，旋转只剩**绕 Y 轴**那一个自由度
     * （FRotator 里对应 Pitch）。绕 X/Z 的旋转被锁死，所以「歪了」只可能表现为 up 轴
     * 在 XZ 平面内偏转 —— 这正是本函数量的东西。
     *
     * 反过来说：用旋转的 Yaw 分量（绕 Z）做倾角指标会恒等于 0，因为那个轴是锁死的。
     * 这两个轴很容易看反，写在这里免得下一个人再踩一次。
     */
    double GetLeanDegrees() const;

    /** 质量（kg）。地基返回 -1。 */
    double GetMassKg() const;

    /** 生成时按体积算出的质量，用于校验 MassInKgOverride 有没有真的落地。 */
    double GetExpectedMassKg() const { return ExpectedMassKg; }

    /** Add owner-side diagnostics for a temporary support-joint tuning probe. */
    void WatchSupportJoint(UPhysicsConstraintComponent* Joint, const FString& OtherBlockId);
    void LogSupportJointStates();

    void SleepBlock();
    void WakeBlock();

    /**
     * 退化成静态碰撞体：停止求解，但保留碰撞。
     *
     * 只给"悬空了又找不到任何可以焊接的邻块"的兜底用。调用会打 Error 日志 ——
     * 走到这条路的块在数据上本来就是错的，必须让人看见。
     */
    void MakeStaticCollider();

    /**
     * 比对 DefaultEngine.ini 里的 [/Script/Engine.PhysicsSettings] 与 NaiwaP2PhysicsProfile.h
     * 的常量是否一致。不一致只打 Error 日志、**不改**运行时设置 ——
     * 子步相关的属性是初始化期读取的，运行时改不会生效，静默改反而会让人以为改成功了。
     * 进程内只跑一次。
     */
    static void VerifyProjectPhysicsSettings();

protected:
    /**
     * 输出碰撞诊断，并为达到物理门槛的碰撞触发轻量表现反馈。
     */
    UFUNCTION()
    void HandleBlockHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                        UPrimitiveComponent* OtherComp, FVector NormalImpulse,
                        const FHitResult& Hit);

    UFUNCTION()
    void HandleSupportJointBroken(int32 ConstraintIndex);

private:
    UPROPERTY(VisibleAnywhere, Category = "Naiwa|P2")
    TObjectPtr<UStaticMeshComponent> BlockMesh;

    UPROPERTY(Transient)
    TObjectPtr<UPhysicalMaterial> PhysMaterial;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UPhysicsConstraintComponent>> WatchedSupportJoints;

    FString BlockId;
    NaiwaP2::Physics::EBlockRole Role = NaiwaP2::Physics::EBlockRole::Column;

    FVector SpawnLocationCm = FVector::ZeroVector;
    double ExpectedMassKg = 0.0;
    bool bSimulating = false;
    TMap<TWeakObjectPtr<AActor>, double> LastCollisionLogTimeByActor;
};
