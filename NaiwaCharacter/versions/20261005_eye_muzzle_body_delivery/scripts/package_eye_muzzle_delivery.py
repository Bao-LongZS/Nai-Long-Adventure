"""Package only the final deliverables, fixed input and required source files."""
import json,zipfile,hashlib,ast,shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'versions/20261005_eye_muzzle_body_delivery'
report=json.loads((OUT/'delivery_validation.json').read_text(encoding='utf8'))
rebuild=json.loads((OUT/'rebuild_validation.json').read_text(encoding='utf8'))
assert report['independent_validation']==rebuild['independent_validation']=='passed'
assert 'FBX_REIMPORT_OK' in (OUT/'fbx_validation.log').read_text(encoding='utf-8-sig')
assert 'EYE_MUZZLE_BODY_REVISION_OK' in (OUT/'build.log').read_text(encoding='utf-8-sig')

scripts=['build_naiwa.py','revise_eye_muzzle_body.py','eye_muzzle_sculpt.py','body_finish_sculpt.py',
 'connection_sculpt.py','head_volume_sculpt.py','neck_extension_sculpt.py','neck_blend_sculpt.py',
 'neck_direction_sculpt.py','longitudinal_surface_sculpt.py','short_round_face_sculpt.py',
 'broad_face_arc_sculpt.py','lip_eye_sculpt.py','continuous_front_face_sculpt.py',
 'eye_projection_measure.py','render_character_delivery.py','render_surface_diagnostics.py',
 'render_eye_muzzle_comparison.py','validate_eye_muzzle_delivery.py','validate_eye_muzzle_revision.py',
 'validate_lip_eye_refinement.py','validate_fbx.py','validate_delivery_rebuild.py',
 'validate_head_revision.py','inspect_eye_muzzle_baseline.py','make_delivery_comparisons.py',
 'package_eye_muzzle_delivery.py']
scriptdir=OUT/'scripts';scriptdir.mkdir(exist_ok=True)
for name in scripts:
    ast.parse((ROOT/name).read_text(encoding='utf-8-sig'),filename=name)
    shutil.copy2(ROOT/name,scriptdir/name)
shutil.copy2(ROOT/'iterations/before_eye_muzzle_body_revision/baseline_dimensions.json',scriptdir/'eye_muzzle_baseline.json')
fixed=scriptdir/'iterations/before_eye_muzzle_body_revision';fixed.mkdir(parents=True,exist_ok=True)
for name in ['Naiwa_Character.blend','baseline_dimensions.json','build_naiwa.py','README_使用说明.md']:
    shutil.copy2(ROOT/'iterations/before_eye_muzzle_body_revision'/name,fixed/name)

eye=report['eyes']['L'];stage=report['stage_geometry'];belly=report['belly_color_contour']
readme=f'''# 奶蛙：眼体、短吻部与身体收尾交付版（2026-10-05）

本目录的 Naiwa_Character.blend 是修改后的新工程，Naiwa_Character.fbx 是相应的骨骼网格与四段基础动作。原主目录工程及 scripts/iterations/before_eye_muzzle_body_revision/Naiwa_Character.blend 保留修改前版本。站立、双臂垂放、头顶和腹部体积保持。

## 实际尺寸（Blender 单位）

| 项目 | 实测 / 采用值 |
|---|---:|
| 修改前脚底至头顶 H | {report['H_before']:.8f} |
| 绿色外边界等面积外径 D | {report['D_fixed']:.8f} |
| Z 轴 / 前方 F | (0,0,1) / (0,-1,0) |
| 面部上移最大值 | {report['face_Z_maximum']:.8f}（0.4D） |
| 双眼中心距 | 0.17595644，保持 |
| 眼体最终凸出量 | {eye['actual_maximum_F_projection']:.8f}（0.20D） |
| 黑瞳外径 | {eye['actual_pupil_diameter']:.8f}（0.53D） |
| 头顶至眼中心 | {eye['forehead_in_D']:.4f}D |
| 嘴缝总宽 / 中央开口 | {report['mouth']['width']:.8f} / {report['mouth']['central_opening']:.8f} |
| 短吻部前推（抬脸后，挖嘴前） | {stage['muzzle_F_advance']:.8f} |
| 嘴下0.9D处喉弧回收 | {stage['throat_minus_F_retraction']:.8f} |
| 嘴缝真实几何凹入 | {stage['final_mouth_geometric_recess']:.8f} |
| 腹部色区宽比 | {belly['actual_width_ratio']*100:.4f}% |
| 腹部色区下缘实际延伸 | {belly['actual_lower_extension']:.8f}（目标0.025H={belly['requested_lower_extension']:.8f}） |

D 按绿色外轮廓包围的可见面积求等效圆外径；不是绿色圆环宽度，瞳孔大小不参与外径定义。原始绿色外轮廓略呈竖椭圆，重做后正面为圆形。实测与指定值之间的细小差值来自保存网格和颜色插值的离散采样。

## 对比与验收

comparison_full.png 和 comparison_head.png 各含前后两行、正面0°/侧面90°/斜侧45°三列。12张 before/after 原图保留原分辨率。camera_protocol.json 记录共同正交相机、距离、取景、灯光与曝光。naiwa_surface_gray_* 为额外灰模检查。

delivery_validation.json 是保存工程的独立核验。stage_face_raise.blend 与 stage_muzzle.blend 是压缩的阶段几何证据，用于分别核验短吻部与喉弧位移。身体、眼体和闭合嘴腔在检查范围内未发现重复、退化面或非相邻自交；眼根嵌入、嘴面贴合、自然腋窝及腿脚几何均已检查。肩根前/后凹沟量测改善约34%/39%。腹部仅修改顶点颜色，全脚覆盖深棕，踝部渐变。

FBX 重导入确认6个网格、10根骨骼、4段动作及身体/双眼顶点颜色。rebuild_validation.json 对独立重建与本工程的坐标、颜色、拓扑、骨架和权重逐项比较，误差为零。

## 再生成

用 Blender 后台运行 scripts/build_naiwa.py，显式指定输出目录，例如：

```powershell
& '你的Blender路径\\blender.exe' --background --python 'scripts\\build_naiwa.py' -- --output '重新生成输出目录'
```

默认入口始终读取随包携带的固定修改前工程，H/D均来自同一份 baseline_dimensions.json；不会读取修改后工程叠加0.4D、90%缩放或其他形变。也可运行 scripts/revise_eye_muzzle_body.py 使用相同输出参数。原始几何构造仅作为历史实验入口保留在 --from-primitives 模式。

独立复检：运行 scripts/validate_eye_muzzle_delivery.py，使用 --target 指向本目录工程；FBX检查使用 scripts/validate_fbx.py -- --fbx 指向本目录FBX。工程尚未在UE内完成导入联调。

参考已核对素材库PDF第3页图1.3、第8页图6.2、第9页图6.5；本次消息未显示可读取的上传图1～5，未将它们列为已经完成的视觉核验。
'''
(OUT/'README_交付说明.md').write_text(readme,encoding='utf8')
files=sorted(p for p in OUT.rglob('*') if p.is_file() and '__pycache__' not in p.parts and
             p.name!='SHA256SUMS.json' and p.suffix!='.blend1')
hashes={str(p.relative_to(OUT)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
(OUT/'SHA256SUMS.json').write_text(json.dumps(hashes,indent=2,ensure_ascii=False),encoding='utf8')
archive=ROOT/'Naiwa_Character_Delivery_20261005.zip'
temporary=archive.with_suffix('.current.zip')
with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as zipout:
    for file in files+[OUT/'SHA256SUMS.json']:zipout.write(file,arcname=str(file.relative_to(OUT)))
with zipfile.ZipFile(temporary) as zipin:assert zipin.testzip() is None
temporary.replace(archive)
print('DELIVERY_PACKAGE_OK',len(files)+1,'files',archive.stat().st_size,'bytes')
