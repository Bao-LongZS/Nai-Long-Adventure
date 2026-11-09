# P7 CSV 数组导入自动化尝试 · 2026-11-01

## 目标

在不构建 P4/P5 的 P7 隔离 Editor 探针中运行 `Naiwa.P7.Data.CsvArrayImport`，验证 UE 5.8 对 Level 路线/建造位/障碍引用数组、隐藏建造位 struct 数组，以及 Wave 刷怪组 struct 数组的 CSV 导入结果。

## 结果

- 11:13 的 P7 隔离 Editor target 编译成功：`P7_IsolatedEditorBuild_2026-11-01_11-13-12.log`。
- 自动化启动命令返回进程码 0，但 UE 没有生成指定的引擎日志，控制台捕获文件也为空（0 字节）。
- 随后补齐 Level CSV 用例缺失的必需列，并于 11:16 再次通过 P7 隔离 Editor 编译：`P7_IsolatedEditorBuild_2026-11-01_11-16-27.log`。
- 因缺少 `Test Completed` 逐项记录，不能确认测试是否启动或断言是否通过；最终源代码版本不记为测试运行通过。
- `build_p7_isolated_probe.ps1 -Target Editor -RunAutomationTests` 已改为要求非空引擎日志及 CSV 用例成功记录，否则退出失败。探针目录已由脚本清理。

## 11:51 再次尝试

- Enhanced Input 接线修正后，隔离 Editor target 重新通过 25/25 动作，详见 `P7_IsolatedEditorBuild_2026-11-01_11-51-01.log`。
- 随后的 `UnrealEditor-Cmd.exe` 自动化启动仍返回进程码 0，但没有生成引擎日志，控制台捕获为 0 字节；严格执行器生成 [启动状态证据](P7_AutomationStatus_2026-11-01_11-51-01.txt) 并判定失败。
- 该次执行没有任何 `Test Completed` 逐项记录，CSV 数组导入及整套 P7 自动化仍未验证通过；临时探针已清理。

## 接续方式

重新建立可运行 UE Editor 命令行的环境后，执行：

```powershell
.\UE5\Scripts\build_p7_isolated_probe.ps1 -Target Editor -RunAutomationTests
```

该脚本只复制 `NaiwaP7Runtime`，生成的临时工程不含 P4/P5。

## 12:35 隔离探针复验与 12:40 主工程验证

- 自动化无日志的原因分为两部分：受限进程环境会让 `UnrealEditor-Cmd` 以 0 退出但无法初始化；在允许 Editor 启动的环境中，旧脚本又对 `-ExecCmds` / `-TestExit` 多包了一层字面双引号，Editor 启动后停在空闲状态。
- 已修复 `build_p7_isolated_probe.ps1` 的参数传递：去除参数值内部的字面双引号并显式启用日志；通过检查 13 个预期测试的逐项成功记录、失败记录及缺失/额外用例来判定结果。
- 12:35 脚本自身完成 P7-only Editor 构建（25/25 动作）和自动化（13/13 成功）：[构建日志](P7_IsolatedEditorBuild_2026-11-01_12-35-22.log)、[自动化日志](P7_Automation_2026-11-01_12-35-22.log)。
- 12:40 在真实 `UE5/NaiwaP0/Nai_Long.uproject` 上再次执行同一组测试，13/13 成功、0 失败：[主工程自动化日志](P7_Automation_MainProject_2026-11-01_12-40-00.log)。
- 这些自动化使用测试内合成 fixture；成功只验证规则与 CSV 数组解析代码，不代表生产关卡数据、真实 CSV 文件重导入、可玩地图、Standalone/Shipping 或视觉验收完成。

## 16:26 真实 CSVImportFactory 文件导入探针

- 为 LevelRow 和 WaveRow 增加了两个磁盘 CSV fixture，分别覆盖 FIntPoint 数组、FName 数组、嵌套 HiddenBuildSite struct 数组，以及 WaveSpawnGroup struct 数组。它们只用于 parser/import 探针，不代表生产关卡数值。
- 新增 p7_datatable_csv_import_probe.py：通过 CSVImportFactory 和 AssetImportTask 导入临时 DataTable，检查 row struct、列名、行名、数组/嵌套字段读回、保存出的 uasset，再删除临时资产。新增 run_p7_datatable_csv_import_probe.ps1，要求新 Engine log、Python JSON 报告及临时资产清理全部有效后才记 PASS。
- Python 语法和 CSV 文件列数/行数检查通过。执行器报告为 [INCONCLUSIVE_NO_ENGINE_LOG](P7_DataTableCsvRunner_2026-11-01_16-26-00.json)：UnrealEditor-Cmd 退出码 0，但没有非空 Engine log、没有新 Python 报告；同一时段的 P7 Automation 控制命令也静默退出 0，没有测试逐项记录。随后用完整 UnrealEditor.exe 重试仍为 0 退出且无 log/stdout/stderr/Python 报告（[备用启动报告](P7_DataTableCsvFullEditor_2026-11-01_16-28-33.json)）。真实 CSV 导入尚未运行，不记通过。
- 检查确认 /Game/Naiwa/P7/Tests 下没有生成临时资产；Level/Wave 生产 DataTable、真实行数值与运行时装配仍未验收。待 Editor-Cmd 能生成日志后运行 run_p7_datatable_csv_import_probe.ps1。

## 16:31 桌面权限下真实 CSV 导入通过

- 首次执行正确完成了 UE CSVImportFactory 导入，但探针把带引号的 FName struct 文本当成精确字符串比较，造成读回断言误报。已将断言改为同时检查字段标签和值 token，继续保留数组/嵌套字段完整读回检查。
- 使用真实主工程和 UnrealEditor-Cmd 重跑后，LevelRow 与 WaveRow 两个 CSV 均导入 0 个问题、保存为 DataTable 包并读回成功：[运行器报告](P7_DataTableCsvRunner_2026-11-01_16-31-02.json)、[Python 读回报告](P7_DataTableCsvProbe_2026-11-01_16-31-02.json)、[Engine log](P7_DataTableCsvEditorImport_2026-11-01_16-31-02.log)。Level 覆盖 FIntPoint/FName 数组和 HiddenBuildSite struct 数组；Wave 覆盖 WaveSpawnGroup struct 数组。
- 两个临时包分别保存为 3538 与 3064 字节，所有预期列、行名和字段值均被读回；脚本成功删除临时对象，测试目录也已移除。真实 CSV 文件导入能力可记 PASS；这些文件只是解析 fixture，不代表生产关卡数据或运行时装配已经验收。
- 执行 Editor-Cmd 与 Game smoke 所在的桌面权限下，Game 启动仍返回 777006；详见 [Game 启动诊断](P7_GameStartupCrashDiagnosis_2026-11-01.md) 及 16:31:58 报告。

## 16:37 运行器日志观察复验

- 运行器现在把编辑器日志中的未分类 `LogAutomationTest: Error` 行和临时资产删除警告计入独立观察字段，不混入 CSV 探针状态。复跑仍为 PASS、进程码 0，两张表的保存/读回值一致：[运行器报告](P7_DataTableCsvRunner_2026-11-01_16-37-29.json)、[读回报告](P7_DataTableCsvProbe_2026-11-01_16-37-29.json)、[Engine log](P7_DataTableCsvEditorImport_2026-11-01_16-37-29.log)。
- 日志记录了 13 条没有用例名的 `LogAutomationTest: Error: Condition failed`，以及 2 条临时包删除期间的 AssetRegistry 警告。相同的 13 条错误行也出现在 12:35/12:40 的 P7 自动化日志中，而那些运行各有 13 条 `Test Completed`；因此这些行本身不足以推翻 CSV 导入结果，但其具体来源仍未定位。
- 两条 AssetRegistry 警告表示编辑器内存索引认为已删除包又在磁盘上被修改；Editor 退出后的文件检查确认 `/Game/Naiwa/P7/Tests` 目录和两个临时包都不存在。CSV 通过只代表导入、保存与读回成功，不能代表整个 Editor 启动日志无错误，也不覆盖运行时消费。

