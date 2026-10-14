from pathlib import Path
import zipfile,sys,json
out=Path(__file__).resolve().parent
def prepare():
    validator=out/'validate_head_revision.py'
    text=validator.read_text(encoding='utf-8-sig')
    text=text.replace("assert old.shape==new.shape","assert old.shape==new.shape\nassert body.matrix_world==old_body.matrix_world")
    text=text.replace("    old_mod=[", "    assert all(m.object is old_rig for m in before.modifiers if m.type=='ARMATURE')\n    assert all(m.object is rig for m in after.modifiers if m.type=='ARMATURE')\n    old_mod=[")
    validator.write_text(text,encoding='utf-8')
    details=out/'render_details.py'
    text=details.read_text(encoding='utf-8-sig')
    text=text.replace("]:\n    if name.startswith", "    ('naiwa_neck_front_close.png', 0.82, (0,-7,2.25), (0,0,2.25)),\n    ('naiwa_neck_side_close.png', 0.82, (6,-0.13,2.25), (0,-0.13,2.25)),\n]:\n    if name.startswith")
    details.write_text(text,encoding='utf-8')
    readme_path=out/'README_使用说明.md'
    text=readme_path.read_text(encoding='utf-8-sig')
    lines=text.splitlines()
    result=[]
    for line in lines:
        if line.startswith('- 总高度约'):
            line='- 总高度约 2.645 Blender 单位。本轮保持上一版头部的形状、体积和五官相对位置，头部及双眼、嘴、双高光整体上移 0.14。原 Z≥2.185 的 30,707 个头部顶点仅作竖直平移；X/Y 坐标完全保留。'
        elif line.startswith('- 双眼中心高度'):
            line='- 双眼中心高度由约 2.325 上移至约 2.465。双眼完整球体、瞳孔、绿色区域及高光形状保留；眼睛、嘴与头部同步平移，各五官间的相对位置不变。'
        elif line.startswith('- 前下脸保留'):
            line='- 仅延长原 Z=2.08–2.185 的头身连接带：延长后覆盖 Z=2.08–2.325，增加 0.14。前颈采用宽缓后倾，建立下颌轮廓；左右略作收拢后向下展宽，后侧沿原斜弧接背。新增形体保留原网格连续曲面，连接端的位置、斜率和曲率连续，不添加局部环形凹沟。'
        elif line.startswith('- 肚皮为'):
            line='- 肩部、躯干和四肢保持上一版形状：原 Z≤2.08 的 329,521 个身体顶点坐标完全相同。身体 NaiwaColor、双眼 EyeColor、骨骼和全部骨骼权重逐项核验保留。肚皮仍为暖米黄色，手指、前掌仍为棕色。'
        elif line.startswith('- ') and 'build_naiwa.py' in line:
            line='- build_naiwa.py：完整再生成脚本；需与 connection_sculpt.py、head_volume_sculpt.py、neck_extension_sculpt.py 保持同目录。完整重建生成工程、FBX 和八张主体预览；再运行 render_details.py 可补齐八张细节近景。'
        elif line.startswith('- head_volume_sculpt.py'):
            line='- head_volume_sculpt.py：前一轮的头部体量收减模块，作为当前形状的基础。'
        elif line.startswith('- revise_head_volume.py'):
            line='- revise_head_volume.py：前一轮的体量修改驱动，会生成前一轮版本。本轮请使用 revise_neck_extension.py。'
        elif line.startswith('- head_volume_validation.json'):
            line='- head_volume_validation.json：前一轮体量收减的历史核验数据。\n- neck_extension_sculpt.py：本轮保持头部刚体、延长颈带的模块。\n- revise_neck_extension.py：固定读取本轮前版备份，生成当前 Blender 工程、FBX 和十六张预览，避免重复抬升。\n- neck_extension_validation.json：本轮头部平移、五官、身体、颜色、骨骼和权重的核验结果。\n- naiwa_neck_front_close.png、naiwa_neck_side_close.png：新增的颈部正面和侧面近景。'
        elif line.startswith('- revise_connection.py'):
            line='- revise_connection.py：历史连接修改驱动，会生成较早版本。本轮请使用 revise_neck_extension.py。'
        elif line.startswith('- validate_head_revision.py'):
            line='- validate_head_revision.py：独立对照本轮前版，检查头部和五官刚体平移、肩身四肢固定、拓扑、颜色、骨架、权重，以及侧后颈轮廓。'
        elif line.startswith('- connection_validation.json'):
            line='- connection_validation.json：早期头胸连接的历史记录；当前请查看 neck_extension_validation.json。'
        result.append(line)
    text='\n'.join(result).rstrip()+'\n'
    text+='\n## 头部上移与颈段延长记录（2026-10-03）\n\n本轮前版完整备份位于 iterations/before_raised_head_neck_revision。头部和全部五官上移 0.14；只对原头身连接带分配新增长度，并作宽缓前颈与轻微侧向截面调整。保护区身体坐标误差为零，头部与五官的平移误差小于 0.000001 Blender 单位。已保留十根骨骼、四段动画、颜色和权重。\n\n重新执行本轮修改：在 Blender 后台运行 revise_neck_extension.py；它始终读取本轮前版备份。validate_head_revision.py 对比保存工程；validate_fbx.py 重新导入 FBX。完整脚本重建检查另存于 iterations/raised_head_neck_rebuild_check，不覆盖主工程。\n'
    readme_path.write_text(text,encoding='utf-8')
if '--prepare' in sys.argv:
    prepare()
    print('NECK_DOCUMENTATION_READY')
else:
    report=json.loads((out/'neck_extension_validation.json').read_text(encoding='utf-8-sig'))
    assert report['independent_validation']=='passed'
    assert 'NECK_INDEPENDENT_VALIDATION_OK' in (out/'neck_independent_validation.log').read_text(encoding='utf-8-sig')
    assert 'FBX_REIMPORT_OK' in (out/'neck_fbx_validation.log').read_text(encoding='utf-8-sig')
    assert 'NAIWA_BUILD_OK' in (out/'iterations/raised_head_neck_rebuild_check/build.log').read_text(encoding='utf-8-sig')
    deliveries=[
      'Naiwa_Character.blend','Naiwa_Character.fbx','README_使用说明.md',
      'build_naiwa.py','connection_sculpt.py','head_volume_sculpt.py','neck_extension_sculpt.py',
      'revise_connection.py','revise_head_volume.py','revise_neck_extension.py','render_details.py',
      'validate_fbx.py','validate_head_revision.py',
      'connection_validation.json','head_volume_validation.json','neck_extension_validation.json',
      'finish_neck_extension.py','neck_independent_validation.log','neck_fbx_validation.log'
    ]+sorted(p.name for p in out.glob('naiwa_*.png'))
    assert len([name for name in deliveries if name.endswith('.png')])==16
    package=out/'Naiwa_Character_Preview.zip'
    with zipfile.ZipFile(package,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
        for name in deliveries:
            archive.write(out/name,arcname=name)
        for revision in ('before_continuous_head_torso_revision','before_smaller_head_depth_revision','before_raised_head_neck_revision'):
            for name in ('Naiwa_Character.blend','README_使用说明.md'):
                path=out/'iterations'/revision/name
                archive.write(path,arcname=path.relative_to(out).as_posix())
    with zipfile.ZipFile(package) as archive:
        assert archive.testzip() is None
    print('NECK_PACKAGE_OK',len(deliveries),'files',package.stat().st_size,'bytes')
