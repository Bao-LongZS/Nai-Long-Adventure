# P2 奶蛙拉弓动作

使用现有 E2 奶蛙骨架制作一段 1.25 秒的拉弓动作。前臂平滑抬起并伸向弹弓，另一侧手臂收向胸前；躯干轻微后倾抵住拉力。动画按标准化进度取样，鼠标拖拽力度直接驱动动作展开，松手后淡回待机并带短促回弹。

## 工程接入

- `Naiwa_P2_SlingshotDraw.blend` 与 `Naiwa_P2_SlingshotDraw.fbx` 是动画工程和 FBX 源文件；原始 E2 模型没有被修改。
- 动画资产 `/Game/Naiwa/P2/Art/Character/AS_Naiwa_P2_SlingshotDraw` 绑定 `/Game/Naiwa/SK_Naiwa_Skeleton`。
- `ANaiwaP2Archer` 由 `ANaiwaP2GameMode` 在 2-1、2-2、2-3 共用场景中生成，禁用碰撞，不影响物理关卡；重试时回到待机。
- `ANaiwaP2PlayerController` 在按下、拖动、松开时将状态和力度传给 GameMode。`UNaiwaP2ArcherAnimInstance` 混合现有 Idle 与拉弓动作。
- 满弓时有幅度很小的呼吸变化；无效拖拽返回待机，不触发发射回弹。

## 验证

- UE 5.8 `Nai_LongEditor` 与 `Nai_Long` Development 构建通过。
- 2-1、2-2、2-3 均通过 `-NaiwaP2PhysicsLevelAutoTest`（每关 `pass=1`）；角色无碰撞，原弹射和物理结算通过。
- 三关均通过满弓姿势运行截图探针。截图、启动日志保存在本目录和 `UE5/NaiwaP0/Saved/Logs`。
- Blender 预览位于 `preview/`。实际鼠标拖拽的主观手感建议在游戏中再做一次人工体验。

## 重建

```powershell
& 'D:\STEAM\steamapps\common\Blender\blender.exe' --background --python-exit-code 1 --python 'UE5\Animation\P2_Archer\build_p2_slingshot_animation.py'
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'UE5\NaiwaP0\Nai_Long.uproject' -run=PythonScript -script='UE5\Scripts\import_p2_archer_animation.py' -unattended -NullRHI -nosplash
```
