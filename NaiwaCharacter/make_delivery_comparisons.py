"""Assemble unchanged Blender renders into labeled comparison sheets."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import argparse

parser=argparse.ArgumentParser()
parser.add_argument('--directory',type=Path,default=Path(__file__).resolve().parent/'versions/20261005_eye_muzzle_body_delivery')
out=parser.parse_args().directory.resolve()
fontfile=Path('C:/Windows/Fonts/msyh.ttc')
font=ImageFont.truetype(str(fontfile),34)
small=ImageFont.truetype(str(fontfile),26)
for group,title in [('full','全身对比'),('head','头部对比')]:
    canvas=Image.new('RGB',(2840,2200),(27,30,34))
    draw=ImageDraw.Draw(canvas)
    draw.text((35,15),title+' · 相同正交相机、灯光与曝光',font=font,fill=(242,243,245))
    for j,(view,label) in enumerate([('front','正面 0°'),('side','侧面 90°'),('45','斜侧 45°')]):
        draw.text((120+j*900,70),label,font=small,fill=(227,231,236))
    for row,(prefix,label) in enumerate([('before','修改前'),('after','修改后')]):
        y=125+row*1030
        draw.text((15,y+430),label,font=small,fill=(247,209,98))
        for j,view in enumerate(['front','side','45']):
            source=out/(prefix+'_'+group+'_'+view+'.png')
            with Image.open(source) as image:
                assert image.size==(900,1000),(source,image.size)
                canvas.paste(image.convert('RGB'),(125+j*900,y))
    canvas.save(out/('comparison_'+group+'.png'))
    print('COMPARISON_OK',group)
