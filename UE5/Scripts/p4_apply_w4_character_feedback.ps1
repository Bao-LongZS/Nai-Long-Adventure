param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$manifestPath = Join-Path $root 'UE5/SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v05.json'
$outputManifestPath = Join-Path $root 'UE5/SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v06.json'
$actionReviewSource = Join-Path $root 'UE5/Records/p4_alpha/evidence/W4/P4_W4_BossActionSequenceReview_v01.html'
$actionReviewPath = Join-Path $root 'UE5/Records/p4_alpha/evidence/W4/P4_W4_BossActionSequenceReview_v02.html'
$artReviewSource = Join-Path $root 'UE5/Records/p4_alpha/evidence/W4/P4_W4_ArtReview_v02.html'
$artReviewPath = Join-Path $root 'UE5/Records/p4_alpha/evidence/W4/P4_W4_ArtReview_v03.html'

$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$rejectedState = 'charge_attack'
$rejectedAssetPrefix = 'boss_nai_tong_charge_attack_side_right_'
$manifest.assets = @($manifest.assets | Where-Object {
    $_.state -ne $rejectedState -and $_.asset_id -notlike "$rejectedAssetPrefix*"
})
$manifest.version = 'v06'
$manifest.created_date = '2026-10-30'
$manifest.status = 'wip_boss_action_sequences_feedback_revision'
$manifest.global_constraints.delivery_status = 'P4 W4 WIP: the charge attack character pose was removed after character-design feedback and needs a replacement aligned to the original character. Other Boss candidates and player VFX remain in the active package.'
$manifest.generation_revision_notes = @($manifest.generation_revision_notes) + @(
    '2026-10-30 user feedback: remove the current Boss charge_attack pose because it does not match the original character appearance; retain the other approved Boss candidates and player effects.'
)

$batch = $manifest.boss_action_sequence_batch
$batch.sequences = @($batch.sequences | Where-Object { $_.state -ne $rejectedState })
$batch.sequence_count = @($batch.sequences).Count
$batch.new_generated_frames = $batch.sequence_count
$batch.states_with_two_pose_sequences = @($batch.states_with_two_pose_sequences | Where-Object { $_ -ne $rejectedState })
$batch.other_states = @(
    'idle has a separate three-view transform loop; its body shape is not hand-animated',
    'charge_attack visual candidate removed after character-design feedback; replacement pending'
)
$batch.image_generation_note = 'Seven adjacent Boss pose sequences remain in this revision. The rejected charge_attack pose and its two-pose sequence are excluded pending a replacement aligned to the original character.'
$manifest.boss_action_review_page = 'UE5/Records/p4_alpha/evidence/W4/P4_W4_BossActionSequenceReview_v02.html'
$manifest | ConvertTo-Json -Depth 24 | Set-Content -LiteralPath $outputManifestPath -Encoding utf8

$actionHtml = Get-Content -Raw -LiteralPath $actionReviewSource
$actionHtml = [regex]::Replace($actionHtml, '(?s)<section class="row"><h2>charge_attack.*?</section>', '')
$actionHtml = $actionHtml.Replace('v01 · 2026-10-30 · 桶盖低挂髋侧设计 · 8 组双姿势对拍样本', 'v02 · 2026-10-30 · 桶盖低挂髋侧设计 · 7 组双姿势对拍样本')
$actionHtml = $actionHtml.Replace('八个非待机状态各有两张相邻姿势，用于检查方向、脚底和事件时间。`move` 为普通移动循环，`charge_attack` 循环到命中/落空；其余状态一次播放。当前仍为双姿势候选，动作中间帧、完整时序、三向一致性和美术逐帧修整待制作。落地冲击 VFX 保持独立。', '当前保留的七个非待机状态各有两张相邻姿势，用于检查方向、脚底和事件时间。`move` 为普通移动循环；其余状态一次播放。冲锋出击姿势依角色形象反馈已移出本版，待按原角色形象补制。当前仍为双姿势候选，动作中间帧、完整时序、三向一致性和美术逐帧修整待制作。落地冲击 VFX 保持独立。')
$actionHtml = $actionHtml.Replace('P4_W4_BossAndVFX_manifest_v05.json', 'P4_W4_BossAndVFX_manifest_v06.json')
$actionHtml = $actionHtml.Replace('姿势和播放候选的逐帧文件、哈希、锚点及事件映射见', '其余姿势和播放候选的逐帧文件、哈希、锚点及事件映射见')
[System.IO.File]::WriteAllText($actionReviewPath, $actionHtml, [System.Text.UTF8Encoding]::new($false))

$artHtml = Get-Content -Raw -LiteralPath $artReviewSource
$artHtml = $artHtml.Replace('版本 v02 · 2026-10-30', '版本 v03 · 2026-10-30')
$artHtml = $artHtml.Replace('11 张 Boss 单帧候选与 13 张独立特效候选', '10 张 Boss 单帧候选与 13 张独立特效候选')
$artHtml = $artHtml.Replace('`charge_attack` 已把腾跃旧稿替换为贴地冲锋候选。', '当前贴地冲锋角色帧依反馈从候选页移除，等待按原角色形象重制。')
$artHtml = [regex]::Replace($artHtml, '(?s)<article class="card">.*?首批动作板参考.*?</article>', '')
$artHtml = [regex]::Replace($artHtml, '(?s)<article class="tile"><h3>贴地冲锋 · 右侧面</h3>.*?</article>', '')
$artHtml = $artHtml.Replace('Boss 状态候选（11 张）', 'Boss 状态候选（10 张）')
[System.IO.File]::WriteAllText($artReviewPath, $artHtml, [System.Text.UTF8Encoding]::new($false))

$recordPath = Join-Path $root 'UE5/Records/p4_alpha/evidence/W4/P4_W4_角色形象反馈修订记录_2026-10-30.md'
$record = @'
# P4 W4 角色形象反馈修订记录（2026-10-30）

收到反馈：附件中的 Boss 贴地冲锋姿势与原角色形象不符，移出当前美术候选。其余已通过的 Boss 候选与主角特效保留。

## 修订范围

- 当前清单 v06 移除 `boss_nai_tong_charge_attack_side_right_00` 和 `boss_nai_tong_charge_attack_side_right_01` 两个冲锋序列帧候选。
- 最新 Boss 动作序列评审页不再展示 `charge_attack` 双姿势序列；该状态等待按原角色形象重制。
- 最新美术评审页保留其他 Boss 状态候选及 13 项特效候选，其中包含 6 项主角特效。
- 其余 Boss 动作序列、Boss 待机序列和主角特效序列均保留；工程事件、碰撞和战斗数值未修改。

## 最新文件

- Manifest：[v06](../../../../SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v06.json)
- Boss 动作序列：[评审页 v02](P4_W4_BossActionSequenceReview_v02.html)
- Boss 与 VFX：[美术评审页 v03](P4_W4_ArtReview_v03.html)

旧版本清单和评审页保留为历史记录；当前候选状态以 v06 和上述新版评审页为准。

本次为候选资产筛选与评审文档更新，没有进行 UE 导入、固定机位验收或 PIE 验收；剩余序列仍为 WIP。
'@
[System.IO.File]::WriteAllText($recordPath, $record, [System.Text.UTF8Encoding]::new($false))

Write-Output "Wrote manifest v06, Boss action review v02, art review v03, and feedback revision record."
