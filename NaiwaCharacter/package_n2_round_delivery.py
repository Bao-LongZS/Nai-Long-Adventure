"""Package only independently accepted saved R1 data; bind all records to SHA."""
import ast
import hashlib
import json
from pathlib import Path
import zipfile

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'versions/20261007_N2_round_refinement'
SOURCE=ROOT/'versions/20261006_head_neck_refinement/candidate_N2/Naiwa_Character.blend'
SOURCE_SHA='9397186f3ae13e93e05cc0d492c12ceb68b743ab4d2a9a0d650426a47a66b0cb'
SOURCE_FBX_SHA='55f648db74606bca94f4d65cd2b5eb72a2dde2fd95e00127a4e8242b442afa59'

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

native=OUT/'candidate_R1/Naiwa_Character.blend'
fbx=OUT/'candidate_R1/Naiwa_Character.fbx'
assert sha(SOURCE)==SOURCE_SHA and sha(SOURCE.with_suffix('.fbx'))==SOURCE_FBX_SHA
saved_sha=sha(native)
validation=read(OUT/'round_validation.json')
diagnostic=read(OUT/'diagnostics/surface_diagnostics.json')
render=read(OUT/'render_protocol.json')
visual=read(OUT/'visual_acceptance.json')
rebuild=read(OUT/'candidate_R1/rebuild_validation.json')
fbx_check=read(OUT/'candidate_R1/fbx_validation.json')
assert validation['passed'] and validation['candidate_sha256']==saved_sha
assert diagnostic['source_hashes']['R1']==saved_sha
assert render['sources']['R1']['sha256']==saved_sha
assert visual['passed'] and visual['candidate_sha256']==saved_sha
assert rebuild['independent_validation']=='passed' and rebuild['target_sha256']==saved_sha
assert fbx_check['status']=='passed' and fbx_check['source_blend_sha256']==saved_sha
assert fbx_check['fbx_sha256']==sha(fbx)
assert render['same_camera_light_exposure_per_comparison']
assert read(OUT/'reference_comparison/reference_review.json')['constraints']['passed']
center=diagnostic['longitudinal_curvature']['0.000']['front']
assert center['diagnostic_target_20_percent']
assert center['neighbor_peak_reduction_fraction']>=.20-1e-5
build=read(OUT/'candidate_R1/build_report.json')
assert build['saved_blend_sha256']==saved_sha

selection={'recommended':'R1','native':'candidate_R1/Naiwa_Character.blend','fbx':'candidate_R1/Naiwa_Character.fbx',
    'native_sha256':saved_sha,'fbx_sha256':sha(fbx),'fixed_N2_sha256':SOURCE_SHA,
    'fixed_N2_fbx_sha256':SOURCE_FBX_SHA,'D0':.117244812742,
    'actual_raw_and_Idle_surface_validation':'passed','visual_acceptance':'passed',
    'actual_surface_curvature_reduction_fraction':center['peak_reduction_fraction'],
    'complete_original_rig_animation_and_fixed_features_retained':True,
    'scope':'lower cheek/throat, side neck/back shoulder, rear head/neck; no C1 or eye installation changes'}
write(OUT/'selection.json',selection)

seeds=['revise_n2_round_refinement.py','n2_round_sculpt.py','n2_round_mouth_curvature.py',
    'validate_n2_round_refinement.py','render_n2_round_comparisons.py','make_n2_round_comparisons.py',
    'analyze_n2_round_refinement.py','validate_n2_round_fbx.py','export_saved_fbx.py',
    'validate_delivery_rebuild.py','package_n2_round_delivery.py']
needed=set()
def collect(name):
    p=ROOT/name
    if name in needed or not p.is_file():return
    needed.add(name)
    tree=ast.parse(p.read_text(encoding='utf-8-sig'))
    for node in ast.walk(tree):
        modules=[]
        if isinstance(node,ast.Import):modules=[a.name.split('.')[0] for a in node.names]
        if isinstance(node,ast.ImportFrom) and node.module:modules=[node.module.split('.')[0]]
        for module in modules:collect(module+'.py')
for name in seeds:collect(name)
needed.add('run_blender_stage.ps1')
with zipfile.ZipFile(OUT/'source_scripts.zip','w',zipfile.ZIP_DEFLATED) as archive:
    for name in sorted(needed):archive.write(ROOT/name,'NaiwaCharacter/'+name)
write(OUT/'source_scripts_manifest.json',{name:sha(ROOT/name) for name in sorted(needed)})

peak=center['target_peak']['curvature'];drop=center['peak_reduction_fraction']*100
surface=validation['objects']['Naiwa unified golden body']
readme=f'''# 奶蛙 N2 圆润精修 R1 · 2026-10-07

推荐工程：`candidate_R1/Naiwa_Character.blend`；推荐导出：`candidate_R1/Naiwa_Character.fbx`。
图文总览为 `index.html`；快速看 `comparisons/comparison_head.png` 和 `comparisons/comparison_lowerface_detail.png`。

本版从固定 N2 重新载入生成，保留原 N2 工程及 FBX。处理嘴下至喉部的圆顺、侧颈和后肩浅纹、后脑至后颈收腰。五官网格及眼体安装、冠顶、肩根、骨架、完整动画、拓扑、UV、颜色和有效权重独立核对保持；全部最终顶点 Z 与 N2 的 float32 位模式一致。没有实施 C1。

实际中线曲率峰 33.77219 → {peak:.5f}，降低 {drop:.2f}%；相邻窗口一起检查，左右颊及 Raw/Idle 的独立结果见 `diagnostics/surface_diagnostics.md`。主体最大总位移 {surface['maximum_total_displacement']:.8f}，最大 X 位移 {surface['maximum_absolute_X_displacement']:.8f}。低颈、后侧法向及切向测量、三层端点、五官接触和 Walk/Laugh/Jump 肩根检查见 `round_validation.json`。

方法是冻结真实 N2 三角面、源法线与保护区，对最终周期截面 XY 进行联合弯曲光顺，再对真实原生顶点的口周剖面曲率作联合约束。嘴缝距离≤0.012完全冻结，0.012～0.040使用非对称连续释放；保护权重在优化的基函数及曲率 Jacobian 内生效。所有投影保持源 Z。有限迭代的全局求解残差如实保存在 `build_report.json`，不以 C2 或收敛标签代替最终曲面验收。

曲率调整把原窄峰摊成较宽的平台，部分位置的曲率相对同Z源值升高；完整邻峰、保护边界及横向数据全部记录。低粗糙度反射仍可显出既有细小锯齿和浅纹，未把“无任何纹路”作为交付结论。

彩色/灰模、真实低粗糙度反射条纹、两方位掠射及全身图均固定相机、光照和曝光。四区域视觉结论与保留的自然口周细节见 `visual_acceptance.json`。固定参考沿用原50°/−2.5°机位及注册系数，逐行误差约束见 `reference_comparison/reference_review.json`。

FBX已独立重导入，逐网格真实顶点和三角面表面、权重及动作均核对；见 `candidate_R1/fbx_validation.json`。独立复建与原生交付的所有坐标/元数据/动画逐项相符，见 `candidate_R1/rebuild_validation.json`。

## 从固定 N2 复建

在项目根目录运行已交付脚本；`candidate_R1/parameters.json` 记录最终参数。命令见 `rebuild_command.ps1`，输出到独立临时目录，避免覆盖推荐工程。源码闭包封存在 `source_scripts.zip`，归档内沿用项目的 `NaiwaCharacter/` 结构；必要固定输入 N2 保留在原版本目录。

复现环境为 Blender 5.2.2、Python/NumPy/Pillow。图文脚本使用 `C:/Windows/Fonts/msyh.ttc`；渲染参考依赖保留的球面眼相机协议、旧N2参考误差表及 `references/20261005/pdf_page03_image03_IM60.jpg`。这些是共同基准，未重新注册参考图。

## 文件索引

- `section_raw.json` / `raw_mesh_measurements.npz`：独立保存工程的 Raw/Idle 真实三角剖面、坐标、源法线。
- `diagnostics/`：曲率 CSV/JSON、纵横剖面、侧颈频段辅助诊断、位移与保护热图。
- `comparisons/`：N2/R1 原图及固定条件拼版。
- `selection.json`：推荐路径、源及交付 SHA；`file_manifest.json`：逐文件校验清单。

固定 N2 SHA256：`{SOURCE_SHA}`。
推荐 R1 SHA256：`{saved_sha}`。
'''
(OUT/'README_交付说明.md').write_text(readme,encoding='utf-8')

items=[('彩色头部','comparisons/comparison_head.png'),('嘴下灰模细节','comparisons/comparison_lowerface_detail.png'),
    ('低粗糙度反射','comparisons/comparison_reflection_detail.png'),('两方位掠射 A','comparisons/comparison_grazing_A.png'),
    ('两方位掠射 B','comparisons/comparison_grazing_B.png'),('全身','comparisons/comparison_full.png'),
    ('实际曲率','diagnostics/actual_curvature_diagnostics.png'),('实际纵剖面','diagnostics/actual_longitudinal_sections.png'),
    ('后脑与后颈','diagnostics/actual_rear_curvature_diagnostics.png'),('位移与保护','diagnostics/displacement_heatmap.png'),
    ('固定参考','reference_comparison/fixed_reference_comparison.png')]
assert all((OUT/p).is_file() for _,p in items)
cards=''.join(f'<figure><figcaption>{title}</figcaption><a href="{p}"><img loading="lazy" src="{p}" alt="{title}"></a></figure>' for title,p in items)
html=f'''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>奶蛙 R1 圆润精修</title><style>body{{font:16px/1.65 system-ui;background:#eef2f6;color:#1a2940;max-width:1200px;margin:36px auto;padding:0 24px}}h1{{font-size:30px}}a{{color:#236ba0}}figure{{margin:28px 0;background:white;border-radius:12px;padding:20px}}figcaption{{font-size:20px;margin-bottom:14px}}img{{display:block;width:100%;height:auto}}.intro{{background:#19283d;color:white;padding:24px;border-radius:12px}}.intro a{{color:#9fcef1}}</style><h1>奶蛙 · N2 圆润精修 R1</h1><div class="intro">实际中线曲率峰下降 {drop:.2f}%；五官、颈中段、肩根和全部 Z 保持。<p><a href="candidate_R1/Naiwa_Character.blend">原生工程</a>　<a href="candidate_R1/Naiwa_Character.fbx">FBX</a>　<a href="README_交付说明.md">交付说明</a>　<a href="diagnostics/surface_diagnostics.md">实际曲面诊断</a></p></div>{cards}</html>'''
(OUT/'index.html').write_text(html,encoding='utf-8')
entries=[]
for p in sorted(OUT.rglob('*')):
    if not p.is_file() or p.name=='file_manifest.json' or '__pycache__' in p.parts:continue
    assert 'trials' not in p.relative_to(OUT).parts,'Remove temporary trial geometry before packaging'
    entries.append({'path':p.relative_to(OUT).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)})
write(OUT/'file_manifest.json',{'candidate_sha256':saved_sha,'source_N2_unchanged':True,'files':entries})
print('N2_ROUND_DELIVERY_PACKAGED',len(entries),'files',saved_sha)
