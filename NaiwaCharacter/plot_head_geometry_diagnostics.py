"""Plot exact saved triangle-plane intersections with equal geometric scale."""
import json
import math
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'diagnostics/20261005_head_geometry'
FONT='C:/Windows/Fonts/msyh.ttc'
def font(size): return ImageFont.truetype(FONT,size)
COLORS={'yellow_body':'#c38d20','green':'#2f843b','black':'#171c20'}
FILL={'yellow_body':'#fff6d6','green':'#dae9d2','black':'#e6e8eb'}

class Panel:
    def __init__(self,image,rect,bounds):
        self.image=image;self.rect=rect
        x0,y0,x1,y1=rect;umin,vmin,umax,vmax=bounds
        self.origin=(int(x0),int(y0))
        self.layer=Image.new('RGBA',(int(x1-x0),int(y1-y0)),(0,0,0,0))
        self.draw=ImageDraw.Draw(self.layer)
        self.scale=min((x1-x0)/(umax-umin),(y1-y0)/(vmax-vmin))
        self.uc=(umin+umax)/2;self.vc=(vmin+vmax)/2
        self.xc=(x0+x1)/2;self.yc=(y0+y1)/2
    def p(self,u,v): return (self.xc+(u-self.uc)*self.scale,self.yc-(v-self.vc)*self.scale)
    def lp(self,u,v):
        x,y=self.p(u,v);return x-self.origin[0],y-self.origin[1]
    def commit(self):self.image.paste(self.layer,self.origin,self.layer)
    def line(self,points,color,width=4):
        self.draw.line([self.lp(*p) for p in points],fill=color,width=width)
    def point(self,u,v,color='#506ba1',label=None):
        x,y=self.lp(u,v);self.draw.ellipse((x-7,y-7,x+7,y+7),fill=color)
        if label:self.draw.text((x+13,y+7),label,font=font(24),fill=color)
    def arrow(self,a,b,color='#4c5f77',width=3):
        self.line([a,b],color,width)
        pa,pb=self.lp(*a),self.lp(*b)
        for tip,other in ((pa,pb),(pb,pa)):
            dx,dy=np.asarray(other)-np.asarray(tip);length=math.hypot(dx,dy)
            if length<1:continue
            dx,dy=dx/length,dy/length
            tri=[tip,(tip[0]+14*dx+6*dy,tip[1]+14*dy-6*dx),
                     (tip[0]+14*dx-6*dy,tip[1]+14*dy+6*dx)]
            self.draw.polygon(tri,fill=color)
    def grid(self,step,bounds):
        u0,v0,u1,v1=bounds
        for u in np.arange(math.ceil(u0/step)*step,u1,step):
            self.line([(u,v0),(u,v1)],'#eef0f3',1)
        for v in np.arange(math.ceil(v0/step)*step,v1,step):
            self.line([(u0,v),(u1,v)],'#eef0f3',1)
        self.draw.rectangle((0,0,self.layer.width-1,self.layer.height-1),outline='#bbc4cd',width=2)

def axis_number(axis):
    return int(axis) if isinstance(axis,int) else {'X':0,'Y':1,'Z':2}[str(axis).upper()]
def segments(section,axes,regions=None):
    for obj in section['objects']:
        for seg in obj.get('segments',[]):
            region=seg.get('region','yellow_body')
            if regions and region not in regions:continue
            p=np.asarray(seg['points'],dtype=float)
            yield p[:,axes],region
def plot_section(panel,section,axes,regions=None):
    grouped=list(segments(section,axes,regions))
    # Plot actual slice lines, never a fitted or artist-edited outline.
    for region in ('yellow_body','green','black'):
        for points,current in grouped:
            if current==region: panel.line(points,COLORS[current],4 if region=='yellow_body' else 6)
def bounds_of(section,axes,regions=None):
    data=np.concatenate([p for p,r in segments(section,axes,regions)])
    return data.min(axis=0),data.max(axis=0)
def canvas(w,h,title,subtitle):
    im=Image.new('RGB',(w,h),'white');d=ImageDraw.Draw(im)
    d.text((55,32),title,font=font(42),fill='#172332')
    d.text((55,97),subtitle,font=font(25),fill='#536375')
    return im,d
def legend(draw,x,y):
    for label,color in [('黄色基础脸面',COLORS['yellow_body']),('绿色眼体',COLORS['green']),('黑瞳区域',COLORS['black'])]:
        draw.line((x,y+15,x+50,y+15),fill=color,width=6)
        draw.text((x+65,y),label,font=font(25),fill='#334355');x+=290

def main():
    report=json.loads((OUT/'head_geometry_measurements.json').read_text(encoding='utf-8'))
    raw=json.loads((OUT/'head_geometry_sections.json').read_text(encoding='utf-8'))
    sections={s['id']:s for s in raw['sections']}
    D=report['D'] if isinstance(report['D'],(int,float)) else report['D']['value']
    eye_z=sections['eye_level']['value'];mouth_z=sections['mouth_level']['value']
    im,d=canvas(2200,1400,'单眼中心侧面剖面：眼体、黑瞳与基础脸面',
                '精确三角面相交线；剖面 X = 各自眼中心 X；坐标单位为 Blender 工程单位，前方为 -Y。')
    legend(d,60,145)
    for index,side in enumerate(('L','R')):
        s=sections['eye_'+side+'_xcenter'];x=70+1100*index
        eye_points=np.concatenate([p for p,r in segments(s,[1,2],{'green','black'})])
        center=(eye_points.min(axis=0)+eye_points.max(axis=0))/2
        # The exposed cap and its buried closure remain fully visible.
        lo=eye_points.min(axis=0);hi=eye_points.max(axis=0)
        bound=[lo[0]-.18*D,eye_z-.88*D,hi[0]+.24*D,eye_z+.88*D]
        panel=Panel(im,(x,280,x+970,975),bound);panel.grid(.25*D,bound)
        plot_section(panel,s,[1,2])
        panel.line([(bound[0],eye_z),(bound[2],eye_z)],'#d28599',2)
        panel.commit()
        d.text((x,217),f'{side} 眼：X = {s["value"]:.7f}',font=font(30),fill='#263749')
        d.text((x,998),'← 前方 -Y                  后方 +Y →',font=font(25),fill='#536375')
        metric=report['eyes'][side]
        d.text((x,1050),f'可见外径={metric["visible_outer_equivalent_diameter"]:.7f}；绿区最前点露出={metric["foremost_visible_green"]["projection_F"]:.7f}',font=font(24),fill='#334355')
        d.text((x,1095),f'绿区最大局部露出={metric["maximum_green_face_gap"]["projection_F"]:.7f}；整眼最大露出={metric["maximum_whole_eye_face_gap"]["projection_F"]:.7f}',font=font(24),fill='#334355')
        thick=metric['local_normal_thickness']
        d.text((x,1140),f'代表法线厚度={thick["thickness"]:.7f} ({thick["thickness_over_D"]:.3f}D)；黑瞳独立台阶=0',font=font(24),fill='#334355')
    d.text((60,1235),'绿色与黑瞳为同一连续眼网格的材质分区；绿色后半封背位于黄色皮肤内部。',font=font(27),fill='#334355')
    d.text((60,1285),'上列数值测量完整三维眼网格；最前测点及局部法线可能不在此剖面内。粉线表示眼心高度。',font=font(25),fill='#536375')
    d.text((60,1330),'黄色曲线是当前基础脸面，排除眼网格。法线厚度为前后交点弦长，包含埋入部分。',font=font(25),fill='#536375')
    im.save(OUT/'eye_side_sections.png')

    im,d=canvas(1600,1350,'头部正中侧剖面与截面高度',
                '面部中线 X = 0；主体精确相交线；同一比例显示 Y、Z 坐标。')
    s=sections['midline_x0']
    head_points=np.concatenate([p for p,r in segments(s,[1,2],{'yellow_body'})])
    head_points=head_points[head_points[:,1]>=2.14]
    low,high=head_points.min(0),head_points.max(0)
    bound=[low[0]-.065,2.14,high[0]+.065,max(high[1],eye_z)+.025]
    panel=Panel(im,(105,220,1160,1135),bound);panel.grid(.5*D,bound)
    plot_section(panel,s,[1,2],{'yellow_body'})
    for label,sid in [('眼中心','eye_level'),('嘴线','mouth_level'),('嘴下 0.5D','mouth_minus_half_D'),('嘴下 1D','mouth_minus_D')]:
        z=sections[sid]['value'];panel.line([(bound[0],z),(bound[2],z)],'#7290ab',2)
        _,py=panel.p(bound[2],z)
        d.text((1190,py-19),f'{label}\nZ={z:.7f}',font=font(23),fill='#425f7a',spacing=6)
    panel.commit()
    d.text((110,1178),'← 前方 -Y             后方 +Y →',font=font(27),fill='#536375')
    d.text((65,1245),'仅截取头部与上颈显示范围；模型本身没有裁切、压短或平移。',font=font(25),fill='#536375')
    im.save(OUT/'head_midline_section.png')

    im,d=canvas(2240,2160,'黄色头部 / 上颈：四个水平截面',
                '宽度沿 X、深度沿 Y；中心使用黄色截面的包围盒中心；眼体不计入尺寸。')
    ids=['eye_level','mouth_level','mouth_minus_half_D','mouth_minus_D']
    titles=['眼中心高度','嘴线高度','嘴下 0.5D','嘴下 1D']
    all_points=np.concatenate([p for sid in ids for p,r in segments(sections[sid],[0,1],{'yellow_body'})])
    g0=all_points.min(axis=0);g1=all_points.max(axis=0);margin=.13*D
    bound=[g0[0]-margin,g0[1]-margin,g1[0]+margin,g1[1]+margin]
    table=[]
    for i,(sid,title) in enumerate(zip(ids,titles)):
        x=70+(i%2)*1120;y=205+(i//2)*925;s=sections[sid]
        low,high=bounds_of(s,[0,1],{'yellow_body'});width,depth=high-low;c=(low+high)/2
        panel=Panel(im,(x+85,y+145,x+960,y+765),bound);panel.grid(.5*D,bound)
        plot_section(panel,s,[0,1],{'yellow_body'})
        panel.line([(0,bound[1]),(0,bound[3])],'#d8dfe7',2)
        panel.point(*c,label='C')
        panel.arrow((low[0],c[1]),(high[0],c[1]))
        panel.arrow((c[0],low[1]),(c[0],high[1]))
        panel.commit()
        d.text((x,y),f'{title}  Z = {s["value"]:.7f}',font=font(32),fill='#263749')
        d.text((x,y+55),f'宽 W={width:.7f} ({width/D:.3f}D)   深 T={depth:.7f} ({depth/D:.3f}D)',font=font(25),fill='#334355')
        d.text((x,y+96),f'C(X,Y,Z)=({c[0]:.7f}, {c[1]:.7f}, {s["value"]:.7f})',font=font(24),fill='#536375')
        d.text((x+120,y+790),'← -X      +X →         图下方为前方 -Y',font=font(23),fill='#536375')
        table.append({'section':sid,'Z':s['value'],'width_X':float(width),'depth_Y':float(depth),
                      'bbox_center_XYZ':[float(c[0]),float(c[1]),s['value']]})
    d.text((60,2090),'各面板使用同一比例；蓝色点 C 与双箭头分别标示中心及宽度、深度。',font=font(25),fill='#536375')
    im.save(OUT/'head_horizontal_sections.png')
    (OUT/'plotted_section_dimensions.json').write_text(json.dumps(table,indent=2),encoding='utf-8')

    im,d=canvas(2500,1460,'眼体曲率灰模近景',
                '模型几何保持；临时统一灰材质展示曲率，黑瞳的颜色边界不代表独立几何凸起。')
    for index,(name,title) in enumerate([('front','正面：偏航 0° / 仰角 0°'),('oblique','斜侧：偏航 50° / 仰角 8°')]):
        x=45+index*1250
        pic=Image.open(OUT/('eye_gray_'+name+'_close.png')).convert('RGB')
        pic.thumbnail((1190,1190),Image.Resampling.LANCZOS)
        im.paste(pic,(x,225));d.text((x,165),title,font=font(29),fill='#263749')
    d.text((55,1420),'小凸点为独立高光网格。',font=font(25),fill='#536375')
    im.save(OUT/'eye_gray_curvature.png')
    print('HEAD_GEOMETRY_DIAGRAMS_OK',D)

if __name__=='__main__':main()
