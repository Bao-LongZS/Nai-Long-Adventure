using UnrealBuildTool;

public class NaiwaRuntime : ModuleRules
{
    public NaiwaRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput"
        });
        PrivateDependencyModuleNames.AddRange(new[] { "Json", "JsonUtilities", "RenderCore", "RHI" });
    }
}
