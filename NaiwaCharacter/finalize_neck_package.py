from pathlib import Path
import ast
out=Path(__file__).resolve().parent
p=out/'finish_neck_extension.py'
text=p.read_text(encoding='utf-8-sig')
start=text.index("validator=out/")
end=text.index("if '--prepare' in sys.argv:")
preparation=text[start:end]
text=text[:start]+"def prepare():\n"+"".join("    "+line+"\n" for line in preparation.splitlines())+text[end:]
text=text.replace("    print('NECK_DOCUMENTATION_READY')","    prepare()\n    print('NECK_DOCUMENTATION_READY')")
text=text.replace("      'connection_validation.json','head_volume_validation.json','neck_extension_validation.json'",
                  "      'connection_validation.json','head_volume_validation.json','neck_extension_validation.json',\n      'finish_neck_extension.py','neck_independent_validation.log','neck_fbx_validation.log'")
text=text.replace("    assert report['independent_validation']=='passed'",
                  "    assert report['independent_validation']=='passed'\n    assert 'NECK_INDEPENDENT_VALIDATION_OK' in (out/'neck_independent_validation.log').read_text(encoding='utf-8-sig')\n    assert 'FBX_REIMPORT_OK' in (out/'neck_fbx_validation.log').read_text(encoding='utf-8-sig')\n    assert 'NAIWA_BUILD_OK' in (out/'iterations/raised_head_neck_rebuild_check/build.log').read_text(encoding='utf-8-sig')")
p.write_text(text,encoding='utf-8')
readme=out/'README_使用说明.md'
text=readme.read_text(encoding='utf-8-sig').replace('本轮依照用户给出的站立姿态、身体比例、面部及材质规格重做基础体型','本工程依照用户给出的站立姿态、身体比例、面部及材质规格建立基础体型')
readme.write_text(text,encoding='utf-8')
for name in ('neck_extension_sculpt.py','revise_neck_extension.py','render_details.py','validate_head_revision.py','finish_neck_extension.py','build_naiwa.py'):
    ast.parse((out/name).read_text(encoding='utf-8-sig'),filename=name)
print('NECK_SCRIPTS_SYNTAX_OK')
