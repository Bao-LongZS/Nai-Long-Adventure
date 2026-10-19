# 奶蛙冒险

<img src="UE5/Records/p1_acceptance_20261018/alpha018_cp08_dying_20261018T140643Z.png" width="820">

横版闯关小游戏。主角奶蛙是「奶龙」的二创形象，非官方。

最早用 C++ / SFML 写了原型，现在用 UE5 重做。角色由一个 Python 脚本在 Blender 里生成，改参数重跑就更新，不用手动雕。

## 现在

| 内容 | 情况 |
| --- | --- |
| 1-1 平台关 | 能完整通关 |
| 奶蛋闯关 2-1 / 2-2 / 2-3 | 设计完成，未开工 |
| 打包 | Development 和 Shipping 都出过包，双击能起 |
| 手柄、跨设备性能 | 没做 |

1-1 有四种机关：隐藏尖刺、弹跳板、限时平台、开关移动平台。前半段一种一种教，后半段组合着用。8 个检查点，摔死就地复活，机关一起复位。终点得先拿到指定的那颗拾取物。

死亡换成了捧腹大笑的动画，音效按检查点分段换人声，一局死到第 3 次会放一段更长的彩蛋。

奶蛋闯关是第二关，分 2-1 斩首、2-2 选蛋、2-3 综合三小关，从 1-1 终点淡出淡入接过去，用不同种类的奶蛋当弹弓子弹打结构。

## 运行

需要 UE 5.8，脚本默认从 `C:\Program Files\Epic Games\UE_5.8` 找引擎，装在别处加 `-Engine <路径>`。

```powershell
powershell -File UE5/launch_p1_v7.ps1          # 进游戏，全高质量、无垂直同步
powershell -File UE5/launch_p1_v7.ps1 -Editor  # 打开编辑器
```

也可以直接用 UE 打开 `UE5/NaiwaP0/Nai_Long.uproject`。

| 按键 | 作用 |
| --- | --- |
| `A` / `D` | 左右移动 |
| `Space` | 跳 |
| `Shift` | 切换走和跑 |
| `R` | 重开本关 |
| `P` / `Esc` | 暂停 |

跑是走的两倍速，有几个断口必须跑着过。

## 命令

```powershell
# 打包，产物在 UE5/NaiwaP0/Saved/DemoBuild*
powershell -File UE5/package_demo.ps1                  # Development
powershell -File UE5/package_demo.ps1 -Config Shipping # Shipping
```

```bash
# 重建角色模型，需要 Blender
python NaiwaCharacter/build_naiwa.py --from-primitives

# 编 SFML 原型，早期练手版，已冻结
cmake -S . -B out/build && cmake --build out/build --config Release
```

每轮迭代目录里都留着 `rebuild_command.ps1` 和 `parameters.json`，可以单独复建那一轮并校验哈希。

## 目录

| 目录 | 内容 |
| --- | --- |
| `UE5/` | 主线。`NaiwaP0/` 是引擎工程，1-1 的地图和资产在 `Content/Naiwa/P1/`；`SourceAssets/` 放音效贴图原素材；`Scripts/` 是建图、导入、验收脚本；`Records/` 是报告和录像 |
| `NaiwaCharacter/` | 建模流水线，`versions/` 按日期留了每一轮工程 |
| `NaiwaAnimation_20261009/` | 打招呼动画，工程加十秒成片 |
| 根目录 `.cpp` | SFML 原型 |

## 文档

| 文档 | 看什么 |
| --- | --- |
| [CHANGELOG.md](CHANGELOG.md) | 每轮做了什么，为什么这么选，放弃了什么 |
| [P1 工程报告](UE5/Records/P1_工程报告_20261018.md) | 1-1 的实现范围、验收结果、还剩什么没清 |
| [关卡结构图](UE5/Records/关卡结构图_v7_20261018.html) | 正式关卡的段落、检查点、机关布局 |
| [P1 计划书](UE5/Records/P1_计划书_20261014.md) | 阶段怎么拆、按什么口径验收 |
| [P2 工程计划书评估](UE5/Records/P2_工程计划书评估_20261019.md) | 奶蛋闯关的结构与风险 |
| [UE5/Records/](UE5/Records) | 全部报告、运行日志、录像 |

P0 的[最终验收](UE5/Records/p0_completion_20261013/completion_report.md)、[接入过程记录](UE5/README_接入记录.md)、[主角接入清单](Naiwa_UE5_2.5D主角接入清单_20261010.md)、[角色流水线用法](NaiwaCharacter/README_使用说明.md)、[E2 形体修改指令](Naiwa_E2对照原图评估与整体修改指令_20261008.md)。

## 版权

「奶蛙」是基于「奶龙」的二次创作形象，和原作者及任何官方方无关。仓库内的参考素材版权归各自原作者，仅用于个人学习与创作。
