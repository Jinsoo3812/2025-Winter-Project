using UnrealBuildTool;

public class UI : ModuleRules
{
    public UI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks",
            "UMG",
            "Slate",
            "SlateCore",
            "Winter2025",
            //Enemy, Player 의 Build.cs에서 "UI"를 사용하면 안됨.
            //순환참조문제 발생.
            "Player",
            "Enemy"
        });
    }
}