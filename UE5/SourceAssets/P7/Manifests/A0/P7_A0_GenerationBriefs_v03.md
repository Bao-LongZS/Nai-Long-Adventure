# P7 A0 提示词记录 v0.3

使用 OpenAI 内置 ImageGen。帧 0 沿用 v0.2 的 standard-milk-frog glance/smile。用户表示后帧的笑脸不像 `Big_Laugh.png`，并指定重点比较脸部；因此本轮仅编辑后帧面部。

## Frame 1 facial refinement

以当前前倾抱腹的大笑帧为编辑目标，以 `Big_Laugh.png` 为决定性面部参考、`Glance.png` 为短吻与眼睛位置参考。锁定身体轮廓、动作、画幅和透明背景；只调整脸部：眼睛收紧为低位的笑眼窄弧，避免大眉形；嘴位于短吻前端，做倾斜的紧凑椭圆/弯月开口，深色口腔、小而清楚的上排牙，少量暗粉色舌头。保留强烈捧腹大笑感，但避免宽阔动漫式大笑口、过大舌头和额外脸部装饰。

## Packaging

图集 `P7_A0_Character_MilkFrogHUD_SmileLaugh_2F_v03.png`：frame 0 看向/微笑，frame 1 脸部修订后的抱腹大笑；每帧 1254×1254，横排两列、4px 透明 gutter，播放时长未指定。
