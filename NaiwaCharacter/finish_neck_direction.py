from pathlib import Path
import zipfile,json,sys,ast
out=Path(__file__).resolve().parent
def prepare():
    readme=out/'README_使用说明.md'
    text=readme.read_text(encoding='utf-8-sig')
    lines=[]
    for line in text.splitlines():
        if line.startswith('- 总高度约'):
            line='- 总高度约 2.645 Blender 单位，头顶高度、头部大小及上一轮新增颈部长度保持。本轮头部和上颈前移 0.055（负 Y），所有身体顶点 Z 坐标逐项完全相同；位移向下逐渐减弱，在上胸 Z≤1.98 归零。头部及双眼、嘴和双高光仅作统一平移，朝向、尺寸和相对位置保留。'
        elif line.startswith('- 双眼中心高度'):
            line='- 双眼中心高度保持约 2.465；眼球、口缝与高光全部同步前移 0.055。绿色、黑色区域比例，眼球半径、高光形状与面部朝向保留。'
        elif line.startswith('- 原 Z=2.18'):
            line='- 下脸以下改为较窄、近直立的柔和上颈，再向下展宽接胸。Z=2.22 的半宽由约 0.3185 收至约 0.2906，约收减 9%；Z=2.22–2.30 的宽度变化约减半，减弱嘴下立即三角展开的趋势。前侧由下脸宽缓收喉，再向前展开胸部；后侧通过一条覆盖全颈段的连续斜弧整理形体。'
        elif line.startswith('- Z≤1.94'):
            line='- Z≤1.98 的身体以及侧外肩臂保护区坐标完全相同。头部按下脸、侧颊、后脑根部的连续边界冻结，其全部顶点仅加相同 Y 位移；头顶、五官和整段颈部的高度不变。身体 NaiwaColor、双眼 EyeColor、拓扑、十根骨骼及全部骨骼权重保留。'
        elif line.startswith('- ') and 'build_naiwa.py' in line:
            line='- build_naiwa.py：完整再生成脚本；依赖 connection_sculpt.py、head_volume_sculpt.py、neck_extension_sculpt.py、neck_blend_sculpt.py、neck_direction_sculpt.py 同目录。重建生成工程、FBX 和八张主体预览，再运行 render_details.py 可补齐八张彩色近景。'
        elif line.startswith('- validate_head_revision.py'):
            line='- validate_head_revision.py：当前验证入口，转交 validate_neck_direction.py 核验。'
        elif line.startswith('- revise_neck_blend.py'):
            line='- revise_neck_blend.py：上一轮接缝平滑驱动，会生成头部前移之前的版本。当前请用 revise_neck_direction.py。'
        elif line.startswith('- validate_neck_blend.py'):
            line='- validate_neck_blend.py：上一轮宽范围平滑的历史核验脚本。当前请用 validate_neck_direction.py。'
        elif line.startswith('- neck_blend_validation.json'):
            line='- neck_blend_validation.json：上一轮接缝平滑的历史核验记录。当前请查看 neck_direction_validation.json。'
        elif line.startswith('- finish_neck_blend.py'):
            line='- finish_neck_blend.py：上一轮交付打包脚本。当前请用 finish_neck_direction.py。'
        elif line.startswith('- connection_validation.json'):
            line='- connection_validation.json：早期连接的历史记录；当前请查看 neck_direction_validation.json。'
        elif line.startswith('- '):
            line=line.replace('当前请使用 revise_neck_blend.py','当前请使用 revise_neck_direction.py')
        lines.append(line)
    text='\n'.join(lines).rstrip()+'\n'
    title='## 头部前移与颈部走向记录（2026-10-04）'
    if title not in text:
        text+='\n'+title+'\n\n本轮前版完整保存在 iterations/before_forward_neck_direction_revision。头部及上颈适度向前移动，在上胸归零；不再抬高头顶，也不缩放头部。仅重新整理颈部的截面、前后位置和走向。\n\n前侧用宽缓曲线形成下脸—喉部—上胸的关系；上颈用共享曲率的连续曲线减弱立即向两侧展开的趋势。后颈采用全长度连续斜弧，并将调整扩到整个后侧弧面。表面整理仅作用于 X/Y，Z 轴完全锁定；避免改变颈段长度。\n\n- neck_direction_sculpt.py：本轮头部前移和颈部形状、方向调整模块。\n- revise_neck_direction.py：固定读取本轮前版，生成当前工程、FBX 和十七张预览，避免位移累积。\n- validate_neck_direction.py：独立核验头部刚体平移、固定高度、五官世界坐标、上颈截面、肩臂、拓扑、颜色、骨架、权重和法线。\n- neck_direction_validation.json：本轮核验结果。\n- finish_neck_direction.py：打包当前工程、预览、脚本、记录和各轮必要前版。\n\n完整脚本重建检查保存在 iterations/forward_neck_direction_rebuild_check。原先“窄颈中段原坐标固定”属于上一轮记录；本轮保持其长度，并按照最新要求调整它的截面和前后位置。\n'
    readme.write_text(text,encoding='utf-8')
    for name in ('build_naiwa.py','neck_direction_sculpt.py','revise_neck_direction.py','validate_neck_direction.py','validate_head_revision.py','finish_neck_direction.py'):
        ast.parse((out/name).read_text(encoding='utf-8-sig'),filename=name)
    print('FORWARD_NECK_DOCUMENTATION_READY')
def package():
    report=json.loads((out/'neck_direction_validation.json').read_text(encoding='utf-8-sig'))
    assert report['independent_validation']=='passed'
    assert report['all_vertex_heights_exactly_preserved']
    assert 'FORWARD_NECK_DELIVERABLES_OK' in (out/'neck_direction_build.log').read_text(encoding='utf-8-sig')
    assert 'FORWARD_NECK_INDEPENDENT_VALIDATION_OK' in (out/'neck_direction_independent_validation.log').read_text(encoding='utf-8-sig')
    assert 'FBX_REIMPORT_OK' in (out/'neck_direction_fbx_validation.log').read_text(encoding='utf-8-sig')
    assert 'NAIWA_BUILD_OK' in (out/'iterations/forward_neck_direction_rebuild_check/build.log').read_text(encoding='utf-8-sig')
    files=[
      'Naiwa_Character.blend','Naiwa_Character.fbx','README_使用说明.md',
      'build_naiwa.py','connection_sculpt.py','head_volume_sculpt.py','neck_extension_sculpt.py','neck_blend_sculpt.py','neck_direction_sculpt.py',
      'revise_connection.py','revise_head_volume.py','revise_neck_extension.py','revise_neck_blend.py','revise_neck_direction.py','render_details.py',
      'validate_fbx.py','validate_head_revision.py','validate_neck_blend.py','validate_neck_direction.py',
      'connection_validation.json','head_volume_validation.json','neck_extension_validation.json','neck_blend_validation.json','neck_direction_validation.json',
      'finish_neck_direction.py','neck_direction_independent_validation.log','neck_direction_fbx_validation.log'
    ]+sorted(p.name for p in out.glob('naiwa_*.png'))
    assert len([name for name in files if name.endswith('.png')])==17
    archive_path=out/'Naiwa_Character_Preview.zip'
    with zipfile.ZipFile(archive_path,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
        for name in files:
            archive.write(out/name,arcname=name)
        for revision in ('before_continuous_head_torso_revision','before_smaller_head_depth_revision','before_raised_head_neck_revision','before_broad_neck_blending_revision','before_forward_neck_direction_revision'):
            for name in ('Naiwa_Character.blend','README_使用说明.md'):
                path=out/'iterations'/revision/name
                archive.write(path,arcname=path.relative_to(out).as_posix())
    with zipfile.ZipFile(archive_path) as archive:
        assert archive.testzip() is None
    print('FORWARD_NECK_PACKAGE_OK',len(files),'files',archive_path.stat().st_size,'bytes')
if '--prepare' in sys.argv:
    prepare()
else:
    package()
