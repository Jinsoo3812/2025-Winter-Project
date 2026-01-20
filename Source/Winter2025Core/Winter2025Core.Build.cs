using UnrealBuildTool;

public class Winter2025Core: ModuleRules
{
    public Winter2025Core(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayAbilities",
            "GameplayTags"
        });

        PublicIncludePaths.AddRange(new string[] {
			"Winter2025Core",
			"Winter2025Core/Public/Interfaces",
			"Winter2025Core/Public/Tags",
            "Winter2025Core/Public/Collision"
		});
    }
}