from pathlib import Path
out=Path(__file__).resolve().parent
path=out/'revise_neck_extension.py'
text=path.read_text(encoding='utf-8-sig')
text=text.replace('extend_neck(body, shell)','neck_result=extend_neck(body, shell)')
text=text.replace('rigid = [(a, b.co) for a,b in zip(before,body.data.vertices) if a.z >= HEAD_BASE_Z]',
                  'rigid = [(before[i],body.data.vertices[i].co) for i in neck_result[\'rigid_head_indices\']]')
path.write_text(text,encoding='utf-8')
print('INCLINED_ATTACHMENT_DRIVER_READY')

