"""Write the human-readable diagnostic summary from saved measurements."""
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'diagnostics/20261005_head_geometry'

def main():
    m=json.loads((OUT/'head_geometry_measurements.json').read_text(encoding='utf-8'));D=m['D']
    align=json.loads((OUT/'reference_alignment.json').read_text(encoding='utf-8'))
    es=m['eyes'];sp=m['eye_spacing']
    rows=[]
    specs=[('可见绿色外缘眼径','visible_outer_equivalent_diameter'),('可见黑瞳直径','visible_pupil_equivalent_diameter'),
        ('绿区最前点相对同XZ脸面的F凸出量',('foremost_visible_green','projection_F')),
        ('绿区最大局部F凸出量',('maximum_green_face_gap','projection_F')),
        ('整个眼冠最大局部F凸出量',('maximum_whole_eye_face_gap','projection_F')),
        ('代表点沿局部法线的闭合眼体厚度',('local_normal_thickness','thickness'))]
    for label,key in specs:
        vals=[es[s][key] if isinstance(key,str) else es[s][key[0]][key[1]] for s in ('L','R')]
        rows.append(f'| {label} | {vals[0]:.9f}（{vals[0]/D:.5f}D） | {vals[1]:.9f}（{vals[1]/D:.5f}D） |')
    heads=[];names={'eye_level':'眼中心','mouth_level':'嘴线','mouth_minus_half_D':'嘴下0.5D','mouth_minus_D':'嘴下1D'}
    for s in m['horizontal_sections']:
        heads.append(f'| {names[s["section"]]} | {s["Z"]:.9f} | {s["width_X"]:.9f}（{s["width_over_D"]:.4f}D） | {s["depth_Y"]:.9f}（{s["depth_over_D"]:.4f}D） | ({s["center_X"]:.9f}, {s["center_Y"]:.9f}, {s["center_Z"]:.9f}) |')
    text=f'''# 奶蛙头部几何诊断

2026-10-06完成；输出目录日期沿用任务启动日2026-10-05。打开 [图文总览](index.html) 可查看全部图和测量表。

输入：`versions/20261005_head_side_continuity/Naiwa_Character.blend`，姿态为Naiwa_Idle第1帧。本轮仅做诊断，未保存或修改原模型，未缩头、缩颈或放大眼睛。源文件SHA256：`{m['source_sha256']}`。

## 坐标与眼径定义

X向右、Y向后、Z向上；水平前方F=−Y。所有几何数据使用Blender工程坐标单位，不将它们擅自换算成毫米。面部中线X=0。

本轮D={D:.12f}。D为正面射线可见的绿色**外缘围成区域**的等面积直径sqrt(4A/pi)，包括外缘内的黑瞳，不是仅用绿色环的面积。左右实际眼径{es['L']['visible_outer_equivalent_diameter']:.12f}、{es['R']['visible_outer_equivalent_diameter']:.12f}。投影采样步长0.0003；构造脚本名义眼径0.117312900897与当前可见D略有差别。

## 眼球剖面、曲率与测量

[单眼中心侧剖面](eye_side_sections.png) · [眼部灰模近景](eye_gray_curvature.png) · [完整眼部CSV](eye_measurements.csv)

![左右眼剖面](eye_side_sections.png)

| 测量项目 | 左眼L | 右眼R |
| --- | --- | --- |
{chr(10).join(rows)}

两眼水平中心距={sp['horizontal_X']:.12f}，即{sp['horizontal_X_over_D']:.8f}D；正面XZ距离={sp['frontal_XZ_distance']:.12f}（{sp['frontal_XZ_over_D']:.8f}D）。采用网格记录的正面外缘中心；当前眼体不是完整球体，不能给出唯一球心。

**最前点与最大局部凸出量须分开理解。**绿区最前点以全局−Y最小点为准，再与同一XZ处黄色脸面比较；由于脸面倾斜，它不是眼冠相对脸面的最大距离。绿区最大局部凸出约0.15D，包含中央黑区的整个眼冠才达到约0.20D。

绿色区和黑瞳区共享同一眼网格，边界96条边、96个顶点完全共用，独立黑瞳几何台阶=**0**。黑区最大局部脸面凸出与绿区最大局部凸出的差，左眼0.005885094（0.05019D），右眼0.005884498（0.05019D），来自连续眼冠曲率，不能解释为额外瞳孔厚度。若直接比较黑、绿各自全局最前点，黑区反而分别比绿区后退0.004747868、0.004755974；这是两测点所处倾斜脸面位置不同。

法线厚度从实际可见绿色三角面的代表点向内，测到闭合眼体后面的首次交点，含埋入黄色主体内部的封背，**不是绿色材料层或均匀薄壳厚度**。左右代表点厚度分别0.089451166、0.087780646（0.76294D、0.74870D）。每眼32个前部代表点采样范围为左0.068125987～0.116065944、右0.066761184～0.123875471；这只是采样范围，不是全表面极值。完整前后端点、法线和三角面编号见JSON。

![眼部灰模](eye_gray_curvature.png)

灰模保留当前几何，仅临时统一灰色材质；小凸点是独立高光网格。可见眼体鼓出，斜侧轮廓呈长斜面，其形状还叠加了基础脸面的斜度。

## 头部截面

[正中侧剖面](head_midline_section.png) · [四个水平截面](head_horizontal_sections.png) · [截面CSV](head_section_measurements.csv)

![正中侧剖面](head_midline_section.png)

![水平截面](head_horizontal_sections.png)

| 截面 | Z | 横向宽度X | 前后深度Y | 中心(X,Y,Z) |
| --- | --- | --- | --- | --- |
{chr(10).join(heads)}

截面直接与实际评估网格三角面求交。水平截面使用黄色主体的头颈主闭合轮廓，排除独立眼体和肩臂分量。中心取截面包围盒中心；面积形心另外记录于CSV/JSON，两者不混用。图中四面板几何比例一致。

从这四个水平截面看，黄色主体从眼线向下逐渐变宽；嘴线宽约4.35D，嘴下1D约4.91D。正中剖面展示当前额脸、嘴和颈前的实际连接。本轮仅记录这些结构，不据此追加调整。

## 与PDF图1.3对齐

[头部并排图](reference_head_comparison.png) · [候选相机](reference_camera_candidates.png) · [角度与对齐说明](reference_alignment.md)

![参考头部并排](reference_head_comparison.png)

采用相机偏航80°、仰角−5°、滚转0°，正交scale=0.86；仅相机改变。按各自近侧眼外缘等面积直径统一显示比例，横纵等比缩放，保留原图宽高比。参考近侧眼用57×54像素外缘椭圆近似，D≈{align['reference_eye']['equivalent_area_diameter_pixels']:.4f}px；模型用可见几何投影D≈{align['selected_camera']['eyes']['R']['aperture']['equivalent_area_diameter_pixels']:.4f}px。

参考近侧眼近圆（外缘椭圆主轴比约1.056），模型在采用角度下主轴比约2.460。远侧眼可见区位置、头顶相对眼位、嘴缝与颈前曲线也无法同时重合。参考相机参数未知，采用角度是观察方向近似，不是唯一三维相机恢复。详细像素位置、误差与四个候选见对齐说明及JSON。

## 生成参数与代码

[头部与眼体参数说明](HEAD_GENERATION_PARAMETERS.md) · [结构化参数JSON](head_generation_parameters.json) · [生成源码快照ZIP](head_generation_code.zip) · [源码清单](head_generation_source_manifest.json)

代码说明保留基础头身横截面、眼冠构造、埋入封背、黑绿颜色分区、上一轮头宽权重及当前连续性修整权重。当前默认入口继承固定上一轮工程，代码包不复制.blend或FBX；复现仍需原项目中的固定输入。`--from-primitives`为历史分支，不能替代当前工程的精确复建。

## 原始数据与校验

[完整测量JSON](head_geometry_measurements.json) · [原始截线JSON](head_geometry_sections.json) · [相机对齐JSON](reference_alignment.json) · [独立数值复核](independent_data_validation.json) · [交付文件校验](diagnostic_validation.json)

测量、临时灰材质渲染与相机候选渲染均未保存源工程。测量前后源SHA256一致，当前模型文件不变。PNG均已人工查看，截面尺寸、测量表、绘图数据和源码包经过交叉核对。
'''
    (OUT/'DIAGNOSTIC_REPORT.md').write_text(text,encoding='utf-8')
    print('HEAD_DIAGNOSTIC_SUMMARY_OK')

if __name__=='__main__':main()
