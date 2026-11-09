# P7 A0 提示词记录 v0.1

使用方式：OpenAI 内置 ImageGen。奶蛙表情帧参考用户提供 PDF 中的角色身份与情绪；两张用户后来选定的 PNG 与项目现有 v01/v02 文件哈希相同。以下记录生成规格摘要与最终帧顺序。

## Tower v01

原创轻松奶主题塔防游戏的单体炮塔精灵：矮圆稳定底座、青蓝陶瓷塔体、奶油色短炮管、橘色炮口环、无文字奶滴符号；轻俯视、粗深蓝轮廓、少量赛璐璐阴影，奶油白/青蓝/橙色；透明底、独立完整轮廓，不要环境、文字或既有塔防游戏造型。

## Jammer enemy v01

原创干扰型快跑敌人：淡紫蓝梨形团子、短腿、微小手臂、橙色闪电标记，身体前倾向右跑；明确没有炮塔底座、轮子和武器；2D 卡通、粗深蓝轮廓，透明底、无环境或文字。

## Cracked crate v01

原创可破坏障碍：方形奶油饼干补给箱，圆角、深蓝外轮廓、奶油色侧带、明显裂缝和青蓝卡扣；轻俯视，损伤用形状而非只靠颜色识别；透明底、无环境或文字。

## Tower jammed UI v01

原创塔受扰/恢复状态 HUD 图标：奶油色圆角徽章内放青蓝炮塔剪影，紫色脉冲与橙色闪电压在炮管上，外围分段环显示倒计时缺口；扁平几何、无文字数字，灰度可读，透明底。

## Milk-frog portrait v01 - frame 1 (belly laugh)

以用户提供 PDF 的奶蛙身份/情绪为参考，保留黄色圆身体、浅色肚皮、深色手脚和大笑，作为 HUD 后帧。用户确认继续使用这张并将 v02 微笑帧排在前面。

## Milk-frog portrait v02 - frame 0 (smile)

编辑 v01 头像，只收敛表情、姿势和夸张程度；保留金黄色圆润身体、浅色肚皮、绿色眼睛、深棕色手、暖深色轮廓和透明背景；双眼睁开、有绿色虹膜，自然微笑，不露齿，不张大黑口；一只手轻放腹部；无橙色笑声符号；友好、温和、略显憨态；禁止恐怖、怪异、躁狂或猎奇细节。用户确认此张作为大笑帧的前一帧。

## Ground cell v01 - editable vector

手工 SVG：960×528（10×96×52.8）低对比燕麦奶色地面格，稀疏浅色/鼠尾草绿微粒与两处轻淡芽叶；外缘保留安静留白，使相邻格能自然拼接。无路径、格线、建造位、边框、文字或可玩状态。PNG 使用相同几何与颜色输出。


## Sequence packaging

Horizontal atlas `Characters/A0/P7_A0_Character_MilkFrogSmileLaugh_2F_v01.png`: frame 0 is v02 smile, frame 1 is v01 belly laugh. A 4px transparent gutter separates cells. Frame duration is intentionally unassigned.
