# G0 开工状态

更新时间：2026-10-29（Asia/Shanghai）  
状态：实现继续推进；G0 尚未通过，不能判定 Go；G1 未开始。

## 已完成

- 记录初始 Git HEAD、UE 5.8.3 版本、非 WebNative 工作树摘要和关键源文件哈希：[`baseline-v1.json`](baseline-v1.json)。
- 建立 TypeScript、Vite、Three.js 页面壳、响应式画布，以及键盘/触屏共用的 P1 输入路由。
- 通过 UE 5.8.3 PythonScript commandlet 只读导出正式 P1 v7 地图：73 个 Actor、272 个组件，含配置、检查点和世界变换/边界快照。
- 通过只读 C++ 导出入口调用 `NaiwaP2::FSceneLibrary::Make21`，取得 2-1 的 17 个场景盒、弹药规则、物理参数和 UE 源文件哈希。
- P1 开场至 CP01 的页面切片已接入地图快照、奶龙游戏网格和共享骨架跑步动画；跳跃缓冲与离台宽限也已加入原型。
- P1 网页切片现按 UE 角色参数采用 42/90 cm 胶囊、Walk150（1.5 m/s）/Run300（3.0 m/s）、18 m/s² 加速度、22 m/s² 刹车、0.4 空中控制、6.5 m/s 跳跃初速、1.6 重力倍率及最多 0.2 秒按住跳跃抑制重力；Shift/触屏按钮可切换步态。走路状态复用已导出的跑步动画并以半速播放，尚无独立 Walk 动画。网页 AABB 碰撞仍为原型近似，尚未做浏览器手动验收。
- 修正了角色 GLB 朝向：移除重复的 X 轴 90° 旋转，并按 UE `MeshRightYaw=-90°` 设置右向、反向加 180° 设置左向。导出根节点变换的数值核对为 GLTF 上方向→世界 Z、角色右向→世界 X；尚无浏览器画面验收。
- 新增独立 P2 物理探针页 [`../p2-probe.html`](../p2-probe.html)：按需下载 Rapier WASM，按 UE 快照创建刚体，可运行 5 秒静置位移测量、0° 命中和 45° 未命中弹丸形状扫掠，并下载带 P2 源哈希的事件轨迹 JSON。
- 导出并接入 UE P2 基础蛋骨骼网格：1031 顶点、1798 三角形、`EyeWide/Scream/Squash` 三个 morph target，UE 报告尺寸 85 × 82.025 × 115 cm；网页 GLB 的 Y-up 尺寸为 0.85 × 1.15 × 0.82025 m。4 个材质槽按 UE 导入报告中的基础蛋颜色与已审面部材质色恢复；剔除 UE glTF 导出器生成的全黑顶点色。P2 探针首次发射时显示该 UE 网格，碰撞体继续使用 UE 的 44 cm 半径球体。
- `npm run build` 通过：TypeScript 严格类型检查和 Vite 双入口生产构建均通过。

## 当前数据指纹

| 产物 | 大小 | SHA-256 / 内容 |
|---|---:|---|
| `public/data/p1_v7_map.json` | 356,098 B | `100c6299cf2ba76e621f3c9806e3e38021f417c01678d14a07bb8742b9854f45`；73 Actor / 272 组件 |
| `public/data/p2_21_scene.json` | 7,538 B | `69d9b6ca2ec5d86793a3a7b2bf5fc9b8bdd656deaf53cac987f790e3ed75600a`；17 个结构盒 |
| `public/assets/asset-manifest.json` | 1,832 B | `382abe28d2bf46cfa412072f4e2deb3897e09e1fba39530350afdf70347d7679` |
| UE P1 奶龙网格 `.glb.gz` | 10,461,046 B | Manifest 记录源 `.uasset` 与压缩文件 SHA-256；解压前 GLB 为 46,374,232 B |
| UE 跑步动画 `.glb.gz` | 10,523 B | Manifest 记录源 `.uasset` 与压缩文件 SHA-256；解压前 GLB 为 37,876 B |
| UE P2 基础蛋 `naiwa-egg-base.glb` | 112,460 B | `14a739d02a4a10008c954f68f097a30ee7c22f1e53fd412fc0be65b2954b2527`；尺寸/面数/morph/材质槽已校验 |

P2 探针单独加载 Rapier WASM，以免 P1 入口承担物理包下载。当前生产构建的 P2 入口约 17.58 kB；Three.js/GLTFLoader chunk 为 616.52 kB（gzip 156.37 kB），Rapier chunk 为 4,335.14 kB（gzip 1,670.00 kB），Rapier 只会在首次运行用例时请求。112,460 B 的基础蛋 GLB 首次发射时请求。首版发布仍需继续评估资源大小。

## 尚未通过的 G0 项

- P1 路线、Walk/Run、触屏控制、危险复活和 UE 行为表尚无本轮人工浏览器验收记录；网页物理仍是原型规则。
- P2 Rapier 静置、命中、未命中逻辑和 UE 蛋模型接入已完成代码与构建；本轮未在浏览器中实际运行，尚无位移数值、命中轨迹或显示验收；也未与 UE Chaos 结果作对应比较。glTF 使用扁平 PBR 颜色替代未能烘焙的 UE 材质图，动画 morph target 已导出但尚未在页面驱动。
- Android Chrome、iPhone Safari、PC Chrome/Edge 实机/浏览器版本、五分钟帧时间、首屏时间、JS/WASM 内存尚未采集；QQ 浏览器也没有人工兼容性结论。
- 没有公网域名或固定发布链接；当前仅有局域网开发入口。

## 复现命令

```powershell
cd WebNative
npm ci
npm run build
npm run dev -- --host 0.0.0.0
```

开发服务器首页提供 P1 切片和 P2 探针入口；探针也可通过 `/p2-probe.html` 直接访问。首次运行物理用例时，页面按需加载 Rapier WASM。

重新导出 P1：`.\tools\export-p1-map.ps1`。  
重新构建 P2 Editor 模块并导出：`.\tools\export-p2-scene.ps1`。  
重新导出 UE 角色与动画：`.\tools\export-gameplay-character.ps1`。

重新导出 UE P2 基础蛋：`.\tools\export-base-egg.ps1`。

导出脚本显式生成的数据和报告只写入 `WebNative/public/data/`、`WebNative/public/assets/` 与 `WebNative/docs/`，不调用 UE 保存关卡或资产的接口。UE commandlet 启动仍会产生引擎日志和缓存。
