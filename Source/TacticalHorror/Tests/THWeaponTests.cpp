// Automated tests: weapon math and weapon content. Run with Scripts/run_tests.ps1 (or Session Frontend > Automation,
// filter "TacticalHorror"). See TESTING.md.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/IAssetRegistry.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/BlendSpace.h"
#include "Misc/PackageName.h"
#include "Animation/Skeleton.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Player/THBodyAnimSet.h"
#include "Weapons/THHandFlashlight.h"
#include "Weapons/THWeaponBase.h"
#include "Weapons/THWeaponData.h"

namespace THTests
{
	/** Blueprint weapons live under /Game/Weapons; the hand flashlight is a native class without a Blueprint. */
	static TArray<UClass*> GetWeaponClasses()
	{
		IAssetRegistry& Registry = IAssetRegistry::GetChecked();
		Registry.SearchAllAssets(true);

		FARFilter Filter;
		Filter.PackagePaths.Add(TEXT("/Game/Weapons"));
		Filter.bRecursivePaths = true;
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		TArray<FAssetData> Assets;
		Registry.GetAssets(Filter, Assets);

		TArray<UClass*> Classes;
		for (const FAssetData& Asset : Assets)
		{
			const FSoftClassPath ClassPath(Asset.GetSoftObjectPath().ToString() + TEXT("_C"));
			if (UClass* Class = ClassPath.TryLoadClass<ATHWeaponBase>())
			{
				Classes.Add(Class);
			}
		}
		Classes.AddUnique(ATHHandFlashlight::StaticClass());
		return Classes;
	}

	static bool SkeletonsMatch(const USkeleton* A, const USkeleton* B)
	{
		if (A == B)
		{
			return true;
		}
#if WITH_EDITOR
		return A && B && A->IsCompatibleForEditor(B);
#else
		return false;
#endif
	}

	static bool HasBoneOrSocket(const USkeletalMesh* Mesh, FName Name)
	{
		return Mesh && (Mesh->FindSocket(Name) || Mesh->GetRefSkeleton().FindBoneIndex(Name) != INDEX_NONE);
	}

	template <typename T>
	static T* GetSoftProperty(const UObject* Object, FName PropertyName)
	{
		const FSoftObjectProperty* Property = FindFProperty<FSoftObjectProperty>(Object->GetClass(), PropertyName);
		return Property ? Cast<T>(Property->GetPropertyValue_InContainer(Object).LoadSynchronous()) : nullptr;
	}
}

// Unit: the recoil pattern sum, which client and server both use to place every bullet of a spray

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHRecoilPatternTest, "TacticalHorror.Unit.RecoilPattern",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTHRecoilPatternTest::RunTest(const FString& Parameters)
{
	UTHWeaponData* Data = NewObject<UTHWeaponData>();
	auto Expect = [this](const TCHAR* What, const FVector2D& Actual, const FVector2D& Expected)
	{
		TestTrue(*FString::Printf(TEXT("%s: got %s, expected %s"), What, *Actual.ToString(), *Expected.ToString()),
			Actual.Equals(Expected, 1e-4));
	};

	Expect(TEXT("No pattern: no recoil"), Data->GetCumulativeRecoil(5), FVector2D::ZeroVector);

	Data->RecoilPattern = { FVector2D(1.0, 2.0), FVector2D(3.0, 4.0) };
	Data->RecoilScale = 1.0f;
	Expect(TEXT("First bullet goes where you aim"), Data->GetCumulativeRecoil(0), FVector2D::ZeroVector);
	Expect(TEXT("Second bullet: first step"), Data->GetCumulativeRecoil(1), FVector2D(1.0, 2.0));
	Expect(TEXT("Third bullet: both steps"), Data->GetCumulativeRecoil(2), FVector2D(4.0, 6.0));
	Expect(TEXT("Past the end the last step repeats"), Data->GetCumulativeRecoil(3), FVector2D(7.0, 10.0));

	Data->RecoilScale = 0.5f;
	Expect(TEXT("RecoilScale scales the whole pattern"), Data->GetCumulativeRecoil(2), FVector2D(2.0, 3.0));

	Data->RoundsPerMinute = 600.0f;
	TestEqual(TEXT("600 rpm fires every 0.1 s"), Data->GetFireInterval(), 0.1f, 1e-5f);
	return true;
}

// Data: DA_M4's spray matches the CS2 M4A4 points it was built from (Scripts/Tools/set_m4_spray.py).
// If the spray is changed on purpose, update MEASURED here and in that script.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHM4SprayTest, "TacticalHorror.Data.M4SprayMatchesCS2",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHM4SprayTest::RunTest(const FString& Parameters)
{
	const UTHWeaponData* Data = LoadObject<UTHWeaponData>(nullptr, TEXT("/Game/Weapons/Data/DA_M4.DA_M4"));
	if (!TestNotNull(TEXT("DA_M4 loads"), Data))
	{
		return false;
	}

	struct FPoint { int32 Bullet; double Yaw; double Pitch; };
	const FPoint Measured[] = {
		{ 1, 0.000, 0.000 }, { 5, 0.310, 3.951 }, { 10, -1.665, 7.518 }, { 15, 3.547, 7.618 },
		{ 20, 2.559, 8.016 }, { 25, -0.203, 8.717 }, { 30, -0.991, 9.114 },
	};
	for (const FPoint& Point : Measured)
	{
		const FVector2D Position = Data->GetCumulativeRecoil(Point.Bullet - 1);
		TestEqual(FString::Printf(TEXT("Bullet %d yaw"), Point.Bullet), Position.X, Point.Yaw, 0.01);
		TestEqual(FString::Printf(TEXT("Bullet %d pitch"), Point.Bullet), Position.Y, Point.Pitch, 0.01);
	}
	return true;
}

// Data: every weapon is complete and its bone names, sockets and animations match its mesh. Catches renamed or moved
// content, a wrong skeleton after retargeting, and half-set-up new guns.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHWeaponSetupTest, "TacticalHorror.Data.WeaponSetup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHWeaponSetupTest::RunTest(const FString& Parameters)
{
	const TArray<UClass*> Classes = THTests::GetWeaponClasses();
	TestTrue(TEXT("Found the weapon Blueprints"), Classes.Num() >= 3);

	const ATHHandFlashlight* HandFlashlight = GetDefault<ATHHandFlashlight>();
	const FFloatProperty* HandIntensityProperty = FindFProperty<FFloatProperty>(ATHHandFlashlight::StaticClass(), TEXT("Intensity"));
	const float HandFlashlightIntensity = HandIntensityProperty ? HandIntensityProperty->GetPropertyValue_InContainer(HandFlashlight) : 0.0f;

	for (UClass* Class : Classes)
	{
		const ATHWeaponBase* Weapon = Class->GetDefaultObject<ATHWeaponBase>();
		const FString Name = Class->GetName();
		const UTHWeaponData* Data = Weapon->GetWeaponData();
		if (!Data)
		{
			AddError(FString::Printf(TEXT("%s: no WeaponData"), *Name));
			continue;
		}
		const FString Where = FString::Printf(TEXT("%s (%s)"), *Name, *Data->GetName());

		TestNotNull(*(Where + TEXT(": IdleAnimation")), Data->IdleAnimation.Get());
		TestNotNull(*(Where + TEXT(": EquipAnimation")), Data->EquipAnimation.Get());

		if (!Weapon->IsFirearm())
		{
			// The hand flashlight: its arms, model and inspect animation are paths set in C++
			const USkeletalMesh* Arms = THTests::GetSoftProperty<USkeletalMesh>(Weapon, TEXT("ArmsMesh"));
			TestNotNull(*(Where + TEXT(": ArmsMesh loads")), Arms);
			TestNotNull(*(Where + TEXT(": FlashlightMesh loads")), THTests::GetSoftProperty<UStaticMesh>(Weapon, TEXT("FlashlightMesh")));
			TestNotNull(*(Where + TEXT(": InspectAnimation loads")), THTests::GetSoftProperty<UAnimSequenceBase>(Weapon, TEXT("InspectAnimation")));
			if (const FNameProperty* Socket = FindFProperty<FNameProperty>(Class, TEXT("HandSocket")))
			{
				const FName SocketName = Socket->GetPropertyValue_InContainer(Weapon);
				TestTrue(*FString::Printf(TEXT("%s: arms have socket %s"), *Where, *SocketName.ToString()), Arms && Arms->FindSocket(SocketName));
			}
			continue;
		}

		// Ammo and firing
		TestTrue(*(Where + TEXT(": MagazineSize > 0")), Data->MagazineSize > 0);
		TestTrue(*(Where + TEXT(": ReloadDuration > 0")), Data->ReloadDuration > 0.0f);
		TestTrue(*(Where + TEXT(": RoundsPerMinute > 0")), Data->RoundsPerMinute > 0.0f);
		TestTrue(*(Where + TEXT(": Damage > 0")), Data->Damage > 0.0f);
		TestTrue(*(Where + TEXT(": has a fire mode")), !Data->FireModes.IsEmpty());
		TestTrue(*(Where + TEXT(": has fire sounds")), !Data->FireSounds.IsEmpty() && !Data->FireSounds.Contains(nullptr));
		TestNotNull(*(Where + TEXT(": FireAnimation")), Data->FireAnimation.Get());
		TestNotNull(*(Where + TEXT(": ReloadAnimation")), Data->ReloadAnimation.Get());

		// First-person mesh and its animations
		const USkeletalMesh* Mesh = Weapon->GetMesh() ? Weapon->GetMesh()->GetSkeletalMeshAsset() : nullptr;
		if (!TestNotNull(*(Where + TEXT(": first-person mesh")), Mesh))
		{
			continue;
		}
		const USkeleton* Skeleton = Mesh->GetSkeleton();
		for (const UAnimSequenceBase* Animation : { Data->IdleAnimation.Get(), Data->EquipAnimation.Get(), Data->FireAnimation.Get(),
			Data->ReloadAnimation.Get(), Data->ReloadEmptyAnimation.Get(), Data->ThirdPersonPose.Get() })
		{
			if (Animation)
			{
				TestTrue(*FString::Printf(TEXT("%s: %s is on the mesh's skeleton"), *Where, *Animation->GetName()),
					THTests::SkeletonsMatch(Animation->GetSkeleton(), Skeleton));
			}
		}

		// Bone and socket names
		TestTrue(*FString::Printf(TEXT("%s: muzzle '%s' on the mesh"), *Where, *Data->MuzzleSocketName.ToString()),
			THTests::HasBoneOrSocket(Mesh, Data->MuzzleSocketName));
		TestTrue(*FString::Printf(TEXT("%s: grip bone '%s' on the mesh"), *Where, *Data->ThirdPersonGripBone.ToString()),
			THTests::HasBoneOrSocket(Mesh, Data->ThirdPersonGripBone));
		if (!Data->ThirdPersonLeftHandBone.IsNone())
		{
			TestTrue(*FString::Printf(TEXT("%s: left hand bone '%s' on the mesh"), *Where, *Data->ThirdPersonLeftHandBone.ToString()),
				THTests::HasBoneOrSocket(Mesh, Data->ThirdPersonLeftHandBone));
		}

		// Third-person reload magazine
		if (!Data->ThirdPersonMagazineBone.IsNone())
		{
			TestTrue(*FString::Printf(TEXT("%s: magazine bone '%s' on the mesh"), *Where, *Data->ThirdPersonMagazineBone.ToString()),
				THTests::HasBoneOrSocket(Mesh, Data->ThirdPersonMagazineBone));
			TestNotNull(*(Where + TEXT(": ThirdPersonMagazineMesh")), Data->ThirdPersonMagazineMesh.Get());
			TestTrue(*(Where + TEXT(": magazine drops, then is grabbed, then seated (0..1)")),
				Data->MagazineDropAt < Data->MagazineGrabAt && Data->MagazineGrabAt < Data->MagazineSeatAt && Data->MagazineSeatAt <= 1.0f);
		}

		// Body animations others see
		if (TestNotNull(*(Where + TEXT(": BodyAnimSet")), Data->BodyAnimSet.Get()))
		{
			TestNotNull(*(Where + TEXT(": BodyAnimSet Idle")), Data->BodyAnimSet->Idle.Get());
			TestNotNull(*(Where + TEXT(": BodyAnimSet Reload")), Data->BodyAnimSet->Reload.Get());
			TestNotNull(*(Where + TEXT(": BodyAnimSet Equip")), Data->BodyAnimSet->Equip.Get());
		}

		// The weapon light is the dimmer one (GDD: the hand flashlight is much brighter)
		if (Data->WeaponLightMesh)
		{
			TestTrue(*(Where + TEXT(": weapon light dimmer than the hand flashlight")), Data->WeaponLightIntensity < HandFlashlightIntensity);
		}
	}
	return true;
}

// Data: every animation the weapons and body sets play lives in our folders under the naming pattern (ANIMATION_GUIDE.md,
// "Naming and folders"), so packs stay untouched and an animation can be found from the gun's name.
//   first person  /Game/Weapons/<Gun>/Animations/A_FP_<Gun>_*
//   third person  /Game/Animations/ThirdPerson/<Set>/A_TP_<Set>_* and AO_TP_<Set> (and their aim offset samples)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHAnimationNamingTest, "TacticalHorror.Data.AnimationNaming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHAnimationNamingTest::RunTest(const FString& Parameters)
{
	auto CheckAnimation = [this](const UObject* Animation, const FString& UsedBy, const FString& Folder, const FString& Prefix)
	{
		if (!Animation)
		{
			return;
		}
		const FString Package = Animation->GetOutermost()->GetName();
		const FString Name = FPackageName::GetShortName(Package);
		TestTrue(*FString::Printf(TEXT("%s uses %s: expected %s%s*"), *UsedBy, *Package, *Folder, *Prefix),
			Package.StartsWith(Folder) && (Name.StartsWith(Prefix) || Name.StartsWith(TEXT("AO_TP_"))));
		if (const UBlendSpace* Space = Cast<UBlendSpace>(Animation))
		{
			for (const FBlendSample& Sample : Space->GetBlendSamples())
			{
				if (Sample.Animation)
				{
					const FString SamplePackage = Sample.Animation->GetOutermost()->GetName();
					TestTrue(*FString::Printf(TEXT("%s sample %s is in %s"), *Name, *SamplePackage, *Folder), SamplePackage.StartsWith(Folder));
				}
			}
		}
	};

	// First person: each gun's own folder
	for (UClass* Class : THTests::GetWeaponClasses())
	{
		const UTHWeaponData* Data = Class->GetDefaultObject<ATHWeaponBase>()->GetWeaponData();
		if (!Data)
		{
			continue;
		}
		// DA_M4 -> M4, DA_HandFlashlight -> HandFlashlight
		const FString Gun = Data->GetName().RightChop(3);
		const FString Folder = FString::Printf(TEXT("/Game/Weapons/%s/Animations/"), *Gun);
		const FString Prefix = FString::Printf(TEXT("A_FP_%s_"), *Gun);
		for (const UAnimSequenceBase* Animation : { Data->IdleAnimation.Get(), Data->EquipAnimation.Get(), Data->FireAnimation.Get(),
			Data->ReloadAnimation.Get(), Data->ReloadEmptyAnimation.Get() })
		{
			CheckAnimation(Animation, Data->GetName(), Folder, Prefix);
		}
		if (const UTHBodyAnimSet* BodySet = Data->BodyAnimSet)
		{
			TestEqual(*FString::Printf(TEXT("%s's body set is named after it"), *Data->GetName()), BodySet->GetName(), TEXT("DA_Body_") + Gun);
		}
	}
	if (const FSoftObjectProperty* Inspect = FindFProperty<FSoftObjectProperty>(ATHHandFlashlight::StaticClass(), TEXT("InspectAnimation")))
	{
		CheckAnimation(Inspect->GetPropertyValue_InContainer(GetDefault<ATHHandFlashlight>()).LoadSynchronous(), TEXT("ATHHandFlashlight"),
			TEXT("/Game/Weapons/HandFlashlight/Animations/"), TEXT("A_FP_HandFlashlight_"));
	}

	// Third person: every body set, every animation field
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	TArray<FAssetData> Sets;
	Registry.GetAssetsByClass(UTHBodyAnimSet::StaticClass()->GetClassPathName(), Sets);
	TestTrue(TEXT("Found the body sets"), Sets.Num() >= 6);
	for (const FAssetData& Asset : Sets)
	{
		const UTHBodyAnimSet* BodySet = Cast<UTHBodyAnimSet>(Asset.GetAsset());
		if (!BodySet)
		{
			continue;
		}
		// DA_Body_M4_Lowered -> M4
		FString SetName = BodySet->GetName().RightChop(8);
		SetName.RemoveFromEnd(TEXT("_Lowered"));
		const FString Folder = FString::Printf(TEXT("/Game/Animations/ThirdPerson/%s/"), *SetName);
		for (TFieldIterator<FObjectProperty> Property(UTHBodyAnimSet::StaticClass()); Property; ++Property)
		{
			const UObject* Value = Property->GetObjectPropertyValue_InContainer(BodySet);
			if (Value && (Value->IsA<UAnimSequenceBase>() || Value->IsA<UBlendSpace>()))
			{
				// The hand flashlight aims like an unarmed player: it shares the unarmed aim offset
				const bool bSharedUnarmed = Value->GetOutermost()->GetName().StartsWith(TEXT("/Game/Animations/ThirdPerson/Unarmed/AO_TP_Unarmed"));
				CheckAnimation(Value, BodySet->GetName() + TEXT(".") + Property->GetName(),
					bSharedUnarmed ? TEXT("/Game/Animations/ThirdPerson/Unarmed/") : Folder,
					bSharedUnarmed ? TEXT("AO_TP_Unarmed") : FString::Printf(TEXT("A_TP_%s_"), *SetName));
			}
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
