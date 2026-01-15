using UnrealBuildTool;

public class Collision: ModuleRules
{
    public Collision(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine"
        });
    }
}