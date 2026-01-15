using UnrealBuildTool;

public class Interface: ModuleRules
{
    public Interface(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayAbilities",
            "GameplayTags",
        });
    }
}