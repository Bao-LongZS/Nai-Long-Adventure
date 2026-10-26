#pragma once

#include "CoreMinimal.h"
#include "NaiwaP2Types.h"
#include "NaiwaP2PhysicsProfile.h"

class AActor;
class UPrimitiveComponent;
class USceneComponent;
class UWorld;

namespace NaiwaP2
{
    /**
     * 一次弹丸接触（契约 2：碰撞 → 命中，事件化）。
     *
     * 注意这里**没有**「这个盒子会被打死谁」的字段，也没有查表结果：
     * 目标死亡由结构真的塌下去压到目标来实现，不由这里推断。
     * 这个结构只是日志/探针/后续 HUD 拖尾的载体。
     */
    struct FProjectileImpact
    {
        /** 接触时刻（秒，从发射算起）。 */
        double Time = 0.0;

        /** 命中的结构块 Id（来自 ANaiwaP2StructureBlock::GetBlockId）。命中非结构体时为空。 */
        FString BlockId;

        /** 命中点（cm）与接触法线（单位向量，指向弹丸一侧）。 */
        FVector Location = FVector::ZeroVector;
        FVector Normal = FVector::ZeroVector;

        /** 撞击前/后的线速度（cm/s）与在 XZ 平面内的角度（度，+X 为 0，逆时针为正）。 */
        FVector IncomingVelocity = FVector::ZeroVector;
        FVector OutgoingVelocity = FVector::ZeroVector;
        double IncomingAngleDeg = 0.0;
        double OutgoingAngleDeg = 0.0;

        /** 入射/出射速度大小（cm/s）。 */
        double IncomingSpeedCmPerS = 0.0;
        double OutgoingSpeedCmPerS = 0.0;

        /** 施加给被撞体的冲量大小（kg·cm/s）。静态体上恒为 0。 */
        double AppliedImpulseKgCmPerS = 0.0;

        /** 撞后弹丸的自转（°/s，绕 Y）。 */
        double SpinDegPerS = 0.0;

        /** 被撞体是否在参与求解（地基/墙为 false）。 */
        bool bHitSimulatingBody = false;

        /** 该次接触是否产生了真正的反弹（法向速度反向）。擦碰时为 false。 */
        bool bBounced = false;
    };

    /**
     * 奶蛋弹丸驱动：**手写积分 + 逐子步 Sweep**，不挂 Chaos 刚体、不用 UProjectileMovementComponent。
     *
     * 为什么不用 UProjectileMovementComponent：
     *   它的重力模型写死 980 cm/s²（ProjectileGravityScale 只能倍率），与项目的
     *   1568 cm/s² 差 1.6 倍；而且它按 TickComponent 走、跟帧率相关，同一次发射
     *   在掉帧时不复现。手感与"锁步可复现"这两条都过不去。
     *
     * 为什么弹丸不是刚体（不 SetSimulatePhysics）：
     *   刚体的运动由 Chaos 的子步调度决定，无法保证"这一步走了多少"。手写积分才能
     *   保证每步正好 1/120 秒、每步正好一次 Sweep，从而同参数同结果。
     *   代价是弹丸不被结构块挡住（结构块不会"顶住"它），所以这里用**冲量对等**补偿：
     *   每次接触把弹丸动量变化量的反向量施加给被撞刚体，双向作用仍然成立。
     *
     * 与结构倒塌的关系（重要）：
     *   弹丸是锁步的（固定子步、固定初速、不异步），**结构倒塌不锁步** ——
     *   倒塌走 Chaos 的正常 Tick。锁步只用于"同一发可以复现"，不用于"把整场物理冻住"。
     *
     * 生命周期：Initialize() 一次 → Launch() 一次 → 每帧 Advance(dt) → IsFinished() 为真后停。
     * 不使用 UObject，不参与 GC；持有的是外部 Actor 的弱引用语义指针（IsValid 检查过）。
     */
    class FNaiwaP2ProjectileDriver
    {
    public:
        /** 一发弹丸的全部数值参数。默认值全部来自 NaiwaP2PhysicsProfile.h，不要在这里另写数。 */
        struct FParams
        {
            double RadiusCm = NaiwaP2::BaseProjectileRadiusM * NaiwaP2::Physics::CentimetersPerMeter;
            double MassKg = NaiwaP2::Physics::BaseEggMassKg;
            double Restitution = NaiwaP2::Physics::EggRestitution;
            double TangentRetention = NaiwaP2::Physics::ProjectileTangentRetention;
            double SpinCoupling = NaiwaP2::Physics::ProjectileSpinCoupling;
            double GravityCmPerS2 =
                NaiwaP2::Physics::GravityMPerS2 * NaiwaP2::Physics::CentimetersPerMeter;
            double SubstepSeconds = NaiwaP2::Physics::ProjectileSubstepSeconds;
            int32 MaxSubstepsPerAdvance = NaiwaP2::Physics::ProjectileMaxSubstepsPerAdvance;
            double MaxFlightSeconds = NaiwaP2::Physics::ProjectileMaxFlightSeconds;
            double SettleSpeedCmPerS = NaiwaP2::Physics::ProjectileSettleSpeedCmPerS;
            double SettleSeconds = NaiwaP2::Physics::ProjectileSettleSeconds;
            double StopSpeedCmPerS = NaiwaP2::Physics::ProjectileStopSpeedCmPerS;
            double GroundCenterZCm = NaiwaP2::Physics::ProjectileGroundCenterZCm;
            double BounceSeparationCm = NaiwaP2::Physics::ProjectileBounceSeparationCm;
            double MaxSpinDegPerS = NaiwaP2::Physics::ProjectileMaxSpinDegPerS;
        };

        /**
         * 绑定世界与视觉。
         *
         * @param InWorld       扫掠查询用。必须非空。
         * @param InVisual      只负责"看起来在哪"。可以为空（纯数值测试）。
         * @param InOwner       扫掠时忽略的 Actor（通常就是视觉 Actor 自己）。
         * @param InParams      数值参数。
         */
        void Initialize(UWorld* InWorld, AActor* InVisual, AActor* InOwner, const FParams& InParams);

        /** 从指定位置与速度发射。会清空上一次的接触记录。 */
        void Launch(const FVector& PositionCm, const FVector& VelocityCmPerS, double SpinDegPerS = 0.0);

        /**
         * 推进物理：内部把 DeltaSeconds 切成固定的 SubstepSeconds 整数份，绝不"这一步走多久"。
         *
         * 剩余不足一个子步的时间留在 Accumulator 里，下一帧继续 —— 这保证不管帧率怎么变，
         * 弹丸走过的子步序列都是同一个。被丢弃的补偿量（超过 MaxSubstepsPerAdvance）会累加到
         * DroppedSeconds 并打一次警告，暗示这一帧掉帧了。
         */
        void Advance(double DeltaSeconds);

        /** 立即停止（命中结算、重试、销毁前调用）。 */
        void Stop();

        bool IsActive() const { return bActive; }
        bool IsFinished() const { return !bActive; }

        /** 结束原因，用于日志：StillFlying / Settled / Ground / OutOfBounds / Timeout / Stopped。 */
        const TCHAR* GetFinishReason() const;

        const FVector& GetPositionCm() const { return PositionCm; }
        const FVector& GetVelocityCmPerS() const { return VelocityCmPerS; }
        double GetSpinDegPerS() const { return SpinDegPerS; }
        double GetElapsedSeconds() const { return ElapsedSeconds; }
        int32 GetSubstepCount() const { return Substeps; }
        double GetRadiusCm() const { return Params.RadiusCm; }
        const TArray<FProjectileImpact>& GetImpacts() const { return Impacts; }

        /** 本发是否发生过"扫掠起点已在几何内部"（真正的穿模，不是擦碰）。 */
        bool HasStartPenetration() const { return bHadStartPenetration; }

        /**
         * 发射参数 → 初速（cm/s）。与 FBallistics::MakeLaunchState 同一套角度/速度约定，
         * 但输出单位是 cm/s 与弹性半径，方便直接喂给驱动。
         *
         * 保留这个换算在驱动这边，是为了让"发射"这件事只有一处定义；
         * GameMode 只负责把 AimAngle/AimPower 传进来。
         */
        static FVector MakeLaunchVelocityCmPerS(double AngleDegrees, double Power,
                                                const NaiwaP2::FEggSpec& Egg);

    private:
        /** 走一个固定子步：积分 → Sweep → 反弹/冲量 → 事件。 */
        void StepOnce();

        /** 处理一次接触：反弹、摩擦、自转、对冲量、记事件。返回反弹后的速度是否还够继续飞。 */
        bool ResolveImpact(const FHitResult& Hit, const FVector& StepStart);

        /** 推进视觉 Actor（没有视觉时为无操作）。 */
        void SyncVisual();

        UWorld* World = nullptr;
        AActor* Visual = nullptr;
        AActor* OwnerActor = nullptr;
        FParams Params;

        FVector PositionCm = FVector::ZeroVector;
        FVector VelocityCmPerS = FVector::ZeroVector;
        double SpinDegPerS = 0.0;

        double AccumulatorSeconds = 0.0;
        double DroppedSeconds = 0.0;
        double ElapsedSeconds = 0.0;
        double SettledSeconds = 0.0;
        int32 Substeps = 0;

        bool bActive = false;
        bool bHadStartPenetration = false;
        FString FinishReason = TEXT("StillFlying");

        TArray<FProjectileImpact> Impacts;
    };
}
