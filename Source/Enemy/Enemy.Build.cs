using UnrealBuildTool;
public class Enemy : ModuleRules
{
    public Enemy(ReadOnlyTargetRules Target) : base(Target)
    {

        PublicIncludePaths.AddRange(new string[] {
            System.IO.Path.Combine(ModuleDirectory, "Dragon/Public")
        });

        PrivateIncludePaths.AddRange(new string[] {
            System.IO.Path.Combine(ModuleDirectory, "Dragon/Private")
        });

        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { 
            "Core", 
            "CoreUObject", 
            "Engine", 
            "InputCore", 
            "AIModule", 
            "GameplayAbilities", 
            "GameplayTags", 
            "GameplayTasks",
            "MotionWarping"
        });
    }
}
