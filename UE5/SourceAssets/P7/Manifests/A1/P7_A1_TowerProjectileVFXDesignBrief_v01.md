# P7 三塔独立弹道特效 v01

日期：2026-11-12（Asia/Shanghai）  
状态：**按用户要求拆分三类炮塔的弹道视觉。奶鸡保留既有 v07 金色尖弹；奶狗与奶鼠各使用新制作的独立特效序列。已导入 UE 并按塔 ID 接入 HUD；Editor 构建通过。PIE/Standalone 动态画面验收待完成。**

## 视觉映射

| 炮塔 | 玩法身份 | 帧序 | 动效表达 |
|---|---|---|---|
| 奶鸡 `P7_Tower_MilkChicken` | 单体攻击 | 沿用 v07 SHOT F00–F03 | 金色蓄光、尖弹飞行、单体命中、余光消散 |
| 奶狗 `P7_Tower_MilkDog` | 范围攻击 | 新图集第 0 行 F00–F03 | 暖橘喇叭蓄波、三枚共振弹群飞行、环形范围爆裂、冲击环消散 |
| 奶鼠 `P7_Tower_MilkMouse` | 减速支援 | 新图集第 1 行 F00–F03 | 薄荷晶体成形、晶体弹飞行、环形减速场扩张、脉冲环消散 |

奶鸡沿用资产：`VFX/A1/P7_A1_VFXAtlas_v07.svg/.png`，Texture2D `/Game/Naiwa/P7/VFX/A1/T_P7_A1_VFXAtlas_v07`。新增透明图集 `VFX/A1/P7_A1_TowerProjectileVFX_v01.svg/.png`，尺寸 848×432；4×2 网格，单帧 192×192，格距 16px。其 Texture2D 路径为 `/Game/Naiwa/P7/VFX/A1/T_P7_A1_TowerProjectileVFX_v01`，以 sRGB、保留 Alpha、UI 组、无 Mipmap 导入。

HUD 根据模拟快照中的 `TowerId` 将攻击事件分流到各自序列。四帧持续 0.48 秒：蓄能 0.08 秒，飞行至 0.26 秒，命中至 0.36 秒，余效淡出至结束。此处只添加画面反馈，不改变塔的攻击间隔、伤害、射程、范围或减速值，也不改变模拟数据。

## 资产与验证

- 逐帧 AssetId、坐标、哈希、用途和导入状态：[`P7_A1_TowerProjectileVFXAssets_v01.csv`](P7_A1_TowerProjectileVFXAssets_v01.csv)。
- 三塔对照评审页：[`P7_A1_TowerProjectileVFXReview_v01.png`](../../VFX/A1/Review/P7_A1_TowerProjectileVFXReview_v01.png)。
- 可编辑图集源：[`P7_A1_TowerProjectileVFX_v01.svg`](../../VFX/A1/P7_A1_TowerProjectileVFX_v01.svg)。
- UE 纹理导入和设置/源文件回读报告：`UE5/Records/p7_td/evidence/P7_A1_Catalog003AndTowerProjectileVFXImport_2026-11-12_10-54-44.json`。
- HUD 按塔 ID 分流后通过 `Nai_LongEditor Development` 编译；PIE/Standalone 中的动态清晰度、遮挡与实机尺度尚待验收。
