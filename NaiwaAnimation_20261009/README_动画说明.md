# 奶蛙 E2：10 秒轻快蹦跳

使用 `NaiwaCharacter/versions/20261007_H2_E2_overall_refinement/candidate_E2/Naiwa_Character.blend`。截至 2026-10-09，它仍是角色说明中的最新正式推荐；后续 C3 试验未被采用。

成片为 `奶蛙_E2_轻快蹦跳_10秒.mp4`，1920×1080、24 fps、240 帧、10 秒，H.264/yuv420p，无音轨。

动作顺序：站定 → 轻快左右摇摆与小幅前臂摆动 → 两次小跳 → 转身摆动 → 小鞠躬 → 回到站姿。使用薄荷色摄影棚与米色圆台，保留原有闭嘴表情、眼球、身体配色及几何。现有自动肩部权重不适合大幅举臂，本片采用小幅前臂动作和整体角色运动。

`Naiwa_E2_Greeting_10s.blend` 是单独保存的可编辑动画工程，已选中 `Naiwa_E2_Greeting_10s` 动作，时间轴为 1–240 帧并带动作阶段标记。原四段动作继续保留。打开工程后在时间轴播放，数字小键盘 0 可查看成片相机。

原模型不被覆盖。源工程 SHA256 为 `317f62278d63217636bf393f831e2fb41ce21acfefbe21beed6ad6d4dc75f322`，渲染前后检查一致；绑定角色网格的原始坐标也检查一致。记录位于 `records/animation_build.json`，成片实际解码帧数、尺寸、帧率及长度校验位于 `records/video_validation.json`。

`preview/` 保存关键姿势，`frames/` 保存完整 PNG 序列。脚本使用 Blender 5.2.2 LTS，以下命令从项目根目录执行。

```powershell
& 'D:\STEAM\steamapps\common\Blender\blender.exe' --background --python-exit-code 1 --python 'NaiwaAnimation_20261009\scripts\make_animation.py' -- --preview
& 'D:\STEAM\steamapps\common\Blender\blender.exe' --background --python-exit-code 1 --python 'NaiwaAnimation_20261009\scripts\make_animation.py' -- --render
& 'D:\STEAM\steamapps\common\Blender\blender.exe' --background --factory-startup --python-exit-code 1 --python 'NaiwaAnimation_20261009\scripts\encode_video.py' -- --frames 'NaiwaAnimation_20261009\frames' --output 'NaiwaAnimation_20261009\奶蛙_E2_轻快蹦跳_10秒.mp4'
```
