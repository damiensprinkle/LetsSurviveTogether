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
			"AssetRegistry",
			"EngineSettings",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"UMG",
			"Slate",
			"SlateCore"
		});

		// Functional tests start and stop play sessions in the editor (Tests/THFunctionalTests.cpp)
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		PublicIncludePaths.Add(ModuleDirectory);
	}
}
