using UnrealBuildTool;

public class Player: ModuleRules
{
    public Player(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayAbilities",
            "GameplayTasks",
            "GameplayTags",
            "Winter2025Core",
            "InputCore",
            "EnhancedInput"
        });
    }
}