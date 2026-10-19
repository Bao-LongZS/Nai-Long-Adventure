# 奶蛙冒险 · Nai-Long-Adventure

横版闯关游戏。主角「奶蛙」是拿网络形象「奶龙」二创的，不是官方作品。

最早用 C++ / SFML 写过一版原型，现在整体搬到 UE5 重做。角色是用 Python 脚本在 Blender 里生成的，改个参数重跑一遍，模型就更新了。

## 现在能玩到什么

第一关能从头跑到尾。往右跑，跳过断口，踩弹跳板飞上高台，赶在平台缩回去之前冲过去，来回按开关把移动平台召到脚下，路上还要躲开平时看不见、踩上去才现形的尖刺。终点得先拿到指定的那颗拾取物才算过。

摔死不用从头来。8 个检查点，掉下去在最近的安全岸上原地复活，机关也一起复位。死亡画面是一段捧腹大笑的动画，音效按检查点分段换人声，单次闯关死到第 3 次会放一段更长的彩蛋。

第二关是奶蛋弹弓关，分 2-1 斩首、2-2 选蛋、2-3 综合三小关，从第一关终点淡出淡入接过去。计划书 v3 写完了，估 34–38 人天，还没开工；先花 6–8 天做个切片，验证引擎能不能复现仿真算出来的弹道。

打包有 Development 和 Shipping 两个版本，双击能起。手柄和别的机器上的帧率还没测。

## 跑起来

需要 UE 5.8。脚本默认在 `C:\Program Files\Epic Games\UE_5.8` 找引擎，装别处就加 `-Engine <路径>`。

```powershell
# 进游戏，全高质量、无垂直同步
powershell -File UE5/launch_p1_v7.ps1

# 或者打开编辑器
powershell -File UE5/launch_p1_v7.ps1 -Editor
```

也可以直接用 UE 打开 `UE5/NaiwaP0/Nai_Long.uproject`。

`A` / `D` 走，`Space` 跳，`Shift` 切走和跑，`R` 重开本关，`P` 或 `Esc` 暂停。跑步速度是走的两倍，有些断口只有跑才过得去。

打包：

```powershell
powershell -File UE5/package_demo.ps1
powershell -File UE5/package_demo.ps1 -Config Shipping
```

产物在 `UE5/NaiwaP0/Saved/DemoBuild*`。`-SkipBuild` 跳过编译直接用现有二进制，`-UseUBA` 打开共享编译加速（默认关着，这台机器上它偶尔会写盘失败）。

改角色模型要装 Blender：

```bash
python NaiwaCharacter/build_naiwa.py --from-primitives
```

每轮迭代目录里都有 `rebuild_command.ps1` 和 `parameters.json`，可以单独复建那一轮并校验哈希。

SFML 原型已经冻结，想编出来看看：

```bash
cmake -S . -B out/build && cmake --build out/build --config Release
```

## 目录

| 目录 | 内容 |
| --- | --- |
| `UE5/` | 主线。`NaiwaP0/` 是引擎工程，第一关的地图和资产在 `Content/Naiwa/P1/`，`SourceAssets/` 放音效和贴图的原始素材，`Scripts/` 是建图、导入、验收的脚本，`Records/` 是报告和录像 |
| `NaiwaCharacter/` | 建模流水线。`versions/` 按日期留了每一轮工程，没采用的候选也在里面 |
| `NaiwaAnimation_20261009/` | 打招呼动画，工程加十秒成片 |
| 根目录 `.cpp` | SFML 原型，早期练手 |

## 文档

- [CHANGELOG.md](CHANGELOG.md)：每轮做了什么、为什么这么选、放弃了什么
- [P1 工程报告](UE5/Records/P1_工程报告_20261018.md)：第一关的实现范围、验收结果、还剩什么没清
- [关卡结构图](UE5/Records/关卡结构图_v7_20261018.html)：正式关卡的段落、检查点、机关布局
- [P1 计划书](UE5/Records/P1_计划书_20261014.md)：当初怎么拆阶段、按什么口径验收
- [P2 工程计划书评估](UE5/Records/P2_工程计划书评估_20261019.md)：第二关三关结构、工时与风险
- [UE5/Records/](UE5/Records)：全部报告、运行日志、录像

另外几份：P0 的[最终验收](UE5/Records/p0_completion_20261013/completion_report.md)、[接入过程记录](UE5/README_接入记录.md)、[主角接入清单](Naiwa_UE5_2.5D主角接入清单_20261010.md)、[角色流水线用法](NaiwaCharacter/README_使用说明.md)、[E2 形体修改指令](Naiwa_E2对照原图评估与整体修改指令_20261008.md)。

## 版权

「奶蛙」是基于「奶龙」的二次创作形象，和原作者及任何官方方无关。仓库里的参考素材版权归各自原作者，仅用于个人学习与创作。
