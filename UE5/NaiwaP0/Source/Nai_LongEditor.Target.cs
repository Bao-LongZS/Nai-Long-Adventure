using UnrealBuildTool;

public class Nai_LongEditorTarget : TargetRules
{
    public Nai_LongEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("NaiwaRuntime");
    }
}
