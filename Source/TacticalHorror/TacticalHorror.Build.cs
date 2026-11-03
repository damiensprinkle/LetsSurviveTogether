using UnrealBuildTool;

public class TacticalHorror : ModuleRules
{
	public TacticalHorror(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"Niagara",
			"PhysicsCore",
			"NetCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AnimationCore",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PublicIncludePaths.Add(ModuleDirectory);
	}
}
