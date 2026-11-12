# P7 美术源资产目录

此目录归 `Nai-Long-Adventure` 项目的 P7 子模式。这里保存可编辑源文件和导出 PNG；UE 导入件放在 `UE5/NaiwaP0/Content/Naiwa/P7/`，不可把 `.uasset`、临时拼图或其他关卡副本放在这里。玩法范围以 [P7 综合计划书 v1.1](../../Records/p7_td/P7_奶罐守卫战_综合计划书_v1.1.md) 为基线；终点守护物改为奶蛋，按[美术变更记录](../../Records/p7_td/P7_美术变更记录_2026-11-01_奶蛋终点.md)执行。

| 子目录 | 内容 | 归属规则 |
|---|---|---|
| `Characters/` | α HUD 奶蛙 2D 头像候选或 P7 专属表情 | Common 候选须先通过角色身份与透明边缘验收；α 不因角色稿未就绪而阻塞 |
| `Towers/` | P7 塔体、升级态、功能图标 | P7 独占，不复用竞品塔图 |
| `Enemies/` | P7 敌人立绘与动作帧 | P7 独占，需与塔的轮廓/明度明确区分 |
| `Environment/` | 地面、A/B 预设路线、分岔门、可破坏物、奶蛋终点与地图组件 | 关卡专属主题可留 P7；被第二关复用且验收通过后再评估 Common；限时占路物属于 β 候选 |
| `UI/` | P7 塔栏、波次、金币、目标状态和结算图形 | 可复用的无主题 UI 基础件需有第二处实际引用后才进入 Common |
| `VFX/` | 子弹、命中、破坏、金币、升级提示 | P7 专属效果不默认晋升 Common |
| `Audio/` | 如有批准的原创声音源文件 | v1 不要求新增音频；已有音效只列候选，试听/授权通过后由工程接入 |
| `Manifests/` | 资产 ID、源路径、版本、哈希、导出和导入设置 | 每次交付同步更新；不以文件名“最终版”作为版本管理 |

每个正式资源需要记录 `AssetId`、源文件、版本/哈希、用途、尺寸/帧数、透明与颜色空间、UE 目标路径、状态和验收记录。敌人组含一个具有固定触发点干扰表现的原创类别；受扰与恢复图形要独立、可读。不得将《保卫萝卜》的截图、拆包资源、商标或临摹关卡图作为交付资产。聊天中临时展示的观感预览未纳入美术输入；仅按计划书和 G1/A0 签收样片制作。



## A0 样片状态

A0 首批原创候选资产与记录位于各类别的 `A0/` 子目录及 `Manifests/A0/`。当前奶蛙 HUD 两帧候选为 `Characters/A0/P7_A0_Character_MilkFrogSmile_v03.png`（看向/微笑前帧）→ `Characters/A0/P7_A0_Character_MilkFrogLaugh_v05.png`（按 Big_Laugh.png 修订的后帧），横向图集为 `Characters/A0/P7_A0_Character_MilkFrogHUD_SmileLaugh_2F_v04.png`。用户提供的标准奶蛙参考用于身份修订；播放时长尚未指定。按 P7 计划，角色只用于 HUD。

用户于 2026-11-11 明确要求直接使用所附《奶蛙学 必修第一册》附录图录图片。四张姿态原图裁切放在 `Characters/A1/CatalogDirect/`，外部原 PDF 保留在用户素材库；不再沿用“PDF 本身没有复制到这里”的旧说明。来源页码、图录编号、哈希及状态见 `Manifests/A1/P7_A1_CatalogHUDPortraitAssets_v02.csv`。四姿态于 2026-11-12 获用户通过，已导入 UE 并接入 HUD；实际 viewport 仍待验收。

v04 已记录奶蛙微笑/大笑帧、干扰敌和地面底图样片获用户通过，可进入 α 批量制作；这只是样片签收，不代表整批美术完成。用户通过的地面主底图 `Environment/A0/P7_A0_GroundBoard_Field_v01.png` 已导入 `/Game/Naiwa/P7/Environment/A0/T_P7_A0_GroundBoard_Field_v01`，正接入 P7 棋盘底层；实际裁切贴合、720p/1080p 与透明边缘验收仍待完成。其他 A0 候选尚未导入。详细规格、哈希、版本与提示词见 `Manifests/A0/`（当前版本为 v04）。


### A1 图录直用 HUD 姿态 v03 · 大笑已换为站立捧腹

当前四个独立状态图为常态图录 139、大笑图录 003（站立捧腹）、胜利图录 121、失守图录 106。图录 003 直接从 PDF 第 107 页裁切，保留白底，不抠透明、不改色、不重绘；原图录 079 与旧纹理保留追溯，不再用于 HUD。预览板 `Characters/A1/Review/P7_A1_CatalogHUDPortraitReview_v04.png` 同时展示原图裁切和真实 48px 下采样结果（2× 最近邻显示）。四张 RGB 纹理为 sRGB、UI 组、无 Mipmap，已导入 `/Game/Naiwa/P7/Characters/A1/CatalogDirect/`，接到布防/防守头像与胜负结算面板。实际 PIE/Standalone、UMG 布局和 720p/1080p 验收待完成。报告和设计说明见 `UE5/Records/p7_td/evidence/P7_A1_Catalog003AndTowerProjectileVFXImport_2026-11-12_10-54-44.json`、`Manifests/A1/P7_A1_CatalogHUDPortraitDesignBrief_v03.md`。


炮塔材质需保持干燥、哑光、清洁：禁止黏液、奶油滴落、糖浆、湿亮高光和软塌表面；优先粉末涂层金属、哑光陶瓷、木材或石材。奶主题用配色/徽记表达。


## A1 奶家动物园炮塔

A1 已按奶家动物衍生形象建立三类炮塔、各三档升级：奶鸡单体攻击（鸡冠、羽尾和喙炮一体结构）、奶狗范围攻击（垂耳与声波号角）、奶鼠减速支援（圆耳、胡须发射杆与环形场装置）。当前九张已审核图为 1254×1254 RGBA 透明 PNG，位于 `Towers/A1/`。

九张炮塔图均已通过用户审核，并已导入主工程 `/Game/Naiwa/P7/Towers/A1`，作为保留 Alpha 的 sRGB Texture2D（UI 纹理组、无 Mipmap）。2026-11-11 已接入灰盒 HUD：塔栏使用各类 L1 图，已放置炮塔按模拟等级显示对应图片；选中、受扰边框和缺纹理回退保留。`Nai_LongEditor Development` 主工程构建通过，PIE/Standalone 游戏内尺度、裁切、灰度和分辨率视觉验收仍待完成。P7 灰盒地图及五张运行数据表已存在，但菜单入口、可玩 Alpha 与实体键鼠验收仍未完成。缩略总览见 `Towers/A1/Review/P7_A1_Tower_MilkZoo_ContactSheet_v04.png`，逐图审核与导入记录见 `Manifests/A1/` 和 `../../Records/p7_td/evidence/`。

A1 统一保持明亮、干燥、哑光的卡通硬质材料；禁止黏液、奶油滴落、糖浆、湿亮高光和软塌表面。


### A1 v02 返修记录

按反馈恢复原直立/固定底座设计，动态姿态试验稿已归档至 `Towers/A1/Review/Superseded-ShapeStudy-v01/`。L1/L2 沿用旧结构，三级改为减配轻量型；奶鼠三档均移除胡须与脸侧横杆。当前九张图见 `Towers/A1/Review/P7_A1_Tower_MilkZoo_ContactSheet_v02.png`；对应资产清单为 `Manifests/A1/P7_A1_Assets_v02.csv`。除奶狗 L1 外，其余均待用户审核。


### A1 v03 返修记录

按最新反馈恢复原站姿与底座，并采用逐级加装模块的成长方式：L2 强化可见装备，L3 仅加一件轻量核心件。奶鼠三档均无胡须。当前九图总览为 `Towers/A1/Review/P7_A1_Tower_MilkZoo_ContactSheet_v03.png`，资产清单和设计说明见 `Manifests/A1/` 的 v03 文件。


### A1 v04 返修记录

用户已通过当前九张炮塔资产。奶鼠终阶为双角度支援环、中央信标和左右发射节点，继续保持无胡须。当前总览、清单和说明见 v04 文件。


### A1 敌人基础剪影 v01

Alpha 普通型与快速型首帧候选位于 `Enemies/A1/`，用户已通过这两种基础剪影；普通型使用矮胖低重心轮廓，快速型使用前倾长肢体轮廓。两者作为 v02 三帧组的第 0 帧。初始设计记录和清单留在 `Manifests/A1/P7_A1_EnemyDesignBrief_v01.md` 与 `P7_A1_EnemyAssets_v01.csv`。

### A1 敌人三帧动作组 v02

普通型、快速型、干扰型现各有 3 张 1254×1254 RGBA 源帧。普通/快速型沿用已通过的首帧；干扰型第 0 帧继续引用通过的 A0 原图，不复制或修改。新生成的 6 张动作帧位于 `Enemies/A1/`，透明通道已检查；2026-11-11 完成 48px 彩色/灰度缩略检查和路线背景下的 36/48px 尺寸比较，预览分别为 `Enemies/A1/Review/P7_A1_EnemyMotionReadabilityReview_v01.png` 和 `P7_A1_EnemyTrackContrastReview_v01.png`。动作身份连续性、游戏内锚点、播放节奏和 UE 导入仍待验收。清单和帧序说明见 `Manifests/A1/P7_A1_EnemyAssets_v02.csv` 与 `P7_A1_EnemyDesignBrief_v02.md`。

2026-11-12 继续制作普通型与干扰型的 F01 压缩过步备选：两张 1254×1254 RGBA 图分别为 `P7_A1_Enemy_Normal_Frame02_v02.png`、`P7_A1_Enemy_Jammer_Frame02_v02.png`。这两项已获用户视觉通过，作为 F01 可选替代保留；原 v01、已通过 F00 与伸步 F02 均未覆盖。48px 彩色/灰度和全尺寸预览见 `Enemies/A1/Review/P7_A1_EnemyMotionAlternateReview_v01.png`；清单与生成提示见 `Manifests/A1/P7_A1_EnemyMotionAlternateAssets_v01.csv`、`P7_A1_EnemyMotionAlternateDesignBrief_v01.md`。源图来自内置 ImageGen，以原角色帧作身份参考、只改跑步姿势；帧选用、锚点校正与 UE 接入待后续动作组验收。

2026-11-12 补齐快速型 F01 压步备选 `P7_A1_Enemy_Fast_Frame02_v02.png`。此图仍是待评审候选，未导入 UE；统一三类对照、真实 48px 彩色/灰度缩略见 `Enemies/A1/Review/P7_A1_EnemyMotionAlternateReview_v02.png`。普通型、干扰型备选继续标记为已通过；源图/导出哈希、提示词摘要及状态见 `Manifests/A1/P7_A1_EnemyMotionAlternateAssets_v02.csv` 和 `P7_A1_EnemyMotionAlternateDesignBrief_v02.md`。


### A1 环境组件 v02

v01 地面路线、分岔门、清障物、奶罐和金币组已被用户否决，原因是信息表达不够直接、识别不够明显；原文件与清单保留作历史对照。路线和清障物 v02 是当前候选；奶罐与液面遮罩 v02 已被用户改为奶蛋终点 v03；金币沿用 v02：

- 地面路线：14 个 960×528 单元，改为高对比亮色通道、深色边界与中心虚线；A/B 门状态以亮色箭头表示开放路线，以红色 X 横栏表示封闭路线。清单 `P7_A1_EnvironmentAssets_v02.csv`，设计说明 `P7_A1_EnvironmentDesignBrief_v02.md`。
- 可破坏物：饼干箱和燕麦饼叠垛各有完好、裂损、将碎三态；加强大裂口、断面和缺块差异。清单 `P7_A1_ObstacleAssets_v02.csv`，设计说明 `P7_A1_ObstacleDesignBrief_v02.md`。
- 奶蛋终点：按用户参考采用金黄色蛋形角色、绿眼睛和小笑嘴，去掉眼泪；主体为 `Environment/A1/P7_A1_GuardianMilkEgg_v04.png`，外观已获用户通过。10 格耐久灯沿用 `P7_A1_GuardianEggAtlas_v03.png`，金币沿用 v02 星形压纹。奶蛋清单/说明见 `P7_A1_GuardianMilkEggAssets_v04.csv`、`P7_A1_GuardianMilkEggDesignBrief_v04.md`。

当前组共 25 个视觉单元，奶蛋主体 v04 已通过并导入 `/Game/Naiwa/P7/Environment/A1/T_P7_A1_GuardianMilkEgg_v04`，现接到地图终点显示。地面路线 v02 与可破坏物 v02 仍待用户评审；其中 6 张障碍图块已完成 48px 彩色/灰度和透明边缘预览，见 `Environment/A1/Review/P7_A1_DestructibleObstacleReadabilityReview_v01.png`。分岔门 v02 的 Y 形及 atlas bleed 作为历史问题保留，对照页为 `Environment/A1/Review/P7_A1_ForkGateTopologyAudit_v01.png`。v03 T 形门片已于 2026-11-12 获用户通过并导入 `/Game/Naiwa/P7/Environment/A1/T_P7_A1_ForkGateStates_v03`；A/B 路线状态按 `(1,5)/(18,5)` 在 Canvas HUD 按 UV 切片绘制。48px 彩色/灰度预览、坐标及哈希见 `Environment/A1/Review/P7_A1_ForkGateTopologyReview_v03.png` 和 `Manifests/A1/P7_A1_ForkGateStatesDesignBrief_v03.md`。`Nai_LongEditor Development` 编译通过；真实 viewport 下 CellId、720p/1080p 与路线切换视觉验收待完成。

2026-11-12 新增 `Environment/A1/Review/P7_A1_TerrainObstacleIntegrationReview_v02.png`：按 Alpha01 实际 A/B 路线、可视行、建造位与障碍坐标拼装地面 v02 和已通过门片 v03，并附地面 14 单元及障碍六种状态缩略。该板仅为静态候选源图复核；地面路线和障碍仍待用户评审，障碍外形只是示例，CrateA/CrateB 对应关系未定。复建脚本为 `Scripts/build_p7_environment_integration_review_v02.ps1`。


### A1 HUD 图标 v01 → v02

HUD v01 已被用户否决，反馈为“意义不明确”；原文件与清单保留作历史。v02 按对象和动作关系重绘 16 项并获用户视觉通过：敌人沿路来袭、炮塔升级、炮塔换金币、瞄准障碍、奶蛋护盾无伤、碎障清除，以及炮塔干扰/恢复。评审预览逐项附中文标签；标签不在正式透明图集中。图集、坐标和哈希见 `UI/A1/P7_A1_HUDIconAtlas_v02.svg`、`P7_A1_HUDIconAtlas_v02.png`、`Manifests/A1/P7_A1_HUDIconAssets_v02.csv` 和 `P7_A1_HUDIconDesignBrief_v02.md`。v02 已于 2026-11-11 后台导入 `/Game/Naiwa/P7/UI/A1/T_P7_A1_HUDIconAtlas_v02`，作为 1144×1144 RGBA UI `Texture2D`；导入报告见 `../../Records/p7_td/evidence/P7_A1_HUDIconAtlasImport_2026-11-11_21-42-43.json`。16 项的 48px 彩色/灰度缩略评审已于 2026-11-11 获用户通过，评审图见 `UI/A1/Review/P7_A1_HUDIconReadability_48px_v02.png`。本轮已实现 Canvas HUD 4×4 UV 切片并接入波次、金币、路线 A/B、奶蛋状态和终点图标；其余图标可按控件需要继续接线。Editor/Game 编译与 Development Cook/Stage 通过，接入后的 viewport 启动退出码 777006，真实画面分辨率与点击区域验收仍待完成。


### A1 结算徽章 v01 → v02

按计划补齐守护成功与奶蛋失守两种结算徽章。用户于 2026-11-12 选择 v02 原候选并接入结算 HUD：保留圆形/盾形外廓、星标/下坠碎片、微笑/难过奶蛋；失守态不画穿过奶蛋的裂缝。透明图集 `UI/A1/P7_A1_ResultBadgeAtlas_v02.svg/.png` 为 832×416 RGBA，每格 384×384；Texture2D 已导入 `/Game/Naiwa/P7/UI/A1/T_P7_A1_ResultBadgeAtlas_v02`，胜利与失守结算分别切片绘制。资产 ID、哈希与坐标见 `Manifests/A1/P7_A1_ResultBadgeAssets_v02.csv`，导入报告见 `../../Records/p7_td/evidence/P7_A1_SelectedResultBadgeV02Import_2026-11-12_16-02-08.json`。PIE/Standalone、720p/1080p 的实际结算布局验收仍待完成。

2026-11-12 曾制作 v03 备选，加入勾记和下箭头；用户选择 v02 后将 v03 保留为未选稿。v02/v03 全尺寸、真实48px彩色/灰度对照见 `UI/A1/Review/P7_A1_ResultBadgeComparisonReview_v03.png`；v03 状态与哈希见 `Manifests/A1/P7_A1_ResultBadgeAssets_v03.csv` 和 `P7_A1_ResultBadgeDesignBrief_v03.md`。


### A1 VFX 图集 v01 → v02 → v03 → v04 → v05 → v06 → v07

VFX v01 因意义不明确被否决；v02 虽画出事件顺序，但使用通用简化炮塔/敌人，没有直接对应 P7 实际资源与场景。v03 将四组效果绑定实际玩法，后由 v04 精修。用户于 2026-11-09 反馈 v04 视觉粗糙，v04 保留作追溯并由 v05 替代评审；用户随后指出 v05 的恢复攻击图标线条混杂、含义不清。v06 改成双暂停条与直向弹体后又被指出太直接、缺少特效感。当前 v07 保留清楚的动作方向，恢复受控光晕、弧形能量尾迹、闸束余辉和粒子余波，已于 2026-11-11 获用户美术视觉评审通过。四组×四帧及玩法不变。正式透明图集只含叠加用特效；玩法评审页使用现有 P7 角色、炮塔和场景对象帮助判断关系，其中障碍仅作场景示意。见 `VFX/A1/P7_A1_VFXAtlas_v07.svg`、`P7_A1_VFXAtlas_v07.png`、`Manifests/A1/P7_A1_VFXAssets_v07.csv`、`P7_A1_VFXDesignBrief_v07.md`、`VFX/A1/Review/P7_A1_VFXGameplayReview_v07.svg` 和 `.png`。v07 于 2026-11-11 以 848×848 RGBA Texture2D 导入 `/Game/Naiwa/P7/VFX/A1/T_P7_A1_VFXAtlas_v07`，HUD 展示层已接入射击、清障、升级、干扰及复攻快照事件，并通过 `Nai_LongEditor Development` 编译。运行时视觉、实际帧时长、游戏内缩放和 720p/1080p 挂点检查仍待 PIE/Standalone 验收；A0 风格输入和批量生产签收仍按 G0-04 跟进。

本轮另补三塔独立弹效图集 `VFX/A1/P7_A1_TowerProjectileVFX_v01.svg/.png`：奶鸡继续使用 v07 金色尖弹；奶狗使用暖橘共振弹群与环形范围命中；奶鼠使用薄荷晶体弹与环形减速场。8 帧按 4×2、单格 192×192、整图 848×432 RGBA 排列，导入 `/Game/Naiwa/P7/VFX/A1/T_P7_A1_TowerProjectileVFX_v01`，HUD 按 `TowerId` 分流播放。玩法数值和模拟逻辑未改。总览见 `VFX/A1/Review/P7_A1_TowerProjectileVFXReview_v01.png`，逐帧映射、哈希和状态见 `Manifests/A1/P7_A1_TowerProjectileVFXAssets_v01.csv` 与 `P7_A1_TowerProjectileVFXDesignBrief_v01.md`。纹理导入和 HUD 接线报告为 `UE5/Records/p7_td/evidence/P7_A1_Catalog003AndTowerProjectileVFXImport_2026-11-12_10-54-44.json`；PIE/Standalone 动态外观仍待验收。
