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
            "NavigationSystem",
            "Winter2025Core",
            "RenderCore"
        });

        PublicIncludePaths.AddRange(new string[] {
			"World",
			"World/Public/Blocks",
			"World/Public/Chunk",
            "World/Public/Blocks/GAS",
            "World/Public/Blocks/Preview"
		});
    }
}
