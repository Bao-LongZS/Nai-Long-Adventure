# P5 A0 背景层间摆放修复记录

日期：2026-10-29（Asia/Shanghai）  
状态：**近景锚点修复已编译并链接；三层背景材质已用 UE 5.8.3 后台 Commandlet 重建并读回为 Translucent；实际 Editor 画面仍待负责人复核。**

## 定位

- v07 使用 2300 cm 正交相机宽度和 16:9 画面；相机 Z 基准为路线面上方 720 cm，平地画面理论下沿约为路线面上方 73 cm。
- 按 v04 雪脊 PNG 的 alpha 轮廓计算，旧近景锚点 -20 cm 时，雪脊最高可见像素约在 +26 cm；整层轮廓低于镜头下沿，所以近景雪脊在画面中基本缺席。中景森林底缘与雪坡之间少了可见的前景衔接层。
- 森林 PNG 底部有 32 行 alpha 渐隐，但原背景材质为 Masked、裁剪阈值 0.01，绝大多数渐变像素会被当作不透明像素显示，视觉上仍像硬边。

## 修订

- `NaiwaP5PrototypeTrackActor.cpp` 将远山/森林/雪脊中心锚点设为 `455 / 185 / 260 cm`。近景锚点由 -20 提至 +260；按当前源图 alpha 轮廓，平地画面下沿至雪脊最高点之间约有 233 cm 的可见雪脊带。路线与玩家仍在前景深度，不改变碰撞/路线数据。
- `p5_build_a0_art_sample_v05.py` 将远山、森林、雪脊材质改为 Unlit Translucent，保留 PNG 的透明渐变与抗锯齿；角色/树石 cutout 仍使用 Masked。A0 地图路径和玩法参数不变。
- 新增 [`P5_A1_Backgrounds_manifest_v08.json`](../../SourceAssets/P5/Manifests/P5_A1_Backgrounds_manifest_v08.json)，保留 v07 历史版。
- 背景材质脚本将远山、森林、雪脊设为 Unlit Translucent，并在保存后断言 Blend Mode。负责人退出 Editor 后，在后台以 NullRHI 重跑材质 Commandlet：12 张纹理/对应材质重建成功，报告 `materials_rebuilt_without_loading_map`，far/mid/near 均读回 `BLEND_TRANSLUCENT`；三张背景材质 UAsset 时间戳更新至 15:34。命令日志与 JSON 报告分别为 [`P5_A0_BackgroundPlacement_materials_2026-10-29.log`](P5_A0_BackgroundPlacement_materials_2026-10-29.log) 和 [`P5_A0_BackgroundPlacement_materials_report_2026-10-29.json`](P5_A0_BackgroundPlacement_materials_report_2026-10-29.json)。
- 新增 293 m / 1123 m 背景静态复合预览。预览用实际背景 PNG、当前相机窗口和三个视差偏移计算合成，可检查相对摆放与重复相位；它不是 UE 材质/透明排序或 PIE 截图，不能作为运行时通过证据。

## 证据

- [293 m 静态复合预览](P5_A0_BackgroundPlacement_v08_293m_composite.png)
- [1123 m 静态复合预览](P5_A0_BackgroundPlacement_v08_1123m_composite.png)
- `Nai_LongEditor Win64 Development` 最终构建成功并写出 `UnrealEditor-NaiwaRuntime.dll`（12/12 actions）；其中 `NaiwaP5PrototypeTrackActor.cpp` 已在前一轮编译，最终轮完成 Runtime 链接。UBT 提示部分 action 结果缓存未写入，不影响 `Result: Succeeded`。
- 该构建首次被工作树中 P4 的两处现存编译错误挡住；为完成 Runtime 链接，按 P4 规格补回玩家奶球基础伤害字段（12 点）并将 `CountByPredicate` 改为等价数组循环，没有改玩法数值。
- 最新材质 Commandlet 日志显示 Python 脚本执行成功、0 个错误、报告读回三层均为 `BLEND_TRANSLUCENT`；日志汇总另有一条约束目标为 `None` 的项目警告，与材质重建无关。当前资产已保存；透明排序、边缘过滤与构图仍需打开地图实际复核。

## 待复核

- 负责人打开 `L_NaiwaP5_A0ArtSample_v05`，检查 293 m、1123 m 及断崖附近：雪脊与真实雪坡是否接合、远/中/近层是否遮挡正确、横线是否消失。
- 在 1080p 和 720p 检查已重建的 Translucent 材质排序、透明边过滤及视差循环；性能需在可见 Editor 中实测。
- 在上述复核完成前，A0/A1 视觉出口门保持未通过，A2 不启动。
- 森林透明渐变已写入并保存为 Translucent；负责人复核真实相机视角与材质排序后，才能通过 A0/A1 视觉出口门。
