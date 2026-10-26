#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"
#include "NaiwaP2Types.h"

/**
 * P2 全物理改造：**物理与碰撞反馈参数的唯一真源**。
 *
 * 约定（改这一轮的人请遵守）：
 *   1. 所有可调数值只在这里出现一次。其它文件里出现"魔数"一律算 bug。
 *   2. 这里只有常量与纯函数，没有 UObject、没有状态，任何 TU 都可以 include。
 *   3. 数值改完必须重新编译（不是 CVar）。理由见下面「为什么不用 CVar」。
 *
 * 为什么不用 CVar 做实时调参：
 *   FBodyInstance 的 SleepFamily / 质量 / 物理材质是在 SetSimulatePhysics(true) 那一刻
 *   被推进 Chaos 的，改 CVar 之后已经生成的刚体不会跟着变，会出现"同一个塔里两块参数不同"，
 *   比不能热调更糟。要热调就整体重建场景（重试那条路），不要做半热。
 *
 * 为什么值取这些数（关键几条，后面调参时别推翻结论）：
 *   - 深度 DepthMeters = 1.5：FBox2 只有 X/Z，Y 是物理化才引入的维度。1.5 不是新编的，
 *     是既有 SpawnAttemptScene 渲染立方体时用的 Y 缩放，沿用它可以做到"几何与观感都不变"。
 *   - 密度分档而不是统一质量：柱 0.30×1.90 与板 3.20×0.30 若质量接近，撞一根柱整排会被
 *     同时带走，读起来像"整排飞"。按体积×密度算质量，柱与板的惯性差才立得住。
 *   - 摩擦 0.80/0.85（静）：细柱高宽比 6.3，摩擦低于 0.5 时它会在自己倒之前先"滑"，
 *     表现为塔尖整体平移而不是倾覆。
 *   - 重力 15.68 m/s²（1.6 倍地球）：与 NaiwaP2::GravityMPerS2 同一个数。弹丸是手写积分
 *     （M2）、刚体走 Chaos，两边必须是同一个重力，否则蛋和完整建筑块的下落速度会对不上。
 */
namespace NaiwaP2
{
namespace Physics
{
    // ---------------------------------------------------------------------
    // 场景尺寸
    // ---------------------------------------------------------------------

    /**
     * 结构块的深度（Y 轴，米）。
     *
     * 必须与 SpawnAttemptScene 里非目标块使用的 Y 缩放一致（1.5）。改这个数等于改关卡观感
     * 与所有刚体的质量，属于"改几何"，不在本次授权范围内。
     */
    constexpr double DepthMeters = 1.5;

    /** 厘米换算，与 GameMode 里的 CentimetersPerMeter 保持同一个值。 */
    constexpr double CentimetersPerMeter = 100.0;

    // ---------------------------------------------------------------------
    // 重力
    // ---------------------------------------------------------------------

    /**
     * 世界重力（m/s²，正向表示下落加速度）。
     *
     * 直接复用 NaiwaP2::GravityMPerS2，不在这里另写一个数。Chaos 里没有 per-body
     * gravity scale（UE 5.8 的 FBodyInstance 只有 SetEnableGravity / SetGravityGroupIndex），
     * 所以只能改 WorldSettings：bGlobalGravitySet = true, GlobalGravityZ = -GravityMPerS2*100。
     */
    constexpr double GravityMPerS2 = NaiwaP2::GravityMPerS2;

    /** 是否在 P2 场景生成时接管世界重力。P2 独占一张关卡，不会影响 P1/P0。 */
    constexpr bool bApplyWorldGravity = true;

    // ---------------------------------------------------------------------
    // 生成：初始缝隙（塔自稳的第一道防线）
    // ---------------------------------------------------------------------

    /**
     * 相邻两个块之间预留的净空（米，**总缝宽**，X/Z 各方向分别计算）。
     *
     * 生成时每个块的四边各内缩 SpawnGapMeters * 0.5，于是两个贴合的 FBox2 之间
     * 会多出正好 SpawnGapMeters 的空隙。
     *
     * 为什么必须留：SceneLibrary 里 250 个 FBox2 全是首尾相接的（例如 T.base 的 MaxZ=0.30
     * 正好等于 T.c1 的 MinZ=0.30）。零缝隙生成等价于初始 penetrated，Chaos 会用
     * MaxDepenetrationVelocity 把重叠一次性顶开，整座塔在第一帧就被弹飞。
     * 留缝比"精确贴合"更稳，代价是柱的宽度少了 2*HalfGap，肉眼不可见。
     *
     * 为什么是 1 mm 而不是 2 mm（实测决定，不是拍脑袋）：
     *   缝隙会被**串联累加**。2-1 顶部的 T.cap 底下叠了 5 层缝，每层沉 2 mm，
     *   5 秒后累计位移 1.07 cm，直接顶破验收第 1 条的 1 cm 门限 ——
     *   而这 1 cm 完全是"落进缝里"，不是抖动（末速 0.00）。
     *   收到 1 mm 后同一位置累计约 0.5 cm，留出余量。
     *
     * 不要为了"几何精确"把这里改成 0。这是最常见的"塔自己倒"原因。
     */
    constexpr double SpawnGapMeters = 0.001;

    /** 尺寸下限保护：小于这个宽/高的块退化成薄片，Chaos 会给出病态的惯性张量。 */
    constexpr double MinBlockDimensionMeters = 0.02;

    // ---------------------------------------------------------------------
    // 生成：支撑关系检查（不焊接、不静态化结构块）
    // ---------------------------------------------------------------------

    /**
     * 判定"这块被下面那块托住"的 Z 容差（米）。
     *
     * SceneLibrary 里的方块是首尾相接写的（A.MinZ 精确等于 B.MaxZ），理论上容差可以取 0；
     * 留 1 cm 是为了容忍生成时的 2 mm 缝隙与浮点误差。
     */
    constexpr double SupportZToleranceMeters = 0.01;

    /** 低于这个高度（米）就算"已经坐在地面上"，不参与悬空判定。 */
    constexpr double GroundSupportZ = 0.05;

    /**
     * 单块支撑接触面"够宽"的下限（米）。
     *
     * 这是「窄支撑桥接」的判据：支撑块与目标块相交但只搭上一条窄边，
     * 目标块的中心落在支撑之间。
     *
     * 2-2 的 E.d3 是后者的教科书案例：
     *   E.d3  X 11.95~13.85（宽 1.90，579 kg）
     *   E.c4  X 11.80~12.10 → 只搭上 11.95~12.10，宽 0.15 m
     *   E.c5  X 13.70~14.00 → 只搭上 13.70~13.85，宽 0.15 m
     *   合计支撑 0.30 m / 1.90 m = 15.7%，且**中心 12.90 正好落在两块之间的空档里**。
     *
     * 实测漂移（5 秒，摩擦 0.85→0.90、角阻尼 0.20→0.45 都救不回来）：
     *   E.c4  dx=-5.04 cm ; E.c5  dx=+8.15 cm, dz=+0.649 cm ; E.d3 dx=-2.75 cm
     * 两根柱子的顶端被"向外劈开"，正是重板把载荷压在两柱内缘上、柱绕底面内棱
     * 向外倾的结果 —— 不是滑动，所以加摩擦没用。
     *
     * 0.20 m 这个阈值是"能把上面两种数据缺陷挑出来、又不误伤正常块"的分界：
     *   命中（每边接触 0.15 m < 0.20）：2-2 的 E.d3、2-3 的 T.d3
     *   放过（每边接触 0.30 m = 0.20）：2-1 的 T.d2（4 根柱全宽接触，实测 0.59 cm 已通过）、
     *                                  2-2 的 B.deck（2 根柱全宽接触，实测 0.20 cm 已通过）
     * 不要为了让别的块也"更稳"而提高这个阈值：一旦把 2-1 的 T.d2 焊到 4 根柱上，
     * 打掉任意一根柱上层都不会塌 —— 那是把玩法拆了，不是把塔修稳。
     */
    constexpr double MinStableSupportPatchMeters = 0.20;

    /**
     * 桥接块允许被支撑覆盖的最大比例。
     *
     * 与上面那条是"与"关系：两个条件同时成立才判为窄支撑桥接。
     * E.d3 = 0.298/1.898 = 15.7%，T.d3(2-3) = 0.298/2.298 = 13.0%，都远低于 25%；
     * 正常压在两柱全宽上的板（B.deck = 0.60/2.10 = 28.6%）会被这条挡在外面。
     */
    constexpr double BridgeMaxCoveredFraction = 0.25;

    // ---------------------------------------------------------------------
    // 求解器：子步
    // ---------------------------------------------------------------------

    /**
     * 子步参数。这三项的真身在 DefaultEngine.ini 的 [/Script/Engine.PhysicsSettings]，
     * 因为是 UPhysicsSettings 的 config 属性、只能在 ini 或项目设置里落地。
     * 这里保留同一份数值是为了让 RuntimeCheckSolverSettings() 能在运行时比对，
     * 一旦 ini 被改而代码没改，日志会直接报出来，避免"参数漂移"这种隐性故障。
     *
     * 为什么 1/120 + 8 子步：最细的柱是 0.30 m 宽 × 1.90 m 高（高宽比 6.3），
     * 1/60 步长下单步位移足以让细柱接触点在两个子步之间"跳过"支撑面，
     * 表现为高频抖动或缓慢自倒。
     */
    constexpr double MaxSubstepDeltaTime = 1.0 / 120.0;
    constexpr int32 MaxSubsteps = 8;

    /** 异步物理会让同一发的表现不可复现，且让"静止判定"更难做。关掉。 */
    constexpr bool bTickPhysicsAsync = false;

    // ---------------------------------------------------------------------
    // 求解器：穿透与休眠
    // ---------------------------------------------------------------------

    /**
     * 最大去穿透速度（cm/s）。
     *
     * 即使留了缝，几何里仍有"看起来贴合但实际交叠"的地方（例如 2-2 的 A.plate 与 A.col
     * 在 HighPlatformApex 处相接）。默认去穿透速度上限很高，遇到 5 mm 重叠时会给出
     * 上百 cm/s 的分离速度 —— 那就是"整个塔炸开"的直接来源。这里压到 50 cm/s：
     * 1/120 秒一步最多分离 0.4 cm，2 mm 的误差两步内消化完，不产生可见弹跳。
     */
    constexpr float MaxDepenetrationVelocityCmPerS = 50.0f;

    /**
     * 休眠阈值档位。
     *
     * Sensitive = 更低的休眠阈值（更不容易被误判成静止）。选它是因为微抖被提前判睡
     * 会导致"上半塔冻在半空"，比多算几帧贵得多。注意它不是"更容易睡"，名字反着看的人很多。
     */
    constexpr ESleepFamily BlockSleepFamily = ESleepFamily::Sensitive;

    // ---------------------------------------------------------------------
    // 结构块材质（按角色分档）
    // ---------------------------------------------------------------------

    /** 结构块的角色。决定"是否参与求解"和"质量/摩擦从哪一档取"。 */
    enum class EBlockRole : uint8
    {
        /** 地基、挡墙、高台立柱与台面：静态碰撞体，不参与求解，不会倒。 */
        Foundation,
        /** 细高柱：塔的竖向支撑，高宽比大。 */
        Column,
        /** 板 / 梁 / 顶盖 / 台面：宽薄，压在柱顶上。 */
        Board,
        /** 目标蛋占位（M3 才真正物理化，这里只用于质量比校验与日志）。 */
        Goal
    };

    struct FBlockMaterial
    {
        /** 密度 kg/m³。质量 = 密度 × 体积，不写死单块质量。 */
        double DensityKgPerM3 = 520.0;

        /** 动摩擦。 */
        float Friction = 0.80f;
        /** 静摩擦。必须 > Friction，否则静止的柱会被自己的重量推动。 */
        float StaticFriction = 0.85f;
        /** 恢复系数。结构块要"闷"，弹起来就是假。 */
        float Restitution = 0.12f;
        /** 线性阻尼。只用来吃掉残余抖动，不能大到让倒塌变慢动作。 */
        float LinearDamping = 0.05f;
        /** 角阻尼。细柱倒下要靠它收住绕 Y 轴的残余摆动。 */
        float AngularDamping = 0.20f;

        /** 是否参与物理求解。地基/墙为 false。 */
        bool bSimulate = true;
        /** 连续碰撞检测。结构块不需要（不做子弹），开它纯亏性能。 */
        bool bUseCCD = false;
    };

    inline FBlockMaterial GetBlockMaterial(const EBlockRole Role)
    {
        FBlockMaterial Result;
        switch (Role)
        {
        case EBlockRole::Foundation:
            // 地基不参与求解，质量/阻尼都无意义；摩擦给高，让建筑块落地后尽快停稳。
            Result.DensityKgPerM3 = 2000.0;
            Result.Friction = 0.90f;
            Result.StaticFriction = 0.95f;
            Result.Restitution = 0.05f;
            Result.bSimulate = false;
            break;

        case EBlockRole::Column:
            // 0.30×1.5×1.90 = 0.855 m³ × 520 ≈ 445 kg（2-1 的 T.c1 实测值见日志）。
            // 比板轻是**故意**的：柱子"细而脆"，板"宽而沉"，撞倒柱子后板才有东西压下来。
            Result.DensityKgPerM3 = 520.0;
            Result.Friction = 0.85f;
            // 静摩擦取到 0.90（规格上限）：2-2 的 E.c4/E.c5 在 0.85 下会被上方的
            // E.d3（579 kg 压在两根 279 kg 的柱顶）**撑开** —— 实测左柱 -5.07 cm、
            // 右柱 +8.15 cm、右柱还被抬起 0.645 cm，是典型的"重板楔在两柱之间"。
            // 摩擦是这里唯一能压住横向滑移的东西，往下调会让它更松。
            Result.StaticFriction = 0.90f;
            Result.Restitution = 0.12f;
            Result.LinearDamping = 0.05f;
            // 角阻尼 0.45：这个楔入是从柱子轻微倾斜开始的。柱子一斜，顶面变成斜面，
            // 上面的板就顺着斜面滑，越滑越斜。角阻尼直接压住这个正反馈的起点。
            Result.AngularDamping = 0.45f;
            break;

        case EBlockRole::Board:
            // 密度更高：胶合板/复合板。3.20×1.5×0.30 = 1.44 m³ × 680 ≈ 979 kg。
            Result.DensityKgPerM3 = 680.0;
            Result.Friction = 0.85f;
            Result.StaticFriction = 0.90f;
            Result.Restitution = 0.10f;
            Result.LinearDamping = 0.05f;
            Result.AngularDamping = 0.40f;
            break;

        case EBlockRole::Goal:
        default:
            // 目标蛋在 M3 之前不生成刚体，这里给的值只用于日志里的质量比自检。
            Result.DensityKgPerM3 = 240.0;
            Result.Friction = 0.60f;
            Result.StaticFriction = 0.70f;
            Result.Restitution = 0.35f;
            Result.LinearDamping = 0.02f;
            Result.AngularDamping = 0.10f;
            break;
        }
        return Result;
    }

    // ---------------------------------------------------------------------
    // 弹丸（M2 使用；M1 只用来做质量比自检）
    // ---------------------------------------------------------------------

    /**
     * 基准蛋的质量（kg）。
     *
     * 提高到 70 kg，保持当前已经调顺的飞行速度，同时增加发射蛋的动量，
     * 改善完整建筑刚体受到直接撞击时的可见响应。2-1 细柱约 443 kg，质量比约 1 : 6.3；
     * 这仍保留轻蛋撞重结构的关系。特殊蛋继续按体积倍率缩放质量。
     */
    constexpr double BaseEggMassKg = 70.0;

    /**
     * 恢复系数：蛋撞结构块时想让它"弹开带旋转"而不是停住或穿过去。
     * 组合模式取 Max（见 NaiwaP2StructureBlock.cpp 里的物理材质设置），
     * 所以实际生效值是本数与结构块值中的较大者。
     */
    constexpr float EggRestitution = 0.38f;
    constexpr float EggFriction = 0.45f;

    /** 蛋用 CCD：18 m/s × 1/120 步长 ≈ 15 cm/步，与 30 cm 厚的柱同一量级，必须开。 */
    constexpr bool bEggUseCCD = true;

    inline double GetEggMassKg(const NaiwaP2::FEggSpec& Egg)
    {
        // 质量按体积缩：半径倍率的三次方。豆奶 RadiusMultiplier = 1.90 → 约 6.86 倍。
        const double VolumeScale = Egg.RadiusMultiplier * Egg.RadiusMultiplier * Egg.RadiusMultiplier;
        return BaseEggMassKg * VolumeScale;
    }

    // ---------------------------------------------------------------------
    // F1 Chaos 物理沙盒
    // ---------------------------------------------------------------------

    /** 单目标测试用球体质量。 */
    constexpr double SandboxGoalMassKg = 18.0;
    /** 静止测试弹丸的速度阈值与持续时间；碰撞后低于该值才结束本次射击。 */
    constexpr double SandboxShotSettleSpeedCmPerS = 100.0;
    constexpr double SandboxShotSettleSeconds = 0.45;
    /** 兜底飞行上限，避免未触碰任何物体的测试弹丸永久占用回合。 */
    constexpr double SandboxShotMaxSeconds = 12.0;
    /** 沙盒测试区域的水平扩展边界（米），出界后结束本次射击。 */
    constexpr double SandboxWorldMarginMeters = 3.0;

    /** 正式物理关卡中，目标蛋被接触结算为击中的最低冲量与相对速度。 */
    constexpr double GoalHitMinImpulseKgCmPerS = 4000.0;
    constexpr double GoalHitMinRelativeSpeedCmPerS = 200.0;
    /** 完整关卡的单发最大等待时间；弹丸离场后仍等受影响结构稳定。 */
    constexpr double PhysicsLevelShotMaxSeconds = 18.0;
    constexpr double PhysicsLevelProjectileSettleSpeedCmPerS = 100.0;
    constexpr double PhysicsLevelSceneSettleSpeedCmPerS = 8.0;
    constexpr double PhysicsLevelSceneSettleAngularSpeedDegPerS = 8.0;
    constexpr double PhysicsLevelSettleSeconds = 0.60;

    // ---------------------------------------------------------------------
    // 弹丸驱动（M2 / 第 3 批）：手写积分 + Sweep
    // ---------------------------------------------------------------------

    /**
     * 弹丸固定子步（秒）。
     *
     * 1/120，与刚体子步同一个数。为什么弹丸也不许跟帧：
     *   18 m/s 满力下 1/60 一步走 30 cm，正好等于柱厚；再快一点就会"这一步在柱前、
     *   下一步在柱后"，那是穿透而不是碰撞。1/120 一步 15 cm，配合 Sweep 逐段求交，
     *   才是"不穿模"的来源（Sweep 与子步是两道独立的保险，不能只留一道）。
     *
     * 为什么不用 UProjectileMovementComponent：它的重力模型写死 980 cm/s²，
     * 与项目的 15.68 m/s²（1568 cm/s²）差 1.6 倍；而且它的子步行走在
     * TickComponent 里、依赖帧率，同一次发射在掉帧时不复现。
     */
    constexpr double ProjectileSubstepSeconds = 1.0 / 120.0;

    /**
     * 单次 Advance 最多推进多少个子步。
     *
     * 掉帧保护：一帧 200 ms 本该走 24 个子步，全走会把这一帧拖成两帧，越拖越长。
     * 上限 8 个子步 = 66 ms 的余量，超出部分丢弃并打日志 —— 宁可让弹丸慢半帧，
     * 也不要让"卡顿 → 物理补偿 → 更卡"这个正反馈成立。
     */
    constexpr int32 ProjectileMaxSubstepsPerAdvance = 8;

    /** 静止判定：线速度低于这个值（cm/s）并持续 ProjectileSettleSeconds 就算停住。 */
    constexpr double ProjectileSettleSpeedCmPerS = 5.0;
    constexpr double ProjectileSettleSeconds = 0.3;

    /** 硬超时（秒）。弹丸不会飞得比这更久；到点强制结算，防止"永远不结束"。 */
    constexpr double ProjectileMaxFlightSeconds = 8.0;

    /**
     * 地面高度（cm，蛋心）。
     *
     * 与 NaiwaP2::GroundCenterZM = -0.60 m 同一个数：蛋半径 0.44 m 时"蛋心落到 -0.60 m"
     * 就等于"贴地"。这里沿用旧弹道的判据，避免同一发在旧 HUD 预览和新物理里
     * 落地时刻不一样。
     */
    constexpr double ProjectileGroundCenterZCm = NaiwaP2::GroundCenterZM * CentimetersPerMeter;

    /**
     * 反弹后切向速度的保留比例。
     *
     * 1.0 = 完全无摩擦的光滑弹跳（会一直滑），0.0 = 撞上就粘住。
     * 取 1 - EggFriction = 0.55：蛋壳是滑的，但撞击必须损失切向速度，
     * 否则反弹后的蛋会以原速蹭着柱面走，读起来像"没撞到"。
     */
    constexpr double ProjectileTangentRetention = 1.0 - EggFriction;

    /**
     * 反弹时把切向速度转成自转的耦合系数。
     *
     * 纯滚动应是 ω = v_t / r；这里只取一部分（0.75），因为蛋不是球，
     * 而且全耦合会让它一撞就疯转，视觉上像陀螺。
     */
    constexpr double ProjectileSpinCoupling = 0.75;

    /** 自转角速度上限（°/s）。防止除零（半径极小）或数值爆掉后网格转成一片模糊。 */
    constexpr double ProjectileMaxSpinDegPerS = 1440.0;

    /** 单次 Sweep 命中后沿法线推出去的距离（cm）。太小会连续两帧都判定为"已穿入"。 */
    constexpr double ProjectileBounceSeparationCm = 0.05;

    /** 反弹后线速度低于这个值（cm/s）就认为"撞停了"，直接结束这一发。 */
    constexpr double ProjectileStopSpeedCmPerS = 20.0;

    /** 蛋与静态体（地基/墙）碰撞时的冲量缩放：静态体不吃冲量，这里只用于日志量级。 */
    constexpr double ProjectileStaticImpactImpulseScale = 1.0;

    // ---------------------------------------------------------------------
    // 预算与诊断
    // ---------------------------------------------------------------------

    /** 碰撞事件数据输出开关。调参期必须开，这些数就是调参依据。 */
    constexpr bool bLogCollisionEvents = true;

    /**
     * 碰撞日志的冲量门限（kg·cm/s）。
     *
     * 塔自稳阶段每秒有上百条微接触（块与块、块与地基），全打出来会把真正的撞击埋掉。
     * 这个门限只作**日志过滤**用，不影响求解。要排查微弱抖动时改成 0。
     */
    constexpr double CollisionLogMinImpulseKgCmPerS = 500.0;

    /** Hit 回调的日志速度门限。低速堆叠/接地回调不作为有效撞击输出。 */
    constexpr double CollisionLogMinRelativeSpeedCmPerS = 25.0;

    /** 同一对象对的碰撞日志冷却时间；只去重诊断输出，不影响 Chaos 接触求解。 */
    constexpr double CollisionLogPairCooldownSeconds = 0.20;

    /** 鸡叫只响应蛋撞击建筑时的有效动能；用碰撞冲量和约化质量估算，单位 J。 */
    constexpr double CollisionSoundMinKineticEnergyJoules = 120.0;
    /** 鸡叫最短间隔；同时按音频时长延长，防止连续重叠播放。 */
    constexpr double CollisionSoundMinIntervalSeconds = 0.60;

    /** 撞击 Niagara 特效的冲量与速度门槛；高于此档的接触才产生可见反馈。 */
    constexpr double ImpactEffectMinImpulseKgCmPerS = 3500.0;
    constexpr double ImpactEffectMinRelativeSpeedCmPerS = 80.0;
    /** 特效自身的对象对冷却时间，不改变物理或碰撞日志冷却。 */
    constexpr double ImpactEffectPairCooldownSeconds = 0.18;
    /** 全场 Niagara 爆点最短间隔，避免塔倒塌时特效同时铺满画面。 */
    constexpr double ImpactEffectGlobalCooldownSeconds = 0.10;
    constexpr double ImpactEffectScaleImpulseRangeKgCmPerS = 60000.0;
    constexpr double ImpactEffectMinScale = 0.45;
    constexpr double ImpactEffectMaxScale = 0.90;
    constexpr double ImpactEffectNormalOffsetCm = 2.0;

    /** 自稳诊断：每秒打印一次每块相对生成位置的位移，用来验"5 秒内 < 1 cm"。 */
    constexpr bool bLogStabilityProbe = true;

    /** 验收第 1 条的判据数值，写在这里是为了让自检代码和验收报告用同一个数。 */
    constexpr double StabilityTestSeconds = 5.0;
    constexpr double StabilityMaxDriftCm = 1.0;
}
}
