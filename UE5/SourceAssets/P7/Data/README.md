# P7 Alpha 技术验证数据 v0.1

此目录的 `P7_Alpha01_*.csv` 是当前 P7 灰盒工程接线用的单一数据源候选：一个 20×11 关卡、A/B 两条共用起终点的预设路线、8 个可见建造位、2 个清障解锁位、3 类塔各 3 级、3 类敌人、2 个障碍和 8 波刷怪表。

这些数值用于接通 DataTable → C++ 校验器 → 灰盒 GameMode/模拟器的真实资产链路，版本状态是 **技术验证候选、未获 G0 设计签收**。5 张表已导入 `/Game/Naiwa/P7/Data/` 并通过主工程自动化与 Development cooked 地图启动；证据见 `UE5/Records/p7_td/evidence/P7_AlphaFixtureImport_2026-11-02_12-10-27.json`、`P7_Automation_MainProject_2026-11-02_12-17-49.json` 和 `P7_MainGameCookedStartup_2026-11-02_12-16-40.json`。负责人、测试 PC、玩家经济节奏与难度仍需项目/设计负责人确认；未确认前不得当作正式平衡数值。CSV 文件名和行名应保持稳定，变更时提升版本并同步校验报告。
