using UnrealBuildTool;

public class TTRPGTabletop : ModuleRules
{
    public TTRPGTabletop(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "InputCore", "Slate", "SlateCore", "Json", "ImageWrapper", "ImageCore", "RenderCore", "RHI" });
        PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "Private", "ThirdParty", "Lua"));
        // Lua is compiled as plain C with no dynamic-library or operating-system libraries.
        PrivateDefinitions.Add("LUA_USE_C89");
    }
}
