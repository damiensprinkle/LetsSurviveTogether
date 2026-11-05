// Automated tests: content health after moves, renames and deletes. Replaces running Scripts/Tools/content_verify.py by
// hand. See TESTING.md.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/PackageName.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/UObjectIterator.h"

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
	}
	TestTrue(TEXT("Found the C++ content paths (hand flashlight, template pawn)"), Count >= 4);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
