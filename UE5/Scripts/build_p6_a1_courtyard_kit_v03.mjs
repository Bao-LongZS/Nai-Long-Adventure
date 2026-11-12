import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const kitDir = path.join(repoRoot, 'UE5/SourceAssets/P6/Environment/A1_CourtyardKit');
const modelDir = path.join(kitDir, 'Models');
const sourceObj = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v02.obj');
const sourceMtl = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v02.mtl');
const targetObj = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v03.obj');
const targetMtl = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v03.mtl');
const sourceLines = fs.readFileSync(sourceObj, 'utf8').trimEnd().split(/\r?\n/);
const kept = [];
const oldToNewVertex = new Map();
let dropGroup = false, oldVertexIndex = 0, newVertexIndex = 0;
for (const line of sourceLines) {
  if (line.startsWith('o ')) dropGroup = /^o P6_Paving_\d+_\d+/.test(line);
  if (line.startsWith('v ')) {
    oldVertexIndex++;
    if (dropGroup) { oldToNewVertex.set(oldVertexIndex, null); continue; }
    oldToNewVertex.set(oldVertexIndex, ++newVertexIndex);
    kept.push(line);
    continue;
  }
  if (dropGroup) continue;
  if (line.startsWith('f ')) {
    const remapped = line.slice(2).trim().split(/\s+/).map(token => {
      const match = token.match(/^(\d+)(.*)$/);
      if (!match) throw new Error(`Unsupported OBJ face index: ${token}`);
      const mapped = oldToNewVertex.get(Number(match[1]));
      if (!mapped) throw new Error(`Kept object refers to removed vertex ${match[1]}`);
      return `${mapped}${match[2]}`;
    });
    kept.push(`f ${remapped.join(' ')}`);
    continue;
  }
  kept.push(line);
}

// Rebalance roof tile tones per tile instead of letting whole courses read as stripes.
const variants = ['RoofTile','RoofTile','RoofTile','RoofTileShade','RoofTile','RoofTileLight','RoofTile','RoofTileGlaze'];
const stoneVariants = ['StoneSoft','StoneSoft','Stone','StoneWarm','StoneSoft','StoneShade','StoneSoft','StoneLight'];
let roofTileGroup = false, stoneCourseGroup = false, roofGroupSalt = 0, tileFaceCount = 0, tileIndex = 0, stoneFaceCount = 0, stoneBlockIndex = 0;
const refinedBase = [];
for (const line of kept) {
  if (line.startsWith('o ')) {
    roofTileGroup = /RoofTiles_.*_Course_/.test(line);
    stoneCourseGroup = /P6_(House_StoneBase_.*Course|Wall_.*_Masonry_Course)/.test(line);
    roofGroupSalt = [...line].reduce((sum, character) => sum + character.charCodeAt(0), 0);
    tileFaceCount = 0;
    tileIndex = 0;
    stoneFaceCount = 0;
    stoneBlockIndex = 0;
    refinedBase.push(line);
    continue;
  }
  if (roofTileGroup && line.startsWith('usemtl ')) {
    refinedBase.push('usemtl RoofTile');
    continue;
  }
  if (roofTileGroup && line.startsWith('f ')) {
    if (tileFaceCount === 0) refinedBase.push(`usemtl ${variants[(tileIndex * 5 + roofGroupSalt) % variants.length]}`);
    refinedBase.push(line);
    tileFaceCount++;
    if (tileFaceCount === 6) { tileFaceCount = 0; tileIndex++; }
    continue;
  }
  if (stoneCourseGroup && line.startsWith('f ')) {
    if (stoneFaceCount === 0) refinedBase.push(`usemtl ${stoneVariants[(stoneBlockIndex * 5 + roofGroupSalt) % stoneVariants.length]}`);
    refinedBase.push(line);
    stoneFaceCount++;
    if (stoneFaceCount === 6) { stoneFaceCount = 0; stoneBlockIndex++; }
    continue;
  }
  refinedBase.push(line);
}
let baseObj = refinedBase.join('\n')
  .replace('P6_A1_CourtyardKit_Blockout_v02.mtl', 'P6_A1_CourtyardKit_Blockout_v03.mtl')
  .replace('Refined v02 source blockout; layered detail only, not final collision or gameplay truth.', 'Refined v03 source blockout; layered detail only, not final collision or gameplay truth.')
  .replace('Layered medium-poly visual detail; UVs, collision, LODs and runtime acceptance remain outstanding.', 'Refined v03 source blockout with irregular paving and prop close-up details; not final collision or gameplay truth.');

const extraMaterials = `
newmtl PavingEdge
Kd 0.36 0.36 0.34
Ka 0.13 0.13 0.12
Ns 5

newmtl PavingPale
Kd 0.46 0.44 0.40
Ka 0.17 0.16 0.14
Ns 8

newmtl RoofTileGlaze
Kd 0.35 0.41 0.48
Ka 0.13 0.15 0.18
Ns 24

newmtl LeafDark
Kd 0.20 0.32 0.24
Ka 0.07 0.12 0.09
Ns 8

newmtl LeafLight
Kd 0.36 0.48 0.31
Ka 0.13 0.18 0.10
Ns 9

newmtl VentShadow
Kd 0.12 0.16 0.18
Ka 0.05 0.06 0.07
Ns 12

newmtl StoneHairline
Kd 0.29 0.29 0.27
Ka 0.11 0.11 0.10
Ns 5

newmtl StoneSoft
Kd 0.47 0.46 0.42
Ka 0.17 0.16 0.14
Ns 8

newmtl StoneLight
Kd 0.54 0.52 0.47
Ka 0.20 0.19 0.16
Ns 8

newmtl StoneShade
Kd 0.38 0.39 0.37
Ka 0.14 0.14 0.13
Ns 6

newmtl LanternGlow
Kd 0.96 0.69 0.29
Ka 0.52 0.30 0.10
Ns 32

newmtl PhotoCloth
Kd 0.82 0.77 0.67
Ka 0.31 0.28 0.23
Ns 7
`;

const baseVertexCount = (baseObj.match(/^v\s/gm) || []).length;
let nextVertex = baseVertexCount;
const detail = [];
const fmt = n => Number(n.toFixed(3)).toString();
const addVertex = p => { detail.push(`v ${fmt(p[0])} ${fmt(p[1])} ${fmt(p[2])}`); return ++nextVertex; };
const addFace = ids => detail.push(`f ${ids.join(' ')}`);
const startObject = (name, material) => detail.push(`o ${name}`, `usemtl ${material}`);
const add = (a,b) => a.map((n,i) => n+b[i]);
const sub = (a,b) => a.map((n,i) => n-b[i]);
const mul = (a,k) => a.map(n => n*k);
const cross = (a,b) => [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]];
const norm = a => mul(a,1/(Math.hypot(...a)||1));

function boxGeometry(cx,cy,cz,sx,sy,sz,rotation=0) {
  const start=nextVertex+1,a=rotation*Math.PI/180,c=Math.cos(a),s=Math.sin(a);
  const pts=[[-.5,-.5,-.5],[.5,-.5,-.5],[.5,.5,-.5],[-.5,.5,-.5],[-.5,-.5,.5],[.5,-.5,.5],[.5,.5,.5],[-.5,.5,.5]];
  for(const [x,y,z] of pts) addVertex([cx+x*sx*c-y*sy*s,cy+x*sx*s+y*sy*c,cz+z*sz]);
  for(const f of [[0,3,2,1],[4,5,6,7],[0,1,5,4],[1,2,6,5],[2,3,7,6],[3,0,4,7]]) addFace(f.map(i=>start+i));
}

function addBox(name,material,...args){ startObject(name,material); boxGeometry(...args); }

function cylinderGeometry(a,b,radius,sides=10) {
  const axis=norm(sub(b,a)); let ref=[0,0,1]; if(Math.abs(axis[2])>.94) ref=[0,1,0];
  const u=norm(cross(axis,ref)),v=norm(cross(axis,u)),start=nextVertex+1;
  for(const p of [a,b]) for(let i=0;i<sides;i++){const t=2*Math.PI*i/sides;addVertex(add(p,add(mul(u,radius*Math.cos(t)),mul(v,radius*Math.sin(t)))));}
  const ca=addVertex(a),cb=addVertex(b);
  for(let i=0;i<sides;i++){const j=(i+1)%sides;addFace([start+i,start+j,start+sides+j,start+sides+i]);addFace([ca,start+j,start+i]);addFace([cb,start+sides+i,start+sides+j]);}
}

function addCylinder(name,material,a,b,radius,sides=10){startObject(name,material);cylinderGeometry(a,b,radius,sides);}

function sphereGeometry(cx,cy,cz,rx,ry,rz,sides=10,rings=6,yaw=0){
  const start=nextVertex+1;
  const co=Math.cos(yaw),si=Math.sin(yaw);
  for(let r=0;r<=rings;r++){const p=-Math.PI/2+Math.PI*r/rings;for(let i=0;i<sides;i++){const t=2*Math.PI*i/sides,x=rx*Math.cos(p)*Math.cos(t),y=ry*Math.cos(p)*Math.sin(t);addVertex([cx+x*co-y*si,cy+x*si+y*co,cz+rz*Math.sin(p)]);}}
  for(let r=0;r<rings;r++)for(let i=0;i<sides;i++){const j=(i+1)%sides;addFace([start+r*sides+i,start+r*sides+j,start+(r+1)*sides+j,start+(r+1)*sides+i]);}
}

function addSphere(name,material,cx,cy,cz,rx,ry,rz,sides=10,rings=6,yaw=0){startObject(name,material);sphereGeometry(cx,cy,cz,rx,ry,rz,sides,rings,yaw);}

function torusGeometry(cx,cy,cz,major,minor,normal=[0,0,1],majorSteps=20,minorSteps=8){
  normal=norm(normal);const ref=Math.abs(normal[2])>.94?[0,1,0]:[0,0,1],u=norm(cross(normal,ref)),v=norm(cross(normal,u)),start=nextVertex+1;
  for(let i=0;i<majorSteps;i++){const a=2*Math.PI*i/majorSteps;for(let j=0;j<minorSteps;j++){const b=2*Math.PI*j/minorSteps,rad=major+minor*Math.cos(b),ring=add(mul(u,Math.cos(a)),mul(v,Math.sin(a)));addVertex(add([cx,cy,cz],add(mul(ring,rad),mul(normal,minor*Math.sin(b)))));}}
  for(let i=0;i<majorSteps;i++)for(let j=0;j<minorSteps;j++){const i2=(i+1)%majorSteps,j2=(j+1)%minorSteps;addFace([start+i*minorSteps+j,start+i2*minorSteps+j,start+i2*minorSteps+j2,start+i*minorSteps+j2]);}
}

function addTorus(name,material,cx,cy,cz,major,minor,normal=[0,0,1]){startObject(name,material);torusGeometry(cx,cy,cz,major,minor,normal);}

function seeded(row,col,salt=0){let x=((row+1)*73856093)^((col+1)*19349663)^((salt+7)*83492791);x=(x^(x>>>13))*1274126177;return ((x^(x>>>16))>>>0)/4294967295;}
function clippedRect(width,depth,clip){return [[-width/2+clip,-depth/2],[width/2-clip,-depth/2],[width/2,-depth/2+clip],[width/2,depth/2-clip],[width/2-clip,depth/2],[-width/2+clip,depth/2],[-width/2,depth/2-clip],[-width/2,-depth/2+clip]];}
function addFlagstone(name,cx,cy,width,depth,angle,topMaterial,thickness=10){
  startObject(name,'PavingEdge');
  const bottom=clippedRect(width,depth,Math.min(width,depth)*.16),upper=clippedRect(width-10,depth-10,Math.min(width,depth)*.15),top=clippedRect(width-24,depth-24,Math.min(width,depth)*.14);
  const radians=angle*Math.PI/180,co=Math.cos(radians),si=Math.sin(radians),rotate=p=>[cx+p[0]*co-p[1]*si,cy+p[0]*si+p[1]*co];
  const rings=[];
  for(const [ringIndex,shape,z] of [[0,bottom,0],[1,upper,thickness-3],[2,top,thickness]]) for(let i=0;i<shape.length;i++){
    const p=shape[i],amount=ringIndex===0?3:ringIndex===1?5:8;
    const q=rotate([p[0]+(seeded(Math.round(cx),Math.round(cy),ringIndex*17+i)-.5)*amount,p[1]+(seeded(Math.round(cy),Math.round(cx),ringIndex*23+i)-.5)*amount]);
    rings.push(addVertex([q[0],q[1],z]));
  }
  for(let i=0;i<8;i++){const j=(i+1)%8;addFace([rings[i],rings[j],rings[8+j],rings[8+i]]);}
  for(let i=0;i<8;i++){const j=(i+1)%8;addFace([rings[8+i],rings[8+j],rings[16+j],rings[16+i]]);}
  detail.push(`usemtl ${topMaterial}`);addFace(rings.slice(16));
  detail.push('usemtl PavingEdge');addFace([...rings.slice(0,8)].reverse());
}

// Replace oversized regular pavers with roughly 1.8 m beveled flagstones in offset courses.
for(let row=0;row<25;row++)for(let col=0;col<33;col++){
  const jitterX=(seeded(row,col,1)-.5)*8,jitterY=(seeded(row,col,2)-.5)*8;
  const offset=row%2===0?0:100;
  const x=-3250+col*200+offset+jitterX,y=-2520+row*200+jitterY;
  const w=178+seeded(row,col,3)*14,d=180+seeded(row,col,4)*14;
  const tone=seeded(row,col,9),mat=tone>.92?'PavingPale':tone<.16?'PavingAlt':'Paving';
  addFlagstone(`P6_PavingStone_Course_${String(row+1).padStart(2,'0')}_${String(col+1).padStart(2,'0')}`,x,y,w,d,(seeded(row,col,5)-.5)*2.6,mat,9+seeded(row,col,6)*2);
}

// A handful of hairline chips keep the paving from reading as a perfectly repeated stamp.
for(const [row,col,angle,length] of [[1,4,-24,34],[3,19,18,28],[7,8,31,38],[11,25,-14,32],[16,13,22,36],[22,29,-29,30]]){
  const x=-3250+col*200+(row%2===0?0:100)+(seeded(row,col,1)-.5)*8;
  const y=-2520+row*200+(seeded(row,col,2)-.5)*8;
  addBox(`P6_PavingStone_HairlineChip_${row+1}_${col+1}`,'StoneHairline',x+16,y-12,13, length,2,1.5,angle);
  addBox(`P6_PavingStone_HairlineBranch_${row+1}_${col+1}`,'StoneHairline',x+35,y-23,13, 16,2,1.5,angle-42);
}

// Door panels and iron ring knockers add a readable focal detail to the house facade.
for(const [leaf,cx] of [['L',1702],['R',1838]]){
  startObject(`P6_House_Door_${leaf}_RaisedPanels`,'TimberEdge');
  for(const z of [280,500]){
    boxGeometry(cx-45,1000,z,12,28,152);boxGeometry(cx+45,1000,z,12,28,152);
    boxGeometry(cx,1000,z-70,94,28,12);boxGeometry(cx,1000,z+70,94,28,12);
    boxGeometry(cx,1006,z,72,12,112);
  }
  addTorus(`P6_House_Door_${leaf}_Knocker`,'Brass',cx,982,395,22,6,[0,-1,0]);
  addSphere(`P6_House_Door_${leaf}_KnockerPin`,'Brass',cx,978,366,8,8,8,8,4);
}
startObject('P6_House_Corner_StoneQuoins','StoneWarm');
for(const x of [480,3060])for(let row=0;row<3;row++){
  boxGeometry(x,1032,55+row*82,112,58,72,row%2?2:-2);
  boxGeometry(x,2420,55+row*82,112,58,72,row%2?-2:2);
}
startObject('P6_House_Window_Sill_Drips','StoneWarm');
for(const x of [1000,2500])for(let i=0;i<5;i++)boxGeometry(x-180+i*90,985,425,18,12,20);

// The east gable side is visible in the courtyard view; add recessed vents, timber lattice and drip sills.
for(const [index,y] of [1480,2040].entries()){
  startObject(`P6_House_East_Vent_${index+1}_Recess`,'VentShadow');
  boxGeometry(3032,y,700,12,218,284);
  startObject(`P6_House_East_Vent_${index+1}_StoneFrame`,'StoneWarm');
  boxGeometry(3043,y,700,22,264,330);
  boxGeometry(3055,y,850,22,286,24);
  boxGeometry(3055,y,550,28,300,24);
  boxGeometry(3054,y-132,700,24,22,322);
  boxGeometry(3054,y+132,700,24,22,322);
  startObject(`P6_House_East_Vent_${index+1}_WoodLattice`,'TimberEdge');
  boxGeometry(3062,y,700,14,18,280);
  boxGeometry(3062,y-69,700,14,12,278);
  boxGeometry(3062,y+69,700,14,12,278);
  boxGeometry(3062,y,631,14,270,12);
  boxGeometry(3062,y,769,14,270,12);
  startObject(`P6_House_East_Vent_${index+1}_Shutters`,'Timber');
  for(const side of [-1,1]){
    boxGeometry(3048,y+side*190,700,22,74,278);
    for(let slat=0;slat<6;slat++)boxGeometry(3060,y+side*190,598+slat*38,12,70,8);
  }
  for(const side of [-1,1])for(const z of [610,790])addSphere(`P6_House_East_Vent_${index+1}_Pin_${side}_${z}`,'Iron',3068,y+side*189,z,5,5,5,8,4);
}
startObject('P6_House_East_Sill_Course','StoneWarm');
for(let i=0;i<10;i++)boxGeometry(3043,1175+i*126,470,28,120,24);

// Eave-end ornaments and ceramic ridge beads stay within the current roof silhouette.
for(const [prefix,cx,cy,baseZ,halfWidth,halfLength,rise] of [
  ['House',1770,1750,1160,1440,825,640],['Stage',-1450,1510,1180,1350,775,520],['Gate',0,-2660,565,825,150,250],
]){
  const peak=baseZ+rise;
  for(const end of [-1,1]){
    const y=cy+end*(halfLength-18);
    addSphere(`P6_${prefix}_RidgeEndCap_${end}`,'RoofTileLight',cx,y,peak-4,74,56,44,10,5);
    for(const side of [-1,1]){
      const x=cx+side*(halfWidth-28);
      const points=[[x,y,baseZ+8],[x,y+end*42,baseZ+20],[x,y+end*70,baseZ+76],[x,y+end*82,baseZ+110]];
      startObject(`P6_${prefix}_UpturnedEave_${side}_${end}`,'RoofTileGlaze');
      for(let i=0;i<points.length-1;i++)cylinderGeometry(points[i],points[i+1],26,8);
    }
  }
}
startObject('P6_House_Ridge_Beads','RoofTileLight');
for(let i=0;i<13;i++)sphereGeometry(1770,990+i*130,1818,24,24,18,8,4);
startObject('P6_Stage_Ridge_Beads','RoofTileLight');
for(let i=0;i<11;i++)sphereGeometry(-1450,820+i*138,1712,22,22,16,8,4);

// Mature tree: tapered limbs and layered leaf clusters break up the three-sphere canopy.
startObject('P6_Tree_Branch_Structure','TimberEdge');
for(const [a,b] of [
  [[-2900,1680,520],[-3220,1650,900]], [[-2900,1680,550],[-2580,1700,1010]],
  [[-2900,1680,670],[-2980,1950,1050]], [[-2900,1680,700],[-2820,1400,1090]],
])cylinderGeometry(a,b,30,9);
for(let i=0;i<34;i++){
  const a=i*2.399963229728653,r=(i%7)/6,z=790+(i%6)*88;
  const cx=-2900+Math.cos(a)*(215+r*220),cy=1680+Math.sin(a)*(190+r*200),cz=z+(seeded(i,3,7)-.5)*44;
  const material=i%6===0?'LeafLight':i%4===0?'LeafDark':'Plant';
  addSphere(`P6_Tree_LeafCluster_${String(i+1).padStart(2,'0')}`,material,cx,cy,cz,86+(i%3)*14,70+(i%4)*10,58+(i%2)*15,9,5);
}
startObject('P6_Tree_Bark_Rings','TimberEdge');
for(const z of [170,330,500])cylinderGeometry([-2900,1680,z],[-2900,1680,z+10],105,8);
for(const [x,y] of [[-3120,1700],[-2670,1540],[-3000,1940]]){
  startObject(`P6_Tree_RootFlare_${x}_${y}`,'Timber');
  cylinderGeometry([-2900,1680,35],[x,y,150],42,8);
}

// Small broadleaf clumps soften hard wall/stage junctions without blocking the evidence props.
function addPlantClump(name,x,y,z,scale=1){
  startObject(`${name}_Stems`,'Plant');
  const leaves=[];
  for(let i=0;i<7;i++){
    const a=i*2.399963229728653+seeded(i,Math.round(x),7)*.45;
    const length=62*scale+(i%3)*14*scale,height=55*scale+(i%4)*11*scale;
    const tip=[x+Math.cos(a)*length,y+Math.sin(a)*length,z+height];
    cylinderGeometry([x,y,z],[tip[0],tip[1],tip[2]-.2*height],3.5*scale,6);
    leaves.push({tip,a,i});
  }
  for(const {tip,a,i} of leaves){
    const mat=i%5===0?'LeafLight':i%4===0?'LeafDark':'Plant';
    addSphere(`${name}_Leaf_${i+1}`,mat,tip[0],tip[1],tip[2],31*scale,13*scale,11*scale,8,4,a);
  }
  addSphere(`${name}_Crown`, 'LeafDark', x,y,z+30*scale,32*scale,26*scale,18*scale,8,4);
}
for(const [name,x,y,z,scale] of [
  ['P6_Plant_House_East_North',3070,1200,18,.88],['P6_Plant_House_East_South',3070,2360,18,.82],
  ['P6_Plant_Stage_Left',-2450,690,20,.78],['P6_Plant_Stage_Right',-650,690,20,.68],
  ['P6_Plant_Gate_Left',-790,-2430,20,.72],['P6_Plant_Gate_Right',790,-2430,20,.72],
])addPlantClump(name,x,y,z,scale);

// Bucket staves/hoops, circular well water ripples and pottery incised bands.
startObject('P6_Well_Bucket_Staves','TimberEdge');
for(let i=0;i<12;i++){
  const a=2*Math.PI*i/12,x=2520+94*Math.cos(a),y=-2240+94*Math.sin(a);
  boxGeometry(x,y,405,38,22,110,a*180/Math.PI+90);
}
addTorus('P6_Well_Bucket_Hoop_Lower','Iron',2520,-2240,368,100,8);
addTorus('P6_Well_Bucket_Hoop_Upper','Iron',2520,-2240,454,112,8);
addTorus('P6_Well_Rim_Cap','StoneWarm',2520,-2240,292,253,18);
addTorus('P6_Well_Water_Ripple_Outer','Water',2520,-2240,100,160,3);
addTorus('P6_Well_Water_Ripple_Inner','Water',2520,-2240,101,90,3);
for(const [x,y,z,r] of [[980,1750,76,150],[1230,1750,68,130],[1480,1750,62,118]]){
  addTorus(`P6_StorageJar_IncisedBand_${x}_A`,'TimberEdge',x,y,z,r,6);
  addTorus(`P6_StorageJar_IncisedBand_${x}_B`,'TimberEdge',x,y,z+90,r-8,6);
}

// Give the evidence camera a framed lens plate, top shutter and a small side strap loop.
addBox('P6_PhotoTable_Camera_LensPlate','TimberEdge',1500,988,490,188,10,132);
for(const x of [1420,1580])for(const z of [438,542])addSphere(`P6_PhotoTable_Camera_Screw_${x}_${z}`,'Brass',x,981,z,6,5,6,8,4);
addTorus('P6_PhotoTable_Camera_FocusRing_Inner','Brass',1500,928,490,56,5,[0,-1,0]);
addCylinder('P6_PhotoTable_Camera_ShutterRelease','Brass',[1590,1060,588],[1590,1060,604],8,8);
addTorus('P6_PhotoTable_Camera_SideDial','Brass',1620,1080,500,19,4,[1,0,0]);
addSphere('P6_PhotoTable_Camera_SideDial_Hub','Brass',1624,1080,500,6,8,8,8,4);
startObject('P6_PhotoTable_Camera_Strap','TimberEdge');
for(const [a,b] of [
  [[1617,1030,552],[1644,1035,522]],[[1644,1035,522],[1644,1125,522]],[[1644,1125,522],[1617,1130,552]],
])cylinderGeometry(a,b,5,7);

// A small cream runner with a restrained red hem follows the concept camera-table dressing.
addBox('P6_PhotoTable_Cloth_Top','PhotoCloth',1500,875,397,580,74,8);
addBox('P6_PhotoTable_Cloth_Front','PhotoCloth',1500,847,314,580,8,142);
addBox('P6_PhotoTable_Cloth_RedHem','Curtain',1500,840,243,580,4,10);
startObject('P6_PhotoTable_Cloth_Stitching','CurtainGold');
for(let i=0;i<11;i++)boxGeometry(1240+i*52,840,260,16,3,2);
for(const x of [1220,1780])boxGeometry(x,840,314,3,3,132);
startObject('P6_PhotoTable_Cloth_Folds','StoneWarm');
for(const x of [1280,1390,1500,1610,1720])cylinderGeometry([x,841,252],[x,841,373],2.2,6);

// Refine the stage approach with tread nosings and small railing cap posts.
for(let i=0;i<11;i++){
  const y=820+i*105;
  addBox(`P6_Stage_WoodDeck_Seam_${i+1}`,'TimberEdge',-1450,y,225,2080,3,3);
  addSphere(`P6_Stage_DeckNail_Left_${i+1}`,'Iron',-2435,y,228,4,4,4,8,4);
  addSphere(`P6_Stage_DeckNail_Right_${i+1}`,'Iron',-465,y,228,4,4,4,8,4);
}
startObject('P6_Stage_Stair_TreadNosing','StoneWarm');
for(const [y,z,w] of [[605,124,1370],[390,74,1560],[175,24,1740]])boxGeometry(-1450,y,z,w,34,24);
startObject('P6_Stage_Railing_Caps','TimberEdge');
for(let i=0;i<15;i++){
  const x=-2460+i*144;
  boxGeometry(x,746,505,52,60,28);
  sphereGeometry(x,746,521,28,28,20,8,4);
}
// Warm framed lanterns echo the concept kit and give the main facade/stage a readable scale cue.
function addWallLantern(prefix,x,wallY,baseZ){
  startObject(`${prefix}_MountPlate`,'Brass');
  boxGeometry(x,wallY+8,baseZ+154,46,18,62);
  addSphere(`${prefix}_MountBolts`,'Brass',x,wallY-2,baseZ+154,7,5,7,8,4);
  startObject(`${prefix}_Bracket`,'TimberEdge');
  cylinderGeometry([x,wallY+2,baseZ+160],[x,wallY-36,baseZ+188],8,8);
  addTorus(`${prefix}_HangerRing`,'Brass',x,wallY-49,baseZ+182,18,5,[0,-1,0]);
  startObject(`${prefix}_GlassBody`,'LanternGlow');
  boxGeometry(x,wallY-62,baseZ+78,62,8,104);
  boxGeometry(x-36,wallY-37,baseZ+78,8,48,100);
  boxGeometry(x+36,wallY-37,baseZ+78,8,48,100);
  startObject(`${prefix}_FrameAndCaps`,'Brass');
  for(const side of [-1,1]){
    boxGeometry(x+side*40,wallY-37,baseZ+78,8,58,124);
    boxGeometry(x+side*40,wallY-37,baseZ+20,10,58,10);
    boxGeometry(x+side*40,wallY-37,baseZ+136,10,58,10);
  }
  boxGeometry(x,wallY-70,baseZ+78,6,6,88);
  boxGeometry(x,wallY-70,baseZ+78,54,6,6);
  boxGeometry(x,wallY-37,baseZ+16,90,62,14);
  boxGeometry(x,wallY-37,baseZ+140,98,68,14);
  startObject(`${prefix}_RoofAndFinial`,'TimberEdge');
  boxGeometry(x,wallY-32,baseZ+151,116,82,14);
  addSphere(`${prefix}_Finial`,'Brass',x,wallY-32,baseZ+166,11,11,13,8,4);
  startObject(`${prefix}_Tassel`,'Curtain');
  cylinderGeometry([x,wallY-32,baseZ+8],[x,wallY-32,baseZ-10],4,6);
  addSphere(`${prefix}_TasselTip`,'CurtainGold',x,wallY-32,baseZ-20,9,8,13,8,4);
}
addWallLantern('P6_House_Lantern_Left',620,1000,825);
addWallLantern('P6_House_Lantern_Right',2920,1000,825);
addWallLantern('P6_Stage_Lantern_Left',-2440,1405,760);
addWallLantern('P6_Stage_Lantern_Right',-460,1405,760);

startObject('P6_Detector_Face_Rivets','Brass');
for(const x of [2185,2415])for(const z of [535,705])sphereGeometry(x,447,z,8,8,8,8,4);
startObject('P6_Detector_ScaleTicks','Iron');
for(let i=0;i<7;i++)boxGeometry(2230+i*23,432,671+(i%2)*5,5,5,18);
addTorus('P6_Detector_Dial_Bezel','Brass',2300,401,560,56,6,[0,-1,0]);
startObject('P6_Detector_Dial_Graduation','Iron');
for(let i=0;i<12;i++){
  const a=(i/12)*Math.PI*2,inner=39,outer=i%3===0?49:45;
  cylinderGeometry([2300+Math.cos(a)*inner,399,560+Math.sin(a)*inner],[2300+Math.cos(a)*outer,399,560+Math.sin(a)*outer],i%3===0?3:2,6);
}
addCylinder('P6_Detector_Dial_Needle','Brass',[2300,397,560],[2325,397,576],4,8);
addSphere('P6_Detector_Dial_Hub','Brass',2300,394,560,8,5,8,8,4);
addTorus('P6_PhotoTable_Lens_OuterRing','Brass',1500,952,490,75,8,[0,-1,0]);

// A few pinned paper layers and ruled lines make the noticeboard read as evidence, not a blank slab.
for(const [index,x,z,width,height] of [[1,-2840,610,148,182],[2,-2680,660,142,218],[3,-2520,600,136,168]]){
  addBox(`P6_Noticeboard_Sheet_${index}`,'Paper',x,800,z,width,8,height);
  addBox(`P6_Noticeboard_Sheet_${index}_Header`,'CurtainGold',x,794,z+height*.36,width*.72,3,8);
  for(let line=0;line<4;line++)addBox(`P6_Noticeboard_Sheet_${index}_Rule_${line+1}`,'StoneShade',x-4,794,z+height*.18-line*20,width*.62-(line%2)*15,3,3);
  addSphere(`P6_Noticeboard_Sheet_${index}_Pin`,'Brass',x,790,z+height*.48,8,6,8,8,4);
}
startObject('P6_Noticeboard_Frame_CornerCaps','TimberEdge');
for(const x of [-2950,-2450])for(const z of [430,812])sphereGeometry(x,800,z,12,7,12,8,4);

// Versioned files stay separate so v01/v02 remain recoverable.
const objText=`${baseObj.trimEnd()}\n# v03 adds beveled irregular flagstones, per-tile ceramic variation and close-range architectural/prop details.\n${detail.join('\n')}\n`;
const mtlText=`${fs.readFileSync(sourceMtl,'utf8').trimEnd()}\n${extraMaterials.trimStart()}`;
fs.writeFileSync(targetObj,objText,'ascii');fs.writeFileSync(targetMtl,mtlText,'ascii');
const lines=objText.split(/\r?\n/),points=lines.filter(line=>line.startsWith('v ')).map(line=>line.split(/\s+/).slice(1).map(Number));
const bounds=[0,1,2].map(axis=>[Math.min(...points.map(p=>p[axis])),Math.max(...points.map(p=>p[axis]))]);
console.log(JSON.stringify({obj:path.relative(repoRoot,targetObj).replaceAll('\\','/'),mtl:path.relative(repoRoot,targetMtl).replaceAll('\\','/'),object_groups:lines.filter(line=>line.startsWith('o ')).length,vertices:points.length,faces:lines.filter(line=>line.startsWith('f ')).length,materials:(mtlText.match(/^newmtl /gm)||[]).length,bounds_cm:{x:bounds[0],y:bounds[1],z:bounds[2]},detail_summary:'offset beveled paving, varied roof tile glaze, four framed lanterns, photo camera plate/shutter/strap and linen table runner, upturned eaves/ridge caps, tree limbs/leaves, door panels, well and prop closeups; no UV/collision/LOD'} ,null,2));

