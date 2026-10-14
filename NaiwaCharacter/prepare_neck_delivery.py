from pathlib import Path
import shutil
out=Path(__file__).resolve().parent
details=out/'render_details.py'
text=details.read_text(encoding='utf-8-sig')
text=text.replace("scene.frame_set(1)","scene.frame_set(1)\nhead_raise=float(bpy.data.objects['Naiwa unified golden body'].get('head_rigid_raise',0.0))")
text=text.replace("    camera.data.ortho_scale = scale","    if name.startswith('naiwa_face'):\n        location=tuple(v+(head_raise if i==2 else 0) for i,v in enumerate(location))\n        target=tuple(v+(head_raise if i==2 else 0) for i,v in enumerate(target))\n    camera.data.ortho_scale = scale")
details.write_text(text,encoding='utf-8')
check=out/'iterations/raised_head_neck_rebuild_check'
check.mkdir(exist_ok=True)
for name in ('build_naiwa.py','connection_sculpt.py','head_volume_sculpt.py','neck_extension_sculpt.py'):
    shutil.copy2(out/name,check/name)
print('NECK_VALIDATION_AND_REBUILD_READY')
