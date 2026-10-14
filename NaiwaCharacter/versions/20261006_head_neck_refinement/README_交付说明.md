# 下脸—颈部第一阶段交付

第一阶段完成，推荐 **N2**；N1保留为温和对照。仅修改下脸—颈部，后续上部头壳未实施。

[推荐N2原生工程](candidate_N2/Naiwa_Character.blend) · [推荐FBX](candidate_N2/Naiwa_Character.fbx) · [N1对照工程](candidate_N1/Naiwa_Character.blend) · [选择记录](selection.json)

![彩色三视图](comparisons/comparison_head.png)

![灰模三视图](comparisons/comparison_head_gray.png)

![固定参考](reference_comparison/reference_stable_comparison.png)

## 修改与验收

从固定球面眼A分别载入制作N1/N2，D0=0.117244812742。只在2.030<Z<2.380执行世界Y截面重映射，设计锚点不变；全局最小三阶变化的受限C2曲线连接锚点，端点位移和一二阶导数归零。连续肩根保护三角面距离配合0.012–0.140羽化，保持肩关节并消除过渡浅纹。对应表面壳同步修正。

| 候选 | 前颈误差减少 | 后颈误差减少 | 剩余平均前/后误差 | 最大实际位移 |
| --- | ---: | ---: | ---: | ---: |
| N1 | 35.98% | 39.22% | 27.10 / 13.19px | 0.054999292 |
| N2 | 48.95% | 54.40% | 21.61 / 9.89px | 0.074998677 |

误差使用同一50°/−2.5°相机和原注册系数，由无光照主体ID量测参考y220/260/300，没有重新拟合相机、缩放或平移。N2改善更明显，三视图和额外镜头均保持短圆下脸、前颈回收、胸部展开；没有新增硬颈环、尖下颌或胸口台阶。剩余误差及基线远眼暴露仍保留记录，未扩展本次修改范围。

独立验收通过：原始及Idle五官坐标、眼球半径、眼径/眼距、冠顶、口周、固定肩根、X/Z和作用区外坐标保持；拓扑、UV、材质、有效权重、骨架与完整动画曲线保持。检查Walk/Laugh/Jump肩根转折姿态，无新穿插或翻面。分别载入独立复建N1/N2，所有网格与骨架的保存及Idle评估坐标逐项一致。推荐FBX重导入为6个绑定网格、10根骨骼、4个动作，主体/眼顶点颜色保留。

[完整几何验收](head_neck_validation.json) · [实际截面CSV](comparisons/section_measurements.csv) · [中线剖面](comparisons/comparison_midline_sections.png) · [水平截面](comparisons/comparison_horizontal_sections.png) · [固定参考复核](reference_comparison/reference_review.md) · [FBX验收](candidate_N2/fbx_validation.json) · [N1复建验收](candidate_N1/rebuild_validation.json) · [N2复建验收](candidate_N2/rebuild_validation.json)

## 重复生成

从项目根目录运行；输出到新目录，避免覆盖固定输入或已选工程。选版记录存在时，生成脚本同时导出推荐N2的FBX。

```powershell
& 'D:\STEAM\steamapps\common\Blender\blender.exe' --background --python-exit-code 1 --python 'NaiwaCharacter\revise_head_neck_refinement.py' -- --candidate neck --output 'NaiwaCharacter\versions\20261006_head_neck_refinement\reproduced'
```

[生成入口](../../revise_head_neck_refinement.py) · [截面修正函数](../../head_neck_refinement_sculpt.py) · [受限C2曲线](../../bounded_c2_profile.py) · [参数JSON](parameters.json) · [独立验收入口](../../validate_head_neck_refinement.py)

固定A SHA256：`f251f5a0fc00fedf1df76fcc6286bc40e102406a536cb6c676a93e176cb71ca0`。推荐N2 SHA256：`9397186f3ae13e93e05cc0d492c12ceb68b743ab4d2a9a0d650426a47a66b0cb`。A/B原交付保持。复建临时副本删除，仅保留验收报告及日志。
