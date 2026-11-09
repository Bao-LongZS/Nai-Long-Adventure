# P4 像素探针记录 · 2026-10-30

## 结果

执行 `UE5/Scripts/p4_pixel_probe.ps1`，W4 候选、A2 源图和选卡代码路径检查通过。负责人随后提供了运行截图，加入 A2 背景朝向像素探针后，旧运行画面被识别为 **上下翻转**，所以本轮运行时总判定为 FAIL，避免把源图通过误报成整体验收通过。

- W4 manifest v06 中 99 张 RuntimeWIP 候选 PNG：文件、SHA-256、尺寸、非空像素、透明边缘均通过；Boss 帧脚底像素锚点落在 y=464–466。
- Boss 待机/动作及 VFX 共 23 段序列：帧数与正时长通过；每对相邻帧至少有 100 个可见像素变化，变化比例不少于 0.001。
- A2 场景源图：SHA-256 与 manifest 相符，尺寸为 1672×941；灰度标准差 44.11，像素亮度范围 0–255，通过非空白/非平坦导出检查。
- 强化卡源码路径：卡面构建读取当前卡阶和玩家属性；重复选卡递增 `NewRank`，冲撞上限随阶数递增。举例：当前上限 3 次时，卡面预告下一阶为 4 次。
- 运行画面（1923×1128）中的实际游戏视口是 x=1、y=48、1920×1080。遮罩 HUD 和角色/伤害字后，A2 背景四朝向采样的相关系数为：正常 `0.10407`、上下翻转 `0.49505`、左右翻转 `0.12122`、双向翻转 `0.42186`。正常朝向与最佳匹配相差 `-0.39098`，故像素探针确认当前画面背景上下颠倒。
- 已修正背景平面法线，使其朝向相机，并加入源码方向断言。`Nai_LongEditor Win64 Development` **4/4 actions** 和截图对应的 `Nai_Long Win64 Development` **81/81 actions** 均成功。日志：[Editor 构建](P4_BackdropOrientation_compile_20261030.log)、[Development 游戏构建](P4_BackdropOrientation_game_compile_20261030.log)。当前截图来自修正前进程；需要重新启动预览后，才可用新版运行截图确认方向修复。
- 修正后的无截图源码/素材探针为 **PASS_PIXEL_PROBE，538/538**；报告：[P4_source_pixel_probe_20261030.json](P4_source_pixel_probe_20261030.json)。修正前运行截图结果单独保存在 [基线报告](P4_runtime_pixel_probe_before_fix_20261030.json)，539/540 通过，唯一失败为背景朝向，保留它作为问题定位证据。

脚本对旧截图报告 `runtime_background.orientation` 失败是预期的缺陷定位结果；不代表素材源像素检查失败。

## 运行时边界

- W4 图片全部仍为 `RuntimeWIP`，manifest 标记 `runtime_ready=false`，没有导入游戏 Content。
- 负责人已提供 `/Game/Naiwa/P4/Maps/L_NaiwaP4_ArtIntegrationPreview` 的 Standalone 截图；运行截图像素探针检查非空画面与 A2 背景方向，并遮罩 HUD/角色影响。背景翻转已修复并编译，但修复后的运行截图仍待更新进程后复测。
- 一次性运行画面核验目标为 `/Game/Naiwa/P4/Maps/L_NaiwaP4_ArtIntegrationPreview`，集中检查 A2 场景取景、主角比例/脚底、HUD 文本，以及重复选冲撞卡后是否从 `3 → 4` 更新。当前该预览不展示 W4 新候选序列。
- 当前 W4 候选若需在 UE 里看动作/特效，需要另建隔离预览接入；现有 A4 地图不会显示这些 WIP 图片。
- 后续复测命令（窗口标题栏在截图顶部时）：

```powershell
powershell -File UE5/Scripts/p4_pixel_probe.ps1 `
  -RuntimeScreenshot 'C:/path/to/new-preview.png' `
  -RuntimeViewport '1,48,1920,1080'
```

## 复现

```powershell
powershell -File UE5/Scripts/p4_pixel_probe.ps1
```

脚本读取源文件和现有构建报告，只写入 `UE5/Records/p4_alpha/evidence/PixelProbe/` 下的报告。运行截图可选；提供后会对四种镜像/翻转方向与 A2 源图进行采样相关性比对。
