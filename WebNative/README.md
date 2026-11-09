# 奶龙冒险 Web Native

网页原生版 G0 工程入口。此目录与 UE 游戏工程分开；G0 导出工具只读 UE 源，输出写入本目录。开工进度与门槛记录见 [`docs/g0-report.md`](docs/g0-report.md)。

## 本机启动

```powershell
cd WebNative
npm ci
npm run dev -- --host 0.0.0.0
```

在同一 Wi-Fi 的手机上打开终端打印的局域网地址。首次测试时记下手机型号、系统、浏览器版本和屏幕方向。当前场景使用 P1 v7 正式地图导出的灰盒几何及 UE 实际游戏角色/跑步动画；支持 Shift/触屏切换 Walk150（1.5 m/s）与 Run300（3.0 m/s），走路暂用跑步动画半速播放。玩法逻辑仍是 G0 原型，不能视为正式关卡验收通过。

不要双击 `index.html` 或 `p2-probe.html` 以 `file://` 方式打开；入口依赖 Vite 提供的模块与资源路径。直接打开时页面会显示启动说明，运行上面的开发命令后再访问 Vite 地址。

P1 首页底部链接可进入 **P2 物理探针**，也可在开发服务中直接访问 `/p2-probe.html`。探针按需加载 Rapier WASM；第一次发射时再加载 UE P2 基础蛋 GLB。蛋的 85 × 115 × 82.025 cm 网格与四个材质槽来自通过 UE 导入校验的基础蛋资产，碰撞仍使用 UE 的 44 cm 球体。页面提供 5 秒静置稳定性、一个命中与一个未命中弹丸用例；用例会按固定步形状扫掠并记录刚体接触/冲量事件，可下载附带 UE 源哈希的 JSON。它用于 G0 对照，不代表 Rapier 已通过 Chaos parity 或手机验收。

## G0 工具

- `node tools/record-baseline.mjs`：重新采集 git HEAD、非 WebNative 工作树状态摘要和关键 UE/模型文件哈希。
- `tools/export-gameplay-character.ps1`：用 UE 5.8 glTF Exporter 只读导出 P1 实际使用的奶龙骨骼网格与跑步动画，压缩成 `.glb.gz` 后供浏览器流式下载和解压；不保存任何 UE 资产。
- `tools/export-base-egg.ps1`：只读导出 UE P2 基础蛋骨骼网格，检查模型尺寸、三角形数、蒙皮与 morph target，并按 UE 已审颜色回填 glTF 材质；不保存任何 UE 资产。
- `node tools/build-asset-manifest.mjs`：计算导出 GLB 和源资产的哈希，写入 `public/assets/asset-manifest.json`。
- `tools/export-p1-map.ps1`：用 UE PythonScript commandlet 只读加载正式 P1 v7 地图，导出 Actor、组件、变换、边界和配置关系。
- `tools/export-p2-scene.ps1`：通过已编译的 UE 只读入口导出 P2 2-1 结构、弹药和物理 profile。
- `npm run build`：类型检查和生成静态构建产物。

P1 地图只从正式 v7 UE 关卡导出。P2 场景通过 `NaiwaP2::FSceneLibrary` 的只读 C++ 导出入口生成数据。导出器不得保存 `.umap` 或 `.uasset`。

## 当前 G0 限制

- 页面使用 Three.js/WebGL 2。当前可以游玩 P1 开场至 CP01 的原型；需要继续校准运动、碰撞和机关规则。P2 物理探针已加入，但尚无本轮浏览器运行结果，也未与 UE Chaos 对照。
- 手机五分钟帧基准须在目标 Android Chrome / iPhone Safari 真机前台执行；模拟器或 PC 结果不能代替实机验收。
- 网页角色 GLB 使用 UE 5.8 glTF Exporter；浏览器从 gzip 侧车资源解码后获得普通 GLB。正式公网部署需把 `/assets/*.glb` 映射到同名 `.glb.gz`，并配置 `Content-Encoding: gzip`、`Content-Type: model/gltf-binary` 和长期缓存；Vite 开发与预览服务器已内置映射。
- P2 的历史 `p2_ballistics_fixture.json` 使用 0.004 秒显式 Euler。它没有被当作当前 projectile driver 的物理真值。
