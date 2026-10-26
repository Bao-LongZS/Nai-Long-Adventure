using UnrealBuildTool;

public class NaiwaRuntime : ModuleRules
{
    public NaiwaRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {
            // PhysicsCore：NaiwaP2PhysicsProfile.h 里用到 ESleepFamily，
            // NaiwaP2StructureBlock 用到 UPhysicalMaterial（摩擦/恢复系数的载体）。
            // 只链 Engine 不够 —— UPhysicalMaterial 的 UClass 符号在 PhysicsCore 里，
            // 不显式依赖会在链接期报 LNK2019 Z_Construct_UClass_UPhysicalMaterial。
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "PhysicsCore",
            "UMG", "Slate", "SlateCore"
        });
        PrivateDependencyModuleNames.AddRange(new[] {
            "Json", "JsonUtilities", "RenderCore", "RHI", "Niagara",
            // P2-ART-INTEG：Geo 风格背景的四层山折线剪影用 ProceduralMeshComponent
            // 在运行时生成三角带，避免为纯色背景引入静态网格资产管线。
            "ProceduralMeshComponent"
        });
    }
}
