# P7 A1 结算徽章 v03

日期：2026-11-12（Asia/Shanghai）  
状态：**用户于 2026-11-12 选择 v02 原候选；v03 未选中，保留为设计备选，不导入 UE。**

## v03 设计变化

| AssetId | 状态 | 主要图形线索 |
|---|---|---|
| `P7-UI-A1-RESULT-WIN` | 守护成功 | 圆形金边、完整薄荷绿护盾、微笑奶蛋、星标和叶枝；新增更大的薄荷绿勾记，强化成功语义 |
| `P7-UI-A1-RESULT-LOSS` | 奶蛋失守 | 下尖盾形、珊瑚边框、难过奶蛋；新增醒目的珊瑚色向下箭头，突出失守方向 |

胜负继续使用不同外形和不同方向符号，灰度下分别通过圆形/盾形轮廓及勾记/下箭头区分。奶蛋没有裂缝或破损，不用角色身体表达失败；图集不含文字。

## 规格与对照

- 可编辑源：[`P7_A1_ResultBadgeAtlas_v03.svg`](../../UI/A1/P7_A1_ResultBadgeAtlas_v03.svg)。
- 导出：[`P7_A1_ResultBadgeAtlas_v03.png`](../../UI/A1/P7_A1_ResultBadgeAtlas_v03.png)，832×416，8-bit RGBA；两格各 384×384，x=16/432，格间 32px，图集外边 16px。
- 透明外边距、格间距 alpha 预检通过（非透明 gutter 像素为 0）。纹理目标建议 `/Game/Naiwa/P7/UI/A1/T_P7_A1_ResultBadgeAtlas_v03`；状态仍为待评审，暂不导入。
- v02/v03 对照板 [`P7_A1_ResultBadgeComparisonReview_v03.png`](../../UI/A1/Review/P7_A1_ResultBadgeComparisonReview_v03.png) 同时显示全尺寸、真实 48×48 彩色/灰度缩略（2×最近邻放大），保留作方案追溯。
- 资产 ID、frame index、AtlasRect、源图/PNG SHA-256 见 [`P7_A1_ResultBadgeAssets_v03.csv`](P7_A1_ResultBadgeAssets_v03.csv)。用户选择 v02；v03 文件和清单保留为未选稿。
- 可用 `UE5/Scripts/generate_p7_a1_result_badges_v03.mjs` 重建 SVG、PNG、对照板和 v03 清单；构建器验证导出尺寸、RGBA 通道及透明间距。

此备选不改变结算规则或游戏数据，也不构成 A4/G0-04 关闭声明。
