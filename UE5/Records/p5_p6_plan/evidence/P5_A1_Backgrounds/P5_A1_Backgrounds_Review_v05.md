# P5 A1 近景雪脊导出复核 v05

日期：2026-10-28  
状态：**静态边缘列已做 RGBA 精确收敛；仍为 WIP，等待三层视差与 UE 导入复核**

## 候选与来源

- 未改动源稿 `UE5/SourceAssets/P5/Backgrounds/Source/BG_Ridge_Near_Source_v04_WIP.png`（2060×763 RGBA）；SHA-256 `4FE56E97FAC24C53E13AC627C700842C53CBA9DDF336D838C029C0FDBAFBEEB6`。
- 新导出 `UE5/SourceAssets/P5/Backgrounds/Exports/BG_Ridge_Near_v03_WIP.png`（2048×768 RGBA）；SHA-256 `60ADA6D386492EADE090FC6DABCE0EA6A104DF93C0E10C15A99F3BF8C4888C09`。
- 双拼复核 `P5_A1_RidgeTiling_v09.png`；机器可读清单 `UE5/SourceAssets/P5/Manifests/P5_A1_Backgrounds_manifest_v05.json`。

## 处理与静态量测

- 导出脚本保留原 v02 默认路径；新增 `-MatchAlphaSeam` 选项，单独产生 v03。清理掉最大雪脊主体之外的孤立 alpha 组件并按规格缩放。
- 仅在导出图左右各 16 px 内，将每行边缘的 alpha 覆盖率与预乘颜色收敛至两侧较低覆盖率样本；原始绘画源稿不变。
- 首尾列 RGBA 平均绝对差与最大绝对差均为 **[0, 0, 0, 0]**。这证明边缘像素一致，不等于整段轮廓斜率或笔触周期自然。
- 双拼图未见颜色竖线；接缝处的轮廓切线与纹理跨边连续性仍需更高倍率判断。该图不能据此标为无缝通过。

## 判定与后续

v03 是较 v02 更严格的透明边界候选，但静态双拼仅是筛查。保留 `_WIP`；待三层视差、相机运动、1080p/720p、材质过滤与压缩进入 UE 后复核。若接缝处仍能察觉形状折点，应回到源稿修形，不扩大 alpha/颜色羽化带掩盖问题。
