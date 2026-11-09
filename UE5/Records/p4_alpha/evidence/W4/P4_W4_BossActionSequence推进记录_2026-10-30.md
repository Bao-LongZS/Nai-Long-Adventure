# P4 W4 Boss 攻击姿势序列推进记录（2026-10-30）

状态：**新增八张相邻姿势，除待机外八个 Boss 状态均有双姿势事件/节奏候选；动画结构仍属 WIP。**

## 本批姿势

- `charge_telegraph / side_right / 01`：蓄力后段，降低重心、面向右侧、桶盖仍低挂髋侧。
- `charge_attack / side_right / 01`：第二个贴地冲锋跨步姿势；沿用直线朝右方案，供冲锋段交替播放候选。
- `lid_slam_telegraph / front / 01`：举起桶盖、双脚压地、脸部保持可读的砸地预警末段姿势。
- `lid_slam_impact / front / 01`：落地后的低位跟随姿势；砸击地波另由独立 VFX 表现。
- `move / side_right / 01`：区别于冲锋的普通追击跨步。
- `hit / front / 01`：受击后的回正姿势，保持与死亡败退姿势可区分。
- `phase_two / front / 01`：强化阶段站姿；强化提示光效由独立 VFX 表现。
- `death / front / 01`：侧倾、闭眼的败退跟随姿势。

四张高分辨率源 PNG 放在 `UE5/SourceAssets/P4/Characters/Alpha/W4_Boss/RuntimeWIP/Sequences/SourceFrames/`，512×512 帧放在相邻的 `Frames512/` 目录。它们分别和现有状态帧 `00` 组成序列；源图和候选帧的哈希/可见边界记录在 [manifest v05](../../../../SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v05.json)。[动作评审页](P4_W4_BossActionSequenceReview_v01.html)展示了四组双姿势。

## 对拍时长与事件

| 状态 | 序列时长提案 | 每帧时长 | 事件/播放 |
|---|---:|---:|---|
| `charge_telegraph` | 0.90 s | 0.45 s × 2 | `boss_telegraph_charge`，一次播放；沿用工程预警时间 |
| `charge_attack` | 跟随模拟结果 | 0.10 s × 2 | 实际冲锋中循环；`boss_charge_hit` / `boss_charge_missed` 收尾 |
| `lid_slam_telegraph` | 1.05 s | 0.525 s × 2 | `boss_telegraph_lid_slam`，一次播放；沿用工程预警时间 |
| `lid_slam_impact` | 0.24 s | 0.12 s × 2 | `boss_lid_slam_hit` / `boss_lid_slam_missed`；独立砸地 VFX 另播 |
| `move` | 0.24 s 循环 | 0.12 s × 2 | 普通追击循环候选，不与冲锋混用 |
| `hit` | 0.20 s | 0.10 s × 2 | 恢复时长提案；伤害事件由工程状态处理 |
| `phase_two` | 1.35 s | 0.675 s × 2 | `boss_phase_two`；沿用工程阶段切换保护时间，VFX 独立 |
| `death` | 0.36 s | 0.18 s × 2 | 败退表现时长提案，事件钩子待工程确认 |

这些时间只用于候选姿势显示，不改变工程锁定方向、冲锋距离、砸地范围、伤害或保护时长。画布仍按临时脚底锚点 `(256,466)` 导出；新帧的脚底边缘与画布外沿检查通过，实际接触感仍需固定机位复核。

## 未完成

- 八组都是双姿势样本，缺少完整中间帧、起动/收势细节、正背侧连贯性、桶盖分层动画与低闪烁替代版本。
- `idle` 是全身缩放呼吸预览，不含眼睛、四肢或桶盖独立动作；各非待机状态的真实帧时长与工程状态机需进一步对拍。
- 尚未导入 UE；需完成可编辑源文件、轮廓边缘精修、0.82 固定机位和工程 PIE 事件对拍后才可进入 RuntimeReady。
