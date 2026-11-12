import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(scriptDir, '../..');
const p6Root = path.join(repoRoot, 'UE5/SourceAssets/P6');
const envDir = path.join(p6Root, 'Environment/A1_CourtyardKit');
const a4Dir = path.join(p6Root, 'InfoLayers/A4_LocalCandidates');
const png = file => `data:image/png;base64,${fs.readFileSync(file).toString('base64')}`;
const svg = file => `data:image/svg+xml;base64,${fs.readFileSync(file).toString('base64')}`;
const xml = value => String(value).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');

function panel({ x, y, width, height, title, subtitle, image, kind = 'png', imageX, imageY, imageW, imageH }) {
  const href = kind === 'svg' ? svg(image) : png(image);
  return `<g>
    <rect x="${x}" y="${y}" width="${width}" height="${height}" rx="22" fill="#223347" stroke="#728397" stroke-width="2"/>
    <text x="${x + 24}" y="${y + 36}" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="22" font-weight="700">${xml(title)}</text>
    <text x="${x + 24}" y="${y + 62}" fill="#B9C6D1" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="13">${xml(subtitle)}</text>
    <image href="${href}" x="${imageX}" y="${imageY}" width="${imageW}" height="${imageH}" preserveAspectRatio="xMidYMid meet"/>
  </g>`;
}

const mtlText = fs.readFileSync(path.join(envDir, 'Models/P6_A1_CourtyardKit_Blockout_v01.mtl'), 'utf8');
const materials = [];
let current = null;
for (const line of mtlText.split(/\r?\n/)) {
  const nm = line.match(/^newmtl\s+(\S+)/);
  if (nm) { current = { name: nm[1], kd: [0.5, 0.5, 0.5] }; materials.push(current); continue; }
  const kd = line.match(/^Kd\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)/);
  if (kd && current) current.kd = kd.slice(1).map(Number);
}
const a1PaletteStrip = materials.map((mat, i) => {
  const column = i % 8, row = Math.floor(i / 8);
  const x = 82 + column * 300, y = 1992 + row * 72;
  const rgb = mat.kd.map(v => Math.round(v * 255));
  const hex = `#${rgb.map(v => v.toString(16).padStart(2, '0')).join('').toUpperCase()}`;
  return `<g><rect x="${x}" y="${y}" width="42" height="42" rx="7" fill="${hex}" stroke="#F1E8D8" stroke-opacity=".45"/><text x="${x+54}" y="${y+17}" fill="#F2EADA" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="13" font-weight="700">${xml(mat.name)}</text><text x="${x+54}" y="${y+34}" fill="#B8C6D1" font-family="Segoe UI,sans-serif" font-size="11">${hex}</text></g>`;
}).join('\n');

const a1Items = [
  panel({x:60,y:138,width:1060,height:820,title:'12 类模块 · 外观方向板 v02',subtitle:'不透明暖灰底 / 仅作外观评审 / 不是生产贴图或模型',image:path.join(envDir,'P6_A1_EnvironmentKit_Concept_v02.png'),imageX:98,imageY:208,imageW:984,imageH:738}),
  panel({x:60,y:982,width:1060,height:936,title:'M2 正交相机公式投影 · 14 点锚点序号',subtitle:'Actor pivot 深度代理色环仅供坐标核对；不是 UE 遮挡或可读性结论',image:path.join(envDir,'P6_A1_M2CameraPreview_v01.png'),imageX:78,imageY:1060,imageW:1024,imageH:576}),
  panel({x:1150,y:138,width:1350,height:900,title:'完整屋顶 · 离线灰盒投影',subtitle:'112 个对象组 / 1,640 顶点 / 1,587 面 / 16 个预览材质',image:path.join(envDir,'P6_A1_CourtyardKit_Blockout_Preview_v01.png'),imageX:1240,imageY:214,imageW:1170,imageH:810}),
  panel({x:1150,y:1060,width:1350,height:858,title:'临时隐藏屋顶/檐口 · 院内对照',subtitle:'只用于讨论视线和屋顶遮挡；不是已批准的关卡改动',image:path.join(envDir,'P6_A1_CourtyardKit_CutawayPreview_v01.png'),imageX:1240,imageY:1136,imageW:1170,imageH:782}),
].join('\n');

const a1Board = `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="2560" height="2200" viewBox="0 0 2560 2200">
  <title>P6 A1 environment-kit review board</title>
  <desc>Self-contained board with embedded concept, full-roof, cutaway and M2 projection images. All items are review candidates; none is a runtime screenshot.</desc>
  <defs><linearGradient id="bg" x2="0" y2="1"><stop stop-color="#172331"/><stop offset="1" stop-color="#263A4E"/></linearGradient></defs>
  <rect width="2560" height="2200" fill="url(#bg)"/>
  <text x="60" y="72" fill="#F7EEDF" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="36" font-weight="700">P6 A1 · 奶家大院环境模块评审板</text>
  <text x="62" y="106" fill="#B8C6D1" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="16">直接对应 M2 点位候选 | 可编辑 OBJ/MTL 源块 | 四张内嵌图与 MTL 色样均可独立审阅</text>
  ${a1Items}
  <rect x="60" y="1940" width="2440" height="166" rx="20" fill="#223347" stroke="#728397" stroke-width="2"/>
  <text x="82" y="1972" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="20" font-weight="700">MTL 平涂预览 · ${materials.length} 色 · 由源文件 Kd 自动读取</text>
  ${a1PaletteStrip}
  <text x="60" y="2160" fill="#D6C6A6" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="14">状态：候选源稿。需在 UE 固定相机、真实 Actor/bounds、1080p/720p 及可达性中复核；方向板不是贴图来源，色样不代表 PBR/灯光结果。</text>
</svg>`;
fs.writeFileSync(path.join(envDir, 'P6_A1_ReviewBoard_v01.svg'), a1Board, 'utf8');

const layers = [
  ['S1 · 视线与遮挡', '扇区到墙沿自然截断', 'P6_A4_S1_Sightline_v01.svg', '#71859A'],
  ['S2 · 局部破损', '断口和少量碎片', 'P6_A4_S2_BrokenEdge_v01.svg', '#B28A58'],
  ['S3 · 短地面痕迹', '压痕与擦亮，不画抛物线', 'P6_A4_S3_ShortGroundTrace_v01.svg', '#788C9E'],
  ['S4 · 水痕与渐淡脚印', '低饱和水膜，局部贴地', 'P6_A4_S4_WaterAndFadingPrints_v01.svg', '#6E928D'],
];
const cards = layers.map(([title, note, file, accent], index) => {
  const x = index % 2 === 0 ? 40 : 1000;
  const y = index < 2 ? 130 : 670;
  const href = svg(path.join(a4Dir, file));
  return `<g>
    <rect x="${x}" y="${y}" width="880" height="500" rx="22" fill="#F0E8D8" stroke="#D4C8B2" stroke-width="2"/>
    <path d="M${x+22} ${y}h836q22 0 22 22v55H${x}v-55q0-22 22-22Z" fill="${accent}"/>
    <text x="${x+28}" y="${y+48}" fill="#FFF5E6" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="23" font-weight="700">${xml(title)}</text>
    <text x="${x+28}" y="${y+104}" fill="#536372" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15">${xml(note)}</text>
    <rect x="${x+72}" y="${y+132}" width="736" height="340" rx="18" fill="#D8D9D1"/>
    <image href="${href}" x="${x+90}" y="${y+136}" width="700" height="330" preserveAspectRatio="xMidYMid meet"/>
    <text x="${x+28}" y="${y+485}" fill="#6F7D86" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="12">1024×768 SVG · 透明底 · 独立开关源图</text>
  </g>`;
}).join('\n');

const a4Board = `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="1920" height="1280" viewBox="0 0 1920 1280">
  <title>P6 A4 local information-layer review board</title>
  <desc>Self-contained review board with four embedded, separate transparent vector sources. Review only; engine materials are not included.</desc>
  <defs><linearGradient id="bg" x2="0" y2="1"><stop stop-color="#172331"/><stop offset="1" stop-color="#263A4E"/></linearGradient></defs>
  <rect width="1920" height="1280" fill="url(#bg)"/>
  <text x="40" y="64" fill="#F7EEDF" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="34" font-weight="700">P6 A4 · 局部信息层单层源图</text>
  <text x="42" y="96" fill="#B8C6D1" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15">独立矢量 / 透明背景 / 低饱和局部提示 / 不提示角色或答案方向</text>
  ${cards}
  <text x="40" y="1227" fill="#D6C6A6" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="14">状态：评审候选。导出贴图、材质深度、排序、单层/叠层开关和 1080p/720p 仍需 UE 集成核验。</text>
</svg>`;
fs.writeFileSync(path.join(a4Dir, 'P6_A4_ReviewBoard_v01.svg'), a4Board, 'utf8');

const swatches = materials.map((mat, i) => {
  const column = i % 4, row = Math.floor(i / 4);
  const x = 50 + column * 375, y = 145 + row * 238;
  const rgb = mat.kd.map(v => Math.round(v * 255));
  const hex = `#${rgb.map(v => v.toString(16).padStart(2, '0')).join('').toUpperCase()}`;
  return `<g>
    <rect x="${x}" y="${y}" width="350" height="214" rx="18" fill="#F1E8D8" stroke="#CDC0A9" stroke-width="2"/>
    <rect x="${x+16}" y="${y+20}" width="132" height="174" rx="12" fill="${hex}" stroke="#263849" stroke-opacity=".3"/>
    <text x="${x+170}" y="${y+58}" fill="#283747" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="20" font-weight="700">${xml(mat.name)}</text>
    <text x="${x+170}" y="${y+98}" fill="#4D5A62" font-family="Segoe UI,sans-serif" font-size="17">${hex}</text>
    <text x="${x+170}" y="${y+130}" fill="#6C746F" font-family="Segoe UI,sans-serif" font-size="13">Kd ${mat.kd.map(v => v.toFixed(2)).join(' / ')}</text>
    <text x="${x+170}" y="${y+165}" fill="#8A8A7E" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="12">OBJ/MTL 平涂方向</text>
  </g>`;
}).join('\n');
const materialBoard = `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="1600" height="1160" viewBox="0 0 1600 1160">
  <title>P6 A1 MTL preview material palette</title>
  <desc>Sixteen flat color previews parsed from the current OBJ MTL. These are not final UE material settings.</desc>
  <defs><linearGradient id="matbg" x2="0" y2="1"><stop stop-color="#172331"/><stop offset="1" stop-color="#304255"/></linearGradient></defs>
  <rect width="1600" height="1160" fill="url(#matbg)"/>
  <text x="50" y="58" fill="#F7EEDF" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="32" font-weight="700">P6 A1 · OBJ/MTL 预览色样</text>
  <text x="52" y="92" fill="#B8C6D1" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="14">${materials.length} 个名称与 Kd 数值由源 MTL 自动读取；用于颜色方向，不代表 UE 的 PBR/灯光/色彩管理结果。</text>
  ${swatches}
  <text x="50" y="1132" fill="#D6C6A6" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="13">正式材质需要在 UE 内单独设置 Base Color、Roughness、Metallic、色彩空间与目标灯光。</text>
</svg>`;
fs.writeFileSync(path.join(envDir, 'P6_A1_MaterialPalette_v01.svg'), materialBoard, 'utf8');

const fullDepth = JSON.parse(fs.readFileSync(path.join(envDir, 'P6_A1_M2CameraDepthProxy_v01.json'), 'utf8'));
const cutawayDepth = JSON.parse(fs.readFileSync(path.join(envDir, 'P6_A1_M2CameraCutawayDepthProxy_v01.json'), 'utf8'));
const actorNamesZh = {Trophy:'奖杯',Noticeboard:'公告栏',Detector:'检测仪',PhotoTable:'相机桌',Flowers:'假花',PatrolLedger:'巡夜簿',NaiDou:'奶逗',NaiPai:'奶拍',NaiBi:'奶鼻',Socks:'袜子',TrashCan:'垃圾桶',RoofTiles:'瓦片堆',WellBucket:'井桶',Clothesline:'晾衣绳'};
const pointKindsZh = {evidence:'主线证据',witness:'口供 NPC',joke:'可跳过笑点'};
const alignment = JSON.parse(fs.readFileSync(path.join(p6Root,'Manifests/P6_A1_PointPropAlignment_v01.json'),'utf8'));
const prefixesById = new Map(alignment.entries.map(item=>[item.actor_id,item.object_group_prefixes||[]]));
const cutawayById = new Map(cutawayDepth.point_rows.map(item=>[item.id,item]));
const matrixRows = fullDepth.point_rows.map((row,index)=>{
  const cut = cutawayById.get(row.id);
  const own = group => !!group && (prefixesById.get(row.id)||[]).some(prefix=>group.startsWith(prefix));
  const fullOwn=own(row.nearest_surface_group), cutOwn=own(cut?.nearest_surface_group);
  let label, fill;
  if(row.visible_proxy && cut?.visible_proxy){label='pivot 像素均通过';fill='#31574F';}
  else if(!row.visible_proxy && !fullOwn && cutOwn){label='完整屋顶挡点 → 剖切落在自家几何';fill='#75563A';}
  else if(!row.visible_proxy && fullOwn && cut?.visible_proxy){label='完整视图落在自家几何';fill='#69563B';}
  else if(fullOwn && cutOwn){label='自身道具 / 低位 pivot';fill='#69563B';}
  else {label='空间遮挡候选';fill='#653E42';}
  const y=272+index*62;
  const fullGroup=row.visible_proxy?'未遮挡':(row.nearest_surface_group||'无表面');
  const cutGroup=cut?.visible_proxy?'未遮挡':(cut?.nearest_surface_group||'无表面');
  return `<g>
    <rect x="50" y="${y}" width="2100" height="56" rx="10" fill="${index%2?'#26394B':'#203245'}"/>
    <text x="72" y="${y+35}" fill="#B9C6D1" font-family="Segoe UI,sans-serif" font-size="15">${String(row.sequence).padStart(2,'0')}</text>
    <text x="120" y="${y+35}" fill="#F3EADA" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="16" font-weight="700">${xml(actorNamesZh[row.id]||row.id)}</text>
    <text x="240" y="${y+35}" fill="#B9C6D1" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="14">${xml(pointKindsZh[row.kind]||row.kind)}</text>
    <text x="460" y="${y+35}" fill="#E7D8BE" font-family="Consolas,Segoe UI,sans-serif" font-size="14">${xml(fullGroup)}</text>
    <text x="1060" y="${y+35}" fill="#E7D8BE" font-family="Consolas,Segoe UI,sans-serif" font-size="14">${xml(cutGroup)}</text>
    <rect x="1730" y="${y+10}" width="392" height="36" rx="10" fill="${fill}"/>
    <text x="1926" y="${y+34}" text-anchor="middle" fill="#F6EBDD" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="${label.length>16?11:14}" font-weight="700">${xml(label)}</text>
  </g>`;
}).join('\n');
const matrixSvg=`<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="2200" height="1280" viewBox="0 0 2200 1280">
  <title>P6 A1 M2 point depth-proxy occlusion matrix</title>
  <desc>Generated from the two software depth-proxy JSON files. Nearest surface groups are diagnostic candidates only and do not prove UE runtime occlusion.</desc>
  <defs><linearGradient id="matrixBg" x2="0" y2="1"><stop stop-color="#172331"/><stop offset="1" stop-color="#263A4E"/></linearGradient></defs>
  <rect width="2200" height="1280" fill="url(#matrixBg)"/>
  <text x="50" y="60" fill="#F7EEDF" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="32" font-weight="700">P6 A1 · M2 点位深度代理遮挡矩阵</text>
  <text x="52" y="94" fill="#B8C6D1" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15">1920×1080 当前相机公式 | 完整屋顶 4/14 pivot 通过 · 临时剖切 5/14 | 单像素最近表面诊断，不是 UE 可见性或模型轮廓验收</text>
  <rect x="50" y="176" width="2100" height="72" rx="12" fill="#344B61"/>
  <text x="72" y="220" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15" font-weight="700">序号</text>
  <text x="120" y="220" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15" font-weight="700">点位</text>
  <text x="240" y="220" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15" font-weight="700">类别</text>
  <text x="460" y="220" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15" font-weight="700">完整屋顶 · 最近表面组</text>
  <text x="1060" y="220" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15" font-weight="700">隐藏屋顶 · 最近表面组</text>
  <text x="1730" y="220" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15" font-weight="700">快速判读</text>
  ${matrixRows}
  <text x="52" y="1208" fill="#D6C6A6" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="14">最近表面可能属于点位自身道具（奖杯脚、花盆、桌面、桶盖、井水）；请结合 actor bounds、billboard 轮廓和点击面在 UE 固定镜头中复核。剖切视图仅作诊断参考。</text>
</svg>`;
fs.writeFileSync(path.join(envDir,'P6_A1_M2OcclusionMatrix_v01.svg'),matrixSvg,'utf8');

const milkFrogDir = path.join(p6Root, 'References/MilkFrogStudy');
const portfolioW = 3600, portfolioH = 2500, cardW = 1120, cardH = 1050;
const portfolioPanels = [
  panel({x:60,y:140,width:cardW,height:cardH,title:'A0 · 已通过场景概念',subtitle:'院落视觉基线；场景原图保持不变',image:path.join(p6Root,'Environment/P6_A0_Courtyard_Concept_v01.png'),imageX:82,imageY:244,imageW:1076,imageH:900}),
  panel({x:1240,y:140,width:cardW,height:cardH,title:'A1 · 环境 kit 与 M2 镜头',subtitle:'112 个 OBJ 组 / 16 色平涂样 / 14 点离线相机审阅',image:path.join(envDir,'P6_A1_ReviewBoard_v01.svg'),kind:'svg',imageX:1260,imageY:244,imageW:1080,imageH:900}),
  panel({x:2420,y:140,width:cardW,height:cardH,title:'A3 · 12 张线索卡',subtitle:'G1–G6 / S1–S4 / X1–X2；评审样片',image:path.join(p6Root,'Evidence/P6_A3_ClueCard_ContactSheet_v01.png'),imageX:2440,imageY:244,imageW:1080,imageH:900}),
  panel({x:60,y:1270,width:cardW,height:cardH,title:'A4 · 四类局部信息层',subtitle:'四张独立透明 SVG 源图；未接 UE 材质',image:path.join(a4Dir,'P6_A4_ReviewBoard_v01.svg'),kind:'svg',imageX:80,imageY:1374,imageW:1080,imageH:900}),
  panel({x:2420,y:1270,width:cardW,height:cardH,title:'A5 · 调查 UI 状态',subtitle:'HUD / 教程 / 未成立与结案反馈；目标视觉稿',image:path.join(p6Root,'UI/P6_A5_ScreenStateBoard_v01.png'),imageX:2440,imageY:1374,imageW:1080,imageH:900}),
].join('\n');
const neutralCrop = png(path.join(milkFrogDir,'P6_REF_MF_PDF01_p110_046_Neutral.png'));
const reactionCrop = png(path.join(milkFrogDir,'P6_REF_MF_PDF01_p110_037_Curious.png'));
const a2Panel = `<g>
  <rect x="1240" y="1270" width="1120" height="1050" rx="22" fill="#223347" stroke="#728397" stroke-width="2"/>
  <text x="1264" y="1306" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="22" font-weight="700">A2 · 直接采用奶蛙图录原图</text>
  <text x="1264" y="1332" fill="#B9C6D1" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="13">图录 046 / 037 原图裁切；三名口供 NPC 共用同一形象</text>
  <rect x="1280" y="1360" width="490" height="720" rx="18" fill="#F8F0E2"/>
  <rect x="1830" y="1360" width="490" height="720" rx="18" fill="#F8F0E2"/>
  <image href="${neutralCrop}" x="1328" y="1430" width="394" height="426" preserveAspectRatio="xMidYMid meet"/>
  <image href="${reactionCrop}" x="1878" y="1430" width="394" height="426" preserveAspectRatio="xMidYMid meet"/>
  <text x="1525" y="1950" text-anchor="middle" fill="#29394A" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="21" font-weight="700">046 · 闲立微笑</text>
  <text x="2075" y="1950" text-anchor="middle" fill="#29394A" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="21" font-weight="700">037 · 好奇询问</text>
  <text x="1525" y="1982" text-anchor="middle" fill="#65727A" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15">共用中性态候选</text>
  <text x="2075" y="1982" text-anchor="middle" fill="#65727A" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15">共用轻反应候选</text>
  <text x="1264" y="2160" fill="#D6C6A6" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="13">保留原图白底；未重绘、去底或调色。图录没有提供三种独立嫌疑人外形。</text>
</g>`;
const portfolio = `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="${portfolioW}" height="${portfolioH}" viewBox="0 0 ${portfolioW} ${portfolioH}">
  <title>P6 art review portfolio A0–A5</title>
  <desc>Self-contained visual overview. It embeds project review samples and direct appendix crops. Only the A0 courtyard concept is approved; other panels are candidates and not runtime screenshots.</desc>
  <defs><linearGradient id="portfolioBg" x2="0" y2="1"><stop stop-color="#172331"/><stop offset="1" stop-color="#263A4E"/></linearGradient></defs>
  <rect width="${portfolioW}" height="${portfolioH}" fill="url(#portfolioBg)"/>
  <text x="60" y="64" fill="#F7EEDF" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="38" font-weight="700">P6 · 美术侧评审总览 A0–A5</text>
  <text x="62" y="100" fill="#B8C6D1" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="16">5-1「禁笑令之夜」 | A0 场景概念已通过；A1–A5 为可追溯视觉候选，尚未完成 UE 导入和运行时验收</text>
  ${portfolioPanels}
  ${a2Panel}
  <text x="60" y="2390" fill="#D6C6A6" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15">下一关口：固定镜头 1080p/720p、真实 Actor bounds 与点击面、可达性、billboard 脚底、信息层材质/排序以及调查 UI 集成。</text>
</svg>`;
fs.writeFileSync(path.join(p6Root, 'P6_ArtReviewPortfolio_v01.svg'), portfolio, 'utf8');

console.log(`Wrote P6 A0-A5 review portfolio, A1/A4 boards, A1 occlusion matrix and MTL palette (${materials.length} colors).`);
