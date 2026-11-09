# P7 A0 美术样片说明 v0.2

日期：2026-11-01（Asia/Shanghai）  
状态：**A0 候选稿，奶蛙身份已按用户提供的标准参考修订；等候美术方向确认，尚未导入 UE。**

## 方向与边界

《捧腹大笑思想与奶龙学概论》用于奶蛙身份与基调参考，强调轻松、无厘头但友好的快乐。后续补充的 `exec-000b9b85-52f5-422d-8e5e-cede2eeabc88.png`、`Big_Laugh.png`、`Glance.png` 和 `player.png` 被用户明确指定为标准奶蛙参考；它们作为角色外形和表情参考，不是工程规则。

当前角色帧据此采用连成一体的平滑蛋/梨形黄身体、浅奶油色肚皮、较小且嵌入面部的绿色眼睛、简短嘴线、深色短手脚。避免蹲蛙姿态、外凸大圆眼、蹼足、疣点或宽扁蛙脸。该角色只用于 HUD 头像候选，不进入战斗场景。

用户确认此前“微笑/看向”与“大笑”构成连续表情。身份修订 v03 的前帧是看向/微笑，后帧是合眼腹笑；两帧从同一外形基底生成/编辑，再打包为横向图集。旧序列保留供对比和追溯，不再作为当前候选。

## 当前候选资产

| 用途 | 资产 | 状态 |
|---|---|---|
| 单体炮塔 | `Towers/A0/P7_A0_Tower_SingleShot_v01.png` | 候选 |
| 干扰型敌人 | `Enemies/A0/P7_A0_Enemy_Jammer_v01.png` | 候选 |
| 可破坏障碍 | `Environment/A0/P7_A0_Obstacle_CrackedCrate_v01.png` | 候选 |
| 中性地面格 | `Environment/A0/P7_A0_GroundCell_Repeat_v01.svg` + `.png` | 可编辑 SVG 与 960×528 PNG；UE 拼接尚未验收 |
| 塔受扰/恢复状态 | `UI/A0/P7_A0_UI_TowerJammed_v01.png` | 候选；小尺寸/灰度检查未完成 |
| 奶蛙表情帧 | `Characters/A0/P7_A0_Character_MilkFrogSmile_v03.png` → `P7_A0_Character_MilkFrogLaugh_v03.png` | 依据四张标准参考修订；同体型两帧；未做 UE 播放验收 |
| 奶蛙两帧图集 | `Characters/A0/P7_A0_Character_MilkFrogHUD_SmileLaugh_2F_v02.png` | 4px gutter；前帧微笑、后帧大笑；时长未定义 |

完整 AssetId、尺寸、透明模式、SHA-256 和计划目标路径见 `P7_A0_Assets_v02.csv`；图集矩形见 `P7_A0_MilkFrogHudExpression_2F_v02.json`。

## 待完成的 A0 验收

- 1080p/720p 中检查角色两帧尺寸、锚点和视觉连续性。
- 对炮塔/敌人/障碍/HUD 图标做 48px 缩略、灰度与色盲替代编码检查。
- 在棋盘底色和透明棋盘上检查边缘；在 UE 中检查地面格实际拼接。
- 批量生产前冻结分层源稿、图集边距、导入预设和相对尺寸。

这些检查完成前，所有资源仍是候选，不标记为验收通过。
