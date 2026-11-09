# P7 A1 实际玩法特效 v04

日期：2026-11-01（Asia/Shanghai）  
状态：精修候选，等待视觉评审；未导入 UE，未接入运行时事件。

## 本版表现的 P7 玩法

| 组别 | 四帧顺序 | 对象对应关系与结果 |
|---|---|---|
| SHOT | 炮口蓄亮 → 金色弹体飞出 → 普通敌受击 → 命中特效消退 | **攻击方：奶鸡炮塔 → 受击方：普通敌**；场景中的奶蛋是防守终点 |
| CLEAR | 炮塔击中饼干箱 → 裂纹扩散 → 碎片散开 → 建造位开放、金币出现 | **攻击方：奶鸡炮塔 → 受击对象：饼干箱**；清除结果是建造位开放并出现金币 |
| UPGRADE | L1 塔就位 → 升级能量环 → 金币投入 → L2 塔完成 | **消耗：金币 → 升级对象：奶鸡炮塔 L1 → 奶鸡炮塔 L2** |
| JAM | 干扰敌到触发点 → 紫色电击传递 → 炮塔锁定停火 1.5 秒 → 解除并恢复开火 | **干扰方：干扰敌 → 受影响炮塔：奶鸡炮塔**；停火 1.5 秒后恢复攻击 |

## 精修内容

- 逐帧改为多层结构：柔和径向光源、明暗渐层、白色高光、外缘色光与残留轨迹。
- 命中使用收束弹道、碰撞闪光和碎屑；清障加入饼干裂缝、立体碎片与建造位显现。
- 升级加入绕塔能量环、向上能量和金币汇入路径；干扰加入带芯闪电、干扰脉冲、红色锁定反馈与绿色复攻反馈。
- 正式图集只放可叠加特效。评审页使用现有 P7 角色、炮塔、饼干箱和金币逐帧合成，明确特效的触发对象与玩法结果。

## 输入资源

- 奶鸡炮塔 L1/L2：`Towers/A1/P7_A1_Tower_SingleTarget_MilkChicken_L1_v01.png`、`Towers/A1/P7_A1_Tower_SingleTarget_MilkChicken_L2_v03.png`
- 普通敌、干扰敌：`Enemies/A1/P7_A1_Enemy_Normal_v01.png`、`Enemies/A0/P7_A0_Enemy_Jammer_v01.png`
- 饼干箱状态：`Environment/A1/P7_A1_DestructibleObstacleAtlas_v02.png`
- 金币、奶蛋：`Environment/A1/P7_A1_GuardianAndCoinAtlas_v02.png`、`Environment/A1/P7_A1_GuardianMilkEgg_v04.png`

## 交付与限制

- 源图集：`VFX/A1/P7_A1_VFXAtlas_v04.svg`
- 透明 PNG：`VFX/A1/P7_A1_VFXAtlas_v04.png`，848×848，RGBA 透明底，4×4，每帧 192×192，格间留 16px。
- 逐帧评审预览：`tmp/P7_A1_VFXGameplayReview_v04.png`
- 独立特效帧用途图：`tmp/P7_A1_VFXFrameLegend_v04.png`，16 帧逐格标明触发方、作用对象和当前帧表现。
- 资产 ID、帧序、源/导出哈希、图集坐标与 Unreal 目标路径：`Manifests/A1/P7_A1_VFXAssets_v04.csv`
- 特效内容是 P7 实际对象上的评审候选和透明帧资产，不等于已经在游戏运行。帧时长、缩放、锚点、UE 导入压缩及运行时事件绑定，待 P7 关卡壳建立后确认。
