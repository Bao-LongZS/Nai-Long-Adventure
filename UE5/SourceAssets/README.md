# UE5 美术源资产

本目录存放可追溯、可导入的 UE 美术源稿。跨关复用成品归 `Common/`；单关独占原稿按各 P 阶段目录管理，不要为整理而复制进 Common。各阶段资产台账和验收记录是状态依据：P4 见 `UE5/Records/p4_alpha/`，P5/P6 见 `UE5/Records/p5_p6_plan/`。

- `Common/`：已通过验收、至少有两个消费者的项目级共用成品。`CharacterReferences/` 子目录中的用户参考图仅作造型校对，不是运行时图。
- `P4/`：奶蛙竞技场 Alpha 专属源图、逐帧动画、环境适配、HUD/卡图和 manifest。P4 角色资产索引在 `P4/Characters/Alpha/README.md`；新的美术接手者先看 `UE5/Records/p4_alpha/P4_美术资产接手说明_v1.md`。当前 A0/A1 美术视觉已通过，运行时/工程 QA 状态另见 P4 记录。
- `P5/`：玩家可见 4-1 滑雪关专属源资产。
- `P6/`：玩家可见 5-1 破案关专属源资产。
- UE 导入派生件统一进 `UE5/NaiwaP0/Content/Naiwa/{Common,P5,P6}/`；同一源稿只导入一次。
