[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputDirectory = Join-Path $projectRoot 'SourceAssets/P6/Environment/A1_CourtyardKit/Models'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$objPath = Join-Path $outputDirectory 'P6_A1_CourtyardKit_Blockout_v01.obj'
$mtlPath = Join-Path $outputDirectory 'P6_A1_CourtyardKit_Blockout_v01.mtl'

$script:Lines = [System.Collections.Generic.List[string]]::new()
$script:VertexCount = 0
$script:Invariant = [System.Globalization.CultureInfo]::InvariantCulture

function Format-Number([double]$Value) {
    return $Value.ToString('0.###', $script:Invariant)
}

function Add-Vertex([double]$X, [double]$Y, [double]$Z) {
    $script:Lines.Add(('v {0} {1} {2}' -f (Format-Number $X), (Format-Number $Y), (Format-Number $Z)))
    $script:VertexCount++
    return $script:VertexCount
}

function Start-Object([string]$Name, [string]$Material) {
    $script:Lines.Add("o $Name")
    $script:Lines.Add("usemtl $Material")
}

function Rotate-Point([double]$X, [double]$Y, [double]$Z, [double]$Pitch, [double]$Yaw, [double]$Roll) {
    $Rx = $Pitch * [Math]::PI / 180.0
    $Ry = $Yaw * [Math]::PI / 180.0
    $Rz = $Roll * [Math]::PI / 180.0
    $Y1 = $Y * [Math]::Cos($Rx) - $Z * [Math]::Sin($Rx)
    $Z1 = $Y * [Math]::Sin($Rx) + $Z * [Math]::Cos($Rx)
    $X2 = $X * [Math]::Cos($Ry) + $Z1 * [Math]::Sin($Ry)
    $Z2 = -$X * [Math]::Sin($Ry) + $Z1 * [Math]::Cos($Ry)
    $X3 = $X2 * [Math]::Cos($Rz) - $Y1 * [Math]::Sin($Rz)
    $Y3 = $X2 * [Math]::Sin($Rz) + $Y1 * [Math]::Cos($Rz)
    return ,@($X3, $Y3, $Z2)
}

function Add-Box([string]$Name, [string]$Material, [double]$X, [double]$Y, [double]$Z,
    [double]$SizeX, [double]$SizeY, [double]$SizeZ, [double]$Pitch = 0, [double]$Yaw = 0, [double]$Roll = 0) {
    Start-Object $Name $Material
    $Start = [int]$script:VertexCount + 1
    foreach ($Local in @(
        @(-0.5,-0.5,-0.5), @(0.5,-0.5,-0.5), @(0.5,0.5,-0.5), @(-0.5,0.5,-0.5),
        @(-0.5,-0.5,0.5), @(0.5,-0.5,0.5), @(0.5,0.5,0.5), @(-0.5,0.5,0.5)
    )) {
        $Point = Rotate-Point ($Local[0] * $SizeX) ($Local[1] * $SizeY) ($Local[2] * $SizeZ) $Pitch $Yaw $Roll
        [void](Add-Vertex ($X + $Point[0]) ($Y + $Point[1]) ($Z + $Point[2]))
    }
    $F = @(
        @($Start,($Start+3),($Start+2),($Start+1)), @(($Start+4),($Start+5),($Start+6),($Start+7)),
        @($Start,($Start+1),($Start+5),($Start+4)), @(($Start+1),($Start+2),($Start+6),($Start+5)),
        @(($Start+2),($Start+3),($Start+7),($Start+6)), @(($Start+3),$Start,($Start+4),($Start+7))
    )
    foreach ($Face in $F) { $script:Lines.Add(('f ' + ($Face -join ' '))) }
}

function Add-Frustum([string]$Name, [string]$Material, [double]$X, [double]$Y, [double]$Z,
    [double]$RadiusBottom, [double]$RadiusTop, [double]$Height, [int]$Sides = 12) {
    Start-Object $Name $Material
    $Start = $script:VertexCount + 1
    for ($Ring = 0; $Ring -lt 2; $Ring++) {
        $Radius = if ($Ring -eq 0) { $RadiusBottom } else { $RadiusTop }
        $RingZ = $Z + $(if ($Ring -eq 0) { 0 } else { $Height })
        for ($I = 0; $I -lt $Sides; $I++) {
            $Angle = 2.0 * [Math]::PI * $I / $Sides
            [void](Add-Vertex ($X + $Radius * [Math]::Cos($Angle)) ($Y + $Radius * [Math]::Sin($Angle)) $RingZ)
        }
    }
    $BottomCenter = Add-Vertex $X $Y $Z
    $TopCenter = Add-Vertex $X $Y ($Z + $Height)
    $TopStart = $Start + $Sides
    for ($I = 0; $I -lt $Sides; $I++) {
        $Next = ($I + 1) % $Sides
        $B0 = $Start + $I; $B1 = $Start + $Next
        $T0 = $TopStart + $I; $T1 = $TopStart + $Next
        $script:Lines.Add("f $B0 $B1 $T1 $T0")
        $script:Lines.Add("f $BottomCenter $B1 $B0")
        $script:Lines.Add("f $TopCenter $T0 $T1")
    }
}

function Add-Cylinder([string]$Name, [string]$Material, [double]$X, [double]$Y, [double]$Z,
    [double]$Radius, [double]$Height, [int]$Sides = 12) {
    Add-Frustum $Name $Material $X $Y $Z $Radius $Radius $Height $Sides
}

function Add-WellRing([string]$Name, [double]$X, [double]$Y, [double]$Z,
    [double]$OuterRadius, [double]$InnerRadius, [double]$Height, [int]$Sides = 16) {
    Start-Object $Name 'Stone'
    $Start = $script:VertexCount + 1
    foreach ($Ring in @(@($OuterRadius,$Z), @($OuterRadius,$Z+$Height), @($InnerRadius,$Z+$Height), @($InnerRadius,$Z))) {
        for ($I = 0; $I -lt $Sides; $I++) {
            $Angle = 2.0 * [Math]::PI * $I / $Sides
            [void](Add-Vertex ($X + $Ring[0] * [Math]::Cos($Angle)) ($Y + $Ring[0] * [Math]::Sin($Angle)) $Ring[1])
        }
    }
    for ($Layer = 0; $Layer -lt 4; $Layer++) {
        $NextLayer = ($Layer + 1) % 4
        for ($I = 0; $I -lt $Sides; $I++) {
            $Next = ($I + 1) % $Sides
            $A = $Start + $Layer*$Sides + $I
            $B = $Start + $Layer*$Sides + $Next
            $C = $Start + $NextLayer*$Sides + $Next
            $D = $Start + $NextLayer*$Sides + $I
            $script:Lines.Add("f $A $B $C $D")
        }
    }
}

function Add-Sphere([string]$Name, [string]$Material, [double]$X, [double]$Y, [double]$Z,
    [double]$RadiusX, [double]$RadiusY, [double]$RadiusZ, [int]$Sides = 10, [int]$Rings = 5) {
    Start-Object $Name $Material
    $Start = $script:VertexCount + 1
    for ($R = 0; $R -le $Rings; $R++) {
        $Phi = -[Math]::PI/2 + [Math]::PI*$R/$Rings
        $RingRadius = [Math]::Cos($Phi)
        $RingZ = [Math]::Sin($Phi)
        for ($I = 0; $I -lt $Sides; $I++) {
            $Angle = 2*[Math]::PI*$I/$Sides
            [void](Add-Vertex ($X + $RadiusX*$RingRadius*[Math]::Cos($Angle)) ($Y + $RadiusY*$RingRadius*[Math]::Sin($Angle)) ($Z + $RadiusZ*$RingZ))
        }
    }
    for ($R = 0; $R -lt $Rings; $R++) {
        for ($I = 0; $I -lt $Sides; $I++) {
            $Next = ($I+1) % $Sides
            $A = $Start + $R*$Sides + $I; $B = $Start + $R*$Sides + $Next
            $C = $Start + ($R+1)*$Sides + $Next; $D = $Start + ($R+1)*$Sides + $I
            $script:Lines.Add("f $A $B $C $D")
        }
    }
}

function Add-GableRoof([string]$Name, [string]$Material, [double]$X, [double]$Y, [double]$Z,
    [double]$Width, [double]$Length, [double]$Rise) {
    Start-Object $Name $Material
    $Start = $script:VertexCount + 1
    $HalfWidth = $Width / 2.0
    $HalfLength = $Length / 2.0
    $Points = @(
        @(($X-$HalfWidth),($Y-$HalfLength),$Z), @($X,($Y-$HalfLength),($Z+$Rise)), @(($X+$HalfWidth),($Y-$HalfLength),$Z),
        @(($X-$HalfWidth),($Y+$HalfLength),$Z), @($X,($Y+$HalfLength),($Z+$Rise)), @(($X+$HalfWidth),($Y+$HalfLength),$Z)
    )
    foreach ($Point in $Points) { [void](Add-Vertex $Point[0] $Point[1] $Point[2]) }
    foreach ($Face in @(
        @($Start,($Start+1),($Start+4),($Start+3)), @(($Start+1),($Start+2),($Start+5),($Start+4)),
        @($Start,($Start+2),($Start+1)), @(($Start+3),($Start+4),($Start+5)),
        @($Start,($Start+3),($Start+5),($Start+2))
    )) { $script:Lines.Add(('f ' + ($Face -join ' '))) }
}

$script:Lines.Add('mtllib P6_A1_CourtyardKit_Blockout_v01.mtl')
$script:Lines.Add('# Units: centimeters. Geometry is an A1 modular art blockout, not final collision or gameplay truth.')

# Shared ground and courtyard boundary.
Add-Box 'P6_Ground_CourtyardPlaza' 'Ground' 0 0 -55 7100 5500 110
Add-Box 'P6_Wall_North' 'Plaster' 0 2660 170 7100 180 340
Add-Box 'P6_Wall_West' 'Plaster' -3460 0 170 180 5320 340
Add-Box 'P6_Wall_East' 'Plaster' 3460 0 170 180 5320 340
Add-Box 'P6_Wall_South_West' 'Plaster' -2300 -2660 170 2500 180 340
Add-Box 'P6_Wall_South_East' 'Plaster' 2300 -2660 170 2500 180 340
Add-Box 'P6_Gate_Post_Left' 'Timber' -600 -2660 250 150 200 500
Add-Box 'P6_Gate_Post_Right' 'Timber' 600 -2660 250 150 200 500
Add-Box 'P6_Gate_Lintel' 'Timber' 0 -2660 500 1400 220 130
Add-GableRoof 'P6_Gate_Cap' 'Roof' 0 -2660 565 1650 330 250
Add-Box 'P6_Wall_StoneFooting_North' 'Stone' 0 2550 55 7100 240 110
Add-Box 'P6_Wall_StoneFooting_West' 'Stone' -3350 0 55 240 5320 110
Add-Box 'P6_Wall_StoneFooting_East' 'Stone' 3350 0 55 240 5320 110

# Main opera stage and trophy.
Add-Box 'P6_Stage_Platform' 'Stone' -1450 1370 90 2200 1300 180
Add-Box 'P6_Stage_WoodDeck' 'Timber' -1450 1360 200 2100 1200 45
Add-Box 'P6_Stage_Backdrop' 'Curtain' -1450 1940 770 2100 100 1000
Add-Box 'P6_Stage_LeftPost' 'Timber' -2440 1480 590 140 150 900
Add-Box 'P6_Stage_RightPost' 'Timber' -460 1480 590 140 150 900
Add-Box 'P6_Stage_FrontRail' 'Timber' -1450 760 355 2200 85 110
Add-Box 'P6_Stage_RoofBeam' 'Timber' -1450 1510 1120 2450 180 120
Add-GableRoof 'P6_Stage_Roof' 'Roof' -1450 1510 1180 2700 1550 520
Add-Box 'P6_Stage_Step_01' 'Stone' -1450 635 60 1350 280 120
Add-Box 'P6_Stage_Step_02' 'Stone' -1450 385 -10 1650 280 120
Add-Box 'P6_Stage_Curtain_Left' 'Curtain' -2220 1810 720 400 90 560
Add-Box 'P6_Stage_Curtain_Right' 'Curtain' -680 1810 720 400 90 560
Add-Box 'P6_Trophy_Pedestal_Base' 'Stone' -320 520 110 170 170 220
Add-Frustum 'P6_Trophy_Cup' 'Brass' -320 520 270 115 180 220 12
Add-Frustum 'P6_Trophy_Stem' 'Brass' -320 520 210 32 48 80 10
Add-Frustum 'P6_Trophy_Foot' 'Brass' -320 520 190 125 125 30 12

# House module with doorway, windows and a gabled double roof.
Add-Box 'P6_House_MainVolume' 'Plaster' 1770 1750 580 2520 1300 1160
Add-Box 'P6_House_StoneBase' 'Stone' 1770 1750 135 2620 1380 270
Add-Box 'P6_House_Door' 'Timber' 1770 1088 390 300 60 650
Add-Box 'P6_House_Window_Left' 'WindowAmber' 1000 1088 660 330 50 330
Add-Box 'P6_House_Window_Right' 'WindowAmber' 2500 1088 660 330 50 330
Add-Box 'P6_House_WindowFrame_Left' 'Timber' 1000 1050 660 410 55 410
Add-Box 'P6_House_WindowFrame_Right' 'Timber' 2500 1050 660 410 55 410
Add-GableRoof 'P6_House_RoofShell' 'Roof' 1770 1750 1160 2880 1650 640
Add-Box 'P6_House_RoofSlope_Left' 'Roof' 1030 1750 1450 1700 1770 110 0 -25 0
Add-Box 'P6_House_RoofSlope_Right' 'Roof' 2510 1750 1450 1700 1770 110 0 25 0
Add-Box 'P6_House_Eave_Front' 'Timber' 1770 850 1190 2950 110 100
Add-Box 'P6_House_Eave_Back' 'Timber' 1770 2650 1190 2950 110 100

# Well, bucket and surrounding props.
Add-WellRing 'P6_Well_StoneRim' 2520 -2240 0 320 205 290 16
Add-Cylinder 'P6_Well_Water' 'Water' 2520 -2240 95 200 6 16
Add-Box 'P6_Well_Crossbeam' 'Timber' 2520 -2240 650 850 120 120
Add-Box 'P6_Well_Post_Left' 'Timber' 2180 -2240 450 90 100 650
Add-Box 'P6_Well_Post_Right' 'Timber' 2860 -2240 450 90 100 650
Add-Cylinder 'P6_Well_RopeSpool' 'Timber' 2520 -2240 675 115 60 10
Add-Box 'P6_Well_Bucket' 'Clay' 2520 -2240 365 160 150 150
Add-Frustum 'P6_Well_BucketBody' 'Clay' 2520 -2240 340 95 120 135 10

# Village tree, pots, jars and benches.
Add-Cylinder 'P6_Tree_Trunk' 'Timber' -2900 1680 0 105 700 8
Add-Sphere 'P6_Tree_Canopy_A' 'Plant' -2900 1680 980 520 480 470 9 5
Add-Sphere 'P6_Tree_Canopy_B' 'Plant' -2550 1730 900 360 390 350 9 5
Add-Sphere 'P6_Tree_Canopy_C' 'Plant' -3210 1770 870 370 390 340 9 5
Add-Frustum 'P6_Pot_Left' 'Clay' 300 2190 0 125 160 210 10
Add-Sphere 'P6_Pot_Left_Plant' 'Plant' 300 2190 300 190 180 170 8 4
Add-Frustum 'P6_Pot_Right' 'Clay' 3120 950 0 125 160 210 10
Add-Sphere 'P6_Pot_Right_Plant' 'Plant' 3120 950 300 190 180 170 8 4
Add-Frustum 'P6_Storage_Jar_A' 'Clay' 980 1750 0 145 170 300 12
Add-Frustum 'P6_Storage_Jar_B' 'Clay' 1230 1750 0 130 145 260 12
Add-Frustum 'P6_Storage_Jar_C' 'Clay' 1480 1750 0 120 135 240 12
Add-Box 'P6_Bench_Seat' 'Timber' 2860 130 300 780 180 70
Add-Box 'P6_Bench_Leg_Left' 'Timber' 2590 130 145 80 150 300
Add-Box 'P6_Bench_Leg_Right' 'Timber' 3130 130 145 80 150 300

# Evidence-area props are neutral and stay separate from the authored clue data.
Add-Box 'P6_Noticeboard_Post' 'Timber' -2700 900 260 100 120 520
Add-Box 'P6_Noticeboard_Frame' 'Timber' -2700 900 620 560 130 430
Add-Box 'P6_Noticeboard_Paper' 'Paper' -2700 820 620 470 24 340
Add-Box 'P6_Detector_Stand' 'Timber' 2300 610 280 180 170 560
Add-Box 'P6_Detector_Unit' 'Roof' 2300 610 620 330 260 280
Add-Cylinder 'P6_Detector_Dial' 'WindowAmber' 2300 460 650 58 10 20
Add-Box 'P6_PhotoTable_Top' 'Timber' 1500 1080 340 760 450 80
Add-Box 'P6_PhotoTable_Leg_A' 'Timber' 1200 920 170 65 65 340
Add-Box 'P6_PhotoTable_Leg_B' 'Timber' 1800 920 170 65 65 340
Add-Box 'P6_PhotoTable_CameraBody' 'Roof' 1500 1080 490 230 170 160
Add-Cylinder 'P6_PhotoTable_Lens' 'Brass' 1500 985 490 72 85 12
Add-Box 'P6_Ledger_Desk' 'Timber' -2250 -1000 300 620 330 80
Add-Box 'P6_Ledger_Book' 'Paper' -2250 -1000 380 230 160 40

# Mainline clue prop: inert paper flowers; color and shape do not state whether they are evidence.
Add-Frustum 'P6_G5_FakeFlowers_Pot' 'Clay' 1050 400 0 96 120 104 10
Add-Cylinder 'P6_G5_FakeFlowers_Stem_A' 'Sage' 1004 400 144 10 120 7
Add-Cylinder 'P6_G5_FakeFlowers_Stem_B' 'Sage' 1050 400 150 10 132 7
Add-Cylinder 'P6_G5_FakeFlowers_Stem_C' 'Sage' 1096 400 139 10 112 7
Add-Sphere 'P6_G5_FakeFlowers_Bloom_A' 'Petal' 1004 400 209 39 37 18 8 4
Add-Sphere 'P6_G5_FakeFlowers_Bloom_B' 'FlowerCenter' 1004 400 216 12 12 9 8 4
Add-Sphere 'P6_G5_FakeFlowers_Bloom_C' 'WindowAmber' 1050 400 222 42 38 18 8 4
Add-Sphere 'P6_G5_FakeFlowers_Bloom_D' 'Petal' 1096 400 201 38 36 17 8 4
Add-Sphere 'P6_G5_FakeFlowers_Bloom_E' 'FlowerCenter' 1096 400 208 12 12 9 8 4
Add-Box 'P6_G5_FakeFlowers_Leaf_A' 'Sage' 981 400 118 70 27 18 0 0 -22
Add-Box 'P6_G5_FakeFlowers_Leaf_B' 'Sage' 1072 400 126 70 27 18 0 0 24

# Optional joke prop aligned to the M2 sock point; a plain rack avoids adding story text.
Add-Box 'P6_SocksRack_Awning' 'Roof' 1770 -810 890 720 520 100
Add-Box 'P6_SocksRack_Bracket_Left' 'Timber' 1580 -810 720 70 80 360
Add-Box 'P6_SocksRack_Bracket_Right' 'Timber' 1960 -810 720 70 80 360
Add-Box 'P6_SocksRack_Post_Left' 'Timber' 1580 -810 340 70 80 680
Add-Box 'P6_SocksRack_Post_Right' 'Timber' 1960 -810 340 70 80 680
Add-Box 'P6_SocksRack_Rail' 'Timber' 1770 -810 680 450 75 70
Add-Box 'P6_SocksPair_Left_Shaft' 'Paper' 1675 -850 520 80 48 250
Add-Box 'P6_SocksPair_Left_Toe' 'Paper' 1710 -850 385 150 48 70
Add-Box 'P6_SocksPair_Left_Cuff' 'Sage' 1675 -850 646 86 53 28
Add-Box 'P6_SocksPair_Right_Shaft' 'Paper' 1840 -770 520 80 48 250
Add-Box 'P6_SocksPair_Right_Toe' 'Paper' 1880 -770 385 150 48 70 0 0 180
Add-Box 'P6_SocksPair_Right_Cuff' 'Sage' 1840 -770 646 86 53 28
Add-Frustum 'P6_TrashCan_Body' 'Roof' -1280 -1430 0 230 260 520 12
Add-Box 'P6_TrashCan_Lid' 'Timber' -1280 -1430 540 560 560 70
Add-Box 'P6_Clothesline_Post_A' 'Timber' -1430 2220 300 90 90 600
Add-Box 'P6_Clothesline_Post_B' 'Timber' -270 2220 300 90 90 600
Add-Box 'P6_Clothesline_Rope' 'Timber' -850 2220 540 1220 25 25
Add-Box 'P6_Clothesline_Scarf_A' 'Curtain' -1120 2220 440 210 45 200
Add-Box 'P6_Clothesline_Scarf_B' 'Paper' -800 2220 440 210 45 200
Add-Box 'P6_Clothesline_Scarf_C' 'Sage' -500 2220 440 210 45 200

# Warm lanterns mark architectural landmarks, not investigation answers.
Add-Box 'P6_Lantern_House_LeftBracket' 'Timber' 440 1000 1030 130 320 45
Add-Frustum 'P6_Lantern_House_Left' 'Lantern' 440 840 880 95 110 250 10
Add-Box 'P6_Lantern_Gate_Bracket' 'Timber' 730 -2660 660 90 240 40
Add-Frustum 'P6_Lantern_Gate' 'Lantern' 730 -2780 490 85 100 200 10
Add-Box 'P6_RoofTile_Stack' 'Roof' 420 -2270 130 650 360 150

$script:Lines | Set-Content -LiteralPath $objPath -Encoding ascii

$materials = @'
# P6 A1 flat material palette; color is a style base for UE material setup.
newmtl Ground
Kd 0.39 0.40 0.38
Ka 0.18 0.19 0.18
Ns 12

newmtl Plaster
Kd 0.72 0.62 0.48
Ka 0.25 0.22 0.18
Ns 16

newmtl Stone
Kd 0.43 0.42 0.38
Ka 0.17 0.17 0.15
Ns 10

newmtl Timber
Kd 0.34 0.20 0.13
Ka 0.14 0.09 0.06
Ns 18

newmtl Roof
Kd 0.19 0.24 0.32
Ka 0.08 0.10 0.14
Ns 14

newmtl Curtain
Kd 0.49 0.19 0.21
Ka 0.18 0.07 0.08
Ns 20

newmtl Brass
Kd 0.73 0.53 0.20
Ka 0.28 0.20 0.08
Ns 55

newmtl Clay
Kd 0.49 0.27 0.19
Ka 0.20 0.10 0.07
Ns 20

newmtl Plant
Kd 0.29 0.40 0.31
Ka 0.12 0.17 0.13
Ns 9

newmtl WindowAmber
Kd 0.92 0.68 0.31
Ka 0.48 0.35 0.16
Ns 24

newmtl Lantern
Kd 0.95 0.58 0.20
Ka 0.48 0.29 0.10
Ns 16

newmtl Paper
Kd 0.88 0.82 0.68
Ka 0.35 0.32 0.25
Ns 6

newmtl Sage
Kd 0.41 0.53 0.45
Ka 0.18 0.23 0.19
Ns 10

newmtl Water
Kd 0.22 0.38 0.43
Ka 0.09 0.15 0.18
Ns 28

newmtl Petal
Kd 0.78 0.29 0.28
Ka 0.29 0.10 0.10
Ns 15

newmtl FlowerCenter
Kd 0.91 0.71 0.33
Ka 0.34 0.25 0.11
Ns 18
'@
Set-Content -LiteralPath $mtlPath -Value $materials -Encoding ascii

Write-Output "P6 A1 blockout kit written: $objPath"
Write-Output "P6 A1 material palette written: $mtlPath"
Write-Output "OBJ vertices: $($script:VertexCount); objects: $((@($script:Lines | Where-Object { $_ -like 'o *' })).Count)"
