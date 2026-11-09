using UnrealBuildTool;

public class NaiwaP7Runtime : ModuleRules
{
    public NaiwaP7Runtime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput"
        });
    }
}
