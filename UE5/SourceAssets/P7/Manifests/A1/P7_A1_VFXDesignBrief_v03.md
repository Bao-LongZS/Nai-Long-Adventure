# P7 A1 实际玩法特效 v03（已被 v04 精修取代）

日期：2026-11-01（Asia/Shanghai）  
状态：v02 泛化示意稿已被实际玩法稿取代；用户反馈 v03 画面粗糙，已由 v04 精修取代。v03 仅作版本追溯，尚未导入 UE 或接入运行时。

## 这批特效对应的 P7 事件

| Group | 连续帧（左到右） | 实际玩法与对象 |
|---|---|---|
| SHOT | 炮口亮起 → 金色弹体飞出 → 普通敌受击 → 命中特效消退 | 使用奶鸡单体塔攻击普通敌；背景示意敌人正沿路线向奶蛋终点前进 |
| CLEAR | 饼干箱受击 → 箱体裂开 → 碎片飞散 → 隐藏建造位和固定金币奖励出现 | 对应选中炮塔攻击指定障碍；清除后开放预设 CellId 建造位并展示固定奖励 |
| UPGRADE | L1 炮塔 → 升级脉冲 → 金币投入 → 实际 L2 炮塔完成 | 以通过的奶鸡 L1/L2 塔图、P7 金币表现一次升级消费 |
| JAM | 干扰敌进入固定触发点 → 紫色电击命中 → 炮塔停火 1.5 秒 → 解除并恢复开火 | 对应现有干扰敌规则；特效是显示层表现，不改变模拟状态或随机目标 |

## 使用到的游戏资源

- 奶鸡 L1：`Towers/A1/P7_A1_Tower_SingleTarget_MilkChicken_L1_v01.png`
- 奶鸡 L2：`Towers/A1/P7_A1_Tower_SingleTarget_MilkChicken_L2_v03.png`
- 普通敌：`Enemies/A1/P7_A1_Enemy_Normal_v01.png`
- 干扰敌：`Enemies/A0/P7_A0_Enemy_Jammer_v01.png`
- 饼干箱状态：`Environment/A1/P7_A1_DestructibleObstacleAtlas_v02.png`（候选）
- 金币：`Environment/A1/P7_A1_GuardianAndCoinAtlas_v02.png`
- 奶蛋终点：`Environment/A1/P7_A1_GuardianMilkEgg_v04.png`（已通过）

## 交付规格

- 特效源：`VFX/A1/P7_A1_VFXAtlas_v03.svg`；透明导出：`VFX/A1/P7_A1_VFXAtlas_v03.png`，848×848，4×4，每帧 192×192，16px gutter。
- 场景评审页：`tmp/P7_A1_VFXGameplayReview_v03.png`。这张图在实际 P7 素材上合成特效帧以说明效果发生的位置；正式特效图集只含可叠加的特效图形，不重复包含炮塔、敌人和障碍贴图。
- 资产 ID、帧序、哈希、裁切坐标与 UE 目标路径见 `P7_A1_VFXAssets_v03.csv`。
- 当前为效果帧候选，不等于 UE 中已播放的动画。播放时长、世界锚点、Actor/Widget 触发接口、压缩及 720p/1080p 验收仍待 P7 关卡壳建立后完成。
