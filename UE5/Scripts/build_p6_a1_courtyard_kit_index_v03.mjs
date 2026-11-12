import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const p6Root = path.join(repoRoot, 'UE5/SourceAssets/P6');
const kitRoot = path.join(p6Root, 'Environment/A1_CourtyardKit');
const objPath = path.join(kitRoot, 'Models/P6_A1_CourtyardKit_Blockout_v03.obj');
const mtlPath = path.join(kitRoot, 'Models/P6_A1_CourtyardKit_Blockout_v03.mtl');
const sha = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const read = file => fs.readFileSync(file, 'utf8');

function parseObj(file) {
  const lines = read(file).split(/\r?\n/);
  const groups = lines.filter(line => line.startsWith('o ')).map(line => line.slice(2));
  const vertices = [];
  const groupFaces = new Map();
  const usedMaterials = new Set();
  let activeGroup = '', faces = 0;
  for(const line of lines){
    if(line.startsWith('o ')){activeGroup=line.slice(2);groupFaces.set(activeGroup,0);continue;}
    if(line.startsWith('v ')){vertices.push(line.slice(2).trim().split(/\s+/).map(Number));continue;}
    if(line.startsWith('usemtl ')){usedMaterials.add(line.slice(7).trim());continue;}
    if(line.startsWith('f ')){
      const ids=line.slice(2).trim().split(/\s+/).map(token=>Number(token.split('/')[0]));
      if(ids.length<3||ids.some(id=>!Number.isInteger(id)||id<1||id>vertices.length))throw new Error(`Invalid OBJ face reference in ${file}: ${line}`);
      faces++;
      if(groupFaces.has(activeGroup))groupFaces.set(activeGroup,groupFaces.get(activeGroup)+1);
    }
  }
  const emptyGroups=[...groupFaces].filter(([,count])=>count===0).map(([name])=>name);
  if(emptyGroups.length)throw new Error(`Empty object groups in ${file}: ${emptyGroups.join(', ')}`);
  const bounds = [0,1,2].map(axis => [Math.min(...vertices.map(v => v[axis])), Math.max(...vertices.map(v => v[axis]))]);
  return {lines, groups, vertices, faces, bounds, usedMaterials};
}

const model = parseObj(objPath);
const original = parseObj(path.join(kitRoot, 'Models/P6_A1_CourtyardKit_Blockout_v01.obj'));
const originalGroupsPreserved = original.groups.filter(name => model.groups.includes(name));
const pavingStoneCount = model.groups.filter(name => /^P6_PavingStone_Course_/.test(name)).length;
const stageDeckSeamCount = model.groups.filter(name => /^P6_Stage_WoodDeck_Seam_/.test(name)).length;
const framedLanternCount = model.groups.filter(name => /^P6_(House|Stage)_Lantern_(Left|Right)_MountPlate$/.test(name)).length;
const photoTableDetailGroupCount = model.groups.filter(name => /^P6_PhotoTable_(Camera_(LensPlate|Screw_|FocusRing|ShutterRelease|SideDial|Strap)|Cloth_)/.test(name)).length;
const materials = (read(mtlPath).match(/^newmtl\s+/gm) || []).length;
const materialNames = new Set(read(mtlPath).split(/\r?\n/).filter(line=>line.startsWith('newmtl ')).map(line=>line.slice(7).trim()));
const undefinedMaterials=[...model.usedMaterials].filter(name=>!materialNames.has(name));
if(undefinedMaterials.length)throw new Error(`Undefined MTL materials: ${undefinedMaterials.join(', ')}`);
const pointMapPath = path.join(p6Root, 'Manifests/P6_A1_CurrentPointArtMap_v01.json');
const pointMap = JSON.parse(read(pointMapPath));
const fullDepth = JSON.parse(read(path.join(kitRoot, 'P6_A1_M2CameraDepthProxy_v03.json')));
const cutDepth = JSON.parse(read(path.join(kitRoot, 'P6_A1_M2CameraCutawayDepthProxy_v03.json')));
if (originalGroupsPreserved.length !== original.groups.length) throw new Error('Some v01 object groups are missing from v03.');
if (pointMap.points.length !== 14) throw new Error(`Expected 14 M2 anchors; found ${pointMap.points.length}.`);
if (materials !== 38) throw new Error(`Expected 38 materials; found ${materials}.`);
if (framedLanternCount !== 4) throw new Error(`Expected 4 framed lanterns; found ${framedLanternCount}.`);

const assets = [
  ['P6_A1_KIT_V03_OBJ','Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_Blockout_v03.obj','editable_detailed_source_blockout'],
  ['P6_A1_KIT_V03_MTL','Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_Blockout_v03.mtl','flat_preview_materials_not_final'],
  ['P6_A1_KIT_V03_PREVIEW_PNG','Environment/A1_CourtyardKit/P6_A1_CourtyardKit_Blockout_Preview_v03.png','offline_axonometric_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_PREVIEW_SVG','Environment/A1_CourtyardKit/P6_A1_CourtyardKit_Blockout_Preview_v03.svg','editable_offline_axonometric_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_CUTAWAY_PNG','Environment/A1_CourtyardKit/P6_A1_CourtyardKit_CutawayPreview_v03.png','offline_cutaway_projection_not_approved_level_change',[1600,1120]],
  ['P6_A1_KIT_V03_CUTAWAY_SVG','Environment/A1_CourtyardKit/P6_A1_CourtyardKit_CutawayPreview_v03.svg','editable_offline_cutaway_projection_not_approved_level_change',[1600,1120]],
  ['P6_A1_KIT_V03_HOUSE_DETAIL_PNG','Environment/A1_CourtyardKit/P6_A1_HouseDetailPreview_v03.png','offline_house_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_HOUSE_DETAIL_SVG','Environment/A1_CourtyardKit/P6_A1_HouseDetailPreview_v03.svg','editable_offline_house_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_STAGE_DETAIL_PNG','Environment/A1_CourtyardKit/P6_A1_StageDetailPreview_v03.png','offline_stage_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_STAGE_DETAIL_SVG','Environment/A1_CourtyardKit/P6_A1_StageDetailPreview_v03.svg','editable_offline_stage_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_PHOTOTABLE_DETAIL_PNG','Environment/A1_CourtyardKit/P6_A1_PhotoTableDetailPreview_v03.png','offline_phototable_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_PHOTOTABLE_DETAIL_SVG','Environment/A1_CourtyardKit/P6_A1_PhotoTableDetailPreview_v03.svg','editable_offline_phototable_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_NOTICEBOARD_DETAIL_PNG','Environment/A1_CourtyardKit/P6_A1_NoticeboardDetailPreview_v03.png','offline_noticeboard_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_NOTICEBOARD_DETAIL_SVG','Environment/A1_CourtyardKit/P6_A1_NoticeboardDetailPreview_v03.svg','editable_offline_noticeboard_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_DETECTOR_DETAIL_PNG','Environment/A1_CourtyardKit/P6_A1_DetectorDetailPreview_v03.png','offline_detector_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_DETECTOR_DETAIL_SVG','Environment/A1_CourtyardKit/P6_A1_DetectorDetailPreview_v03.svg','editable_offline_detector_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_WELL_DETAIL_PNG','Environment/A1_CourtyardKit/P6_A1_WellDetailPreview_v03.png','offline_well_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_WELL_DETAIL_SVG','Environment/A1_CourtyardKit/P6_A1_WellDetailPreview_v03.svg','editable_offline_well_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_TROPHY_DETAIL_PNG','Environment/A1_CourtyardKit/P6_A1_TrophyDetailPreview_v03.png','offline_trophy_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_TROPHY_DETAIL_SVG','Environment/A1_CourtyardKit/P6_A1_TrophyDetailPreview_v03.svg','editable_offline_trophy_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V03_M2_PNG','Environment/A1_CourtyardKit/P6_A1_M2CameraPreview_v03.png','offline_exact_m2_projection_and_pivot_depth_proxy',[1920,1080]],
  ['P6_A1_KIT_V03_M2_SVG','Environment/A1_CourtyardKit/P6_A1_M2CameraPreview_v03.svg','editable_offline_exact_m2_projection_not_runtime_visibility',[1920,1080]],
  ['P6_A1_KIT_V03_M2_DEPTH','Environment/A1_CourtyardKit/P6_A1_M2CameraDepthProxy_v03.json','offline_per_point_nearest_surface_proxy_not_ue_occlusion'],
  ['P6_A1_KIT_V03_M2_CUTAWAY_PNG','Environment/A1_CourtyardKit/P6_A1_M2CameraCutawayPreview_v03.png','offline_exact_m2_cutaway_projection_not_approved_level_change',[1920,1080]],
  ['P6_A1_KIT_V03_M2_CUTAWAY_SVG','Environment/A1_CourtyardKit/P6_A1_M2CameraCutawayPreview_v03.svg','editable_offline_exact_m2_cutaway_projection_not_runtime_visibility',[1920,1080]],
  ['P6_A1_KIT_V03_M2_CUTAWAY_DEPTH','Environment/A1_CourtyardKit/P6_A1_M2CameraCutawayDepthProxy_v03.json','offline_cutaway_per_point_nearest_surface_proxy_not_ue_occlusion'],
  ['P6_A1_KIT_V03_REVIEW_BOARD','Environment/A1_CourtyardKit/P6_A1_ReviewBoard_v03.svg','self_contained_visual_review_board_not_runtime_screenshot',[2560,3700]],
  ['P6_A1_KIT_V03_HANDOFF','Environment/A1_CourtyardKit/P6_A1_CourtyardKit_Handoff_v03.md','artist_handoff'],
  ['P6_A1_KIT_V03_BUILD_SCRIPT','../../Scripts/build_p6_a1_courtyard_kit_v03.mjs','source_generation_script'],
  ['P6_A1_KIT_V03_PREVIEW_SCRIPT','../../Scripts/render_p6_a1_courtyard_preview_v03.mjs','preview_generation_script'],
  ['P6_A1_KIT_V03_BOARD_SCRIPT','../../Scripts/build_p6_a1_review_board_v03.mjs','review_board_generation_script'],
  ['P6_A1_KIT_V03_INDEX_SCRIPT','../../Scripts/build_p6_a1_courtyard_kit_index_v03.mjs','index_generation_script']
].map(([asset_id,assetPath,type,dimensions_px]) => {
  const absolute = path.resolve(p6Root, assetPath);
  if (!fs.existsSync(absolute)) throw new Error(`Missing indexed asset: ${assetPath}`);
  return {asset_id, path: assetPath, type, ...(dimensions_px ? {dimensions_px} : {}), sha256: sha(absolute)};
});

const manifest = {
  schema_version: '1.0',
  package_id: 'P6_A1_CourtyardKit_Blockout_v03',
  display_level: '5-1',
  case_title: '禁笑令之夜',
  status: 'detailed_source_blockout_candidate_not_engine_approved',
  source_basis: {
    preserved_v01_obj: {path:'Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_Blockout_v01.obj',sha256:sha(path.join(kitRoot,'Models/P6_A1_CourtyardKit_Blockout_v01.obj'))},
    preserved_v01_mtl: {path:'Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_Blockout_v01.mtl',sha256:sha(path.join(kitRoot,'Models/P6_A1_CourtyardKit_Blockout_v01.mtl'))},
    current_m2_point_map: 'Manifests/P6_A1_CurrentPointArtMap_v01.json',
    m2_point_count: pointMap.points.length,
    style_reference: 'Environment/A1_CourtyardKit/P6_A1_EnvironmentKit_Concept_v02.png',
    v01_source_preserved: true,
    v01_object_groups: original.groups.length,
    v01_object_groups_preserved_in_v03: originalGroupsPreserved.length
  },
  geometry: {
    units: 'centimeters',
    axis_semantics: {x:'courtyard long axis',y:'courtyard depth',z:'height'},
    object_group_count: model.groups.length,
    vertex_count: model.vertices.length,
    face_count: model.faces,
    bounds_cm: {x:model.bounds[0],y:model.bounds[1],z:model.bounds[2]},
    material_count: materials,
    material_file_is_final: false,
    uvs_generated: false,
    collision_generated: false,
    lods_generated: false,
    detail_counts: {courtyard_flagstones:pavingStoneCount, stage_deck_seams:stageDeckSeamCount, framed_lanterns:framedLanternCount, phototable_detail_groups:photoTableDetailGroupCount, original_v01_groups_preserved:originalGroupsPreserved.length}
  },
  refinements: [
    '825 offset, lightly irregular beveled flagstones with a small set of hairline chips.',
    'Per-tile low-contrast ceramic variation, upturned eave ends and ceramic ridge details.',
    'Raised door panels, ring knockers, window sill drips, east-side lattice vents, shutters and sill course.',
    'Tree limb structure, layered canopy and small broadleaf planting at wall and stage junctions.',
    'Four framed warm lanterns at the house facade and stage posts; stage and house closeups temporarily hide roof groups to expose facade detail.',
    'Photo evidence camera gains a screwed lens plate, inset focus ring, top shutter, side dial/strap and a stitched cream table runner with red hem.',
    'Bucket staves/iron hoops, well rim cap and water ripples, incised jar bands, stage stair nosings and railing caps, detector dial graduations/needle and noticeboard sheets/pins.',
    'Subtle stone tone variation on house bases and wall masonry courses, plus 11 stage deck seam/nail groups.'
  ],
  assets,
  offline_m2_depth_proxy: {
    full_roof_pivots_visible: fullDepth.point_rows.filter(row=>row.visible_proxy===true).length,
    cutaway_pivots_visible: cutDepth.point_rows.filter(row=>row.visible_proxy===true).length,
    total_points: pointMap.points.length,
    interpretation: 'Coordinate hotspot only. A pivot can be hidden by its own prop geometry. Not a runtime mesh silhouette, clickable-area, bounds, walkability or UE occlusion result.'
  },
  acceptance: {
    v01_source_preserved: true,
    all_v01_object_groups_preserved: originalGroupsPreserved.length===original.groups.length,
    original_14_m2_point_map_preserved: pointMap.points.length===14,
    detailed_source_model_generated: true,
    house_and_stage_and_phototable_detail_views_created: true,
    review_board_created: true,
    ue_imported: false,
    fixed_camera_review: false,
    readability_1080p: false,
    readability_720p: false,
    runtime_occlusion_review: false,
    walkability_review: false,
    uv_collision_lod_review: false
  },
  preview_kind: 'offline software projection; not an Unreal Engine render',
  placement_authority: 'UE5/SourceAssets/P6/Manifests/P6_A1_CurrentPointArtMap_v01.json remains the current M2 reference; final placement requires Level Design review.'
};

const out = path.join(p6Root,'Manifests/P6_A1_CourtyardKit_Index_v03.json');
fs.writeFileSync(out,`${JSON.stringify(manifest,null,2)}\n`,'utf8');
  console.log(JSON.stringify({index:path.relative(repoRoot,out).replaceAll('\\','/'),object_groups:model.groups.length,vertices:model.vertices.length,faces:model.faces,materials,paving_stones:pavingStoneCount,framed_lanterns:framedLanternCount,phototable_detail_groups:photoTableDetailGroupCount,original_groups_preserved:originalGroupsPreserved.length,m2_points:pointMap.points.length,assets:assets.length},null,2));
