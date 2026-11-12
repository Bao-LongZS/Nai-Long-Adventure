import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const kitRoot = path.join(repoRoot, 'UE5/SourceAssets/P6/Environment/A1_CourtyardKit');
const evidenceRoot = path.join(repoRoot, 'UE5/Records/p6_art/evidence');
const pointMap = JSON.parse(fs.readFileSync(path.join(repoRoot, 'UE5/SourceAssets/P6/Manifests/P6_A1_CurrentPointArtMap_v01.json'), 'utf8'));
const fullDepth = JSON.parse(fs.readFileSync(path.join(kitRoot, 'P6_A1_M2CameraDepthProxy_v04.json'), 'utf8'));
const cutawayDepth = JSON.parse(fs.readFileSync(path.join(kitRoot, 'P6_A1_M2CameraCutawayDepthProxy_v04.json'), 'utf8'));
const captureReport = JSON.parse(fs.readFileSync(path.join(evidenceRoot, 'P6_A1_M2LightingReviewCapture_v11.json'), 'utf8'));
const screenshotPath = path.join(evidenceRoot, 'P6_A1_M2LightingReview_1920x1080_v11.png');
const conceptPath = path.join(repoRoot, 'UE5/SourceAssets/P6/Environment/P6_A0_Courtyard_Concept_v01.png');
if (captureReport.status !== 'captured' || !fs.existsSync(screenshotPath)) throw new Error('The M2 camera UE screenshot is missing or incomplete.');
if (pointMap.points.length !== 14 || fullDepth.point_rows.length !== 14 || cutawayDepth.point_rows.length !== 14) throw new Error('Expected 14 points in both source and depth proxies.');

const enc = file => `data:image/png;base64,${fs.readFileSync(file).toString('base64')}`;
const xml = value => String(value).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
const fullById = new Map(fullDepth.point_rows.map(row => [row.id, row]));
const cutById = new Map(cutawayDepth.point_rows.map(row => [row.id, row]));
const kindColor = {evidence:'#F1BF72', witness:'#74CFC4', joke:'#C0C6CF'};
const image = enc(screenshotPath);
const concept = enc(conceptPath);
const img = {x:56,y:178,w:1520,h:855};
const markerSvg = pointMap.points.map(point => {
  const row = fullById.get(point.id);
  const cx = img.x + point.projected_px[0] * img.w / 1920;
  const cy = img.y + point.projected_px[1] * img.h / 1080;
  const color = kindColor[point.kind] || '#E7E9EA';
  const mark = row?.visible_proxy === true
    ? `<circle cx="${cx}" cy="${cy}" r="13" fill="#172331" fill-opacity=".92" stroke="${color}" stroke-width="3"/><circle cx="${cx}" cy="${cy}" r="4" fill="#FFF4E2"/>`
    : `<circle cx="${cx}" cy="${cy}" r="13" fill="#172331" fill-opacity=".92" stroke="${color}" stroke-width="3"/><path d="M${cx-5} ${cy-5}L${cx+5} ${cy+5}M${cx+5} ${cy-5}L${cx-5} ${cy+5}" stroke="#FFF4E2" stroke-width="2.5" stroke-linecap="round"/>`;
  return `<g><title>${xml(point.id)} · ${row?.visible_proxy ? 'pivot proxy passes' : `occluder candidate: ${row?.occluding_group_candidate ?? row?.nearest_surface_group}`}</title>${mark}<rect x="${cx+11}" y="${cy-12}" width="21" height="21" rx="5" fill="#172331" fill-opacity=".94"/><text x="${cx+21.5}" y="${cy+3}" text-anchor="middle" font-family="Segoe UI,sans-serif" font-size="10" font-weight="700" fill="#FFF4E2">${point.sequence}</text></g>`;
}).join('\n');

const rows = pointMap.points.map(point => {
  const row = fullById.get(point.id);
  const cut = cutById.get(point.id);
  const y = 236 + (point.sequence - 1) * 49;
  const type = point.required ? '必看' : '可跳过';
  const fullStatus = row?.visible_proxy === true ? '○ 屋顶完整：通过' : '× 屋顶完整：遮挡';
  const cutStatus = cut?.visible_proxy === true ? '剖切：通过' : '剖切：遮挡';
  const surface = row?.visible_proxy === true ? '无遮挡候选' : (row?.occluding_group_candidate ?? row?.nearest_surface_group ?? '未知');
  return `<g>
    <path d="M1638 ${y+9}H2642" stroke="#48596A" stroke-width="1"/>
    <circle cx="1660" cy="${y-5}" r="16" fill="#283C50" stroke="${kindColor[point.kind] || '#D7DCE0'}" stroke-width="2"/>
    <text x="1660" y="${y}" text-anchor="middle" fill="#F8F2E8" font-family="Segoe UI,sans-serif" font-size="12" font-weight="700">${point.sequence}</text>
    <text x="1688" y="${y-4}" fill="#F2E7D2" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15" font-weight="700">${xml(point.name)} · ${xml(point.id)} <tspan fill="#AEBBC6" font-size="12">${type}</tspan></text>
    <text x="1688" y="${y+16}" fill="${row?.visible_proxy === true ? '#A6D5C7' : '#E5BBA2'}" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="12">${fullStatus}　${cutStatus}　<tspan fill="#ABBAC6">最近面：${xml(surface)}</tspan></text>
  </g>`;
}).join('\n');

const WIDTH = 2700, HEIGHT = 1640;
const svg = `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="${WIDTH}" height="${HEIGHT}" viewBox="0 0 ${WIDTH} ${HEIGHT}">
  <title>P6 A1 · M2 固定机位遮挡核对 v01</title>
  <desc>Self-contained editor review board. It overlays the 14 current M2 actor pivot projections on the actual UE screenshot and compares full-roof and roof-cutaway offline depth proxies. Markers do not represent actor bounds or clickable surfaces.</desc>
  <defs><linearGradient id="bg" x2="0" y2="1"><stop stop-color="#172331"/><stop offset="1" stop-color="#25394B"/></linearGradient></defs>
  <rect width="${WIDTH}" height="${HEIGHT}" fill="url(#bg)"/>
  <text x="54" y="62" fill="#F7EEDF" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="34" font-weight="700">P6 · A1 M2 固定机位遮挡核对 v01</text>
  <text x="56" y="97" fill="#B9C7D0" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="17">UE 5.8.3 D3D12 截图 v11；相机与当前 M2 点位表一致 · 截图上叠加 14 个 pivot 投影（编辑器诊断，不是游戏 HUD）</text>
  <rect x="40" y="124" width="1550" height="946" rx="20" fill="#202F3F" stroke="#53687C" stroke-width="2"/>
  <image href="${image}" x="${img.x}" y="${img.y}" width="${img.w}" height="${img.h}" preserveAspectRatio="none"/>
  <g>${markerSvg}</g>
  <text x="56" y="1050" fill="#D9CBAF" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="13">圆环：pivot 最近面代理通过　十字：被最近投影面挡住　颜色：证据 / 口供 / 可跳过点位类型</text>
  <rect x="1610" y="124" width="1050" height="826" rx="20" fill="#202F3F" stroke="#53687C" stroke-width="2"/>
  <text x="1640" y="174" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="23" font-weight="700">14 点 pivot 最近面代理</text>
  <text x="1640" y="200" fill="#AEBBC6" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="13">全屋顶通过 ${fullDepth.point_rows.filter(row=>row.visible_proxy).length}/14 · 临时隐藏屋顶/檐口通过 ${cutawayDepth.point_rows.filter(row=>row.visible_proxy).length}/14</text>
  ${rows}
  <rect x="1610" y="972" width="1050" height="598" rx="20" fill="#202F3F" stroke="#53687C" stroke-width="2"/>
  <text x="1640" y="1020" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="22" font-weight="700">已通过的 A0 夜景概念</text>
  <image href="${concept}" x="1640" y="1040" width="990" height="510" preserveAspectRatio="xMidYMid meet"/>
  <text x="54" y="1120" fill="#E7D4B2" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="20" font-weight="700">审阅结论</text>
  <text x="54" y="1155" fill="#C7D2D9" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="16">前景戏台/房屋屋面占住视野并挡住多个必看点；隐藏屋顶和檐口只多通过 1 个 pivot。下一轮需同时重排环境轮廓与点位视线，并重做 A0 的冷夜背景、暖灯对比。</text>
  <text x="54" y="1184" fill="#AEBBC6" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="14">边界：深度代理只比较点位中心与离线源模型最近投影面；不代表物件轮廓、UE 遮挡结果、Actor bounds、可点击范围、碰撞或可达性。</text>
  <text x="54" y="1215" fill="#AEBBC6" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="13">证据：UE5/Records/p6_art/evidence/P6_A1_M2LightingReviewCapture_v11.json · 当前点位来源：P6_A1_CurrentPointArtMap_v01.json</text>
</svg>`;

const outPath = path.join(evidenceRoot, 'P6_A1_M2VisibilityReview_v01.svg');
fs.writeFileSync(outPath, svg, 'utf8');
console.log(JSON.stringify({path:path.relative(repoRoot,outPath).replaceAll('\\','/'),points:pointMap.points.length,full_roof_visible:fullDepth.point_rows.filter(row=>row.visible_proxy).length,cutaway_visible:cutawayDepth.point_rows.filter(row=>row.visible_proxy).length,blocked_required:fullDepth.point_rows.filter(row=>row.visible_proxy!==true&&pointMap.points.some(point=>point.id===row.id&&point.required)).length,embedded_images:2},null,2));
