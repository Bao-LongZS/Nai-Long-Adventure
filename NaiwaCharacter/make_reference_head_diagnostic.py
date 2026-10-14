"""Compose existing reference and read-only camera renders with uniform scaling."""
import hashlib
import json
import math
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'diagnostics/20261005_head_geometry'
REF=ROOT/'references/20261005/pdf_page03_image03_IM60.jpg'
FONT='C:/Windows/Fonts/msyh.ttc'
def font(size):return ImageFont.truetype(FONT,size)
def hull(points):
    pts=sorted(set(map(tuple,points)))
    def cross(a,b,c):return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    lower=[];upper=[]
    for p in pts:
        while len(lower)>1 and cross(lower[-2],lower[-1],p)<=0:lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper)>1 and cross(upper[-2],upper[-1],p)<=0:upper.pop()
        upper.append(p)
    return lower[:-1]+upper[:-1]

def reference_eye(im):
    box=(348,97,413,158);a=np.asarray(im.crop(box),dtype=float)
    mask=(a[:,:,1]>1.08*a[:,:,0])&(a[:,:,1]>1.10*a[:,:,2])
    yy,xx=np.nonzero(mask);assert len(xx)>500
    boundary=hull(zip(xx.tolist(),yy.tolist()))
    fill=Image.new('1',(box[2]-box[0],box[3]-box[1]));ImageDraw.Draw(fill).polygon(boundary,fill=1)
    yy,xx=np.nonzero(np.asarray(fill));points=np.column_stack((xx+box[0],yy+box[1])).astype(float)
    center=points.mean(0);eig=np.linalg.eigvalsh(np.cov(points.T,bias=True))
    # Yellow light at the green rim makes strict color segmentation contract the
    # outer boundary. Use the visually verified outer ellipse, not its dark core.
    outer_bbox=[[352.,101.],[409.,155.]]
    return {'center_pixels':[380.5,128.],'bbox_pixels':outer_bbox,
        'sample_count':len(points),'equivalent_area_diameter_pixels':math.sqrt(57*54),
        'principal_axis_ratio':57/54,
        'method':'Manual outer-boundary ellipse approximation, 57px by 54px; includes pupil. Strict color segmentation retained as an interior check only.',
        'diameter_definition':'sqrt(outer_width*outer_height), equal-area diameter of fitted ellipse',
        'interior_color_hull_measurement':{'center_pixels':center.tolist(),'D_pixels':math.sqrt(4*len(points)/math.pi),
            'principal_axis_ratio':float(math.sqrt(eig[1]/eig[0]))},
        'roi_pixels':list(box),'convex_hull_pixels':[[x+box[0],y+box[1]] for x,y in boundary],
        'estimated_boundary_uncertainty_pixels':2}

def crown(im,box):
    a=np.asarray(im.crop(box),dtype=float)
    mask=(a[:,:,0]>1.04*a[:,:,1])&(a[:,:,1]>1.25*a[:,:,2])&(a[:,:,1]>100)
    yy,xx=np.nonzero(mask);top=yy.min();xs=xx[yy<=top+1]
    return [float(xs.mean()+box[0]),float(top+box[1])]

def normalize(point,center,D):return ((np.asarray(point)-center)/D).tolist()

def main():
    records=json.loads((OUT/'reference_candidate_cameras.json').read_text(encoding='utf-8'))
    chosen=next(c for c in records['candidates'] if c['yaw_from_front_F_degrees']==80 and c['pitch_from_horizontal_degrees']==-5)
    ref=Image.open(REF).convert('RGB');model=Image.open(OUT/chosen['filename']).convert('RGB')
    r=reference_eye(ref);m=chosen['eyes']['R']['aperture']
    refD=r['equivalent_area_diameter_pixels'];modelD=m['equivalent_area_diameter_pixels']
    rc=np.asarray(r['center_pixels']);mc=np.asarray(m['center_pixels'])
    rfeatures={'near_eye':rc.tolist(),'far_visible_eye_centroid_approx':[314.5,137.0],
        'mouth_approx':[325.0,180.0],'crown':crown(ref,(290,40,650,300))}
    mfeatures={'near_eye':mc.tolist(),'far_visible_eye_centroid_approx':chosen['eyes']['L']['aperture']['center_pixels'],
        'mouth_approx':[129.0,519.0],'crown':crown(model,(80,150,850,340))}
    rnorm={k:normalize(p,rc,refD) for k,p in rfeatures.items()}
    mnorm={k:normalize(p,mc,modelD) for k,p in mfeatures.items()}
    displayD=150.;umin,vmin=-1.9,-1.7;size=(1200,720)
    canvas=Image.new('RGB',(2520,1220),'white');draw=ImageDraw.Draw(canvas)
    draw.text((50,32),'图1.3参考与当前模型：头部观察角度诊断',font=font(44),fill='#203448')
    draw.text((50,103),'近侧眼外缘等面积直径统一为150像素；各图横纵同比缩放，不拉伸、不镜像。',font=font(27),fill='#536577')
    transforms=[]
    for im,data,D,center,title,xpos,features in [
        (ref,r,refD,rc,'PDF图1.3原图',40,rfeatures),
        (model,m,modelD,mc,'当前模型：偏航80° / 仰角−5° / 滚转0°',1280,mfeatures)]:
        scale=displayD/D;offset=center+np.array([umin,vmin])*D
        panel=im.transform(size,Image.Transform.AFFINE,(1/scale,0,float(offset[0]),0,1/scale,float(offset[1])),
            resample=Image.Resampling.BICUBIC,fillcolor='white')
        pd=ImageDraw.Draw(panel)
        for k,col,label in [('near_eye','#d24d8b','E'),('crown','#3493c8','C'),('mouth_approx','#ac4c2e','M')]:
            pt=(np.asarray(features[k])-offset)*scale;xx,yy=pt
            pd.line((xx-9,yy,xx+9,yy),fill=col,width=3);pd.line((xx,yy-9,xx,yy+9),fill=col,width=3)
            pd.text((xx+12,yy-28),label,font=font(25),fill=col)
        pd.rectangle((0,0,size[0]-1,size[1]-1),outline='#a3b0be',width=2)
        canvas.paste(panel,(xpos,208));draw.text((xpos,163),title,font=font(27),fill='#203448')
        draw.line((xpos+25,965,xpos+25+displayD,965),fill='#435f79',width=4)
        draw.text((xpos+190,947),f'近侧眼径 1D；原图 {D:.2f} px',font=font(24),fill='#435f79')
        draw.text((xpos,1000),f'近侧眼主轴比 {data["principal_axis_ratio"]:.3f}；远侧可见区 ΔX={normalize(features["far_visible_eye_centroid_approx"],center,D)[0]:.3f}D',font=font(25),fill='#203448')
        transforms.append({'label':title,'original_resolution':list(im.size),'D_pixels':D,
            'uniform_scale':scale,'scale_X_equals_Y':True,'crop_origin_original_pixels':offset.tolist(),
            'crop_size_original_pixels':[size[0]/scale,size[1]/scale],'output_size_pixels':list(size)})
    draw.text((50,1070),'E：近侧眼中心  C：头顶像素位置  M：嘴缝近似中心；远侧标记仅为可见区中心。',font=font(25),fill='#536577')
    draw.text((50,1114),'无法同时对齐：近侧眼圆度、远侧眼遮挡、头顶相对眼位、嘴与颈前曲线。参考相机参数未知。',font=font(25),fill='#536577')
    draw.text((50,1158),'本轮仅调整诊断相机；模型几何、尺寸和位置保持。裁剪只限定显示范围，原图比例保持。',font=font(25),fill='#536577')
    canvas.save(OUT/'reference_head_comparison.png')
    contact=Image.new('RGB',(2040,1210),'white');cd=ImageDraw.Draw(contact)
    cd.text((35,20),'相机候选：相同模型、灯光、曝光；仅改变观察角度',font=font(36),fill='#203448')
    summaries=[]
    for i,c in enumerate(records['candidates']):
        x=20+(i%2)*1020;y=90+(i//2)*560
        im=Image.open(OUT/c['filename']).convert('RGB');im.thumbnail((490,490));contact.paste(im,(x,y))
        ap=c['eyes']['R']['aperture'];far=c['eyes']['L']['aperture']
        delta=normalize(far['center_pixels'],ap['center_pixels'],ap['equivalent_area_diameter_pixels'])
        summary={'yaw':c['yaw_from_front_F_degrees'],'pitch':c['pitch_from_horizontal_degrees'],
            'near_axis_ratio':ap['principal_axis_ratio'],'far_visible_delta_over_near_D':delta}
        summaries.append(summary)
        lines=[f'偏航 {summary["yaw"]}°',f'仰角 {summary["pitch"]}°',f'近侧眼主轴比 {summary["near_axis_ratio"]:.3f}',
            f'远侧可见 ΔX {delta[0]:.3f}D',f'远侧可见 ΔY {delta[1]:.3f}D']
        if c is chosen:lines.append('采用的近似观察角度')
        for j,line in enumerate(lines):cd.text((x+515,y+45+j*54),line,font=font(27),fill='#203448')
    contact.save(OUT/'reference_camera_candidates.png')
    result={'source_blend':records['source_blend'],'source_sha256':records['source_sha256'],'geometry_unchanged':True,
        'reference_source_pdf':'C:/Users/31314/Desktop/奶龙大冒险素材库/《捧腹大笑思想与奶龙学概论》.pdf',
        'reference_pdf_page_1based':3,'reference_figure':'1.3','reference_image':str(REF),
        'reference_image_sha256':hashlib.sha256(REF.read_bytes()).hexdigest(),
        'reference_eye':r,'selected_camera':chosen,'candidate_summary':summaries,'transforms':transforms,
        'reference_features_pixels':rfeatures,'model_features_pixels':mfeatures,
        'reference_features_over_near_D':rnorm,'model_features_over_near_D':mnorm,
        'method':'Near aperture area-equivalent diameter: reference manually fitted outer ellipse includes black; model visible geometry ray sampling includes black. Uniform XY scale, eye-centered crop.',
        'limitations':['Reference silhouette and camera projection are unknown; orientation is approximate, not a recovered unique camera.',
            'Reference green boundary approximately ±2px; manual mouth/far-eye points approximate. Model geometry projection step 2px.',
            'Far-eye location refers to centroid of visible region, not hidden anatomical center.',
            'Different lighting/backgrounds retained from inputs; comparison assesses contour and feature positions, not color.']}
    (OUT/'reference_alignment.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8')
    md=f'''# PDF图1.3观察方向对齐

本轮保持模型几何。输入为侧面连续性版本，SHA256 `{records['source_sha256']}`。参考是PDF第3页图1.3的原嵌入图（900×1130），模型原渲染1000×1000；均按原始宽高比显示。

## 相机与显示方法

采用偏航80°、仰角−5°、滚转0°。偏航从脸朝向F=−Y开始，正角度将相机移向+X；角色朝画面左侧。相机为正交，ortho_scale={chosen['ortho_scale']:.9f}，目标{chosen['target']}，位置{chosen['location']}，相距7工程单位。曝光−0.4、Standard视图变换；四个候选使用同一场景灯光。完整协议见JSON及渲染脚本。

近侧眼用外缘围成区域的**等面积直径**统一为150像素。参考D≈{refD:.4f}px（目测外缘椭圆57×54像素，取sqrt(57×54)，包含黑瞳），模型D≈{modelD:.4f}px（可见几何射线投影）。缩放系数分别{150/refD:.8f}和{150/modelD:.8f}，横纵完全相同；没有镜像或拉伸。裁剪以近侧眼中心对齐，范围横向−1.9D至+6.1D、纵向−1.7D至+3.1D。E为近侧眼，C为头顶，M为嘴缝近似中心。

## 无法同时对齐的部分

- 近侧眼：参考主轴比约{r['principal_axis_ratio']:.3f}，当前模型在采用角度下约{m['principal_axis_ratio']:.3f}；模型斜长轮廓明显。改变候选角度后仍无法同时取得参考的近圆眼体与远侧眼遮挡关系。
- 远侧眼：参考可见区相对近侧眼ΔX≈{rnorm['far_visible_eye_centroid_approx'][0]:.3f}D、ΔY≈{rnorm['far_visible_eye_centroid_approx'][1]:.3f}D；模型分别{mnorm['far_visible_eye_centroid_approx'][0]:.3f}D、{mnorm['far_visible_eye_centroid_approx'][1]:.3f}D。这里测可见区中心，不能作为完整眼中心距。
- 头顶相对近侧眼：参考(C−E)/D≈({rnorm['crown'][0]:.3f},{rnorm['crown'][1]:.3f})，模型≈({mnorm['crown'][0]:.3f},{mnorm['crown'][1]:.3f})。头顶位置及后侧饱满轮廓无法在眼径统一后重合。
- 嘴缝相对近侧眼：参考(M−E)/D≈({rnorm['mouth_approx'][0]:.3f},{rnorm['mouth_approx'][1]:.3f})，模型≈({mnorm['mouth_approx'][0]:.3f},{mnorm['mouth_approx'][1]:.3f})；短嘴方向与前脸斜度也有差异。参考颈前回收更明确，当前模型前颈更接近长直曲线。

参考没有已知三维相机、焦距或透视协议。80°/−5°是本轮四个候选中的观察方向近似：负仰角使远近眼纵向顺序更接近参考，但牺牲部分近眼圆度，并不能认定为参考真实相机角度。候选图保留用于核查。

## 测量边界

参考绿色边界估计误差约±2像素；嘴缝及远侧可见区中心是人工近似标记。模型投影采样步长2像素。参考主轴比用目测外缘椭圆57/54；模型主轴比用可见区域像素协方差。两者均为图像轮廓指标，不作为精确三维球径。严格绿色阈值会漏掉黄色光照影响的外缘，得到较小的约50.10像素色彩核心，因此仅保留作辅助记录，没有用于对齐比例。图像颜色、光照和背景沿用原输入，不能据此评价材质一致性。本次没有追加造型修改。

![并排图](reference_head_comparison.png)

![候选相机](reference_camera_candidates.png)
'''
    (OUT/'reference_alignment.md').write_text(md,encoding='utf-8')
    print('REFERENCE_HEAD_COMPARISON_OK',refD,modelD)

if __name__=='__main__':main()
