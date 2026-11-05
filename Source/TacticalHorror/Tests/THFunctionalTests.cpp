// Functional tests: play the test map (/Game/Tests/Maps/L_FunctionalTests, made by Scripts/Tools/create_test_map.py)
// in a play session and drive the real player: fire, reload, switch weapons, lights, damage. See TESTING.md.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Player/Components/THHealthComponent.h"
#include "Tests/AutomationCommon.h"
#include "Weapons/THWeaponBase.h"
#include "Weapons/THWeaponComponent.h"
#include "Weapons/THWeaponData.h"
#include "Tests/THTestHelpers.h"

namespace THFunctional
{
	using namespace THTestHelpers;

	static APlayerController* GetController()
	{
		UWorld* World = GetPlayWorld();
		return World ? World->GetFirstPlayerController() : nullptr;
	}

	static APawn* GetPlayer()
	{
		APlayerController* Controller = GetController();
		return Controller ? Controller->GetPawn() : nullptr;
	}

	static UTHWeaponComponent* GetWeapons()
	{
		APawn* Player = GetPlayer();
		return Player ? Player->FindComponentByClass<UTHWeaponComponent>() : nullptr;
	}

	/** Waits until the held weapon's draw (its EquipDuration, from when this command starts) is over. */
	static void WaitForDraw()
	{
		TSharedRef<double> ReadyTime = MakeShared<double>(-1.0);
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([ReadyTime]()
		{
			const UTHWeaponComponent* Weapons = GetWeapons();
			const ATHWeaponBase* Weapon = Weapons ? Weapons->GetCurrentWeapon() : nullptr;
			if (*ReadyTime < 0.0)
			{
				*ReadyTime = GameTime() + (Weapon && Weapon->GetWeaponData() ? Weapon->GetWeaponData()->EquipDuration : 0.0) + 0.1;
			}
			return !Weapon || GameTime() >= *ReadyTime;
		}));
	}

	/** Opens the test map in a play session and waits for the player and their weapons. */
	static void StartPlay(FAutomationTestBase* Test)
	{
		AutomationOpenMap(TestMap);
		ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(Test, TEXT("the player to spawn with their loadout"), 60.0, []()
		{
			const UTHWeaponComponent* Weapons = GetWeapons();
			return Weapons && Weapons->GetWeapons().Num() >= 2 && Weapons->GetCurrentWeapon();
		}));
		WaitForDraw();
		Step([Test]()
		{
			const APawn* Player = GetPlayer();
			Test->TestTrue(*FString::Printf(TEXT("Player on the floor (Z %.0f), not fallen through it"), Player ? Player->GetActorLocation().Z : -1.0),
				Player && Player->GetActorLocation().Z > 0.0);
		});
	}

	/** What a bullet from the player's view along their aim hits first, for failure messages. */
	static FString DescribeAimTrace()
	{
		APlayerController* Controller = GetController();
		const UTHWeaponComponent* Weapons = GetWeapons();
		if (!Controller || !Weapons)
		{
			return TEXT("no player");
		}
		FVector Location;
		FRotator Rotation;
		Controller->GetPlayerViewPoint(Location, Rotation);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(THTestTrace), false, Controller->GetPawn());
		Params.AddIgnoredActor(Weapons->GetCurrentWeapon());
		FHitResult Hit;
		// ECC_GameTraceChannel1 is the "Weapon" channel bullets use (DefaultEngine.ini)
		if (!Controller->GetWorld()->LineTraceSingleByChannel(Hit, Location, Location + Rotation.Vector() * 10000.0f, ECC_GameTraceChannel1, Params))
		{
			return TEXT("the aim trace hits nothing on the Weapon channel");
		}
		return FString::Printf(TEXT("the aim trace hits %s / %s (bone %s) at %.0f cm"), *GetNameSafe(Hit.GetActor()),
			*GetNameSafe(Hit.GetComponent()), *Hit.BoneName.ToString(), Hit.Distance);
	}
}

using namespace THFunctional;

// Weapons: fire one shot, full auto, reload, switch to the pistol, weapon light, hand flashlight

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHFunctionalWeaponsTest, "TacticalHorror.Functional.Weapons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHFunctionalWeaponsTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		TWeakObjectPtr<ATHWeaponBase> Rifle;
		int32 Ammo = 0;
		int32 Reserve = 0;
		double StartTime = 0.0;
		bool bLightWasOn = false;
	};
	TSharedRef<FState> State = MakeShared<FState>();

	StartPlay(this);

	// One click: one bullet
	Step([this, State]()
	{
		UTHWeaponComponent* Weapons = GetWeapons();
		if (!TestNotNull(TEXT("Player has a weapon component"), Weapons))
		{
			return;
		}
		ATHWeaponBase* Rifle = Weapons->GetWeapons()[0];
		State->Rifle = Rifle;
		TestTrue(TEXT("The first weapon is held"), Weapons->GetCurrentWeapon() == Rifle);
		TestTrue(TEXT("The first weapon is a firearm"), Rifle->IsFirearm());
		TestEqual(TEXT("Starts with a full magazine"), Rifle->GetCurrentAmmo(), Rifle->GetWeaponData()->MagazineSize);

		const int32 Before = Rifle->GetCurrentAmmo();
		Rifle->StartFire();
		Rifle->StopFire();
		TestEqual(TEXT("One click fires one bullet"), Rifle->GetCurrentAmmo(), Before - 1);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.6));

	// Full auto for about half a second: rounds match the fire rate
	Step([State]()
	{
		if (ATHWeaponBase* Rifle = State->Rifle.Get())
		{
			State->Ammo = Rifle->GetCurrentAmmo();
			State->StartTime = GameTime();
			Rifle->StartFire();
		}
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.5));
	Step([this, State]()
	{
		ATHWeaponBase* Rifle = State->Rifle.Get();
		if (!Rifle)
		{
			return;
		}
		Rifle->StopFire();
		const int32 Fired = State->Ammo - Rifle->GetCurrentAmmo();
		const double Expected = (GameTime() - State->StartTime) / Rifle->GetWeaponData()->GetFireInterval() + 1.0;
		TestTrue(*FString::Printf(TEXT("Full auto fired %d rounds, about %.1f expected from the fire rate"), Fired, Expected),
			Fired >= 3 && FMath::Abs(Fired - Expected) <= 2.0);
	});

	// Reload: magazine refilled from the reserve after ReloadDuration
	Step([this, State]()
	{
		ATHWeaponBase* Rifle = State->Rifle.Get();
		if (!Rifle)
		{
			return;
		}
		State->Ammo = Rifle->GetCurrentAmmo();
		State->Reserve = Rifle->GetReserveAmmo();
		Rifle->StartReload();
		TestTrue(TEXT("Reload starts"), Rifle->IsReloading());
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the reload to finish"), 15.0, [State]()
	{
		return !State->Rifle.IsValid() || !State->Rifle->IsReloading();
	}));
	Step([this, State]()
	{
		ATHWeaponBase* Rifle = State->Rifle.Get();
		if (!Rifle)
		{
			return;
		}
		const int32 Magazine = Rifle->GetWeaponData()->MagazineSize;
		TestEqual(TEXT("Magazine full after reloading"), Rifle->GetCurrentAmmo(), Magazine);
		TestEqual(TEXT("Reserve gave exactly the missing rounds"), Rifle->GetReserveAmmo(), State->Reserve - (Magazine - State->Ammo));

		// Reloading a full magazine does nothing
		Rifle->StartReload();
		TestFalse(TEXT("No reload with a full magazine"), Rifle->IsReloading());
	});

	// Switch to the second weapon and fire it
	Step([this]()
	{
		UTHWeaponComponent* Weapons = GetWeapons();
		Weapons->EquipWeaponAtIndex(1);
		TestTrue(TEXT("Switching to slot 2 holds the second weapon"), Weapons->GetCurrentWeapon() == Weapons->GetWeapons()[1]);
	});
	WaitForDraw();
	Step([this]()
	{
		ATHWeaponBase* Pistol = GetWeapons()->GetCurrentWeapon();
		const int32 Before = Pistol->GetCurrentAmmo();
		Pistol->StartFire();
		Pistol->StopFire();
		TestEqual(*FString::Printf(TEXT("%s fires after being drawn"), *Pistol->GetName()), Pistol->GetCurrentAmmo(), Before - 1);
	});

	// Weapon light: back to the rifle, L on and off
	Step([]() { GetWeapons()->EquipWeaponAtIndex(0); });
	WaitForDraw();
	Step([this, State]()
	{
		UTHWeaponComponent* Weapons = GetWeapons();
		TestTrue(TEXT("Weapon light unlocked (bStartWithWeaponLight)"), Weapons->IsWeaponLightUnlocked());
		TestTrue(TEXT("The rifle has a weapon light"), Weapons->GetCurrentWeapon()->HasWeaponLight());
		State->bLightWasOn = Weapons->IsWeaponLightOn();
		Weapons->ToggleWeaponLight();
		TestEqual(TEXT("L switches the weapon light"), Weapons->IsWeaponLightOn(), !State->bLightWasOn);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.2));
	Step([this, State]()
	{
		UTHWeaponComponent* Weapons = GetWeapons();
		TestEqual(TEXT("Weapon light beam shown when on, hidden when off"), IsBeamVisible(Weapons->GetCurrentWeapon()), Weapons->IsWeaponLightOn());
		Weapons->ToggleWeaponLight();
		TestEqual(TEXT("L again switches it back"), Weapons->IsWeaponLightOn(), State->bLightWasOn);
	});

	// Hand flashlight: given, goes in slot 3, beam on, left mouse switches it off
	Step([this]()
	{
		UTHWeaponComponent* Weapons = GetWeapons();
		TestFalse(TEXT("No hand flashlight before picking one up"), Weapons->HasHandFlashlight());
		Weapons->GiveHandFlashlight(false);
		TestTrue(TEXT("Hand flashlight given"), Weapons->HasHandFlashlight());
		TestEqual(TEXT("It takes the third slot"), Weapons->GetWeapons().Num(), 3);
		Weapons->EquipWeaponAtIndex(2);
	});
	WaitForDraw();
	Step([this]()
	{
		ATHWeaponBase* Flashlight = GetWeapons()->GetCurrentWeapon();
		TestFalse(TEXT("The hand flashlight is not a firearm"), Flashlight->IsFirearm());
		TestTrue(TEXT("Flashlight beam on when drawn"), IsBeamVisible(Flashlight));
		Flashlight->StartFire();
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.2));
	Step([this]()
	{
		TestFalse(TEXT("Left mouse switches the flashlight off"), IsBeamVisible(GetWeapons()->GetCurrentWeapon()));
	});

	EndPlay(this);
	return true;
}

// Damage: a shot at the chest of a character standing 5 m away takes health off it

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHFunctionalShotDamageTest, "TacticalHorror.Functional.ShotDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHFunctionalShotDamageTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		TWeakObjectPtr<APawn> Target;
		float Health = 0.0f;
	};
	TSharedRef<FState> State = MakeShared<FState>();

	StartPlay(this);

	// A second character (no controller, like an enemy) 5 m in front, facing the player
	Step([this, State]()
	{
		APawn* Player = GetPlayer();
		const FVector Forward = Player->GetActorForwardVector().GetSafeNormal2D();
		const FVector Location = Player->GetActorLocation() + Forward * 500.0f;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		APawn* Target = Player->GetWorld()->SpawnActor<APawn>(Player->GetClass(), Location, (-Forward).Rotation(), Params);
		if (TestNotNull(TEXT("Target character spawned"), Target))
		{
			State->Target = Target;
			TestNotNull(TEXT("Target has health"), Target->FindComponentByClass<UTHHealthComponent>());
		}
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(1.0));

	// Aim at its chest and fire one shot (first shot standing still: no spread)
	Step([this, State]()
	{
		APawn* Target = State->Target.Get();
		const UTHHealthComponent* Health = Target ? Target->FindComponentByClass<UTHHealthComponent>() : nullptr;
		APlayerController* Controller = GetController();
		if (!Health || !Controller)
		{
			return;
		}
		State->Health = Health->GetHealth();

		FVector Chest = Target->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
		const UTHWeaponComponent* TargetWeapons = Target->FindComponentByClass<UTHWeaponComponent>();
		if (const USkeletalMeshComponent* Body = TargetWeapons ? TargetWeapons->GetBodyMesh() : nullptr)
		{
			if (Body->GetBoneIndex(TEXT("spine_03")) != INDEX_NONE)
			{
				Chest = Body->GetBoneLocation(TEXT("spine_03"));
			}
		}
		FVector ViewLocation;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		Controller->SetControlRotation((Chest - ViewLocation).Rotation());
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
	Step([]()
	{
		ATHWeaponBase* Rifle = GetWeapons()->GetCurrentWeapon();
		Rifle->StartFire();
		Rifle->StopFire();
	});
	TSharedRef<FString> AimTrace = MakeShared<FString>();
	Step([AimTrace]() { *AimTrace = DescribeAimTrace(); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the shot to damage the target"), 3.0, [State]()
	{
		const APawn* Target = State->Target.Get();
		const UTHHealthComponent* Health = Target ? Target->FindComponentByClass<UTHHealthComponent>() : nullptr;
		return Health && Health->GetHealth() < State->Health;
	}));
	Step([this, State, AimTrace]()
	{
		const APawn* Target = State->Target.Get();
		const UTHHealthComponent* Health = Target ? Target->FindComponentByClass<UTHHealthComponent>() : nullptr;
		AddInfo(FString::Printf(TEXT("Target %s at %s, player at %s facing %s; %s"), *GetNameSafe(Target),
			Target ? *Target->GetActorLocation().ToCompactString() : TEXT("-"), *GetPlayer()->GetActorLocation().ToCompactString(),
			*GetController()->GetControlRotation().ToCompactString(), **AimTrace));
		const UTHWeaponData* Data = GetWeapons()->GetCurrentWeapon()->GetWeaponData();
		if (Health && Data)
		{
			const float Damage = State->Health - Health->GetHealth();
			AddInfo(FString::Printf(TEXT("One %s shot at 5 m did %.1f damage"), *Data->GetName(), Damage));
			TestTrue(*FString::Printf(TEXT("Damage is the weapon's (body shot, a little range falloff); %s"), **AimTrace),
				Damage <= Data->Damage * Data->HeadshotMultiplier && Damage >= Data->Damage * 0.9f);
		}
	});

	EndPlay(this);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
