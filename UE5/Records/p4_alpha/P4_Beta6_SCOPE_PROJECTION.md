# P4 Beta6 Scope Projection

Date: 2026-11-11

Status: **Six-chapter and content projections, balance fixture, row structs, P4-only progress/save schema, and generic chapter flow are drafted. Native DataTable import and C++ build/readback remain pending; repeated editor-build attempts have not passed the existing shared Build.bat lock.**

## Scope

The temporary Beta delivery contains six chapters total: frozen Alpha Chapter 1 plus Beta Chapters 2–6. Chapters 7–10 remain in the ten-chapter plan and are not changed here. The source chapter JSON remains untouched.

| Chapter | Name | Waves | Ordinary enemies | Boss | Wave source |
|---:|---|---:|---:|---|---|
| 1 | 沙丘晒笑场 | 5 | 20 | — | Accepted Alpha wave CSV |
| 2 | 沙丘补给线 | 6 | 30 | — | Chapter JSON v2 |
| 3 | 沙暴猪圈 | 7 | 42 | — | Chapter JSON v2 |
| 4 | 奶桶擂台 | 3 | 9 | `boss_nai_tong` | Chapter JSON v2 |
| 5 | 沙丘夜市 | 8 | 56 | — | Chapter JSON v2 |
| 6 | 秋林校场 | 3 | 12 | `boss_nai_jia` | Chapter JSON v2 |
| **Total** |  | **32** | **169** | **2** |  |

## Generated files

The reproducible generators are:

- `powershell -NoProfile -ExecutionPolicy Bypass -File UE5/Scripts/p4_project_beta6_scope.ps1`
- `powershell -NoProfile -ExecutionPolicy Bypass -File UE5/Scripts/p4_project_beta6_enemy_boss_data.ps1`
- `powershell -NoProfile -ExecutionPolicy Bypass -File UE5/Scripts/p4_project_beta6_balance_fixture.ps1`
- `powershell -NoProfile -ExecutionPolicy Bypass -File UE5/Scripts/p4_project_beta6_theme_binding_audit.ps1`

- `UE5/SourceAssets/P4/Data/Beta6/P4Beta6_Chapters.csv`: 6 chapter rows.
- `UE5/SourceAssets/P4/Data/Beta6/P4Beta6_Themes.csv`: 6 derived theme rows.
- `UE5/SourceAssets/P4/Data/Beta6/P4Beta6_WaveSpawns.csv`: 66 chapter/wave/enemy composition rows.
- `UE5/SourceAssets/P4/Data/Beta6/P4Beta6_ScopeProjection.json`: source hashes, projected rows, totals, unresolved data, and CSV hashes.
- `UE5/SourceAssets/P4/Data/Beta6/P4Beta6_Enemies.csv`: 12 enemy design/runtime projection rows. Only the two accepted Alpha archetypes have complete runtime movement/attack tuning.
- `UE5/SourceAssets/P4/Data/Beta6/P4Beta6_Bosses.csv`: two scoped Boss rows with high-level phase/telegraph/story fields. Attack-specific runtime data is still missing.
- `UE5/SourceAssets/P4/Data/Beta6/P4Beta6_BossAttacks.csv`: eight ordered attack contract rows (four per Boss), with missing damage/geometry/timing/summon values left explicitly unconfigured.
- `UE5/SourceAssets/P4/Data/Beta6/P4Beta6_ContentProjection.json`: source hashes, debut overrides, enemy/Boss rows, and unresolved runtime fields.
- `UE5/SourceAssets/P4/Data/Beta6/P4Beta6_BalanceFixture.csv` and `P4Beta6_BalanceFixture_Waves.csv`: chapter and wave HP, incoming-damage, XP/level, and lower-bound DPS/TTK calculations.
- `UE5/SourceAssets/P4/Data/Beta6/P4Beta6_BalanceFixture.json`: reproducible source record, calculation assumptions, hashes, and unresolved balance inputs.
- `UE5/SourceAssets/P4/Data/Beta6/P4Beta6_ThemeBindingAudit.csv` and `.json`: audit v0.5 verifies all six theme backdrop texture references, the Alpha A2 reference, five production Beta6 texture paths and source hashes, plus review/runtime/prop binding status.

The theme ID rule is `theme_<lowercase scene.base>_<lowercase scene.tint>`. `DesignPropIds` copy design tokens from the chapter JSON; they are not UE asset references. Chapter 1's theme is frozen to the accepted Alpha presentation. Chapters 2–6 now project a production backdrop soft reference, but the compiled DataTable import, scene props ownership/reuse audit, and fixed-camera readability acceptance are still required before their chapters can run.

## Source decisions and gaps

- Chapter 1 JSON has wave totals `3, 4, 5, 5, 3`; the accepted Alpha CSV has `4, 4, 4, 4, 4`. Both total 20, but the accepted Alpha wave composition is authoritative and was used for Chapter 1. The old JSON composition is recorded in the projection manifest and is not copied into the runtime table.
- Chapter JSON v2 does not define `SpawnIntervalSeconds` for Chapters 2–6. Their projected rows use `SpawnIntervalSeconds=0` with `bSpawnIntervalConfigured=false`. Zero is a placeholder and must not drive runtime spawning. Approved interval values are still required by the handoff acceptance criteria.
- The design enemy `debut_chapter` values disagree with the actual six-chapter wave projection for `naizhou` (design 1 / effective 2), `naiping_lei` (1 / 2), `naidun` (2 / 1), and `naixiang` (1 / 5). Both values and the source of the effective value are retained in the enemy projection; the accepted Alpha wave CSV is authoritative for Chapter 1.
- The balance fixture reports a perfect-hit lower bound using the existing 12 damage / 0.59 s Alpha projectile rate, source enemy HP/damage, and current one-XP-per-ordinary-kill implementation. It is not a playtest prediction. Boss XP, retry XP reset, exact Beta upgrade offer count, and attack uptime still need a Beta freeze.
- The P2/P3 prop identifiers remain design candidates. They have not been resolved to source paths, authorization, or approved runtime references.
- The latest art direction requires distinct Chapter 2–6 environments; the v01 color study is superseded. The owner approved all five v02 concepts and confirmed the Chapter 2–6 scene review on 2026-11-12. Their concept textures/materials were imported to UE and five fixed-camera preview maps were saved. Five separate production background textures have since been generated and imported under `/Game/Naiwa/P4/Art/Beta6/Environment/`; see `UE5/SourceAssets/P4/Environment/Beta6/P4_Beta6_EnvironmentArt_manifest_v01.json` and the separate import report. Scope projection v0.4 now writes the accepted Alpha A2 texture for Chapter 1 and the five production textures for Chapters 2–6 into `FNaiwaP4Beta6ThemeRow::BackdropTexture`. The isolated Beta6 test presenter reads the current chapter theme and displays that plate with its 420/50 preview camera. This path awaits UHT/build and DataTable import/readback; it does not change chapter readiness. Chapters 2–6 remain blocked until review-map, prop/source audit, and fixed-camera readability gates are accepted.

## Progress and save schema

`UE5/NaiwaP0/Source/NaiwaRuntime/P4/Progress/NaiwaP4ProgressSubsystem.h` and `.cpp` add a P4-only `UNaiwaP4SaveGame` and `UNaiwaP4ProgressSubsystem`. The dedicated slot is `NaiwaP4Beta6Progress`; no P1/P2/P3 menu save type or slot is referenced. The schema records sequential unlocks, active chapter and entry seed, retry count, ordered acquired card IDs, run metrics, and Beta6 best records. Chapter entry seed uses the design's `run_seed XOR (chapter_index × 0x9E3779B9)` rule. Retry and resume reuse the same chapter entry seed and retain the acquired card list. Failed slot writes roll progress mutations back in memory. Reaching Chapter 6 marks only `bBeta6StageComplete`; it does not mark the ten-chapter story cleared. Resume restarts the saved chapter from its entry seed; mid-wave simulation state is not serialized.

`UE5/NaiwaP0/Source/NaiwaRuntime/P4/Data/NaiwaP4Beta6ChapterCatalog.h` and `.cpp` add a reusable catalog over the six Beta6 tables. It checks row structs/counts, chapter continuity, theme/enemy/Boss/attack references, per-chapter wave totals, and runtime readiness. It also exposes each chapter's sorted wave rows, referenced enemy rows, Boss row, and attacks sorted by authored order, so a runtime director can consume the same validated rows. Chapters with unconfigured spawn intervals, incomplete enemy/Boss tuning, unimplemented enemy behavior IDs, or unapproved theme bindings are rejected by `CanStartChapter`.

`UE5/NaiwaP0/Source/NaiwaRuntime/P4/Progress/NaiwaP4Beta6ChapterFlow.h` and `.cpp` define the shared six-chapter phase flow: run start/resume/retry, chapter readiness gating, wave-to-Boss transition, chapter settlement, sequential unlock, and the Beta6-only endpoint. The retry path checks that its chapter entry seed remains unchanged.

`UNaiwaP4ArenaSubsystem` now has a Beta6 entry path that loads the six-table catalog separately from the legacy W3 preview data, converts the selected chapter's validated enemy rows and wave rows into the fixed-step runtime, and exposes start/resume/retry/next/settlement calls. Wave clears advance the shared phase flow, upgrade choices append to the P4-only save, and chapter settlement updates sequential unlock. Runtime spawn conversion rechecks the tuning flags. Chapter settlement derives cumulative run time from fixed simulation ticks plus previously settled chapter time, rather than requiring a caller-supplied metric.

Chapters with a Boss are explicitly blocked by the catalog because the Beta6 Boss attack-row executor is not connected to Arena yet; this prevents those chapters from falling back to the W3 preview's hard-coded Nai Tong Boss. Enemy behavior IDs beyond the implemented generic chase and front-shield behaviors are also blocked, so ranged, suicide, leap, area, disguise, patrol, lob, and bomb archetypes cannot silently run as chasers. Chapters with incomplete spawn/enemy/theme acceptance data remain blocked as before. The five Beta scene concepts are approved; concept preview textures/materials/maps and five separate production background textures are imported. The v0.5 audit verifies every projected backdrop reference, all five production texture assets/source hashes, and the pending review-map status. The theme-row property and Beta6 test presenter are wired in source but await UHT/build and DataTable import/readback; the readiness catalog still rejects Chapters 2–6. Engineering still owns prop audit and fixed-camera readability review. The owner scene review is approved; the audit reads the separate approval record and keeps Beta runtime readiness false. A separate `ANaiwaP4Beta6TestGameMode` now loads the six expected Beta6 data tables plus the existing P4 upgrade table and starts/resumes the Beta flow without changing a project default or Alpha GameMode. The existing native result panel now has Beta6-specific retry and settlement/continue behavior. The Beta6 gameplay test-map asset still needs to be created/assigned, and the new path has not compiled or been exercised in UE. A new run is persisted before the first chapter readiness check so a blocked chapter can be resumed after its missing data is supplied.

## Native UE assets

The import script is `UE5/Scripts/p4_import_beta6_scope_tables.py`; the build/import runner is `UE5/Scripts/import_p4_beta6_scope_tables.ps1`. Intended assets are:

- `/Game/Naiwa/P4/Data/DT_P4_Beta6_Chapters` — 6 rows.
- `/Game/Naiwa/P4/Data/DT_P4_Beta6_Themes` — 6 rows.
- `/Game/Naiwa/P4/Data/DT_P4_Beta6_WaveSpawns` — 66 rows.
- `/Game/Naiwa/P4/Data/DT_P4_Beta6_Enemies` — 12 design/runtime projection rows.
- `/Game/Naiwa/P4/Data/DT_P4_Beta6_Bosses` — 2 design projection rows.
- `/Game/Naiwa/P4/Data/DT_P4_Beta6_BossAttacks` — 8 attack contract rows.

In addition to row structs and counts, the importer reads the six `BackdropTexture` values back from the theme DataTable and compares their package paths with the source CSV. No successful readback has been recorded yet.

Beta6 scene preview imports are tracked separately from production environment plates and the chapter theme table. Chapters 2–6 have five texture assets under `/Game/Naiwa/P4/Art/Beta6/Scenes`, five materials under `/Game/Naiwa/P4/Materials/Beta6/Scenes`, and maps `L_NaiwaP4_Beta6_02ScenePreview_v01` through `L_NaiwaP4_Beta6_06ScenePreview_v01`. The import report [P4_Beta6_ScenePreview_build_report_v01.json](evidence/Beta6/P4_Beta6_ScenePreview_build_report_v01.json) records the original import-only `maps_saved_pending_visual_review` status. The owner scene approval is recorded separately in [P4_Beta6_SceneReview_Approval_v01.json](evidence/Beta6/P4_Beta6_SceneReview_Approval_v01.json) and reflected by the v0.5 audit. These concept scene plates are not final production art or chapter theme textures.

The separate gameplay test map is planned at `/Game/Naiwa/P4/Maps/L_NaiwaP4_Beta6_InternalTest_v01`; `UE5/Scripts/p4_build_beta6_internal_test_map.ps1` creates it only after the Beta6 native GameMode is compiled and all seven required DataTables exist. It does not modify the five scene preview maps or the project's default GameMode.

Production environment background textures are tracked separately from concept scene previews. Chapters 2–6 each have one texture under `/Game/Naiwa/P4/Art/Beta6/Environment`, with source files, import settings, and SHA-256 values in `UE5/SourceAssets/P4/Environment/Beta6/P4_Beta6_EnvironmentArt_manifest_v01.json`; the import report is `evidence/Beta6/P4_Beta6_EnvironmentArt_import_report_v01.json`. The v0.5 theme audit verifies all five Content assets against their source manifest hashes and checks their soft references in the projected theme CSV. The native theme table has not been reimported or read back after adding the property, and the textures have not passed fixed-camera readability review.

`UE5/Scripts/p4_build_beta6_environment_review_maps.py` and `.ps1` prepare five fixed-camera maps using those production textures. Invocations on 2026-11-12 at 10:47 and 15:33 returned editor exit code 0 but produced empty stdout/stderr and no fresh report or engine log; no material/map creation is claimed. The separate Beta6 gameplay map builder awaits native compilation plus DataTable import.

The Chapter 2 viewport log records a 1280×720 capture request, but the referenced PNG is not present in `Saved/Screenshots`; this is an evidence-archive gap, not a pending owner review. The separate Chapter 2 runtime log loaded `NaiwaP4ArtIntegrationGameMode` and reported missing Alpha combat DataTables; it is not Beta6 gameplay evidence.

These counts are currently CSV projection row counts, not claimed UE asset readbacks. The generated manifest will record native row structs and actual DataTable counts only after the Unreal import report succeeds.

The 2026-11-11 editor build attempts between 21:46 and 23:17 produced no success report. The 21:46 invocation produced no build output for more than ten minutes; later attempts reported that `Build.bat` was already running and waited on the shared engine build lock. On 2026-11-12 at 10:00, 10:14, 10:19, 10:26, 10:36, 10:49, 15:32, 15:47, and 15:56 the formal importer again waited on that lock; each waiting invocation was stopped without terminating the unknown lock holder. A separate P7 cook also wrote its package log during this interval and was left untouched. Process inspection does not expose the lock owner. No successful UHT header, editor DLL, or Beta6 DataTable import/readback has been recorded. See the `P4_Beta6_EditorBuild_*.log` files in `UE5/Records/p4_alpha/evidence/Beta6`; rerun the import runner once the existing build releases the lock.
