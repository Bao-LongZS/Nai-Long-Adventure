# P2 连闯与打包验证（2026-10-21）

## 构建与缓存

- `Nai_Long Win64 Development` 和 `Nai_LongEditor Win64 Development` 均编译成功。
- Development 包制作成功，目录：`D:\Nai-Long-Adventure-Cache\UE5\NaiwaP0\Saved\DemoBuild_P2_20261021_LinkDefault`；约 839 MB，Pak 约 395 MB。
- Cooked 和 StagedBuilds 仍通过原工程 C 盘路径访问，两个 Junction 分别指向 D 盘缓存目录。打包流程经这两个链接完成。验证后 C 盘剩余约 13.28 GB，D 盘剩余约 274.32 GB。

## 启动、音频资源与物理

- 最终包普通启动进入 P1，启动日志标记 `campaign_mode=linked`；使用 `-NaiwaP1Standalone` 时标记 `standalone`。两种方式都正常启动并退出。
- 工程内 P1 运行探针均显式使用 standalone 标记；`launch_p1.ps1` 与 `launch_p1_v7.ps1` 保留 `-StandaloneP1` 旁路选项。15 个相关 PowerShell 启动/回归脚本通过语法检查。
- 最终包的 P2 2-1、2-2、2-3 地图均加载成功，分别选中 Title Theme、雨爱、熊出没循环曲目。
- Pak 清单包含三首背景音乐及“嗯？”未命中音、“冰冰冰”命中音五个资源。
- P2 运行日志确认实际使用的基础奶蛋网格 `SK_NaiwaEgg` 已加载（4 个材质槽、3 个形态键）。Cook 另报告 `EggCandidates/LowPoly1798` 候选网格引用了不存在的候选 Skeleton；该路径不属于运行时使用的 `Art/Egg` 网格，打包和关卡启动均通过，未删除或改动候选素材。
- 最终包的三关单发物理测试均通过并检测到目标命中；2-1 零命中及补发测试通过。
- 多发物理回归脚本三关均通过。2-1 使用 `19.85+19.85+1`；2-2 使用五个 `19.85`（第 4 发已清场）；2-3 使用四个 `19.85`（验证了结算和碰撞，未清场）。
- 原默认多发序列含 34°，在当前参数下第一发会飞出场外，因此已更新 [run_p2_physics_level.ps1](../run_p2_physics_level.ps1) 的默认序列，并用脚本逐关验证通过。

## 记录与测试边界

- 打包日志：`package_development_20261021_222437.log`。
- 最终包 P1 启动日志：`runtime_P1_default_linked_20261021.log`、`runtime_P1_explicit_standalone_20261021.log`。
- 最终包 P2 启动日志：`runtime_LinkDefault_P2_21_20261021.log`、`runtime_LinkDefault_P2_22_20261021.log`、`runtime_LinkDefault_P2_23_20261021.log`。
- 物理测试摘要日志保存在 `.workbuddy/work/20261020_全物理改造/stability/physics_level_campaign_angles_20261021_*.log`；最终包命中和未命中测试日志位于本目录的 `physics_LinkDefault_*.log`。
- 启动和物理测试使用无声、无画面模式；资源已打入包，测试验证了曲目选择和命中/未命中逻辑分支，未作人工听感检查。完整人工操作的 P1→P2→P2 后续关卡通关仍需在可视窗口中游玩确认。
