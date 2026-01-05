using UnrealBuildTool;

public class World: ModuleRules
{
    public World(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "GameplayAbilities",
            "GameplayTasks",
            "GameplayTags",
            "NavigationSystem"
        });
    }
}