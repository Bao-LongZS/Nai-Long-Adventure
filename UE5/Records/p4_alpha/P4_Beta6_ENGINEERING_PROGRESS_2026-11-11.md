# P4 Beta6 Engineering Progress

Date: 2026-11-12

Scope: Frozen Alpha Chapter 1 plus Beta Chapters 2–6. Chapters 7–10 stay in the full ten-chapter plan and are deferred from this package.

## Work packages

| WP | Work | Current state | Preliminary engineering estimate |
|---|---|---|---:|
| E1 | Project chapters, themes, waves, enemy/Boss rows with source hashes and migration decisions | Projection v0.4 includes six chapter backdrop soft references; 6 chapters, 6 themes, 32 waves, 66 wave rows, 12 enemies, 2 Bosses, 8 Boss attacks | 1–2 days |
| E2 | Compile row structs; import and read back all six Beta6 DataTables | `Nai_LongEditor Win64 Development` build succeeded (110/110 actions); all six tables imported and read back with matching row structs, expected row counts, and all six theme texture references | 1–2 days |
| E3 | Data-driven sequential chapter flow, settlement, retry/resume and progress integration | Generic phase flow, P4-only saves, retry/resume, isolated Beta6 test GameMode, chapter settlement/continue UI, and the internal test map are present; Boss execution and runtime acceptance remain | 6–9 days |
| E4 | Runtime tuning for 12 enemies and data-driven attacks/phases for both scoped Bosses | Base rows and eight ordered Boss attack contracts drafted; 10 enemy runtime parameter sets/behavior executors and Boss damage/geometry/timing/summon values remain unresolved | 8–13 days |
| E5 | Bind six theme IDs to approved scene assets/components | Five Beta6 concept previews and five production background textures are imported and audited; six backdrop refs are compiled and read back. Five production fixed-camera review maps are saved. Owner scene review is approved; prop audit and fixed-camera readability review remain | 3–5 days |
| E6 | Chapter DPS/TTK, wave, reward and level fixtures with simulation calibration | Analytical lower-bound fixture generated; deterministic combat calibration remains | 3–5 days |
| E7 | Six-chapter PIE, seed/retry/exit regression and Windows package hash checks | Not started | 5–8 days |
|  | **Total** |  | **27–44 engineering days** |

Estimates exclude art production, non-P4 integration, and waiting time for content decisions or the shared UE build lock. The range is provisional and should be updated after the missing enemy/Boss/runtime data is frozen. No named staffing or calendar dates are inferred.

## Completed artifacts in this handoff slice

- Projection scripts preserve Chapter 1's accepted Alpha wave data and do not edit the source JSON.
- Enemy projection preserves both design and effective first-appearance chapters. Four differences are recorded for `naizhou`, `naiping_lei`, `naidun`, and `naixiang`.
- A separate P4 Beta6 SaveGame/progress subsystem, reusable table catalog, and generic chapter phase state machine are connected to `UNaiwaP4ArenaSubsystem` for table-driven ordinary waves. The save layer stores sequential unlock, chapter entry seed, retry count, ordered upgrades, and stage-best values; Arena restores the chapter entry seed/upgrades and persists subsequent card picks. Save mutations now roll back in-memory state if slot writes fail. The catalog exposes chapter-specific enemy/Boss/wave records in runtime-friendly order and refuses unresolved runtime data, including unimplemented enemy behavior IDs and Boss chapters until their row-driven executor exists. Chapter 6 is a Beta stage endpoint only. An isolated native Beta6 GameMode loads the six scope tables and P4 upgrade table; the result panel now distinguishes Beta6 settlement, next-chapter progression, and same-seed chapter retry. The isolated internal test map is saved at `/Game/Naiwa/P4/Maps/L_NaiwaP4_Beta6_InternalTest_v01`; runtime acceptance remains pending.
- Chapter/wave balance fixtures calculate source-backed HP and nominal damage totals, current Alpha XP progression, and labeled perfect-hit DPS/TTK lower bounds.
- The native DataTable importer verifies source hashes and row counts/structs for chapters, themes, waves, enemies, Bosses, and all eight ordered Boss attacks; it also reads back all six theme `BackdropTexture` columns and checks their projected package paths. The successful import report is `evidence/Beta6/P4_Beta6_DataTableImport.json`. A generator at `UE5/Scripts/p4_build_beta6_environment_review_maps.py` with runner `.ps1` has saved five production fixed-camera review maps; their report leaves readability review pending.
- Theme binding audit supersedes the old v01 color-study mapping with five distinct v02 environments. The owner approved the concepts and confirmed the Chapter 2–6 scene review on 2026-11-12; five concept scene textures/materials and preview maps are present in UE. Five production background textures have been generated and imported under `/Game/Naiwa/P4/Art/Beta6/Environment/`. Scope projection v0.4 writes a `BackdropTexture` soft reference for each chapter theme (accepted Alpha baseline for Chapter 1, production textures for Chapters 2–6); audit v0.5 verifies all six references and five production source hashes. `FNaiwaP4Beta6ThemeRow` and the isolated Beta6 test presenter/camera consume the selected chapter texture and 420/50 preview calibration. Native compilation, DataTable import, and generation of five fixed-camera production review maps succeeded. Chapter 2–6 readiness remains blocked by the catalog status. Prop source audit and 720p/1080p fixed-camera readability review remain. Approval record: `evidence/Beta6/P4_Beta6_SceneReview_Approval_v01.json`; production art manifest: `../../SourceAssets/P4/Environment/Beta6/P4_Beta6_EnvironmentArt_manifest_v01.json`.

## Verification update — 2026-11-12

- Full native `Nai_LongEditor Win64 Development` build completed successfully with 110/110 actions.
- DataTable readback passed: chapters 6, themes 6, wave spawns 66, enemies 12, Bosses 2, Boss attacks 8. All six theme `BackdropTexture` references resolved to the expected package paths.
- Five production environment review maps were saved with `runtime_bound: false`; report status is `review_maps_saved_pending_fixed_camera_readability_review`.
- Internal test map `/Game/Naiwa/P4/Maps/L_NaiwaP4_Beta6_InternalTest_v01` was saved with `NaiwaP4Beta6TestGameMode`, the fixed P4 camera, PlayerStart, and a neutral floor. Report status is `map_saved_pending_runtime_acceptance`; `runtime_played` is false. The map builder/runner now use that status to distinguish completed compile/import prerequisites from remaining runtime acceptance.
- Static source audit confirms the runtime readiness gate is backed by real missing authored data: all five Chapter 2–6 spawn intervals are unconfigured, 10/12 enemy rows lack runtime tuning, both Boss rows are unconfigured for attack execution, and all eight attack rows have `bRuntimeTuningConfigured=false` with required action fields blank.
- Successful UE commandlet logs consistently contain 13 generic `LogAutomationTest: Error: Condition failed` lines near the engine's `UnifiedErrorTest` startup output. The same count appears in the successful DataTable import, review-map, and internal-map runs; those logs have no fatal error or `LogPython`/`LogScript` error, and the map/import reports are complete. No P4 stack trace is present around the lines. Treat this as a recurring engine startup diagnostic baseline and revisit it if a runtime run adds a failure or report regression.

The fixture totals are 7,232 ordinary-enemy HP and 8,448 Boss HP across the six chapters. At the existing 12 damage / 0.59 s projectile rate, perfect-hit combined-health TTK lower bounds are 174.25 s for Chapter 4 and 290.43 s for Chapter 6, against 165 s and 180 s chapter targets. These values omit melee, upgrades, targeting downtime, movement, and Boss behavior; they flag chapters for simulation calibration rather than predict completion time.

## Pending acceptance gates

- Freeze spawn intervals for Chapters 2–6 and runtime movement/range/cooldown/collision tuning for ten enemies; implement their authored behavior IDs instead of mapping them to generic chase.
- Freeze the remaining values in eight Boss attack rows: damage, geometry, timing, recovery, summon IDs/placement, phase deltas, and the structured trigger/effects for Boss gag events.
- Finish the row-driven Boss executor and connect internal-map/UI entry points to `UNaiwaP4ArenaSubsystem`'s Beta6 flow. Wave spawn rows already feed the fixed-step director for Boss-free chapters; combat settlement/upgrades feed progress, and runtime rejects rows whose tuning flags are false.
- Complete the environment prop source/reuse audit and 720p/1080p readability review against the five saved fixed-camera production review maps. The five distinct production environment background textures are imported, the six theme references are read back, and scene concept review is approved. Its requested Chapter 2 1280×720 screenshot was not found in `Saved/Screenshots` and is not attached as evidence.
- Calibrate the analytical fixture in the deterministic simulation, then capture PIE and Windows package regression evidence.

No PIE, package, or runtime pass is claimed by this progress record.
