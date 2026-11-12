import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const p6Root = path.join(repoRoot, 'UE5/SourceAssets/P6');
const envDir = path.join(p6Root, 'Environment/A1_CourtyardKit');
const npcDir = path.join(p6Root, 'Characters/NPC');
const refDir = path.join(p6Root, 'References/MilkFrogStudy');
const a1Capture = path.join(repoRoot, 'UE5/Records/p6_art/evidence/P6_A1_M2LightingReview_1920x1080_v11.png');
const png = file => `data:image/png;base64,${fs.readFileSync(file).toString('base64')}`;
const svg = file => `data:image/svg+xml;base64,${fs.readFileSync(file).toString('base64')}`;
const xml = value => String(value).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
const sha = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');

function panel({x,y,w,h,title,subtitle,file,kind='png',ix,iy,iw,ih}) {
  const href = kind === 'svg' ? svg(file) : png(file);
  return `<g><rect x="${x}" y="${y}" width="${w}" height="${h}" rx="22" fill="#223347" stroke="#728397" stroke-width="2"/>
    <text x="${x+24}" y="${y+36}" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="22" font-weight="700">${xml(title)}</text>
    <text x="${x+24}" y="${y+62}" fill="#B9C6D1" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="13">${xml(subtitle)}</text>
    <image href="${href}" x="${ix}" y="${iy}" width="${iw}" height="${ih}" preserveAspectRatio="xMidYMid meet"/></g>`;
}

const W=3600,H=2500,cardW=1120,cardH=1050;
const panels = [
  panel({x:60,y:140,w:cardW,h:cardH,title:'A0 · 已通过场景概念',subtitle:'院落视觉基线；原图保持不变',file:path.join(p6Root,'Environment/P6_A0_Courtyard_Concept_v01.png'),ix:82,iy:244,iw:1076,ih:900}),
  panel({x:1240,y:140,w:cardW,h:cardH,title:'A1 · M2 固定机位首轮审阅 v11',subtitle:'UE 截图与当前 M2 相机一致；离线 pivot 代理 4/14，灯光色彩待调',file:a1Capture,ix:1260,iy:244,iw:1080,ih:900}),
  panel({x:2420,y:140,w:cardW,h:cardH,title:'A3 · 12 张线索卡',subtitle:'G1–G6 / S1–S4 / X1–X2；评审样片',file:path.join(p6Root,'Evidence/P6_A3_ClueCard_ContactSheet_v01.png'),ix:2440,iy:244,iw:1080,ih:900}),
  panel({x:60,y:1270,w:cardW,h:cardH,title:'A4 · 四类局部信息层',subtitle:'四张独立透明 SVG 源图；未接 UE 材质',file:path.join(p6Root,'InfoLayers/A4_LocalCandidates/P6_A4_ReviewBoard_v01.svg'),kind:'svg',ix:80,iy:1374,iw:1080,ih:900}),
  panel({x:2420,y:1270,w:cardW,h:cardH,title:'A5 · 调查 UI 状态',subtitle:'HUD / 教程 / 未成立与结案反馈；目标视觉稿',file:path.join(p6Root,'UI/P6_A5_ScreenStateBoard_v01.png'),ix:2440,iy:1374,iw:1080,ih:900}),
].join('\n');

const neutral=png(path.join(refDir,'P6_REF_MF_PDF01_p110_046_Neutral.png'));
const reaction=png(path.join(refDir,'P6_REF_MF_PDF01_p110_037_Curious.png'));
const a2=`<g>
  <rect x="1240" y="1270" width="1120" height="1050" rx="22" fill="#223347" stroke="#728397" stroke-width="2"/>
  <text x="1264" y="1306" fill="#F5EBD8" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="22" font-weight="700">A2 · 人物图像选型已通过</text>
  <text x="1264" y="1332" fill="#B9C6D1" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="13">046 / 037 供三名口供 NPC 共用 · UE billboards 仍待集成验收</text>
  <rect x="1280" y="1360" width="490" height="720" rx="18" fill="#F8F0E2"/><rect x="1830" y="1360" width="490" height="720" rx="18" fill="#F8F0E2"/>
  <image href="${neutral}" x="1328" y="1430" width="394" height="426" preserveAspectRatio="xMidYMid meet"/><image href="${reaction}" x="1878" y="1430" width="394" height="426" preserveAspectRatio="xMidYMid meet"/>
  <text x="1525" y="1950" text-anchor="middle" fill="#29394A" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="21" font-weight="700">046 · 闲立微笑</text>
  <text x="2075" y="1950" text-anchor="middle" fill="#29394A" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="21" font-weight="700">037 · 好奇询问</text>
  <text x="1525" y="1982" text-anchor="middle" fill="#65727A" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15">共用中性态 · 选型通过</text><text x="2075" y="1982" text-anchor="middle" fill="#65727A" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15">共用轻反应 · 选型通过</text>
  <text x="1264" y="2160" fill="#D6C6A6" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="13">保留原图白底；不代表三种独立嫌疑人造型，也不是 UE 运行截图。</text>
</g>`;

const portfolio=`<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}" viewBox="0 0 ${W} ${H}">
 <title>P6 art review portfolio A0-A5 v04</title>
 <desc>Updated self-contained visual overview with refined A1 source blockout and approved A2 image selection. Remaining panels are review samples; the board is not a runtime screenshot.</desc>
 <defs><linearGradient id="bg" x2="0" y2="1"><stop stop-color="#172331"/><stop offset="1" stop-color="#263A4E"/></linearGradient></defs><rect width="${W}" height="${H}" fill="url(#bg)"/>
 <text x="60" y="64" fill="#F7EEDF" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="38" font-weight="700">P6 · 美术侧评审总览 A0–A5 v04</text>
 <text x="62" y="100" fill="#B8C6D1" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="16">5-1「禁笑令之夜」 | A0 场景概念、A2 图像选型已通过；A1 已按 M2 固定机位采图，遮挡和材质/灯光待调整；A3–A5 为评审样片</text>
 ${panels}${a2}
 <text x="60" y="2390" fill="#D6C6A6" font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15">下一关口：调整屋顶/树冠遮挡与夜景光色，再验 14 点真实轮廓、Actor 点击面、可达性、billboard 脚底及其余 A3–A5 集成。</text>
</svg>`;
const out=path.join(p6Root,'P6_ArtReviewPortfolio_v04.svg');
fs.writeFileSync(out,portfolio,'utf8');
const idx={
  schema_version:'1.0',package_id:'P6_ArtReviewPortfolio_v04',status:'self_contained_review_overview_not_runtime_approved',asset:'P6_ArtReviewPortfolio_v04.svg',dimensions_px:[W,H],
  embedded_visuals:['Environment/P6_A0_Courtyard_Concept_v01.png','UE5/Records/p6_art/evidence/P6_A1_M2LightingReview_1920x1080_v11.png','References/MilkFrogStudy/P6_REF_MF_PDF01_p110_046_Neutral.png','References/MilkFrogStudy/P6_REF_MF_PDF01_p110_037_Curious.png','Evidence/P6_A3_ClueCard_ContactSheet_v01.png','InfoLayers/A4_LocalCandidates/P6_A4_ReviewBoard_v01.svg','UI/P6_A5_ScreenStateBoard_v01.png'],
  source_manifests:['Manifests/P6_A0_AssetIndex_v02.json','Manifests/P6_A1_CourtyardKit_Index_v04.json','Manifests/P6_A2_SuspectArt_Index_v01.json','Manifests/P6_A3_ClueCard_Index_v01.json','Manifests/P6_A4_LocalLayer_Index_v01.json','Manifests/P6_A5_UIScreen_Index_v01.json'],
  embedded_image_count:7,external_image_references:0,approval_boundary:'A0 scene concept and the A2 selection of figures 046/037 as shared witness art were approved. A1 shows an actual UE capture at the current M2 camera in an isolated review map; flat-color graybox materials, roof occlusion, 14-point visibility, click surfaces, walkability and runtime acceptance remain pending. A3-A5 remain review samples.',evidence:['UE5/Records/p6_art/evidence/P6_A1_M2LightingReviewCapture_v11.json','UE5/Records/p6_art/evidence/P6_A1_M2LightingReviewMap_v06.json','UE5/Records/p6_art/evidence/P6_A1_M2VisibilityReview_v01.svg','UE5/Records/p6_art/evidence/P6_A1_LightingReviewCapture_v10.json'],sha256:sha(out),generator:'UE5/Scripts/build_p6_art_review_portfolio_v04.mjs',generator_sha256:sha(fileURLToPath(import.meta.url))
};
fs.writeFileSync(path.join(p6Root,'Manifests/P6_ArtReviewPortfolio_Index_v04.json'),`${JSON.stringify(idx,null,2)}\n`,'utf8');
console.log(`Wrote ${out} and index; ${idx.embedded_image_count} embedded visuals; sha256=${idx.sha256}`);

