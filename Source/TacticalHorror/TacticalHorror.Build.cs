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
			"NetCore",
			// Enemies: AI controllers and navigation (Milestone 4)
			"AIModule",
			"NavigationSystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AnimationCore",
			// MetaHuman enemies: the zombie animations retargeted onto their bodies, and their hair (groom)
			"IKRig",
			"HairStrandsCore",
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
