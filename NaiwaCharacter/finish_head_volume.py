from pathlib import Path
import sys
import zipfile

out = Path(__file__).resolve().parent
validation = out / 'validate_head_revision.py'
text = validation.read_text(encoding='utf-8-sig')
text = text.replace("from connection_sculpt import section\n\nout = Path(__file__).resolve().parent",
                    "import sys\nout = Path(__file__).resolve().parent\nsys.path.insert(0, str(out))\nfrom connection_sculpt import section")
text = text.replace('2.04+i*0.006) for i in range(58)', '2.00+i*0.006) for i in range(78)')
validation.write_text(text, encoding='utf-8')

readme_path = out / 'README_使用说明.md'
lines = readme_path.read_text(encoding='utf-8-sig').splitlines()
if '## 头部收减记录' not in '\n'.join(lines):
    new_lines = []
    for line in lines:
        if line.startswith('- 总高度约'):
            line = '- 总高度约 2.505 Blender 单位。本轮整体收减头部体量，兼顾横向宽度与前后厚度：眼高对应截面的宽度约减 13%，前后厚度约减 18%。两颊收窄，眼后及后脑下部鼓起减少，后半圆顶更早转入连续斜背。收减量向头身连接处逐渐归零；Z≤2.00 的上胸以下身体逐顶点与前版核验，坐标完全相同。'
        elif line.startswith('- 眼中心'):
            line = '- 双眼中心高度随头部由约 2.361 降至约 2.325，左右位置随两颊收减重新贴合。每只眼睛继续使用一个完整球体和 EyeColor 顶点颜色；球体半径、绿色区域及黑瞳孔比例保留，瞳孔仍按正面投影对齐外露绿色区域。眼球和小高光作为刚体旋转、移动；不把球体压扁，眼周不增加底座。'
        elif line.startswith('- 下半头'):
            line = '- 前下脸保留短小前伸轮廓，嘴下保留宽缓、浅的内收，口缝重新贴合。前方细节向两侧消失；后脑用位置和斜率连续的曲线提早接入斜背。下脸到上胸连续展宽，避免独立球头、细脖子及环形领圈。'
        elif line.startswith('- ') and 'build_naiwa.py' in line:
            line = '- build_naiwa.py：完整再生成脚本；需与 connection_sculpt.py、head_volume_sculpt.py 保持同目录。'
        elif line.startswith('- ') and 'connection_sculpt.py' in line:
            line = '- connection_sculpt.py：上一轮头胸连接曲面模块，为当前基础造型的依赖。\n- head_volume_sculpt.py：本轮头部宽度、前后厚度及后脑轮廓收减模块。\n- revise_head_volume.py：固定读取本轮前版备份，重新生成当前模型、FBX 和十四张预览。\n- head_volume_validation.json：本轮坐标与截面对照数据。'
        elif line.startswith('- ') and 'validate_head_revision.py' in line:
            line = '- validate_head_revision.py：检查范围外身体、眼球球形、骨架、权重、顶点颜色，以及侧向和后侧轮廓的连续展宽。'
        elif line.startswith('- ') and 'revise_connection.py' in line:
            line = '- revise_connection.py：上一轮连接修改驱动；它生成上一轮版本。本轮请使用 revise_head_volume.py。'
        elif line.startswith('- ') and 'connection_validation.json' in line:
            line = '- connection_validation.json：上一轮头胸连接的核验数据；本轮请查看 head_volume_validation.json。'
        new_lines.append(line)
    readme = '\n'.join(new_lines).rstrip()+'\n'
    if '## 头部收减记录' not in readme:
        readme += '\n## 头部收减记录（2026-10-03）\n\n前版位于 iterations/before_smaller_head_depth_revision。本轮收窄两颊、减薄后脑并缩短厚重脸颈过渡；更新 Blender 工程、FBX、全部预览及可重复生成脚本。revise_head_volume.py 每次固定从前版工程开始，避免对已收减的模型再次累积收减。\n'
    readme_path.write_text(readme, encoding='utf-8')

if '--prepare' in sys.argv:
    print('HEAD_VOLUME_DOCUMENTATION_READY')
else:
    deliveries = [
        'Naiwa_Character.blend', 'Naiwa_Character.fbx', 'README_使用说明.md',
        'build_naiwa.py', 'connection_sculpt.py', 'head_volume_sculpt.py',
        'revise_connection.py', 'revise_head_volume.py', 'render_details.py',
        'validate_fbx.py', 'validate_head_revision.py',
        'connection_validation.json', 'head_volume_validation.json'
    ] + sorted(p.name for p in out.glob('naiwa_*.png'))
    assert len([p for p in deliveries if p.endswith('.png')]) == 14
    package = out / 'Naiwa_Character_Preview.zip'
    with zipfile.ZipFile(package, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for name in deliveries:
            archive.write(out/name, arcname=name)
        for revision in ('before_continuous_head_torso_revision', 'before_smaller_head_depth_revision'):
            for name in ('Naiwa_Character.blend', 'README_使用说明.md'):
                path = out / 'iterations' / revision / name
                archive.write(path, arcname=str(path.relative_to(out)).replace('\\', '/'))
    with zipfile.ZipFile(package) as archive:
        assert archive.testzip() is None
    print('HEAD_VOLUME_PACKAGE_OK', len(deliveries), 'files', package.stat().st_size, 'bytes')

