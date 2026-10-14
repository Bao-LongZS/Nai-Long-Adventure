using UnrealBuildTool;

public class Nai_LongTarget : TargetRules
{
    public Nai_LongTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("NaiwaRuntime");
    }
}
