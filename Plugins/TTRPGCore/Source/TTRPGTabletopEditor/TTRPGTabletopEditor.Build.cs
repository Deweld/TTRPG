using UnrealBuildTool;

public class TTRPGTabletopEditor : ModuleRules
{
    public TTRPGTabletopEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnrealEd", "TTRPGTabletop" });
    }
}
