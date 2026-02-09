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
            "GameplayTags",
            "Winter2025Core",
            "InputCore"
        });

        PublicIncludePaths.AddRange(new string[] {
			"Skill",
			"Skill/Public/Data",
			"Skill/Public/GA",
            "Skill/Public/GA/Task",
            "Skill/Public/Rune"
		});
    }
}
