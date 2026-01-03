using UnrealBuildTool;
public class Enemy : ModuleRules
{
    public Enemy(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { 
            "Core", 
            "CoreUObject", 
            "Engine", 
            "InputCore", 
            "AIModule", 
            "GameplayAbilities", 
            "GameplayTags", 
            "GameplayTasks" });
    }
}
