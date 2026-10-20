# 奶蛙 UE5 P0 工作副本

开始日期：2026-10-10；最终更新：2026-10-13。**P0已完成**：资产接入、灰盒控制、五动作自然循环、有限动态视觉和普通/真实键盘验收通过。最终步态版本三轮24/24通过；A/D或箭头移动保持，Shift按次切换Walk150/Run300，界面显示模式，普通窗口实际两模式速度与代理动画均核对。手柄不在P0范围。见[最终验收](Records/p0_completion_20261013/completion_report.md)。下文10月11日结果为历史基线，最终参数以新报告为准。

## 工程与固定输入

- 工作工程：`C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/NaiwaP0/Nai_Long.uproject`。
- 源工程：`C:/Users/31314/Documents/Unreal Projects/Nai_Long/Nai_Long.uproject`；本轮复制其Content和Config到工作区。**该源工程已于 2026-10-20 移入回收站清除**（其 Content 391 个文件在本工程中全部存在，无唯一资产；Config 与 uproject 另存于 `.workbuddy/work/20261020_旧工程清理/`）。
- 实际引擎：UE 5.8.3，CL 58210709。
- 固定角色：正式E2 FBX，SHA256 `8dae9ce28c4b44465dcb8e73dd462804c3a561daee9eebaf79842900a3005147`。
- 平台：本机Windows；CPU Intel Core Ultra 9 275HX，GPU RTX 5070 Ti Laptop 12GB，内存32GB。

## 首轮测试规范

角色目标身高180cm；横向X、纵深Y固定0、上下Z；透视侧视相机。目标1920×1080、60fps。平面偏差容差0.1cm；平地稳定Idle脚底间隙容差3cm；停止速度阈值1cm/s。参数是本轮选定的测试规格，实测结果另记，不等于已验收。

## 当前资产与跑动

固定E2已由Legacy FBX导入。生成一个Skeletal Mesh、一个Skeleton和四个独立动画Sequence；四动作共享Skeleton。实际导入参考高度263.683cm，5个材质槽。报告与完整引擎日志位于`Records`。

E2在UE中实际LOD0：387,974渲染顶点、775,296三角形、11节点（源10骨＋FBX的Naiwa_Rig）、1LOD、5材质槽。原四动画时长分别1.25／1.33333／0.79167／1秒；同源网格重导入数据及四动作包稳定。

运行模块、输入／材质／蓝图及灰盒地图均已保存。当前使用独立`/Game/Naiwa/Gameplay/SK_Naiwa_Elastic`和`AS_Naiwa_Run`，共用原Skeleton。原E2网格与四动作包未改；源模型、骨架及四动作保留。[Run副本说明](C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/Animation/Run_v1/README_跑动与腹部弹性.md)

Run周期0.667秒，身体前倾18–20°，弯肘大幅交替摆臂，短腿支撑后收腿腾空；播放率匹配实际300cm/s移动速度。肚子使用局部压缩、回弹和软肉迟滞三个Morph，单形态最大游戏位移1.55／1.07／2.12cm，头脸和四肢保持原形。UE实际组件权重随Run变化、与动画曲线一致，连续Idle超过0.2秒后均归零。内部移动状态仍名Walk，实际代理采样的是Run。[实机权重检查](C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/Records/run_runtime_validation.json)

黄色偏浅已定位为Blender FBX输出sRGB数值、UE骨骼顶点色作为线性BaseColor使用。仅在新游戏Body/Eye材质中加入准确分段解码；关闭运动模糊以显示清晰摆臂。原材质和FBX保留。[诊断与源码证据](C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/Records/run_export_diagnostics.md)／[游戏材质配置](C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/Records/gameplay_visuals.json)

导入警告保留：平滑组缺失、部分UV默认生成；Morph FBX另有身体专用局部BindPose，产生六条MissingDeformers警告，但六个完整Skin Pose均Valid，UE新旧参考姿势逐项一致。数据通过不代替所有动作的表面观感验收。

## 最终运行结果

有效运行`20261011T074412Z_287152`：24用例、3轮通过、4,674采样帧及24张截图。覆盖起停、反向、双侧墙、平台跳跃／跌落、长滞空、提前落地后二跳、暂停／Reset。每例验证实际Run采样；Y偏差均0，稳定Idle实际蒙皮最低点约+2.15cm，满足3cm容差。[完整报告](C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/Records/runtime_run_summary.json)／[逐用例表](C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/Records/run_acceptance.csv)

[最终实机录像](C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/Records/Naiwa_Run_Runtime_Final_20261011.mp4)：1920×1080、H264、86.625秒；648张真实UE截图按墙钟时间重采样至8fps，没有插帧或音轨，不能用于测量游戏帧率。[编码核验](C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/Records/Naiwa_Run_Runtime_Final_20261011_encoding_report.json)／[实机跑姿截图](C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/Records/Naiwa_Run_Gameplay.png)。Blender三分之四循环另保存在Run_v1，属于动作制作预览。

抽样截图已检查黄色、前倾、摆臂轮廓、平地落脚及平台画面。自动测试经保存的EnhancedInput映射发送模拟PlayerController输入，场景起始位置由夹具设置；物理键盘、OS焦点、手柄硬件和暂停UI未验。四旧动作完整人工预览、连续动态观感和最终美术镜头仍在实施清单中，P0整体未完成。

## 性能样本

D3D12、1920×1080、ScreenPercentage100、sg画质均3（Epic）、运动模糊0、VSync0、不限帧。两组均实际平地往返跑，各丢弃2秒预热后采样5秒，排除截图与CPU蒙皮取样。

| 阶段 | 平均墙钟 | Game | Render | GPU | 墙钟P95 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 可见网格，Run及Morph启用 | 13.256ms | 2.011ms | 13.247ms | 11.227ms | 15.569ms |
| 隐藏网格、关闭动画tick | 11.588ms | 2.087ms | 11.577ms | 9.265ms | 18.425ms |

隐藏组仍保留Character和移动逻辑；这是本机编辑器游戏初步对照，不能替代打包或其他设备测试。可见组平均约75fps，不将短样本写成稳定60fps保证。

## 分阶段复现

现有工程可直接使用`launch_p0.ps1`运行，加`-Editor`打开编辑器。回归用`run_stage.ps1 -Stage Test`或`Record`；只读检查为`Audit`及`RunAudit`，游戏颜色材质可用`Visuals`重建。Import／RunImport仅供首次导入，不重复对已有资产执行；重导入读取专用脚本保护条件。Setup用于灰盒初建／重建，会重新配置基础材质和默认主角，当前游戏适配须保留。

运行示例：

```powershell
& 'C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/launch_p0.ps1'
& 'C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/run_stage.ps1' -Stage RunAudit
& 'C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/run_stage.ps1' -Stage Test
```

自动测试输出在工程`Saved/NaiwaTests`，关键记录另存到`UE5/Records`。编辑器打开`/Game/Naiwa/Maps/L_NaiwaGraybox`并Play；A/D或左右键移动、Space跳跃、P/Escape暂停、R重置。Reset可解除暂停并清零速度／恢复出生点、朝向和动画时钟。手柄映射已建立，硬件未测。早期两轮FAIL和最终PASS均保留为修复／复测链；当前状态见`Records/TASK_STATE.json`和实施清单。
