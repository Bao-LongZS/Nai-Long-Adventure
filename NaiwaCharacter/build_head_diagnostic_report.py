"""Assemble a local, portable diagnostic viewer after measurements pass."""
import csv
import hashlib
import html
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'diagnostics/20261005_head_geometry'
SOURCE=ROOT/'versions/20261005_head_side_continuity/Naiwa_Character.blend'
esc=html.escape

def csv_table(filename):
    with (OUT/filename).open(encoding='utf-8-sig',newline='') as f:
        rows=list(csv.reader(f))
    if not rows:return ''
    return '<div class="table-wrap"><table><thead><tr>'+''.join('<th>'+esc(v)+'</th>' for v in rows[0])+\
        '</tr></thead><tbody>'+''.join('<tr>'+''.join('<td>'+esc(v)+'</td>' for v in row)+'</tr>' for row in rows[1:])+\
        '</tbody></table></div>'

def figure(filename,caption):
    assert (OUT/filename).is_file(),filename
    return f'<figure><a href="{filename}" target="_blank"><img src="{filename}" alt="{esc(caption)}"></a><figcaption>{esc(caption)}</figcaption></figure>'

def main():
    metrics=json.loads((OUT/'head_geometry_measurements.json').read_text(encoding='utf-8'))
    sections=json.loads((OUT/'plotted_section_dimensions.json').read_text(encoding='utf-8'))
    alignment=json.loads((OUT/'reference_alignment.json').read_text(encoding='utf-8'))
    D=float(metrics['D'])
    eye_rows=''
    for side,e in metrics['eyes'].items():
        values=[side,f'{e["visible_outer_equivalent_diameter"]:.9f}',
            f'{e["visible_pupil_equivalent_diameter"]:.9f}',f'{e["foremost_visible_green"]["projection_F"]:.9f}',
            f'{e["maximum_green_face_gap"]["projection_F"]:.9f}',f'{e["maximum_whole_eye_face_gap"]["projection_F"]:.9f}',
            f'{e["local_normal_thickness"]["thickness"]:.9f}','0']
        eye_rows+='<tr>'+''.join('<td>'+esc(v)+'</td>' for v in values)+'</tr>'
    sha=hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    assert sha=='a95a5c7efb25cb6f379897dd6ec4af98a201bc2619dc8caa2bb5b5de8ea3dfd5','Source changed during diagnosis'
    section_rows=''.join('<tr>'+''.join(f'<td>{v}</td>' for v in [esc(s['section']),f'{s["Z"]:.7f}',
        f'{s["width_X"]:.7f}',f'{s["width_X"]/D:.4f}D',f'{s["depth_Y"]:.7f}',
        f'{s["depth_Y"]/D:.4f}D','('+', '.join(f'{v:.7f}' for v in s['bbox_center_XYZ'])+')'])+'</tr>' for s in sections)
    content=f'''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>奶蛙头部几何诊断</title><style>
body{{margin:0;background:#f3f5f7;color:#243447;font:16px/1.65 "Microsoft YaHei",sans-serif}}
main{{max-width:1540px;margin:25px auto;padding:32px 40px;background:white}}
h1{{font-size:30px;margin:0 0 10px}}h2{{font-size:24px;margin:38px 0 12px;border-top:1px solid #dce2e8;padding-top:22px}}
a{{color:#23618a}}nav{{display:flex;gap:25px;flex-wrap:wrap;margin:22px 0}}
.note{{background:#edf4f8;padding:16px 20px;border-left:4px solid #5e83a1}}
figure{{margin:20px 0}}img{{display:block;max-width:100%;height:auto}}figcaption{{color:#5b6b7d;margin-top:9px}}
.table-wrap{{overflow:auto}}table{{border-collapse:collapse;min-width:900px;font-size:14px}}
th,td{{border:1px solid #dce2e8;padding:9px 13px;text-align:left;white-space:nowrap}}th{{background:#eef2f5}}
pre{{white-space:pre-wrap;overflow-wrap:anywhere;background:#f6f8fa;padding:20px;font-size:13px}}
small{{color:#627487}}details{{margin:15px 0}}code{{overflow-wrap:anywhere}}
@media(max-width:900px){{main{{margin:0;padding:22px 18px}}}}
</style><main><h1>奶蛙头部几何诊断</h1>
<p>2026-10-06 完成；目录日期沿用任务启动日。输入为当前侧面连续性版本。本轮仅进行测量、剖面和相机对齐，模型几何、尺寸、位置及原工程保持。</p>
<p><a href="DIAGNOSTIC_REPORT.md">阅读完整中文诊断说明</a></p>
<nav><a href="#eyes">眼部诊断</a><a href="#sections">头部截面</a><a href="#reference">图1.3对齐</a><a href="#code">参数与代码</a></nav>
<div class="note">D = {D:.10f} 工程单位。D 为绿色外缘围成区域的可见等面积直径，外缘内的黑瞳不从面积中扣除。X 向右、Y 向后、Z 向上，前方 F = -Y。</div>
<h2 id="eyes">眼部几何与测量</h2>
<p>黄色基础脸面指当前黄色主体表面，排除眼网格。黑瞳和绿色区是同一闭合网格的颜色分区；材质边界没有独立几何台阶。局部法线厚度测量闭合眼体的前后交点弦长，包含埋入黄色主体内的封背部分。</p>
{figure('eye_side_sections.png','通过左右单眼中心的侧面剖面；绿色、黑色和黄色分别表示实际网格区域。')}
{figure('eye_gray_curvature.png','同一眼体的正面、斜侧灰模曲率近景；小凸点是高光网格。')}
<div class="table-wrap"><table><thead><tr><th>眼</th><th>绿色外缘眼径</th><th>黑瞳直径</th><th>绿区最前点对脸面凸出</th><th>绿区最大局部凸出</th><th>全眼冠最大局部凸出</th><th>代表法线厚度</th><th>黑瞳独立台阶</th></tr></thead><tbody>{eye_rows}</tbody></table></div>
<p>两眼水平中心距 {metrics['eye_spacing']['horizontal_X']:.9f} = {metrics['eye_spacing']['horizontal_X_over_D']:.6f}D。绿区最前点对脸面凸出约0.0569D，绿区最大局部凸出约0.1499D，整个眼冠最大局部凸出约0.2001D；三者测量定义不同。黑区与绿区最大局部凸出之差约0.0502D来自共享眼冠曲率，独立黑瞳台阶为0。</p>
<details><summary>完整眼部CSV字段</summary>{csv_table('eye_measurements.csv')}</details>
<p><a href="eye_measurements.csv">下载眼部测量表 CSV</a> · <a href="head_geometry_measurements.json">完整测量 JSON</a></p>
<h2 id="sections">头部正中剖面与四个水平截面</h2>
{figure('head_midline_section.png','面部中线 X=0 的侧剖面及各水平截面所在高度。')}
{figure('head_horizontal_sections.png','黄色主体的宽度、深度和包围盒中心；四面板保持相同几何比例。')}
<div class="table-wrap"><table><thead><tr><th>截面</th><th>Z</th><th>宽度 X</th><th>宽度 / D</th><th>深度 Y</th><th>深度 / D</th><th>中心 (X,Y,Z)</th></tr></thead><tbody>{section_rows}</tbody></table></div>
<p><a href="head_section_measurements.csv">下载头部截面表 CSV</a> · <a href="head_geometry_sections.json">原始三角面截线与点坐标 JSON</a></p>
<h2 id="reference">按图1.3观察方向与近侧眼径对齐</h2>
{figure('reference_head_comparison.png','参考与模型头部并排；以各自近侧眼的二维投影外径统一比例，等比缩放，保持原始宽高比。')}
<p><a href="reference_alignment.md">查看相机角度、像素标记与无法同时对齐的部位</a> · <a href="reference_alignment.json">对齐测量 JSON</a></p>
<p>采用近似观察方向：偏航80°、仰角−5°、滚转0°，正交相机scale=0.86。参考近侧眼外缘主轴比约1.056，模型在此角度约2.460。</p>
{figure('reference_camera_candidates.png','四个仅改变相机的候选角度；保留用于核对遮挡与近侧眼轮廓。')}
<p>参考近侧眼更接近圆形，而当前模型侧向视角的近侧眼呈斜长形。接近参考的远侧眼遮挡后，近侧眼圆度仍有差异；相机对齐不会修正这些几何差异。原参考没有可恢复的唯一三维相机参数，候选角度用于近似观察方向。</p>
<h2 id="code">生成参数与对应代码</h2>
<p><a href="HEAD_GENERATION_PARAMETERS.md">头部、眼体与局部形变参数说明</a> · <a href="head_generation_parameters.json">结构化参数</a> · <a href="head_generation_code.zip">原始生成代码及依赖快照</a></p>
<p>当前模型继承固定上一轮工程。代码包保留轮廓、眼球曲面、形变权重与实际调用链；不复制原生工程或 FBX。复现需要项目中保留相应固定输入版本。</p>
<details><summary>完整测量数据</summary><pre>{esc(json.dumps(metrics,indent=2,ensure_ascii=False))}</pre></details>
<details><summary>参考对齐数据</summary><pre>{esc(json.dumps(alignment,indent=2,ensure_ascii=False))}</pre></details>
<p><small>输入 SHA256：<code>{sha}</code>。本轮没有保存模型或改变模型坐标。</small></p>
</main></html>'''
    (OUT/'index.html').write_text(content,encoding='utf-8')
    print('HEAD_DIAGNOSTIC_REPORT_OK')

if __name__=='__main__':main()
