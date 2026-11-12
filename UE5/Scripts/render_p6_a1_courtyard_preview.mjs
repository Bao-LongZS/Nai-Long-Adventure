import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(scriptDir, '../..');
const kitDir = path.join(repoRoot, 'UE5/SourceAssets/P6/Environment/A1_CourtyardKit');
const modelDir = path.join(kitDir, 'Models');
const objPath = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v01.obj');
const mtlPath = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v01.mtl');
const cutaway = process.argv.includes('--cutaway');
const m2Camera = process.argv.includes('--m2');
const previewTag = m2Camera
  ? (cutaway ? 'P6_A1_M2CameraCutawayPreview_v01' : 'P6_A1_M2CameraPreview_v01')
  : (cutaway ? 'P6_A1_CourtyardKit_CutawayPreview_v01' : 'P6_A1_CourtyardKit_Blockout_Preview_v01');
const outPath = path.join(kitDir, `${previewTag}.svg`);
const pngPath = path.join(kitDir, `${previewTag}.png`);

const material = new Map();
let active = null;
for (const line of fs.readFileSync(mtlPath, 'utf8').split(/\r?\n/)) {
  const m = line.match(/^newmtl\s+(\S+)/);
  if (m) { active = m[1]; material.set(active, [0.6, 0.6, 0.6]); continue; }
  const kd = line.match(/^Kd\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)/);
  if (kd && active) material.set(active, kd.slice(1).map(Number));
}

const vertices = [[0, 0, 0]];
const faces = [];
let objectName = 'Unassigned';
let materialName = 'Ground';
for (const line of fs.readFileSync(objPath, 'utf8').split(/\r?\n/)) {
  const v = line.match(/^v\s+([\d.-]+)\s+([\d.-]+)\s+([\d.-]+)/);
  if (v) { vertices.push(v.slice(1).map(Number)); continue; }
  const o = line.match(/^o\s+(.+)/);
  if (o) { objectName = o[1]; continue; }
  const use = line.match(/^usemtl\s+(\S+)/);
  if (use) { materialName = use[1]; continue; }
  const f = line.match(/^f\s+(.+)/);
  if (f) {
    const ids = f[1].trim().split(/\s+/).map(token => Number(token.split('/')[0]));
    faces.push({ ids, objectName, materialName });
  }
}

const add = (a, b) => a.map((v, i) => v + b[i]);
const sub = (a, b) => a.map((v, i) => v - b[i]);
const dot = (a, b) => a.reduce((s, v, i) => s + v * b[i], 0);
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const norm = v => { const n = Math.hypot(...v) || 1; return v.map(x => x / n); };
const target = [0, 0, 500];
const camera = [8200, -10500, 9600];
const towardCamera = norm(sub(camera, target));
const forward = towardCamera.map(x => -x);
const right = norm(cross(forward, [0, 0, 1]));
const up = norm(cross(right, forward));
const axonProjected = vertices.slice(1).map(p => {
  const d = sub(p, target);
  return { x: dot(d, right), y: dot(d, up), depth: dot(d, towardCamera) };
});
const m2Projection = point => {
  const pixelsPerCm = 0.24615384615384617;
  const vertical = (point[1] - 2078) * -0.8660254037844387 + (point[2] - 3600) * 0.5;
  return { x: 960 + point[0] * pixelsPerCm, y: 540 - vertical * pixelsPerCm, depth: point[1] * 0.5 + point[2] * 0.8660254037844387 };
};
const projected = m2Camera ? vertices.slice(1).map(m2Projection) : axonProjected;
const minX = Math.min(...projected.map(p => p.x));
const maxX = Math.max(...projected.map(p => p.x));
const minY = Math.min(...projected.map(p => p.y));
const maxY = Math.max(...projected.map(p => p.y));
const width = m2Camera ? 1920 : 1600, height = m2Camera ? 1080 : 1120;
const margin = m2Camera ? { left: 0, right: 0, top: 0, bottom: 0 } : { left: 90, right: 90, top: 125, bottom: 115 };
const scale = m2Camera ? 1 : Math.min((width - margin.left - margin.right) / (maxX - minX), (height - margin.top - margin.bottom) / (maxY - minY));
const fitW = (maxX - minX) * scale, fitH = (maxY - minY) * scale;
const offsetX = m2Camera ? 0 : margin.left + (width - margin.left - margin.right - fitW) / 2 - minX * scale;
const offsetY = m2Camera ? 0 : margin.top + (height - margin.top - margin.bottom - fitH) / 2 + maxY * scale;
const screen = p => m2Camera ? [p.x, p.y] : [offsetX + p.x * scale, offsetY - p.y * scale];
const rgb = values => values.map(x => Math.max(0, Math.min(255, Math.round(x * 255))));
const fillFor = face => {
  const ids = face.ids;
  const a = vertices[ids[0]], b = vertices[ids[1]], c = vertices[ids[2]];
  const normal = norm(cross(sub(b, a), sub(c, a)));
  const keyLight = norm([-0.35, -0.48, 0.8]);
  const intensity = 0.52 + Math.max(0, dot(normal, keyLight)) * 0.42;
  const base = material.get(face.materialName) || [0.6, 0.6, 0.6];
  const colors = rgb(base.map(v => v * intensity));
  return `rgb(${colors.join(',')})`;
};

const selectedFaces = cutaway ? faces.filter(face => !/(Roof|Eave|RoofTile|Gate_Cap)/i.test(face.objectName)) : faces;
const esc = s => s.replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;');
const sorted = selectedFaces.map(face => {
  const points = face.ids.map(id => projected[id - 1]);
  const center = points.reduce((acc, p) => add(acc, [p.x, p.y, p.depth]), [0, 0, 0]).map(x => x / points.length);
  return { face, points, depth: center[2] };
}).sort((a, b) => a.depth - b.depth);

const pointMap = JSON.parse(fs.readFileSync(path.join(repoRoot, 'UE5/SourceAssets/P6/Manifests/P6_A1_CurrentPointArtMap_v01.json'), 'utf8'));
const pointColors = { evidence: '#E8BC77', witness: '#81C9C2', joke: '#B6BDC0' };
const pointOverlay = m2Camera ? pointMap.points.map(point => {
  const p = m2Projection(point.loc);
  const color = pointColors[point.kind] || '#D5D5D5';
  return `    <g data-point="${esc(point.id)}"><circle cx="${p.x.toFixed(1)}" cy="${p.y.toFixed(1)}" r="9" fill="#172331" fill-opacity=".86" stroke="${color}" stroke-width="3"/><text x="${p.x.toFixed(1)}" y="${(p.y + 4.5).toFixed(1)}" text-anchor="middle" fill="#FFF5E4" font-family="Segoe UI,sans-serif" font-size="11" font-weight="700">${point.sequence}</text><title>${esc(point.id)} · M2 pivot overlay; SVG marker is not depth-tested</title></g>`;
}).join('\n') : '';
const svgBackdrop = m2Camera
  ? `<rect width="${width}" height="${height}" fill="#202D3A"/><text x="28" y="31" fill="#F4E9D4" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="19" font-weight="700">P6 A1 · M2 正交镜头投影（离线）</text><text x="28" y="55" fill="#B8C7D2" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="12">14 个 M2 点位序号叠加；SVG 标记不做深度测试。PNG 中淡灰标记表示被模型遮住。</text>`
  : `<rect width="${width}" height="${height}" fill="url(#bg)"/><ellipse cx="800" cy="590" rx="760" ry="470" fill="url(#glow)"/><path d="M0 929 Q800 870 1600 925 V1120 H0Z" fill="#111A23" fill-opacity=".48"/><text x="68" y="60" fill="#F4E9D4" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="29" font-weight="700">P6 A1 · 奶家大院模块灰盒预览${cutaway ? '（临时隐藏屋顶）' : ''}</text><text x="70" y="94" fill="#B8C7D2" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15">OBJ/MTL 源块 · ${selectedFaces.length} 个可见面 · ${vertices.length - 1} 个顶点 · ${new Set(selectedFaces.map(f => f.objectName)).size} 个可选对象组 · 预览投影，非 UE 截图</text>`;

const polygons = sorted.map(({ face, points }) => {
  const coords = points.map(p => screen(p).map(x => x.toFixed(1)).join(',')).join(' ');
  return `    <polygon points="${coords}" fill="${fillFor(face)}" stroke="#25303a" stroke-opacity=".5" stroke-width="1.25" stroke-linejoin="round"><title>${esc(face.objectName)} · ${esc(face.materialName)}</title></polygon>`;
}).join('\n');

const svg = `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">
  <title>P6 A1 Courtyard Kit ${m2Camera ? 'M2 Camera ' : ''}${cutaway ? 'Cutaway ' : ''}Blockout Preview</title>
  <desc>Offline projection of the OBJ source blockout. ${m2Camera ? 'Uses the exact current M2 orthographic screen projection and overlays the 14 actor pivots. SVG markers are not depth tested.' : cutaway ? 'Roof and eave objects are temporarily hidden to inspect courtyard props.' : 'All geometry is shown.'} Preview only; not an engine render or final placement.</desc>
  <defs>
    <linearGradient id="bg" x2="0" y2="1"><stop stop-color="#26394B"/><stop offset="1" stop-color="#172331"/></linearGradient>
    <radialGradient id="glow"><stop stop-color="#8A744D" stop-opacity=".16"/><stop offset="1" stop-color="#8A744D" stop-opacity="0"/></radialGradient>
    <filter id="panelShadow" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="16"/></filter>
  </defs>
  ${svgBackdrop}
  <g id="P6_A1_BLOCKOUT_GEOMETRY">\n${polygons}\n  </g>
  <g id="P6_A1_M2_PointOverlay">${pointOverlay}</g>
  ${m2Camera ? '' : `<g font-family="Segoe UI,Microsoft YaHei,sans-serif" fill="#E9D5AF">
    <path d="M73 1037h88" stroke="#E9D5AF" stroke-width="3"/><path d="M73 1037l11 -6v12Z" fill="#E9D5AF"/>
    <text x="171" y="1043" font-size="14">镜头前方 · 院门侧</text>
    <rect x="1137" y="1004" width="396" height="63" rx="12" fill="#1A2837" fill-opacity=".88" stroke="#53677A"/>
    <text x="1160" y="1030" font-size="14" fill="#F3E5CC">几何指标</text>
    <text x="1160" y="1052" font-size="13" fill="#BFCBD1">X −3550…3550cm · Y −2875…2750cm · Z −110…1859cm</text>
  </g>`}
</svg>`;
fs.writeFileSync(outPath, svg, 'utf8');

// Software raster preview with a real per-pixel depth buffer. The first painter-order
// preview exposed ground/prop overlap artifacts, so this PNG uses depth-tested triangles.
const pngWidth = width, pngHeight = height;
const rgbPixels = Buffer.alloc(pngWidth * pngHeight * 3);
const zBuffer = new Float32Array(pngWidth * pngHeight);
zBuffer.fill(-Infinity);
const objectNames = [...new Set(selectedFaces.map(face => face.objectName))];
const objectIdByName = new Map(objectNames.map((name, index) => [name, index + 1]));
const zOwner = new Uint16Array(pngWidth * pngHeight);
for (let y = 0; y < pngHeight; y++) {
  const t = y / pngHeight;
  const r = Math.round(38 * (1 - t) + 20 * t);
  const g = Math.round(57 * (1 - t) + 30 * t);
  const b = Math.round(75 * (1 - t) + 42 * t);
  for (let x = 0; x < pngWidth; x++) {
    const i = (y * pngWidth + x) * 3;
    rgbPixels[i] = r; rgbPixels[i + 1] = g; rgbPixels[i + 2] = b;
  }
}
const toScreen = p => screen(p);
function shadeColor(face) {
  const ids = face.ids;
  const a = vertices[ids[0]], b = vertices[ids[1]], c = vertices[ids[2]];
  const n = norm(cross(sub(b, a), sub(c, a)));
  const k = norm([-0.35, -0.48, 0.8]);
  const intensity = 0.55 + Math.max(0, dot(n, k)) * 0.44;
  return (material.get(face.materialName) || [0.6, 0.6, 0.6]).map(v => Math.max(0, Math.min(255, Math.round(v * intensity * 255))));
}
function rasterTriangle(a, b, c, color, ownerName) {
  const area = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1]);
  if (Math.abs(area) < 1e-7) return;
  const x0 = Math.max(0, Math.floor(Math.min(a[0], b[0], c[0])));
  const x1 = Math.min(pngWidth - 1, Math.ceil(Math.max(a[0], b[0], c[0])));
  const y0 = Math.max(0, Math.floor(Math.min(a[1], b[1], c[1])));
  const y1 = Math.min(pngHeight - 1, Math.ceil(Math.max(a[1], b[1], c[1])));
  for (let y = y0; y <= y1; y++) {
    const py = y + 0.5;
    for (let x = x0; x <= x1; x++) {
      const px = x + 0.5;
      const wa = ((b[1] - c[1]) * (px - c[0]) + (c[0] - b[0]) * (py - c[1])) / area;
      const wb = ((c[1] - a[1]) * (px - c[0]) + (a[0] - c[0]) * (py - c[1])) / area;
      const wc = 1 - wa - wb;
      if (wa < -1e-6 || wb < -1e-6 || wc < -1e-6) continue;
      const depth = wa * a[2] + wb * b[2] + wc * c[2];
      const at = y * pngWidth + x;
      if (depth <= zBuffer[at]) continue;
      zBuffer[at] = depth;
      zOwner[at] = objectIdByName.get(ownerName) || 0;
      const i = at * 3;
      rgbPixels[i] = color[0]; rgbPixels[i + 1] = color[1]; rgbPixels[i + 2] = color[2];
    }
  }
}
for (const { face, points } of sorted) {
  const c = shadeColor(face);
  const projectedPoints = points.map(p => { const [x, y] = toScreen(p); return [x, y, p.depth]; });
  for (let i = 1; i < projectedPoints.length - 1; i++) rasterTriangle(projectedPoints[0], projectedPoints[i], projectedPoints[i + 1], c, face.objectName);
}
let visiblePointCount = 0;
const depthVisibleIds = [];
const depthHiddenIds = [];
const depthRows = [];
if (m2Camera) {
  for (const point of pointMap.points) {
    const p = m2Projection(point.loc);
    const x = Math.round(p.x), y = Math.round(p.y);
    if (x < 0 || x >= pngWidth || y < 0 || y >= pngHeight) {
      depthRows.push({ id: point.id, sequence: point.sequence, kind: point.kind, screen_px: [x, y], in_viewport: false, visible_proxy: null, nearest_surface_group: null });
      continue;
    }
    const at = y * pngWidth + x;
    const visible = p.depth >= zBuffer[at] - 20;
    const surfaceDepth = Number.isFinite(zBuffer[at]) ? Number(zBuffer[at].toFixed(3)) : null;
    const surfaceGroup = objectNames[zOwner[at] - 1] || null;
    depthRows.push({
      id: point.id,
      sequence: point.sequence,
      kind: point.kind,
      screen_px: [x, y],
      in_viewport: true,
      actor_depth_proxy: Number(p.depth.toFixed(3)),
      nearest_surface_depth_proxy: surfaceDepth,
      depth_delta: surfaceDepth === null ? null : Number((p.depth - surfaceDepth).toFixed(3)),
      visible_proxy: visible,
      nearest_surface_group: surfaceGroup,
      occluding_group_candidate: visible ? null : surfaceGroup
    });
    if (visible) { visiblePointCount++; depthVisibleIds.push(point.id); }
    else depthHiddenIds.push(point.id);
    const colorHex = visible ? (pointColors[point.kind] || '#D5D5D5') : '#8B7272';
    const color = [parseInt(colorHex.slice(1,3),16), parseInt(colorHex.slice(3,5),16), parseInt(colorHex.slice(5,7),16)];
    for (let dy = -9; dy <= 9; dy++) for (let dx = -9; dx <= 9; dx++) {
      const r = Math.hypot(dx, dy);
      if (r < 5.5 || r > 9 || x+dx < 0 || x+dx >= pngWidth || y+dy < 0 || y+dy >= pngHeight) continue;
      const i = ((y+dy)*pngWidth + x+dx)*3;
      rgbPixels[i] = color[0]; rgbPixels[i+1] = color[1]; rgbPixels[i+2] = color[2];
    }
  }
}
if (m2Camera) {
  const reportName = cutaway ? 'P6_A1_M2CameraCutawayDepthProxy_v01.json' : 'P6_A1_M2CameraDepthProxy_v01.json';
  const reportPath = path.join(kitDir, reportName);
  const report = {
    schema_version: '1.0',
    package_id: cutaway ? 'P6_A1_M2CameraCutawayDepthProxy_v01' : 'P6_A1_M2CameraDepthProxy_v01',
    view: cutaway ? 'roof_and_eave_hidden_diagnostic' : 'all_roofs_visible',
    source_model: 'Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_Blockout_v01.obj',
    point_map: 'Manifests/P6_A1_CurrentPointArtMap_v01.json',
    reference_viewport_px: [pngWidth, pngHeight],
    comparison_rule: 'Actor pivot depth is compared with the nearest rasterized surface at the projected pixel using a 20 cm tolerance.',
    warning: 'Offline point-pivot diagnostic only. The nearest surface group may be the point prop itself because some actor origins are at or inside prop bases. It does not establish mesh silhouette readability, Actor bounds visibility, collision, clickable area, or UE runtime occlusion.',
    point_rows: depthRows
  };
  fs.writeFileSync(reportPath, `${JSON.stringify(report, null, 2)}\n`, 'utf8');
  console.log(`Wrote ${reportPath}`);
}
const crcTable = new Uint32Array(256);
for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; crcTable[n] = c >>> 0; }
const crc32 = buffer => { let c = 0xffffffff; for (const b of buffer) c = crcTable[(c ^ b) & 0xff] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
const chunk = (type, data) => {
  const name = Buffer.from(type, 'ascii');
  const length = Buffer.alloc(4); length.writeUInt32BE(data.length);
  const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(Buffer.concat([name, data])));
  return Buffer.concat([length, name, data, crc]);
};
const raw = Buffer.alloc((pngWidth * 3 + 1) * pngHeight);
for (let y = 0; y < pngHeight; y++) rgbPixels.copy(raw, y * (pngWidth * 3 + 1) + 1, y * pngWidth * 3, (y + 1) * pngWidth * 3);
const ihdr = Buffer.alloc(13);
ihdr.writeUInt32BE(pngWidth, 0); ihdr.writeUInt32BE(pngHeight, 4); ihdr[8] = 8; ihdr[9] = 2;
const png = Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw, { level: 7 })), chunk('IEND', Buffer.alloc(0))]);
fs.writeFileSync(pngPath, png);

console.log(`Wrote ${outPath}`);
console.log(`Wrote ${pngPath}`);
console.log(`faces=${selectedFaces.length}; vertices=${vertices.length - 1}; visible objects=${new Set(selectedFaces.map(f => f.objectName)).size}; cutaway=${cutaway}; M2 camera=${m2Camera}; ${m2Camera ? `visible point pivots=${visiblePointCount}/${pointMap.points.length}; ` : ''}bounds=X[-3550,3550] Y[-2875.106,2750] Z[-110,1859.072] cm`);
if (m2Camera) console.log(`Depth-proxy point pivots: visible=[${depthVisibleIds.join(',')}]; covered=[${depthHiddenIds.join(',')}]`);
