using UnrealBuildTool;

public class TTRPGTabletop : ModuleRules
{
    public TTRPGTabletop(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "InputCore", "Slate", "SlateCore", "Json" });
    }
}
