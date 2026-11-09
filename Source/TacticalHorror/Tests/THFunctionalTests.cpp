// Functional tests: play the test map (/Game/Tests/Maps/L_FunctionalTests, made by Scripts/Tools/create_test_map.py)
// in a play session and drive the real player: fire, reload, switch weapons, lights, damage. See TESTING.md.

#include "Misc/AutomationTest.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
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
	static void StartPlay(FAutomationTestBase* Test, const TCHAR* Map = TestMap)
	{
		AutomationOpenMap(Map);
		ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(Test, TEXT("the player to spawn with their loadout"), 60.0, []()
		{
			const UTHWeaponComponent* Weapons = GetWeapons();
			return Weapons && Weapons->GetWeapons().Num() >= 2 && Weapons->GetCurrentWeapon();
		}));
		WaitForDraw();
		Step([Test]()
		{
			const APawn* Player = GetPlayer();
			const ACharacter* Character = Cast<ACharacter>(Player);
			const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
			Test->TestTrue(*FString::Printf(TEXT("Player standing on the floor (at Z %.0f), not falling through it"), Player ? Player->GetActorLocation().Z : -1.0),
				Movement && Movement->IsMovingOnGround());
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

// The enemy test map (build_enemy_test_map.py): players spawn on its floor, and the navigation mesh it builds at
// runtime reaches every area, so enemies can get anywhere

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHEnemyTestMapTest, "TacticalHorror.Functional.EnemyTestMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHEnemyTestMapTest::RunTest(const FString& Parameters)
{
	// Cell centres in the script's layout (4 m tiles), a little above the floor
	static const auto Cell = [](double X, double Y) { return FVector(X * 400.4, Y * 400.4, 50.0); };
	static const TArray<TPair<FString, FVector>> Places = {
		{ TEXT("the hall"), Cell(4.5, 1.0) },
		{ TEXT("the hall's far corner"), Cell(6.0, 0.0) },
		{ TEXT("the side room"), Cell(0.5, 4.5) },
		{ TEXT("the long corridor"), Cell(3.5, 4.0) },
	};

	StartPlay(this, TEXT("/Game/Maps/L_EnemyTest"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(2.0));
	Step([this]()
	{
		const ACharacter* Character = Cast<ACharacter>(GetPlayer());
		TestTrue(TEXT("The player stands on the floor"), Character && Character->GetCharacterMovement()->IsMovingOnGround());
		TestTrue(TEXT("The player starts in the start room"), Character && Character->GetActorLocation().X < 600.0 && Character->GetActorLocation().Y < 600.0); // cells (0..1, 0..1)
	});
	Step([this]()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetPlayWorld());
		AddInfo(FString::Printf(TEXT("Navigation system %s, %d navigation data, %d bounds, building %d"), *GetNameSafe(Navigation),
			Navigation ? Navigation->NavDataSet.Num() : -1, Navigation ? Navigation->GetNavigationBounds().Num() : -1,
			Navigation ? Navigation->IsNavigationBuildInProgress() : -1));
		if (Navigation)
		{
			for (const ANavigationData* Data : Navigation->NavDataSet)
			{
				const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(Data);
				AddInfo(FString::Printf(TEXT("Navigation data %s, runtime generation %d, tiles %d"), *GetNameSafe(Data),
					Data ? (int32)Data->GetRuntimeGenerationMode() : -1, Recast ? Recast->GetNavMeshTilesCount() : -1));
			}
			FNavLocation OnMesh;
			AddInfo(FString::Printf(TEXT("Player at %s, on the navmesh: %d; hall on it: %d"), *GetPlayer()->GetActorLocation().ToCompactString(),
				Navigation->ProjectPointToNavigation(GetPlayer()->GetActorLocation(), OnMesh, FVector(100.0, 100.0, 300.0)),
				Navigation->ProjectPointToNavigation(Cell(4.5, 1.0), OnMesh, FVector(100.0, 100.0, 300.0))));
		}
	});
	// The navigation mesh is generated at runtime
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("navigation from the start room to every area"), 20.0, [this]()
	{
		UWorld* World = GetPlayWorld();
		const FVector Start = GetPlayer()->GetActorLocation();
		for (const TPair<FString, FVector>& Place : Places)
		{
			const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Start, Place.Value);
			if (!Path || !Path->IsValid() || Path->IsPartial())
			{
				return false;
			}
		}
		return true;
	}));
	Step([this]()
	{
		UWorld* World = GetPlayWorld();
		for (const TPair<FString, FVector>& Place : Places)
		{
			const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, GetPlayer()->GetActorLocation(), Place.Value);
			AddInfo(FString::Printf(TEXT("Path to %s: %.0f m"), *Place.Key, Path ? Path->GetPathLength() / 100.0 : -1.0));
		}
		// Walls block it: from the hall's bottom row to the long corridor below it (a wall and an empty cell between) the
		// way is round through the hall's south doorway, far longer than the straight line
		const FVector Hall = Cell(4.0, 2.3);
		const FVector Corridor = Cell(4.0, 3.8);
		const UNavigationPath* Around = UNavigationSystemV1::FindPathToLocationSynchronously(World, Hall, Corridor);
		AddInfo(FString::Printf(TEXT("Hall to the corridor below it: %.1f m round, %.1f m straight"), Around ? Around->GetPathLength() / 100.0 : -1.0,
			FVector::Dist2D(Hall, Corridor) / 100.0));
		TestTrue(TEXT("Paths go round walls through the doorways, not through them"),
			Around && !Around->IsPartial() && Around->GetPathLength() > 1.6 * FVector::Dist2D(Hall, Corridor));
	});
	EndPlay(this);
	return true;
}

// Iron sights: aiming lines each gun's sights up on the middle of the screen, and letting go puts it back at the hip

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHIronSightsTest, "TacticalHorror.Functional.IronSights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHIronSightsTest::RunTest(const FString& Parameters)
{
	StartPlay(this);
	for (const int32 Slot : { 0, 1 })
	{
		Step([Slot]() { GetWeapons()->EquipWeaponAtIndex(Slot); });
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(1.5));
		Step([]() { GetWeapons()->SetAimingForTest(true); });
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.8));
		Step([this]()
		{
			const UTHWeaponComponent* Weapons = GetWeapons();
			const ATHWeaponBase* Gun = Weapons->GetCurrentWeapon();
			const FString Name = GetNameSafe(Gun ? Gun->GetWeaponData() : nullptr);
			FVector RearWorld;
			FVector FrontWorld;
			if (!TestTrue(*(Name + TEXT(" has iron sights")), Weapons->GetSightPoints(RearWorld, FrontWorld)))
			{
				return;
			}
			// In the space of the camera the weapon hangs from: X forward, the screen's middle at Y = Z = 0
			const FTransform& View = Gun->GetRootComponent()->GetAttachParent()->GetComponentTransform();
			const FVector Rear = View.InverseTransformPositionNoScale(RearWorld);
			const FVector Front = View.InverseTransformPositionNoScale(FrontWorld);
			AddInfo(FString::Printf(TEXT("%s aimed: rear sight %s, front sight %s (cm, camera space)"), *Name, *Rear.ToCompactString(), *Front.ToCompactString()));
			TestTrue(*(Name + TEXT(": rear sight on the middle of the screen")), FMath::Abs(Rear.Y) < 0.2 && FMath::Abs(Rear.Z) < 0.2);
			TestTrue(*(Name + TEXT(": front sight on the middle of the screen")), FMath::Abs(Front.Y) < 0.2 && FMath::Abs(Front.Z) < 0.2);
			TestTrue(*(Name + TEXT(": front sight beyond the rear one")), Front.X > Rear.X + 5.0);
			TestTrue(*(Name + TEXT(": rear sight in front of the eye, past the near clip plane")), Rear.X > 2.0);
		});
		Step([]() { GetWeapons()->SetAimingForTest(false); });
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.8));
		Step([this]()
		{
			const ATHWeaponBase* Gun = GetWeapons()->GetCurrentWeapon();
			TestTrue(*(GetNameSafe(Gun->GetWeaponData()) + TEXT(": back at the hip hold after letting go")),
				Gun->GetRootComponent()->GetRelativeTransform().Equals(Gun->GetWeaponData()->CameraHoldOffset, 0.01));
		});
	}
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
// Maps: each playable map spawns the player standing on its floor (catches missing collision on imported levels)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHFiringRangeSpawnTest, "TacticalHorror.Functional.FiringRangeSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHFiringRangeSpawnTest::RunTest(const FString& Parameters)
{
	StartPlay(this, TEXT("/Game/Maps/L_FiringRange"));
	// A couple of seconds more: a player without a floor under them is clearly falling by then
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(2.0));
	Step([this]()
	{
		const ACharacter* Character = Cast<ACharacter>(GetPlayer());
		const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		TestTrue(*FString::Printf(TEXT("Still standing in the firing range after 2 s (Z %.0f)"), Character ? Character->GetActorLocation().Z : -1.0),
			Movement && Movement->IsMovingOnGround());
		// The players start in the room behind the range door (build_range_room.py), west of X -390
		TestTrue(*FString::Printf(TEXT("Starts in the room behind the range door (X %.0f)"), Character ? Character->GetActorLocation().X : 0.0),
			Character && Character->GetActorLocation().X < -400.0);
	});
	EndPlay(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHFiringRangeDoorTest, "TacticalHorror.Functional.FiringRangeDoor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHFiringRangeDoorTest::RunTest(const FString& Parameters)
{
	// The shell Scripts/Tools/build_range_room.py builds. A player-sized sweep straight through the doorway:
	static const FVector DoorwayEast(-300.0, -193.0, -16.0);
	static const FVector DoorwayWest(-500.0, -193.0, -16.0);
	static const FVector RoomCentre(-790.0, -40.0, 0.0);

	static const auto DoorwayBlocked = []()
	{
		FHitResult Hit;
		return GetPlayWorld()->SweepSingleByChannel(Hit, DoorwayEast, DoorwayWest, FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeCapsule(30.0f, 88.0f), FCollisionQueryParams(SCENE_QUERY_STAT(THTestTrace), false, GetPlayer()));
	};

	StartPlay(this, TEXT("/Game/Maps/L_FiringRange"));
	Step([this]()
	{
		UWorld* World = GetPlayWorld();
		const FVector Start = GetPlayer()->GetActorLocation();
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(THTestTrace), false, GetPlayer());
		TestTrue(TEXT("There is a ceiling above the player start"),
			World->LineTraceSingleByChannel(Hit, Start, Start + FVector(0.0, 0.0, 2000.0), ECC_Visibility, Query));
		TestTrue(TEXT("There is a floor in the room behind the door"),
			World->LineTraceSingleByChannel(Hit, RoomCentre, RoomCentre - FVector(0.0, 0.0, 500.0), ECC_Visibility, Query));
		TestTrue(TEXT("There is a ceiling in the room behind the door"),
			World->LineTraceSingleByChannel(Hit, RoomCentre, RoomCentre + FVector(0.0, 0.0, 500.0), ECC_Visibility, Query));
		TestTrue(TEXT("The closed door blocks the doorway"), DoorwayBlocked());

		// Open it the way the player's interact key does: the template's BPI_Interaction Interact event
		AActor* Door = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName().StartsWith(TEXT("BP_Door")))
			{
				Door = *It;
				break;
			}
		}
		UFunction* Interact = Door ? Door->FindFunction(TEXT("Interact")) : nullptr;
		if (!TestNotNull(TEXT("BP_Door is in the level and has the Interact event"), Interact))
		{
			return;
		}
		TArray<uint8> Arguments;
		Arguments.SetNumZeroed(Interact->ParmsSize);
		Interact->InitializeStruct(Arguments.GetData());
		for (TFieldIterator<FObjectProperty> It(Interact); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			if (GetPlayer()->IsA(It->PropertyClass))
			{
				It->SetObjectPropertyValue_InContainer(Arguments.GetData(), GetPlayer());
			}
		}
		Door->ProcessEvent(Interact, Arguments.GetData());
		Interact->DestroyStruct(Arguments.GetData());
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(3.0));
	Step([this]()
	{
		TestFalse(TEXT("The opened door leaves the doorway clear for a player"), DoorwayBlocked());
	});
	EndPlay(this);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR