// Automated tests: content health after moves, renames and deletes. Replaces running Scripts/Tools/content_verify.py by
// hand. See TESTING.md.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/PackageName.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/UObjectIterator.h"
#include "Enemies/THEnemyAnimSet.h"
#include "Enemies/THEnemyAppearance.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstance.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWave.h"

namespace THContentTests
{
	/**
	 * Missing packages that came broken with the horror template and the packs: editor preview meshes and materials
	 * that were never in this project. A new missing reference outside these fails the test.
	 */
	static const TCHAR* KnownMissing[] = {
		TEXT("/Game/ArchVis/"),
		TEXT("/Game/Characters/Heroes/"),
		TEXT("/Game/FPSTemplate/"),
		TEXT("/Game/Content/Sky/"),
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny"),
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn"),
		// Editing sequences of the template's attack animations (now in _Archive), never included
		TEXT("/Game/Characters/Mannequins/Animations/EditableAnimations/"),
		// Enemy and blood packs (2026-11-09): the vendors' own projects' paths (other zombies of the series not owned, their
		// demo templates), and the packs' old top-level folders from before the move into ThirdParty
		TEXT("/Game/YI_ModularZombies/"),
		TEXT("/Game/YI_BloodDroplets/"),
		TEXT("/Game/YI_BloodDecals/"),
		TEXT("/Game/YI_NPC/"),
		TEXT("/Game/Zombie_01/"),
		TEXT("/Game/ThirdPerson/"),
		TEXT("/Game/StarterContent/"),
		TEXT("/Game/DemoRoom/"),
		TEXT("/Game/Developers/Jeremie/"),
		TEXT("/Game/Blueprints/BP_AreaRectLight"),
		TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny"),
		TEXT("/Game/Characters/Mannequins/Animations/ABP_Quinn"),
		TEXT("/Game/Characters/Mannequins/Rigs/ABP_Manny_PostProcess"),
		TEXT("/Game/Characters/Mannequins/Rigs/ABP_Quinn_PostProcess"),
		TEXT("/Game/Characters/Mannequins/Rigs/CR_Mannequin_BasicFootIK"),
		// Dramatic Death (2026-11-11): its demo mannequin's meshes and rigs still point at the pack's own top-level folder
		// (and its Quinn mesh, never imported); the animations are fine
		TEXT("/Game/DramaticDeath/"),
		// The packs added 2026-11-16: their demo materials point at the authors' own folders (the full MHBOP pack, a test
		// MetaHuman, Vefects' template folder); nothing we use refers to those demos
		TEXT("/Game/MetaHumans/Test1/"),
		TEXT("/Game/MHBOP/"),
		TEXT("/Game/Vefects/VFX_Pack_Name/"),
	};

	static bool IsKnownMissing(const FString& Package)
	{
		for (const TCHAR* Prefix : KnownMissing)
		{
			if (Package.StartsWith(Prefix))
			{
				return true;
			}
		}
		return false;
	}

	static IAssetRegistry& GetRegistry()
	{
		IAssetRegistry& Registry = IAssetRegistry::GetChecked();
		Registry.SearchAllAssets(true);
		return Registry;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHNoRedirectorsTest, "TacticalHorror.Content.NoRedirectors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHNoRedirectorsTest::RunTest(const FString& Parameters)
{
	FARFilter Filter;
	Filter.PackagePaths.Add(TEXT("/Game"));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());
	TArray<FAssetData> Redirectors;
	THContentTests::GetRegistry().GetAssets(Filter, Redirectors);

	for (const FAssetData& Redirector : Redirectors)
	{
		AddError(FString::Printf(TEXT("Redirector left at %s: right-click its folder > Fix Up Redirectors"), *Redirector.PackageName.ToString()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHNoMissingReferencesTest, "TacticalHorror.Content.NoMissingReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHNoMissingReferencesTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& Registry = THContentTests::GetRegistry();

	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(TEXT("/Game"), Assets, true);
	TSet<FName> Packages;
	for (const FAssetData& Asset : Assets)
	{
		Packages.Add(Asset.PackageName);
	}

	int32 KnownCount = 0;
	for (const FName Package : Packages)
	{
		TArray<FName> Dependencies;
		Registry.GetDependencies(Package, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
		for (const FName Dependency : Dependencies)
		{
			const FString Path = Dependency.ToString();
			if (!Path.StartsWith(TEXT("/Game/")) || Packages.Contains(Dependency) || FPackageName::DoesPackageExist(Path))
			{
				continue;
			}
			if (THContentTests::IsKnownMissing(Path))
			{
				++KnownCount;
				continue;
			}
			AddError(FString::Printf(TEXT("%s refers to %s, which doesn't exist"), *Package.ToString(), *Path));
		}
	}
	AddInfo(FString::Printf(TEXT("%d known missing references from the template and packs ignored"), KnownCount));
	return true;
}

// Paths written in our C++ (soft references set in constructors) still point at real content

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHCodeContentPathsTest, "TacticalHorror.Content.CodePathsExist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHCodeContentPathsTest::RunTest(const FString& Parameters)
{
	TSet<FSoftObjectPath> Checked;
	int32 Count = 0;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->GetOutermost()->GetName() != TEXT("/Script/TacticalHorror") || Class->HasAnyClassFlags(CLASS_NewerVersionExists))
		{
			continue;
		}

		const UObject* Defaults = Class->GetDefaultObject();
		for (TFieldIterator<FSoftObjectProperty> Property(Class, EFieldIteratorFlags::ExcludeSuper); Property; ++Property)
		{
			const FSoftObjectPath Path = Property->GetPropertyValue_InContainer(Defaults).ToSoftObjectPath();
			if (Path.IsNull() || Checked.Contains(Path))
			{
				continue;
			}
			Checked.Add(Path);
			++Count;
			TestTrue(*FString::Printf(TEXT("%s.%s: %s exists"), *Class->GetName(), *Property->GetName(), *Path.ToString()),
				FPackageName::DoesPackageExist(Path.GetLongPackageName()));
		}
		// Lists of them too (sounds, blood masks, music)
		for (TFieldIterator<FArrayProperty> Property(Class, EFieldIteratorFlags::ExcludeSuper); Property; ++Property)
		{
			const FSoftObjectProperty* Inner = CastField<FSoftObjectProperty>(Property->Inner);
			if (!Inner)
			{
				continue;
			}
			FScriptArrayHelper Array(*Property, Property->ContainerPtrToValuePtr<void>(Defaults));
			for (int32 Index = 0; Index < Array.Num(); ++Index)
			{
				const FSoftObjectPath Path = Inner->GetPropertyValue(Array.GetRawPtr(Index)).ToSoftObjectPath();
				if (Path.IsNull() || Checked.Contains(Path))
				{
					continue;
				}
				Checked.Add(Path);
				++Count;
				TestTrue(*FString::Printf(TEXT("%s.%s: %s exists"), *Class->GetName(), *Property->GetName(), *Path.ToString()),
					FPackageName::DoesPackageExist(Path.GetLongPackageName()));
			}
		}
	}
	TestTrue(TEXT("Found the C++ content paths (hand flashlight, template pawn)"), Count >= 4);
	return true;
}

// Every piece that can come off an enemy has a material in each slot (developer, 2026-11-15: the Priest's limbs came in
// with none, and fell off in the default grid)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHSeveredPieceMaterialsTest, "TacticalHorror.Content.SeveredPieceMaterials",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHSeveredPieceMaterialsTest::RunTest(const FString& Parameters)
{
	int32 Pieces = 0;
	for (const TCHAR* Path : { TEXT("/Game/Enemies/Appearance/DA_ZombieLooks.DA_ZombieLooks"), TEXT("/Game/Enemies/Appearance/DA_AmbusherLooks.DA_AmbusherLooks") })
	{
		const UTHEnemyAppearance* Looks = LoadObject<UTHEnemyAppearance>(nullptr, Path);
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), Path), Looks))
		{
			continue;
		}
		for (const FTHEnemyLook& Look : Looks->Looks)
		{
			for (const FTHSeverableLimb& Limb : Look.Limbs)
			{
				for (const USkeletalMesh* Mesh : Limb.Meshes)
				{
					if (!Mesh)
					{
						continue;
					}
					++Pieces;
					for (const FSkeletalMaterial& Slot : Mesh->GetMaterials())
					{
						const FString What = FString::Printf(TEXT("%s (%s): slot %s"), *Mesh->GetName(), *Look.Name.ToString(), *Slot.MaterialSlotName.ToString());
						TestNotNull(*(What + TEXT(" has a material")), Slot.MaterialInterface.Get());
						// And not one with its textures all emptied: the pack's own for the Priest's pieces set every texture to
						// none, and the pieces fell black (developer, 2026-11-16). One optional mask left empty is fine (F02's
						// body leaves its colour mask empty)
						for (const UMaterialInstance* Instance = Cast<UMaterialInstance>(Slot.MaterialInterface.Get()); Instance;
							Instance = Cast<UMaterialInstance>(Instance->Parent.Get()))
						{
							int32 Set = 0;
							for (const FTextureParameterValue& Texture : Instance->TextureParameterValues)
							{
								Set += Texture.ParameterValue ? 1 : 0;
							}
							TestTrue(*FString::Printf(TEXT("%s: %s sets some of its %d textures"), *What, *Instance->GetName(),
								Instance->TextureParameterValues.Num()), Instance->TextureParameterValues.IsEmpty() || Set > 0);
						}
					}
				}
			}
		}
	}
	TestTrue(TEXT("Found the limb pieces (setup_dismemberment.py)"), Pieces > 0);
	return true;
}

// Blood on the sirens' faces (developer, 2026-11-16: MetaHuman Blood Overlay; setup_siren_blood.py, re-run after
// setup_metahumans.py, which rebuilds the looks without it)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHSirenBloodTest, "TacticalHorror.Content.SirenBlood",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHSirenBloodTest::RunTest(const FString& Parameters)
{
	const UTHEnemyAppearance* Looks = LoadObject<UTHEnemyAppearance>(nullptr, TEXT("/Game/Enemies/Appearance/DA_MetaHumanLooks.DA_MetaHumanLooks"));
	UMaterialInterface* Bloody = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Enemies/Materials/M_skin_unified_baked_Blood.M_skin_unified_baked_Blood"));
	if (!TestNotNull(TEXT("DA_MetaHumanLooks loads"), Looks) || !TestNotNull(TEXT("The bloodied skin material loads"), Bloody))
	{
		return false;
	}
	for (const FTHEnemyLook& Look : Looks->Looks)
	{
		int32 Wearing = 0;
		for (const FTHOutfitPart& Part : Look.Parts)
		{
			for (const FTHOutfitChoice& Choice : Part.Choices)
			{
				for (const FTHMaterialChoice& Slot : Choice.Materials)
				{
					for (UMaterialInterface* Material : Slot.Materials)
					{
						Wearing += Material && Material->GetBaseMaterial() == Bloody->GetBaseMaterial() ? 1 : 0;
					}
				}
			}
		}
		TestTrue(*FString::Printf(TEXT("Siren look %s has blood on its face (both head slots)"), *Look.Name.ToString()), Wearing >= 2);
	}
	return true;
}

// A zombie with both arms off bites (developer, 2026-11-16: Mixamo's Zombie Neck Bite; setup_enemies.py)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHArmlessAttackTest, "TacticalHorror.Content.ArmlessAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHArmlessAttackTest::RunTest(const FString& Parameters)
{
	const UTHEnemyAnimSet* Zombie = LoadObject<UTHEnemyAnimSet>(nullptr, TEXT("/Game/Enemies/Animations/DA_ZombieAnims.DA_ZombieAnims"));
	if (!TestNotNull(TEXT("DA_ZombieAnims loads"), Zombie))
	{
		return false;
	}
	TestTrue(TEXT("DA_ZombieAnims has an armless attack with its clip"), !Zombie->ArmlessAttacks.IsEmpty()
		&& Zombie->ArmlessAttacks[0].Animation && Zombie->ArmlessAttacks[0].GetDuration() > 0.5f);
	// One index for both lists, as replicated: past the arm attacks are the armless ones
	const FTHEnemyAttack* Bite = Zombie->GetAttack(Zombie->Attacks.Num());
	TestTrue(TEXT("The index after the arm attacks is the first armless one"), Bite && !Zombie->ArmlessAttacks.IsEmpty() && Bite == &Zombie->ArmlessAttacks[0]);
	TestNull(TEXT("Past both lists: none"), Zombie->GetAttack(Zombie->Attacks.Num() + Zombie->ArmlessAttacks.Num()));
	return true;
}

// 3D audio (developer, 2026-11-16: enemy footsteps hard to place on headphones): the Steam Audio plugin renders the enemy
// attenuations binaurally, which works only on mono sounds (make_enemy_sounds_mono.py)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHEnemySpatialAudioTest, "TacticalHorror.Content.EnemySpatialAudio",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHEnemySpatialAudioTest::RunTest(const FString& Parameters)
{
	FString Plugin;
	GConfig->GetString(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("SpatializationPlugin"), Plugin, GEngineIni);
	TestEqual(TEXT("Windows spatialization plugin"), Plugin, FString(TEXT("Steam Audio Spatialization")));
	if (GEngine && GEngine->GetMainAudioDevice())
	{
		TestTrue(TEXT("The audio device loaded the spatialization plugin"), GEngine->GetMainAudioDevice()->IsSpatializationPluginEnabled());
	}

	for (const TCHAR* Path : { TEXT("/Game/Audio/SA_EnemyVoice.SA_EnemyVoice"), TEXT("/Game/Audio/SA_EnemyFootsteps.SA_EnemyFootsteps") })
	{
		const USoundAttenuation* Attenuation = LoadObject<USoundAttenuation>(nullptr, Path);
		if (TestNotNull(*FString::Printf(TEXT("%s loads"), Path), Attenuation))
		{
			TestTrue(*FString::Printf(TEXT("%s is binaural"), Path), Attenuation->Attenuation.SpatializationAlgorithm == ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF);
		}
	}

	// The enemies' own sounds and the charger's stomps (not LightCrackle: the player's own light). One wave a package:
	// an import over an asset can leave the old one behind as SoundWave_0 (make_enemy_sounds_mono.py removes it)
	TArray<FAssetData> Sounds;
	IAssetRegistry::GetChecked().GetAssetsByPath(TEXT("/Game/Audio/Enemies"), Sounds, true);
	int32 Checked = 0;
	for (const FAssetData& Data : Sounds)
	{
		TestEqual(*FString::Printf(TEXT("%s is the package's own asset"), *Data.GetObjectPathString()), Data.AssetName, FName(*FPackageName::GetShortName(Data.PackageName)));
		const USoundWave* Wave = Cast<USoundWave>(Data.GetAsset());
		if (Wave && Data.AssetName != TEXT("LightCrackle"))
		{
			++Checked;
			TestEqual(*FString::Printf(TEXT("%s is mono"), *Wave->GetName()), (int32)Wave->NumChannels, 1);
		}
	}
	TestTrue(TEXT("Found the enemy sounds"), Checked >= 10);
	if (const UTHEnemyAppearance* Mutants = LoadObject<UTHEnemyAppearance>(nullptr, TEXT("/Game/Enemies/Appearance/DA_MutantLooks.DA_MutantLooks")))
	{
		for (const USoundBase* Step : Mutants->FootstepSounds)
		{
			const USoundWave* Wave = Cast<USoundWave>(Step);
			TestTrue(*FString::Printf(TEXT("Stomp %s is a mono wave"), *GetNameSafe(Step)), Wave && Wave->NumChannels == 1);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
