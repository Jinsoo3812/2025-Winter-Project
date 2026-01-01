using UnrealBuildTool;

public class Skill: ModuleRules
{
    public Skill(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayAbilities",
            "GameplayTasks",
            "GameplayTags"
        });
    }
}