# P7 A1 奶蛋终点角色 v04

日期：2026-11-01（Asia/Shanghai）  
状态：按用户提供的奶蛋角色参考重做；角色外观已获用户通过，透明 PNG 已导出并导入 UE。场景比例、遮挡和耐久 UI 组合待 viewport 验收。

## 角色识别

- 以用户参考中的角色为准：金黄色、圆润直立的蛋形身体；绿色大眼、黑色瞳孔、小笑嘴。
- 按用户追加要求去掉眼泪，不出现泪痕、水滴或哭泣表情。
- 采用透明底独立角色 PNG，正面居中完整入画，不添加外壳裂纹、四肢、服装、标牌或环境。
- 10 点耐久图形继续沿用 `P7_A1_GuardianEggAtlas_v03.png` 第二格的 10 格耐久灯；金币继续沿用 `P7_A1_GuardianAndCoinAtlas_v02.png`。

## 导出与导入

- 资产：`Environment/A1/P7_A1_GuardianMilkEgg_v04.png`。
- 尺寸：1254×1254，8-bit RGBA，透明背景；单帧，正面角色。
- 来源：按用户提供的奶蛋图像作为身份参考，用于生成此原创 P7 角色图；参考原图不复制到 P7 资产目录。
- 资产 ID、哈希和建议 UE 路径见 `P7_A1_GuardianMilkEggAssets_v04.csv`。
- 已导入 `/Game/Naiwa/P7/Environment/A1/T_P7_A1_GuardianMilkEgg_v04`，作为 sRGB UI `Texture2D` 保留 Alpha、禁用 Mipmap 与纹理流送；导入报告见 `../../Records/p7_td/evidence/P7_ApprovedMapPresentationAssetImport_2026-11-11_22-36-25.json`。

## 待验收

- 角色身份和无泪表情按参考制作；用户已通过角色外观。P7 场景相对尺寸和终点组合仍待运行时检查。
- 待做 48px 缩略、灰度、透明边缘、720p/1080p 屏幕尺寸及与路线终点组合检查。
- HUD 终点锚点已接入，实际 viewport 尺寸和角色与地图组合仍待复核；10 点耐久灯候选仍需独立审批和导入。
