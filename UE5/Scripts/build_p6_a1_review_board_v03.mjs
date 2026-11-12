import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const p6Root = path.join(repoRoot, 'UE5/SourceAssets/P6');
const envDir = path.join(p6Root, 'Environment/A1_CourtyardKit');
const modelDir = path.join(envDir, 'Models');
const image = file => `data:image/png;base64,${fs.readFileSync(file).toString('base64')}`;
const esc = value => String(value).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');

function panel({x,y,w,h,title,subtitle,file,ix,iy,iw,ih}) {
  return `<g><rect x="${x}" y="${y}" width="${w}" height="${h}" rx="20" fill="#223347" stroke="#728397" stroke-width="2"/>
    <text x="${x+24}" y="${y+36}" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="21" font-weight="700">${esc(title)}</text>
    <text x="${x+24}" y="${y+61}" fill="#B9C6D1" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="13">${esc(subtitle)}</text>
    <image href="${image(file)}" x="${ix}" y="${iy}" width="${iw}" height="${ih}" preserveAspectRatio="xMidYMid meet"/></g>`;
}

const mtlText = fs.readFileSync(path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v03.mtl'), 'utf8');
const materials = [];
let current = null;
for (const line of mtlText.split(/\r?\n/)) {
  const nm = line.match(/^newmtl\s+(\S+)/);
  if (nm) { current = {name:nm[1], kd:[.5,.5,.5]}; materials.push(current); continue; }
  const kd = line.match(/^Kd\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)/);
  if (kd && current) current.kd = kd.slice(1).map(Number);
}
const swatches = materials.map((mat,i) => {
  const col=i%8,row=Math.floor(i/8),x=82+col*300,y=3330+row*46;
  const rgb=mat.kd.map(v=>Math.round(v*255));
  const hex=`#${rgb.map(v=>v.toString(16).padStart(2,'0')).join('').toUpperCase()}`;
  return `<g><rect x="${x}" y="${y}" width="34" height="34" rx="6" fill="${hex}" stroke="#F1E8D8" stroke-opacity=".45"/><text x="${x+44}" y="${y+15}" fill="#F2EADA" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="12" font-weight="700">${esc(mat.name)}</text><text x="${x+44}" y="${y+30}" fill="#B8C6D1" font-family="Segoe UI,sans-serif" font-size="10">${hex}</text></g>`;
}).join('\n');

const panels = [
  panel({x:60,y:138,w:1060,h:820,title:'12 类模块 · 造型方向参考',subtitle:'方向板仍只作比例/风格对照，不是模型贴图来源',file:path.join(envDir,'P6_A1_EnvironmentKit_Concept_v02.png'),ix:94,iy:210,iw:992,ih:716}),
  panel({x:1150,y:138,w:1350,h:820,title:'精细化灰盒 v03 · 完整屋顶',subtitle:'1,476 组 · 52,904 顶点 · 47,938 面 · 38 色；保留原 M2 锚点',file:path.join(envDir,'P6_A1_CourtyardKit_Blockout_Preview_v03.png'),ix:1200,iy:210,iw:1250,ih:710}),
  panel({x:60,y:982,w:1200,h:850,title:'当前 M2 相机公式 · 14 点锚点对照',subtitle:'离线 pivot 深度代理仍为 4/14；只作热点提示，不是 UE 可见性验收',file:path.join(envDir,'P6_A1_M2CameraPreview_v03.png'),ix:94,iy:1060,iw:1132,ih:680}),
  panel({x:1290,y:982,w:1210,h:850,title:'隐藏屋顶对照 · 仅供检查院内关系',subtitle:'临时剖切未改关卡屋顶；用于查看铺地与道具层次',file:path.join(envDir,'P6_A1_CourtyardKit_CutawayPreview_v03.png'),ix:1330,iy:1060,iw:1130,ih:720}),
  panel({x:60,y:1852,w:1200,h:850,title:'房屋立面近景 · 临时隐藏屋面',subtitle:'门扇分格、环形门扣、壁灯、百叶格栅、石窗台与错缝石板',file:path.join(envDir,'P6_A1_HouseDetailPreview_v03.png'),ix:94,iy:1930,iw:1132,ih:720}),
  panel({x:1290,y:1852,w:1210,h:850,title:'戏台构造近景 · 临时隐藏屋面',subtitle:'帘褶/金色帘头、两侧壁灯、木地板拼缝、台阶鼻口和栏杆帽',file:path.join(envDir,'P6_A1_StageDetailPreview_v03.png'),ix:1330,iy:1930,iw:1130,ih:720}),
].join('\n');

const propDetails = [
  {title:'相机桌 · 镜头/机身',file:'P6_A1_PhotoTableDetailPreview_v03.png'},
  {title:'公告栏 · 纸页/压签',file:'P6_A1_NoticeboardDetailPreview_v03.png'},
  {title:'检测仪 · 刻度/指针',file:'P6_A1_DetectorDetailPreview_v03.png'},
  {title:'井 · 木桶/井口',file:'P6_A1_WellDetailPreview_v03.png'},
  {title:'奖杯 · 杯沿/底座',file:'P6_A1_TrophyDetailPreview_v03.png'},
].map(({title,file},i)=>{const x=82+i*484;return `<g><text x="${x}" y="2760" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15" font-weight="700">${esc(title)}</text><image href="${image(path.join(envDir,file))}" x="${x}" y="2780" width="452" height="392" preserveAspectRatio="xMidYMid meet"/></g>`}).join('\n');

const board = `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="2560" height="3700" viewBox="0 0 2560 3700">
  <title>P6 A1 courtyard kit refined blockout v03 review</title>
  <desc>High-detail editable OBJ/MTL source blockout review sample. The original v01 and all 14 M2 XY point anchors are preserved; engine import, collision, UV, LOD, fixed-camera and walkability acceptance remain pending.</desc>
  <defs><linearGradient id="bg" x2="0" y2="1"><stop stop-color="#172331"/><stop offset="1" stop-color="#263A4E"/></linearGradient></defs>
  <rect width="2560" height="3700" fill="url(#bg)"/>
  <text x="60" y="72" fill="#F7EEDF" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="36" font-weight="700">P6 A1 · 奶家大院精细化灰盒 v03</text>
  <text x="62" y="106" fill="#B8C6D1" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="16">保留 v01 源件与 14 点 M2 锚点 · 总览/M2/剖切/房屋与戏台近景/五个交互物细节 · 尚非 UE 场景验收</text>
  ${panels}
  <rect x="60" y="2720" width="2440" height="520" rx="18" fill="#223347" stroke="#728397" stroke-width="2"/>
  <text x="82" y="2750" fill="#B9C6D1" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="12">可编辑 OBJ/MTL 细节视图 · 全部是离线轴测投影</text>
  ${propDetails}
  <rect x="60" y="3260" width="2440" height="340" rx="18" fill="#223347" stroke="#728397" stroke-width="2"/>
  <text x="82" y="3298" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="19" font-weight="700">MTL 平涂色样 · ${materials.length} 色 · 石材采用低对比变体</text>
  ${swatches}
  <text x="60" y="3660" fill="#D6C6A6" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="14">边界：无 UV/贴图/碰撞/LOD；需进 UE 核实真实材质、视线、可走区域、点击面和 1080p/720p。离线深度代理完整屋顶 4/14、剖切 5/14。</text>
</svg>`;
fs.writeFileSync(path.join(envDir, 'P6_A1_ReviewBoard_v03.svg'), board, 'utf8');
console.log(`Wrote ${path.join(envDir, 'P6_A1_ReviewBoard_v03.svg')} with ${materials.length} material swatches.`);

