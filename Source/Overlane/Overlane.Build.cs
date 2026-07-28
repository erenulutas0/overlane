using UnrealBuildTool;

public class Overlane : ModuleRules
{
    public Overlane(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",

            // Session discovery / host / join. The classic OnlineSubsystem path is
            // used rather than the newer OnlineServices because OnlineSubsystemSteam
            // is the mature implementation we intend to swap in for Phase 7.
            "OnlineSubsystem",
            "OnlineSubsystemUtils"
        });
    }
}
