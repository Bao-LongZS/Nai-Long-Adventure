# P7 A0 美术样片说明 v0.3

日期：2026-11-01（Asia/Shanghai）  
状态：**A0 候选稿；奶蛙身份参考与大笑表情已按用户反馈修订；尚未导入 UE。**

## 奶蛙身份与大笑表情

用户指定的标准身份参考包括 `Big_Laugh.png`、`Glance.png`、`player.png` 和补充侧面图。奶蛙保持连成一体的平滑蛋/梨形黄身体、奶油色肚皮、小型嵌入式绿色眼睛、简短嘴线与深色短手脚；避免外凸蛙眼、蹲蛙比例、蹼足和疣点。角色只作为 HUD 头像候选。

用户反馈指出先前大笑帧更像普通微笑，脸部与 `Big_Laugh.png` 不一致。当前 v04 只替换后帧笑脸：眼睛收紧成笑弧，嘴部压缩成倾斜的深色开口，保留上排小牙和克制舌色；前倾抱腹动作和身体轮廓继续沿用。前帧 v03 看向/微笑不变。

## 当前两帧候选

| 帧 | 资产 | 说明 |
|---|---|---|
| 0 | `Characters/A0/P7_A0_Character_MilkFrogSmile_v03.png` | 标准奶蛙看向/微笑 |
| 1 | `Characters/A0/P7_A0_Character_MilkFrogLaugh_v04.png` | 前倾抱腹，脸部按 `Big_Laugh.png` 修订 |
| 图集 | `Characters/A0/P7_A0_Character_MilkFrogHUD_SmileLaugh_2F_v03.png` | 横向两帧，4px gutter；时长仍未指定 |

完整尺寸、SHA-256、透明模式和 UE 目标路径见 `P7_A0_Assets_v03.csv`；图集帧矩形见 `P7_A0_MilkFrogHudExpression_2F_v03.json`。

## A0 尚待验收

- 对照原图确认脸部特征；测试 48px/720p/1080p 可读性和两帧连续性。
- 运行灰度与色盲替代编码检查；验证透明边缘。
- 完成其余 A0 资产的棋盘缩略图、灰度和 1080p/720p 检查。
- 负责人确认风格方向后才开始批量制作。

所有资产仍为候选，未标记为验收通过。
