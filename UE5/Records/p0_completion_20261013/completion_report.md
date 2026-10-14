# P0 最终验收（2026-10-13）

**P0已完成：角色资产接入与最小可玩灰盒闭环通过。** 正式E2保留；游戏使用Elastic副本。手柄按用户要求不纳入P0。闯关流程、死亡/复活、Laugh触发属于P1，打包与进一步性能适配属于P2。

## 当前操作

A/D或左右方向键移动；Space跳跃；P/Esc暂停/继续；R重置。**按一次左或右Shift切换行走与奔跑**，无须配合方向键。默认奔跑300cm/s；行走150cm/s。HUD显示WALK/RUN，行走使用原Walk，奔跑使用Run；动画播放率跟随各模式速度。Reset保留玩家选择的步态。

启动现有工程：./UE5/launch_p0.ps1。需要输入证据时加 -InputEvidence。工程为 UE5/NaiwaP0/Nai_Long.uproject，地图 /Game/Naiwa/Maps/L_NaiwaGraybox，UE5.8.3（CL58210709）。勿直接重跑Setup、Import或RunImport；Setup是初建入口，会恢复旧胶囊、资源和输入设置。

## 验收结果

| 项目 | 结果与证据 |
| --- | --- |
| 最终构建与保存 | gait_build.log成功；输入资源与BP GaitAction已保存。[输入映射](gait_input_mapping.json)记录原移动/Jump/Pause/Reset保留及左右Shift的新Action。新进程实际使用保存工程。 |
| 最终功能回归 | 20261013T053123Z_44908，三轮24/24 PASS、24截图齐全，覆盖移动/转身、双墙、跳台/跌落、长滞空、提前落地二跳、暂停及Reset。 |
| 高低帧率基线 | 30上限20261013T003433Z_23096、60上限20261013T003721Z_12492各三轮24/24通过。[运行摘要](runtime_digest.json)保留所有历史与最终轮，不混合统计。 |
| Shift步态实际运行 | 普通窗口无AutoTest，用户实际操作记录6次切换；133个稳定行走采样达到150且实际代理采样原Walk，100个稳定奔跑采样达到300且代理采样Run。[实际步态验收](gait_acceptance.json)保留完整判据及例帧。工具最初短按修饰键未触发，未冒充成功。 |
| 普通键盘、暂停、焦点 | Windows按键通路和用户真实键盘已核对；用户确认除键位定义外其余正常，最终需求已明确为Shift切换步态。原日志/答复在[普通输入验收](ordinary_input_acceptance.json)，[暂停HUD](ordinary_pause_HUD.png)由游戏F9保存。 |
| 五动作连续播放 | 20261013T002807Z_23720的manifest完整；Idle/Walk/Jump/Laugh/Run各两次自然循环，52张真实截图、五拼图与[时间报告](clips/playback_review_summary.json)保留。 |
| 动态视觉 | 52张动作采样、11原图和第一轮8类用例61张序列采样已检查，未见明显眼嘴脱离、爆裂、旧截面穿墙或整身裁切；墙/平台/跑跳/落脚/暂停可读。[视觉报告](视觉验收.md)保留采样与侧视限制。不为微纹和细腻度重复修模。 |
| 审阅录像 | [91秒1080p录像](Naiwa_P0_Runtime_20261012.mp4)由当前形体/物理基线的504张真实UE截图按墙钟8fps重采样，[编码核验](Naiwa_P0_Runtime_20261012_encoding_report.json)通过。此10月12日素材不含新Shift步态提示；新增输入由最终回归及普通窗口日志补证，不把该录像标成新输入版本。 |

## 参数与实际修改

胶囊半径86、半高90cm；目标180cm，Mesh缩放约0.6826376994、Z偏移−90.546109cm。移动X/Z、Y固定0；加速度1800、行走减速2200cm/s²、摩擦8、空中控制0.4。跳力650cm/s、重力倍率1.6、持跳上限0.2s，持跳力期间不施加重力。相机相对胶囊(0,900,40)cm、FOV40°；只转Mesh。

动画混合0.12s，空中优先，Jump采样停在片段55%处等待落地；Run三个腹部Morph及稳定Idle归零保留。Shift进入原Walk后使用其实际片段与150参考速度；未增加骨架、面部Morph或修改源模型。

本轮最终修改是角色Shift切换/证据字段、按模式选Walk/Run、HUD，以及保存的输入映射/BP引用。早先Shift+箭头试验已经撤销，原A/D/箭头不受门控；其文件和运行结果仅为历史，不是当前键位。

## 回归失败与范围限制

首个30上限窗口轮20261013T002931Z_40560在22/24后发生D3D12截图越界：BackBuffer缩为8×8，仍要求1920×1080读回；没有用例断言失败，但该轮只记INCOMPLETE。run_stage.ps1增加可选FrameRateCap/Offscreen，依据[Epic的RenderOffScreen参数](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-command-line-arguments-reference)进行离屏自动截图，复测通过。普通输入另在可见窗口验证。

30/60是运行上限，CSV包含截图、CPU蒙皮和编辑器开销；不能写成打包或其他机器稳定60fps。性能短样本、原平滑组/默认UV、局部BindPose/MissingDeformers与双DirectionalLight优先级提示保留，未见本轮明显可见阻碍。目标性能、细腻动作和最终美术继续按P1/P2需要处理，当前不扩展高成本打磨。

正式E2 Blend/FBX SHA256为317f62278d63217636bf393f831e2fb41ce21acfefbe21beed6ad6d4dc75f322 / 8dae9ce28c4b44465dcb8e73dd462804c3a561daee9eebaf79842900a3005147；Run输入f5688581ae103f27901bab67a98ba9deef983156ec985308435bb453e2621d2e。原资产和历史失败记录保留。
