from pathlib import Path
import zipfile,json,sys,ast
out=Path(__file__).resolve().parent
def prepare():
    module=out/'neck_blend_sculpt.py'
    text=module.read_text(encoding='utf-8-sig')
    needle="    body['neck_seams_blended']=True"
    if "body['preserved_body_below_z']=BLEND_MIN_Z" not in text:
        text=text.replace(needle,needle+"\n    body['preserved_body_below_z']=BLEND_MIN_Z\n    body['preserved_crown_above_z']=REAR_TOP_Z")
        module.write_text(text,encoding='utf-8')
    check=out/'iterations/broad_neck_blend_rebuild_check/neck_blend_sculpt.py'
    check.write_text(text,encoding='utf-8')
    readme=out/'README_使用说明.md'
    text=readme.read_text(encoding='utf-8-sig')
    result=[]
    for line in text.splitlines():
        if line.startswith('- 总高度约'):
            line='- 总高度约 2.645 Blender 单位，保留上一轮上移后的头部位置和五官关系。本轮对头根、颈下的接缝两侧作宽范围几何融合：上端进入下脸及后脑下缘，下端进入前胸和后背。头顶与全部五官保持上一版坐标。'
        elif line.startswith('- 仅延长原'):
            line='- 原 Z=2.18–2.255 的窄颈中段逐顶点固定，保留颈段长度、较窄截面和宽缓喉弧。平滑范围延伸至 Z=1.94–2.44；前侧上界止于嘴下 Z≈2.354，侧后上界进入后脑下缘。侧肩及手臂作空间保护，左右连续展宽接胸，后侧沿背线展开。'
        elif line.startswith('- 肩部、躯干和四肢'):
            line='- Z≤1.94 的身体和 Z≥2.44 的头顶逐顶点保持，侧肩、手臂及全部五官单独核验不动。身体 NaiwaColor、双眼 EyeColor、拓扑、骨骼和所有骨骼权重保留。当前上胸是本轮平滑范围的一部分。'
        elif line.startswith('- ') and 'build_naiwa.py' in line:
            line='- build_naiwa.py：完整再生成脚本；依赖 connection_sculpt.py、head_volume_sculpt.py、neck_extension_sculpt.py、neck_blend_sculpt.py 同目录。重建生成工程、FBX 和八张主体预览，再运行 render_details.py 可补齐八张彩色近景。'
        elif line.startswith('- revise_neck_extension.py'):
            line='- revise_neck_extension.py：上一轮头部上移驱动，会生成上移后尚未宽范围修顺的版本。当前请使用 revise_neck_blend.py。'
        elif line.startswith('- neck_extension_validation.json'):
            line='- neck_extension_validation.json：上一轮头部平移的历史核验数据。'
        elif line.startswith('- validate_head_revision.py'):
            line='- validate_head_revision.py：当前验证入口，转交 validate_neck_blend.py 核验。'
        elif line.startswith('- revise_head_volume.py') or line.startswith('- revise_connection.py'):
            line=line.replace('本轮请使用 revise_neck_extension.py','当前请使用 revise_neck_blend.py')
        elif line.startswith('- connection_validation.json'):
            line='- connection_validation.json：早期连接的历史记录；当前请查看 neck_blend_validation.json。'
        result.append(line)
    text='\n'.join(result).rstrip()+'\n'
    record='## 颈部两端宽范围融合记录（2026-10-03）'
    if record not in text:
        text+='\n'+record+'\n\n本轮前版完整备份位于 iterations/before_broad_neck_blending_revision。已消除头根和颈下的横向直边；通过覆盖接缝两侧的几何平滑改变曲率，不仅磨圆一条窄带。窄颈中段保持原位，前颈保留宽缓喉弧。轮廓修顺后，检查法线方向、单位长度、平滑面、自定义法线及锐边。\n\n- neck_blend_sculpt.py：本轮上下接缝宽范围几何融合模块。\n- revise_neck_blend.py：固定读取本轮前版，生成当前工程、FBX 和十七张预览。\n- validate_neck_blend.py：独立对照保护区、窄颈中段截面、接缝曲率、五官、拓扑、颜色、骨架、权重和法线。\n- neck_blend_validation.json：本轮核验结果。\n- naiwa_neck_gray_three_quarter.png：灰色材质斜侧近景，供检查几何与法线连续性。\n- finish_neck_blend.py：打包当前模型、预览、脚本、核验记录和各轮必要前版。\n\n前一轮记录描述当时的保护范围；本轮已按最新要求将形体调整扩展进入下脸、后脑下缘和上胸。完整重建检查另存 iterations/broad_neck_blend_rebuild_check。\n'
    readme.write_text(text,encoding='utf-8')
    for name in ('build_naiwa.py','neck_blend_sculpt.py','revise_neck_blend.py','validate_neck_blend.py','render_details.py','finish_neck_blend.py'):
        ast.parse((out/name).read_text(encoding='utf-8-sig'),filename=name)
    print('BROAD_NECK_DOCUMENTATION_READY')
def package():
    report=json.loads((out/'neck_blend_validation.json').read_text(encoding='utf-8-sig'))
    assert report['independent_validation']=='passed'
    assert 'BROAD_NECK_DELIVERABLES_OK' in (out/'neck_blend_build.log').read_text(encoding='utf-8-sig')
    assert 'BROAD_NECK_INDEPENDENT_VALIDATION_OK' in (out/'neck_blend_independent_validation.log').read_text(encoding='utf-8-sig')
    assert 'FBX_REIMPORT_OK' in (out/'neck_blend_fbx_validation.log').read_text(encoding='utf-8-sig')
    assert 'NAIWA_BUILD_OK' in (out/'iterations/broad_neck_blend_rebuild_check/build.log').read_text(encoding='utf-8-sig')
    files=[
      'Naiwa_Character.blend','Naiwa_Character.fbx','README_使用说明.md',
      'build_naiwa.py','connection_sculpt.py','head_volume_sculpt.py','neck_extension_sculpt.py','neck_blend_sculpt.py',
      'revise_connection.py','revise_head_volume.py','revise_neck_extension.py','revise_neck_blend.py','render_details.py',
      'validate_fbx.py','validate_head_revision.py','validate_neck_blend.py',
      'connection_validation.json','head_volume_validation.json','neck_extension_validation.json','neck_blend_validation.json',
      'finish_neck_blend.py','neck_blend_independent_validation.log','neck_blend_fbx_validation.log'
    ]+sorted(p.name for p in out.glob('naiwa_*.png'))
    assert len([name for name in files if name.endswith('.png')])==17
    archive_path=out/'Naiwa_Character_Preview.zip'
    with zipfile.ZipFile(archive_path,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
        for name in files:
            archive.write(out/name,arcname=name)
        for revision in ('before_continuous_head_torso_revision','before_smaller_head_depth_revision','before_raised_head_neck_revision','before_broad_neck_blending_revision'):
            for name in ('Naiwa_Character.blend','README_使用说明.md'):
                path=out/'iterations'/revision/name
                archive.write(path,arcname=path.relative_to(out).as_posix())
    with zipfile.ZipFile(archive_path) as archive:
        assert archive.testzip() is None
    print('BROAD_NECK_PACKAGE_OK',len(files),'files',archive_path.stat().st_size,'bytes')
if '--prepare' in sys.argv:
    prepare()
else:
    package()
