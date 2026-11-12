import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const p6Root = path.join(repoRoot, 'UE5/SourceAssets/P6');
const kitRoot = path.join(p6Root, 'Environment/A1_CourtyardKit');
const objPath = path.join(kitRoot, 'Models/P6_A1_CourtyardKit_Blockout_v04.obj');
const mtlPath = path.join(kitRoot, 'Models/P6_A1_CourtyardKit_Blockout_v04.mtl');
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
const importReportPath = path.join(repoRoot, 'UE5/Records/p6_art/evidence/P6_A1_CourtyardKit_UEImport_v04.json');
const lightingReportPath = path.join(repoRoot, 'UE5/Records/p6_art/evidence/P6_A1_M2LightingReviewMap_v06.json');
const captureReportPath = path.join(repoRoot, 'UE5/Records/p6_art/evidence/P6_A1_M2LightingReviewCapture_v11.json');
const obliqueCaptureReportPath = path.join(repoRoot, 'UE5/Records/p6_art/evidence/P6_A1_LightingReviewCapture_v10.json');
const importReport = fs.existsSync(importReportPath) ? JSON.parse(read(importReportPath)) : null;
const lightingReport = fs.existsSync(lightingReportPath) ? JSON.parse(read(lightingReportPath)) : null;
const captureReport = fs.existsSync(captureReportPath) ? JSON.parse(read(captureReportPath)) : null;
const obliqueCaptureReport = fs.existsSync(obliqueCaptureReportPath) ? JSON.parse(read(obliqueCaptureReportPath)) : null;
const ueImported = importReport?.status === 'imported' && importReport.imported_static_mesh_count === 1 && importReport.lod0_fallback_triangle_count > 0;
const naniteBuilt = ueImported && importReport.nanite_triangle_count > 0;
const reviewMapSaved = lightingReport?.status === 'isolated_review_map_saved';
const captures = captureReport?.captures ?? [];
const captureFilesExist = captures.length === 2 && captures.every(capture => fs.existsSync(capture.path));
const reviewCaptureCompleted = captureReport?.status === 'captured' && captureReport.map_asset === lightingReport?.map_asset && captureReport.map_saved_before_capture === true && captureFilesExist;
const fullDepth = JSON.parse(read(path.join(kitRoot, 'P6_A1_M2CameraDepthProxy_v04.json')));
const cutDepth = JSON.parse(read(path.join(kitRoot, 'P6_A1_M2CameraCutawayDepthProxy_v04.json')));
const fullRoofVisiblePoints = fullDepth.point_rows.filter(row => row.visible_proxy === true).map(row => row.id);
const cutawayVisiblePoints = cutDepth.point_rows.filter(row => row.visible_proxy === true).map(row => row.id);
const blockedRequiredPoints = fullDepth.point_rows
  .filter(row => row.visible_proxy !== true && pointMap.points.some(point => point.id === row.id && point.required))
  .map(row => ({id:row.id,sequence:row.sequence,occluding_group_candidate:row.occluding_group_candidate ?? row.nearest_surface_group}));
if (originalGroupsPreserved.length !== original.groups.length) throw new Error('Some v01 object groups are missing from v04.');
if (pointMap.points.length !== 14) throw new Error(`Expected 14 M2 anchors; found ${pointMap.points.length}.`);
if (materials !== 41) throw new Error(`Expected 41 materials; found ${materials}.`);
if (framedLanternCount !== 4) throw new Error(`Expected 4 framed lanterns; found ${framedLanternCount}.`);

const assets = [
  ['P6_A1_KIT_V04_OBJ','Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_Blockout_v04.obj','editable_detailed_source_blockout'],
  ['P6_A1_KIT_V04_MTL','Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_Blockout_v04.mtl','flat_preview_materials_not_final'],
  ['P6_A1_KIT_V04_PREVIEW_PNG','Environment/A1_CourtyardKit/P6_A1_CourtyardKit_Blockout_Preview_v04.png','offline_axonometric_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_PREVIEW_SVG','Environment/A1_CourtyardKit/P6_A1_CourtyardKit_Blockout_Preview_v04.svg','editable_offline_axonometric_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_CUTAWAY_PNG','Environment/A1_CourtyardKit/P6_A1_CourtyardKit_CutawayPreview_v04.png','offline_cutaway_projection_not_approved_level_change',[1600,1120]],
  ['P6_A1_KIT_V04_CUTAWAY_SVG','Environment/A1_CourtyardKit/P6_A1_CourtyardKit_CutawayPreview_v04.svg','editable_offline_cutaway_projection_not_approved_level_change',[1600,1120]],
  ['P6_A1_KIT_V04_HOUSE_DETAIL_PNG','Environment/A1_CourtyardKit/P6_A1_HouseDetailPreview_v04.png','offline_house_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_HOUSE_DETAIL_SVG','Environment/A1_CourtyardKit/P6_A1_HouseDetailPreview_v04.svg','editable_offline_house_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_STAGE_DETAIL_PNG','Environment/A1_CourtyardKit/P6_A1_StageDetailPreview_v04.png','offline_stage_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_STAGE_DETAIL_SVG','Environment/A1_CourtyardKit/P6_A1_StageDetailPreview_v04.svg','editable_offline_stage_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_PHOTOTABLE_DETAIL_PNG','Environment/A1_CourtyardKit/P6_A1_PhotoTableDetailPreview_v04.png','offline_phototable_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_PHOTOTABLE_DETAIL_SVG','Environment/A1_CourtyardKit/P6_A1_PhotoTableDetailPreview_v04.svg','editable_offline_phototable_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_NOTICEBOARD_DETAIL_PNG','Environment/A1_CourtyardKit/P6_A1_NoticeboardDetailPreview_v04.png','offline_noticeboard_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_NOTICEBOARD_DETAIL_SVG','Environment/A1_CourtyardKit/P6_A1_NoticeboardDetailPreview_v04.svg','editable_offline_noticeboard_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_DETECTOR_DETAIL_PNG','Environment/A1_CourtyardKit/P6_A1_DetectorDetailPreview_v04.png','offline_detector_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_DETECTOR_DETAIL_SVG','Environment/A1_CourtyardKit/P6_A1_DetectorDetailPreview_v04.svg','editable_offline_detector_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_WELL_DETAIL_PNG','Environment/A1_CourtyardKit/P6_A1_WellDetailPreview_v04.png','offline_well_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_WELL_DETAIL_SVG','Environment/A1_CourtyardKit/P6_A1_WellDetailPreview_v04.svg','editable_offline_well_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_TROPHY_DETAIL_PNG','Environment/A1_CourtyardKit/P6_A1_TrophyDetailPreview_v04.png','offline_trophy_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_TROPHY_DETAIL_SVG','Environment/A1_CourtyardKit/P6_A1_TrophyDetailPreview_v04.svg','editable_offline_trophy_detail_projection_not_engine_render',[1600,1120]],
  ['P6_A1_KIT_V04_M2_PNG','Environment/A1_CourtyardKit/P6_A1_M2CameraPreview_v04.png','offline_exact_m2_projection_and_pivot_depth_proxy',[1920,1080]],
  ['P6_A1_KIT_V04_M2_SVG','Environment/A1_CourtyardKit/P6_A1_M2CameraPreview_v04.svg','editable_offline_exact_m2_projection_not_runtime_visibility',[1920,1080]],
  ['P6_A1_KIT_V04_M2_DEPTH','Environment/A1_CourtyardKit/P6_A1_M2CameraDepthProxy_v04.json','offline_per_point_nearest_surface_proxy_not_ue_occlusion'],
  ['P6_A1_KIT_V04_M2_CUTAWAY_PNG','Environment/A1_CourtyardKit/P6_A1_M2CameraCutawayPreview_v04.png','offline_exact_m2_cutaway_projection_not_approved_level_change',[1920,1080]],
  ['P6_A1_KIT_V04_M2_CUTAWAY_SVG','Environment/A1_CourtyardKit/P6_A1_M2CameraCutawayPreview_v04.svg','editable_offline_exact_m2_cutaway_projection_not_runtime_visibility',[1920,1080]],
  ['P6_A1_KIT_V04_M2_CUTAWAY_DEPTH','Environment/A1_CourtyardKit/P6_A1_M2CameraCutawayDepthProxy_v04.json','offline_cutaway_per_point_nearest_surface_proxy_not_ue_occlusion'],
  ['P6_A1_KIT_V04_REVIEW_BOARD','Environment/A1_CourtyardKit/P6_A1_ReviewBoard_v04.svg','self_contained_visual_review_board_not_runtime_screenshot',[2560,3700]],
  ['P6_A1_KIT_V04_HANDOFF','Environment/A1_CourtyardKit/P6_A1_CourtyardKit_Handoff_v04.md','artist_handoff'],
  ['P6_A1_KIT_V04_UE_COMBINED_OBJ','Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_v04_UECombined.obj','ue_import_source_one_mesh_forty_batched_materials'],
  ['P6_A1_KIT_V04_UE_COMBINED_MTL','Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_v04_UECombined.mtl','ue_import_material_source_copy'],
  ['P6_A1_KIT_V04_BUILD_SCRIPT','../../Scripts/build_p6_a1_courtyard_kit_v04.mjs','source_generation_script'],
  ['P6_A1_KIT_V04_UE_SOURCE_SCRIPT','../../Scripts/build_p6_a1_ue_source_v04.mjs','ue_merge_source_generation_script'],
  ['P6_A1_KIT_V04_UE_IMPORT_SCRIPT','../../Scripts/import_p6_a1_courtyard_v04.py','ue_static_mesh_import_script'],
  ['P6_A1_KIT_V04_UE_REVIEW_MAP_SCRIPT','../../Scripts/build_p6_a1_lighting_review_map.py','ue_lighting_review_map_script'],
  ['P6_A1_KIT_V05_UE_REVIEW_MAP_SCRIPT','../../Scripts/build_p6_a1_lighting_review_map_v05.py','ue_lighting_review_map_script'],
  ['P6_A1_KIT_V06_M2_REVIEW_MAP_SCRIPT','../../Scripts/build_p6_a1_m2_lighting_review_map_v06.py','ue_m2_camera_lighting_review_map_script'],
  ['P6_A1_KIT_V06_M2_CAPTURE_SCRIPT','../../Scripts/capture_p6_a1_m2_lighting_review_v06.py','ue_m2_camera_capture_script'],
  ['P6_A1_KIT_V04_UE_INSPECTION_SCRIPT','../../Scripts/inspect_p6_a1_static_mesh.py','ue_mesh_inspection_script'],
  ['P6_A1_KIT_V05_REVIEW_MAP_REPORT','../../Records/p6_art/evidence/P6_A1_LightingReviewMap_v05.json','ue_isolated_review_map_evidence'],
  ['P6_A1_KIT_V10_CAPTURE_REPORT','../../Records/p6_art/evidence/P6_A1_LightingReviewCapture_v10.json','ue_engine_capture_evidence'],
  ['P6_A1_KIT_V10_CAPTURE_720P','../../Records/p6_art/evidence/P6_A1_LightingReview_1280x720_v10.png','ue_engine_screenshot_first_pass_review_only',[1280,720]],
  ['P6_A1_KIT_V10_CAPTURE_1080P','../../Records/p6_art/evidence/P6_A1_LightingReview_1920x1080_v10.png','ue_engine_screenshot_first_pass_review_only',[1920,1080]],
  ['P6_A1_KIT_V06_M2_REVIEW_MAP_REPORT','../../Records/p6_art/evidence/P6_A1_M2LightingReviewMap_v06.json','ue_isolated_m2_camera_review_map_evidence'],
  ['P6_A1_KIT_V11_M2_CAPTURE_REPORT','../../Records/p6_art/evidence/P6_A1_M2LightingReviewCapture_v11.json','ue_m2_camera_engine_capture_evidence'],
  ['P6_A1_KIT_V11_M2_CAPTURE_720P','../../Records/p6_art/evidence/P6_A1_M2LightingReview_1280x720_v11.png','ue_m2_camera_engine_screenshot_first_pass_review_only',[1280,720]],
  ['P6_A1_KIT_V11_M2_CAPTURE_1080P','../../Records/p6_art/evidence/P6_A1_M2LightingReview_1920x1080_v11.png','ue_m2_camera_engine_screenshot_first_pass_review_only',[1920,1080]],
  ['P6_A1_KIT_M2_VISIBILITY_REVIEW_BOARD','../../Records/p6_art/evidence/P6_A1_M2VisibilityReview_v01.svg','annotated_m2_pivot_occlusion_review_not_runtime_visibility',[2700,1640]],
  ['P6_A1_KIT_M2_VISIBILITY_REVIEW_SCRIPT','../../Scripts/build_p6_a1_m2_visibility_review_v01.mjs','m2_visibility_review_board_generation_script'],
  ['P6_A1_KIT_V04_UE_INTEGRATION_HANDOFF','Environment/A1_CourtyardKit/P6_A1_UE_Integration_v04.md','ue_integration_preparation_handoff'],
  ['P6_A1_KIT_V04_PREVIEW_SCRIPT','../../Scripts/render_p6_a1_courtyard_preview_v04.mjs','preview_generation_script'],
  ['P6_A1_KIT_V04_BOARD_SCRIPT','../../Scripts/build_p6_a1_review_board_v04.mjs','review_board_generation_script'],
  ['P6_A1_KIT_V04_INDEX_SCRIPT','../../Scripts/build_p6_a1_courtyard_kit_index_v04.mjs','index_generation_script']
].map(([asset_id,assetPath,type,dimensions_px]) => {
  const absolute = path.resolve(p6Root, assetPath);
  if (!fs.existsSync(absolute)) throw new Error(`Missing indexed asset: ${assetPath}`);
  return {asset_id, path: assetPath, type, ...(dimensions_px ? {dimensions_px} : {}), sha256: sha(absolute)};
});

const manifest = {
  schema_version: '1.0',
  package_id: 'P6_A1_CourtyardKit_Blockout_v04',
  display_level: '5-1',
  case_title: '禁笑令之夜',
  status: ueImported && reviewMapSaved && reviewCaptureCompleted ? 'm2_camera_capture_first_pass_reviewed_pending_point_visibility_and_final_art_acceptance' : 'detailed_source_blockout_candidate_not_engine_approved',
  source_basis: {
    preserved_v01_obj: {path:'Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_Blockout_v01.obj',sha256:sha(path.join(kitRoot,'Models/P6_A1_CourtyardKit_Blockout_v01.obj'))},
    preserved_v01_mtl: {path:'Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_Blockout_v01.mtl',sha256:sha(path.join(kitRoot,'Models/P6_A1_CourtyardKit_Blockout_v01.mtl'))},
    current_m2_point_map: 'Manifests/P6_A1_CurrentPointArtMap_v01.json',
    m2_point_count: pointMap.points.length,
    style_reference: 'Environment/A1_CourtyardKit/P6_A1_EnvironmentKit_Concept_v02.png',
    v01_source_preserved: true,
    v01_object_groups: original.groups.length,
    v01_object_groups_preserved_in_v04: originalGroupsPreserved.length
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
  ue_integration: {
    status: reviewCaptureCompleted ? 'ue_mesh_imported_nanite_built_m2_camera_capture_reviewed_pending_point_visibility_and_runtime_acceptance' : reviewMapSaved ? 'ue_mesh_imported_nanite_built_and_m2_camera_review_map_saved_pending_capture' : ueImported ? 'ue_mesh_imported_pending_lighting_review_map' : 'merged_source_and_editor_scripts_prepared_pending_editor_import',
    merged_source_path: 'Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_v04_UECombined.obj',
    merged_source_groups: 1,
    used_material_slots: 40,
    imported_asset_path: '/Game/Naiwa/P6/Art/A1/CourtyardKit_v04/P6_A1_CourtyardKit_v04_UECombined',
    imported_asset_report: 'UE5/Records/p6_art/evidence/P6_A1_CourtyardKit_UEImport_v04.json',
    source_material_batches: importReport?.source_material_batch_count ?? null,
    axis_policy: importReport?.axis_policy ?? null,
    preview_uv_policy: importReport?.preview_uv_policy ?? null,
    lod_count: importReport?.lod_count ?? null,
    lod0_fallback_triangle_count: importReport?.lod0_fallback_triangle_count ?? null,
    nanite_enabled: naniteBuilt,
    nanite_triangle_count: importReport?.nanite_triangle_count ?? null,
    lod0_section_count: importReport?.lod0_section_count ?? null,
    isolated_review_map_path: lightingReport?.map_asset ?? null,
    lighting_review_report: 'UE5/Records/p6_art/evidence/P6_A1_M2LightingReviewMap_v06.json',
    lighting_plan: lightingReport?.lighting ?? null,
    review_camera: lightingReport?.camera ?? null,
    capture_report: 'UE5/Records/p6_art/evidence/P6_A1_M2LightingReviewCapture_v11.json',
    captured_screenshots: captures.map(capture => ({path:path.relative(repoRoot,capture.path).replaceAll('\\','/'),resolution_px:capture.resolution_px,bytes:capture.bytes})),
    previous_oblique_capture: obliqueCaptureReport?.captures?.map(capture => ({path:path.relative(repoRoot,capture.path).replaceAll('\\','/'),resolution_px:capture.resolution_px})),
    first_pass_visual_findings: ['The v11 capture matches the current M2 camera location, rotation and orthographic width.','The scene remains a flat-color graybox; warm lanterns and a cool night mood do not yet match the approved concept.','Foreground roofs occupy much of the frame and obscure courtyard points.'],
    offline_point_visibility_proxy: {
      status:'offline_pivot_depth_proxy_only_not_engine_silhouette_or_click_test',
      full_roof_visible_count:fullRoofVisiblePoints.length,
      full_roof_visible_points:fullRoofVisiblePoints,
      roof_cutaway_visible_count:cutawayVisiblePoints.length,
      roof_cutaway_visible_points:cutawayVisiblePoints,
      blocked_required_points:blockedRequiredPoints,
      interpretation:'The proxy compares each M2 pivot to the nearest projected source-model surface. It does not measure object silhouette visibility, Actor bounds, clickability, collision or walkability.'
    },
    p6_m2_map_modified: false,
    engine_visual_review: Boolean(reviewCaptureCompleted),
    engine_visual_review_scope: 'First-pass exact-M2 composition and lighting review in an isolated map; graybox and lighting candidate only, not point visibility, click-area, walkability, or runtime acceptance.'
  },
  refinements: [
    '825 offset, lightly irregular beveled flagstones with a small set of hairline chips.',
    'Per-tile low-contrast ceramic variation, upturned eave ends and ceramic ridge details.',
    'Raised door panels, ring knockers, window sill drips, east-side lattice vents, shutters and sill course.',
    'Tree limb structure, layered canopy and small broadleaf planting at wall and stage junctions.',
    'Four framed warm lanterns at the house facade and stage posts; stage and house closeups temporarily hide roof groups to expose facade detail.',
    'Photo evidence camera gains a screwed lens plate, inset focus ring, top shutter, side dial/strap and a stitched cream table runner with red hem.',
    'Bucket staves/iron hoops, well rim cap and water ripples, incised jar bands, stage stair nosings and railing caps, detector dial graduations/needle and noticeboard sheets/pins.',
    'Subtle stone tone variation on house bases and wall masonry courses, plus 11 stage deck seam/nail groups.',
    'New v04 detail pass adds five framed stage fascia reliefs, 44 fine deck-grain accents, door hinge straps/rivets, and two window latches.',
    'Raised slate-roof and primary timber base tones; the offline preview lifts the display midtones, adds warm key/cool fill and lantern glow, and reduces the flattened ground-shadow darkening.'
    
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
    ue_merged_import_source_prepared: true,
    ue_lighting_review_map_script_prepared: true,
    ue_imported: Boolean(ueImported),
    ue_nanite_built: Boolean(naniteBuilt),
    ue_lighting_review_map_saved: Boolean(reviewMapSaved),
    ue_lighting_review_capture_saved: Boolean(reviewCaptureCompleted),
    first_pass_capture_reviewed: Boolean(reviewCaptureCompleted),
    fixed_camera_review: Boolean(reviewCaptureCompleted),
    m2_camera_matches_current_point_map: lightingReport?.m2_camera_matches_point_map === true,
    point_visibility_approved: false,
    readability_1080p: false,
    readability_720p: false,
    runtime_occlusion_review: false,
    walkability_review: false,
    uv_collision_lod_review: false
  },
  preview_kind: 'offline software projection; not an Unreal Engine render',
  placement_authority: 'UE5/SourceAssets/P6/Manifests/P6_A1_CurrentPointArtMap_v01.json remains the current M2 reference; final placement requires Level Design review.'
};

const out = path.join(p6Root,'Manifests/P6_A1_CourtyardKit_Index_v04.json');
fs.writeFileSync(out,`${JSON.stringify(manifest,null,2)}\n`,'utf8');
  console.log(JSON.stringify({index:path.relative(repoRoot,out).replaceAll('\\','/'),object_groups:model.groups.length,vertices:model.vertices.length,faces:model.faces,materials,paving_stones:pavingStoneCount,framed_lanterns:framedLanternCount,phototable_detail_groups:photoTableDetailGroupCount,original_groups_preserved:originalGroupsPreserved.length,m2_points:pointMap.points.length,assets:assets.length},null,2));
