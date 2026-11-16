// Functional tests: play the test map (/Game/Tests/Maps/L_FunctionalTests, made by Scripts/Tools/create_test_map.py)
// in a play session and drive the real player: fire, reload, switch weapons, lights, damage. See TESTING.md.

#include "Misc/AutomationTest.h"
#include "NavigationPath.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Core/THAmbienceSubsystem.h"
#include "Core/THDirector.h"
#include "Enemies/THLightingSubsystem.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "Enemies/THEnemyCharacter.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/UObjectIterator.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "GroomAsset.h"
#include "Player/Components/THFootstepComponent.h"
#include "GroomComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Player/Components/THHealthComponent.h"
#include "Player/THPlayerController.h"
#include "Player/THCameraModifier.h"
#include "Player/THCharacter.h"
#include "Perception/AISense_Hearing.h"
#include "PhysicsEngine/BodySetup.h"
#include "Sound/SoundBase.h"
#include "UI/THStatusWidget.h"
#include "Settings/THControlsSettingsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Enemies/THEnemyAIController.h"
#include "Enemies/THEnemyAnimInstance.h"
#include "Enemies/THEnemyAnimSet.h"
#include "Enemies/THEnemyBehavior.h"
#include "Multiplayer/THDoorSyncComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Tests/AutomationCommon.h"
#include "Weapons/THWeaponBase.h"
#include "Weapons/THWeaponComponent.h"
#include "Weapons/THImpactSurfaces.h"
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

// Sensitivity: the settings' look sensitivity scales turning, and aim sensitivity scales it further while aiming

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHLookSensitivityTest, "TacticalHorror.Functional.LookSensitivity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHLookSensitivityTest::RunTest(const FString& Parameters)
{
	// The player's own saved values, put back before the session ends (the subsystem saves on shutdown)
	TSharedRef<FVector2f> Saved = MakeShared<FVector2f>(1.0f, 1.0f);
	auto Controls = []() -> UTHControlsSettingsSubsystem*
	{
		const APlayerController* Controller = GetController();
		const UGameInstance* GameInstance = Controller ? Controller->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UTHControlsSettingsSubsystem>() : nullptr;
	};
	// One unit of yaw and pitch input, as the look action gives, and how far it would turn the view
	auto Turn = []() -> FVector2D
	{
		APlayerController* Controller = GetController();
		Controller->RotationInput = FRotator::ZeroRotator;
		Controller->AddYawInput(1.0f);
		Controller->AddPitchInput(1.0f);
		const FVector2D Result(Controller->RotationInput.Yaw, Controller->RotationInput.Pitch);
		Controller->RotationInput = FRotator::ZeroRotator;
		return Result;
	};

	StartPlay(this);
	Step([this, Saved, Controls, Turn]()
	{
		UTHControlsSettingsSubsystem* Settings = Controls();
		if (!TestNotNull(TEXT("Controls settings"), Settings) || !TestNotNull(TEXT("Player controller"), Cast<ATHPlayerController>(GetController())))
		{
			return;
		}
		*Saved = FVector2f(Settings->GetLookSensitivity(), Settings->GetAimSensitivity());
		Settings->SetLookSensitivity(1.0f);
		Settings->SetAimSensitivity(0.5f);
		const FVector2D Base = Turn();
		Settings->SetLookSensitivity(2.0f);
		const FVector2D Doubled = Turn();
		AddInfo(FString::Printf(TEXT("Turn at look 1: yaw %.3f pitch %.3f; at look 2: yaw %.3f pitch %.3f"), Base.X, Base.Y, Doubled.X, Doubled.Y));
		TestTrue(TEXT("Look input turns the view"), !FMath::IsNearlyZero(Base.X) && !FMath::IsNearlyZero(Base.Y));
		TestTrue(TEXT("Look sensitivity 2 turns twice as far"), Doubled.Equals(Base * 2.0, 0.001));
		GetWeapons()->SetAimingForTest(true);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.8));
	Step([this, Controls, Turn]()
	{
		UTHControlsSettingsSubsystem* Settings = Controls();
		if (!Settings)
		{
			return;
		}
		Settings->SetLookSensitivity(1.0f);
		const FVector2D Aimed = Turn();
		Settings->SetAimSensitivity(1.0f);
		const FVector2D AimedFull = Turn();
		AddInfo(FString::Printf(TEXT("Aimed (alpha %.2f): yaw %.3f at aim 0.5, %.3f at aim 1"), GetWeapons()->GetAimAlpha(), Aimed.X, AimedFull.X));
		TestTrue(TEXT("Aim sensitivity 0.5 halves turning while aimed"), Aimed.Equals(AimedFull * 0.5, 0.001));
		GetWeapons()->SetAimingForTest(false);
	});
	Step([Saved, Controls]()
	{
		if (UTHControlsSettingsSubsystem* Settings = Controls())
		{
			Settings->SetLookSensitivity(Saved->X);
			Settings->SetAimSensitivity(Saved->Y);
		}
	});
	EndPlay(this);
	return true;
}

// Mouse: real mouse counts sent through the input path the viewport uses turn UTHControlsSettingsSubsystem::DegreesPerCount
// each (hip and aimed, slow and fast), and a sensitivity converted from CS2 turns as far as it does there

namespace THFunctionalMouse
{
	/** Sends Counts of horizontal mouse movement, PerFrame counts each frame, then waits a few frames for smoothing to settle. */
	static void MoveMouse(int32 Counts, int32 PerFrame)
	{
		TSharedRef<int32> Left = MakeShared<int32>(Counts);
		TSharedRef<int32> Settle = MakeShared<int32>(30);
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Left, Settle, PerFrame]()
		{
			APlayerController* Controller = GetController();
			if (!Controller)
			{
				return true;
			}
			if (*Left > 0)
			{
				const int32 Now = FMath::Min(*Left, PerFrame);
				*Left -= Now;
				const float DeltaTime = Controller->GetWorld()->GetDeltaSeconds();
				// As a 1000 Hz mouse does: several samples a frame (engine mouse smoothing divided by these, turning
				// real mice about an eighth as far; developer, 2026-11-12)
				Controller->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), EKeys::MouseX,
					static_cast<float>(Now), DeltaTime, 8, FPlatformTime::Cycles64()));
				return false;
			}
			return --(*Settle) <= 0;
		}));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHMouseTurnTest, "TacticalHorror.Functional.MouseTurn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHMouseTurnTest::RunTest(const FString& Parameters)
{
	using namespace THFunctionalMouse;
	constexpr int32 Counts = 1000;
	TSharedRef<double> StartYaw = MakeShared<double>(0.0);
	TSharedRef<FVector2f> Saved = MakeShared<FVector2f>(1.0f, 1.0f);
	auto Controls = []() -> UTHControlsSettingsSubsystem*
	{
		const APlayerController* Controller = GetController();
		const UGameInstance* GameInstance = Controller ? Controller->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UTHControlsSettingsSubsystem>() : nullptr;
	};
	// Turns the mouse Counts counts and checks the view turned ExpectedDegrees (within 0.5%)
	auto Measure = [this, StartYaw](const TCHAR* What, int32 PerFrame, double ExpectedDegrees)
	{
		Step([StartYaw]() { *StartYaw = GetController()->GetControlRotation().Yaw; });
		MoveMouse(Counts, PerFrame);
		Step([this, StartYaw, What, PerFrame, ExpectedDegrees]()
		{
			const APlayerController* Controller = GetController();
			const double Turned = FMath::UnwindDegrees(Controller->GetControlRotation().Yaw - *StartYaw);
			AddInfo(FString::Printf(TEXT("%s, %d counts/frame: %d counts turned %.3f deg = %.5f deg/count (camera FOV %.1f)"), What, PerFrame, Counts,
				Turned, Turned / Counts, Controller->PlayerCameraManager ? Controller->PlayerCameraManager->GetFOVAngle() : -1.0f));
			TestTrue(*FString::Printf(TEXT("%s: %d mouse counts turn %.2f deg (turned %.2f)"), What, Counts, ExpectedDegrees, Turned),
				FMath::IsNearlyEqual(Turned, ExpectedDegrees, ExpectedDegrees * 0.005));
		});
	};

	StartPlay(this);
	Step([Saved, Controls]()
	{
		if (UTHControlsSettingsSubsystem* Settings = Controls())
		{
			*Saved = FVector2f(Settings->GetLookSensitivity(), Settings->GetAimSensitivity());
			Settings->SetLookSensitivity(1.0f);
			Settings->SetAimSensitivity(1.0f);
		}
	});
	const double Expected = UTHControlsSettingsSubsystem::DegreesPerCount * Counts;
	Measure(TEXT("Hip, slow"), 5, Expected);
	Measure(TEXT("Hip, fast"), 40, Expected);
	Step([]() { GetWeapons()->SetAimingForTest(true); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.8));
	Measure(TEXT("Aimed, aim sensitivity 1"), 5, Expected);
	Step([]() { GetWeapons()->SetAimingForTest(false); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.8));
	// Converter: CS2 sensitivity 2 turns 0.022 x 2 degrees per count there, so it must here too
	Step([this, Controls]()
	{
		const float Converted = UTHControlsSettingsSubsystem::ConvertFrom(0, 2.0f);
		AddInfo(FString::Printf(TEXT("%s sensitivity 2 converts to %.4f"), *UTHControlsSettingsSubsystem::GetOtherGames()[0].Name.ToString(), Converted));
		Controls()->SetLookSensitivity(Converted);
	});
	Measure(TEXT("Converted from CS2 sensitivity 2"), 5, 0.022 * 2.0 * Counts);
	Step([Saved, Controls]()
	{
		if (UTHControlsSettingsSubsystem* Settings = Controls())
		{
			Settings->SetLookSensitivity(Saved->X);
			Settings->SetAimSensitivity(Saved->Y);
		}
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

// Enemies: a zombie 6 m in front stands on the floor, a body shot takes health and bleeds (no bullet hole), a headshot
// kills it and it falls as a ragdoll

namespace THFunctionalEnemy
{
	/** Made by Scripts/Tools/setup_enemies.py. */
	static const TCHAR* ZombieClass = TEXT("/Game/Enemies/BP_Zombie.BP_Zombie_C");

	/** Points the player's view at the middle of this bone's physics body (a bone's own point is its joint: the head's is at the neck). */
	static void AimAtBone(const ATHEnemyCharacter* Enemy, FName Bone)
	{
		APlayerController* Controller = GetController();
		if (!Enemy || !Controller)
		{
			return;
		}
		FVector ViewLocation;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		Controller->SetControlRotation((BodyCentre(Enemy->GetMesh(), Bone) - ViewLocation).Rotation());
	}

	/**
	 * Aims at the bone every frame until the aim trace is on it (the view follows a frame later, and the idle sway moves
	 * the bone), up to 2 s. The command after it then fires in the same frame the aim was confirmed. Takes a getter
	 * because the zombie is spawned by an earlier command, after this one is queued.
	 */
	static void AimUntilOnBone(FAutomationTestBase* Test, TFunction<ATHEnemyCharacter*()> GetEnemy, FName Bone)
	{
		const FString Wanted = FString::Printf(TEXT("(bone %s)"), *Bone.ToString());
		ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(Test, FString::Printf(TEXT("the aim trace to be on the zombie's %s"), *Bone.ToString()), 2.0, [GetEnemy, Bone, Wanted]()
		{
			if (DescribeAimTrace().Contains(Wanted))
			{
				return true;
			}
			AimAtBone(GetEnemy(), Bone);
			return false;
		}));
	}

	/** The surface the player's aim trace hits (an EPhysicalSurface), and the actor. */
	static EPhysicalSurface AimSurface(AActor*& OutActor)
	{
		APlayerController* Controller = GetController();
		FVector Location;
		FRotator Rotation;
		Controller->GetPlayerViewPoint(Location, Rotation);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(THTestTrace), false, Controller->GetPawn());
		Params.AddIgnoredActor(GetWeapons()->GetCurrentWeapon());
		Params.bReturnPhysicalMaterial = true;
		FHitResult Hit;
		Controller->GetWorld()->LineTraceSingleByChannel(Hit, Location, Location + Rotation.Vector() * 10000.0f, ECC_GameTraceChannel1, Params);
		OutActor = Hit.GetActor();
		return UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
	}

	/** Decals within Radius of Point (bullet holes, blood). */
	static int32 CountDecalsNear(UWorld* World, const FVector& Point, float Radius)
	{
		int32 Count = 0;
		for (TObjectIterator<UDecalComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && FVector::Dist(It->GetComponentLocation(), Point) < Radius)
			{
				++Count;
			}
		}
		return Count;
	}

	/** Particle systems playing in the world whose asset name contains Name. */
	static int32 CountEffects(UWorld* World, const TCHAR* Name)
	{
		int32 Count = 0;
		for (TObjectIterator<UNiagaraComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->GetAsset() && It->GetAsset()->GetName().Contains(Name))
			{
				++Count;
			}
		}
		return Count;
	}

	static void FireOnce()
	{
		ATHWeaponBase* Gun = GetWeapons()->GetCurrentWeapon();
		Gun->StartFire();
		Gun->StopFire();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHEnemyHitTest, "TacticalHorror.Functional.EnemyHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHEnemyHitTest::RunTest(const FString& Parameters)
{
	using namespace THFunctionalEnemy;

	struct FState
	{
		TWeakObjectPtr<ATHEnemyCharacter> Enemy;
		float Health = 0.0f;
		FVector Chest = FVector::ZeroVector;
		int32 DecalsAtChest = 0;
		int32 Wounds = 0;
		double PelvisZ = 0.0;
	};
	TSharedRef<FState> State = MakeShared<FState>();

	StartPlay(this);

	Step([this, State]()
	{
		UClass* Class = LoadClass<ATHEnemyCharacter>(nullptr, ZombieClass);
		if (!TestNotNull(TEXT("The zombie Blueprint loads (setup_enemies.py)"), Class))
		{
			return;
		}
		APawn* Player = GetPlayer();
		const FVector Forward = Player->GetActorForwardVector().GetSafeNormal2D();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		State->Enemy = Player->GetWorld()->SpawnActor<ATHEnemyCharacter>(Class, Player->GetActorLocation() + Forward * 600.0f,
			(-Forward).Rotation(), Params);
		TestTrue(TEXT("Zombie spawned"), State->Enemy.IsValid());
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(1.5));

	// Standing on the floor, its feet at the bottom of its capsule, with a physics asset (bullets hit bones)
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		if (!Enemy)
		{
			return;
		}
		const USkeletalMeshComponent* Body = Enemy->GetMesh();
		TestNotNull(TEXT("The zombie has a body"), Body->GetSkeletalMeshAsset());
		TestNotNull(TEXT("The zombie's body has a physics asset"), Body->GetPhysicsAsset());
		TestTrue(TEXT("The zombie stands on the floor"), Enemy->GetCharacterMovement()->IsMovingOnGround());
		const double CapsuleBottom = Enemy->GetActorLocation().Z - Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const double FootZ = FMath::Min(Body->GetBoneLocation(TEXT("ball_l")).Z, Body->GetBoneLocation(TEXT("ball_r")).Z);
		AddInfo(FString::Printf(TEXT("Capsule bottom %.0f, lower ball of the foot %.0f, head %.0f"), CapsuleBottom, FootZ,
			Body->GetBoneLocation(TEXT("head")).Z));
		TestTrue(TEXT("Its feet are on the floor (balls of the feet 0 to 20 cm above the capsule's bottom)"),
			FootZ > CapsuleBottom - 2.0 && FootZ < CapsuleBottom + 20.0);
		// The bones alone don't show a missing body part (the YI M03 meshes are torso and up on a full skeleton):
		// the mesh's geometry has to reach from the feet to the head
		if (const USkeletalMesh* Asset = Body->GetSkeletalMeshAsset())
		{
			const FBox Geometry = Asset->GetImportedBounds().GetBox();
			AddInfo(FString::Printf(TEXT("Body geometry from %.0f to %.0f cm (mesh space, feet at 0)"), Geometry.Min.Z, Geometry.Max.Z));
			TestTrue(TEXT("The body's geometry reaches the feet (lowest point within 15 cm of the floor): it has legs"), Geometry.Min.Z < 15.0);
			TestTrue(TEXT("The body's geometry reaches the head (over 150 cm)"), Geometry.Max.Z > 150.0);
		}
		TestEqual(TEXT("It has full health"), Enemy->GetHealthComponent()->GetHealth(), Enemy->GetHealthComponent()->GetMaxHealth());
	});

	// A body shot: the bullet finds flesh, takes the gun's damage, bleeds, leaves no bullet hole on the body
	Step([this, State]()
	{
		ATHEnemyCharacter* Enemy = State->Enemy.Get();
		if (!Enemy)
		{
			return;
		}
		State->Chest = Enemy->GetMesh()->GetBoneLocation(TEXT("spine_02"));
		State->Health = Enemy->GetHealthComponent()->GetHealth();
	});
	AimUntilOnBone(this, [State]() { return State->Enemy.Get(); }, TEXT("spine_02"));
	Step([this, State]()
	{
		AActor* HitActor = nullptr;
		const EPhysicalSurface Surface = AimSurface(HitActor);
		AddInfo(FString::Printf(TEXT("Body shot: %s"), *DescribeAimTrace()));
		TestTrue(*FString::Printf(TEXT("The aim trace hits the zombie (%s)"), *DescribeAimTrace()), HitActor == State->Enemy.Get());
		TestEqual(TEXT("It hits flesh (surface type 7, PM_Flesh on its bones)"), (int32)Surface, (int32)SurfaceType7);
		State->DecalsAtChest = CountDecalsNear(GetPlayer()->GetWorld(), State->Chest, 40.0f);
		State->Wounds = CountEffects(GetPlayer()->GetWorld(), TEXT("Wound"));
		FireOnce();
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the body shot to damage the zombie"), 3.0, [State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		return Enemy && Enemy->GetHealthComponent()->GetHealth() < State->Health;
	}));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const UTHWeaponData* Data = GetWeapons()->GetCurrentWeapon()->GetWeaponData();
		if (!Enemy || !Data)
		{
			return;
		}
		const float Damage = State->Health - Enemy->GetHealthComponent()->GetHealth();
		AddInfo(FString::Printf(TEXT("Body shot did %.1f of %.0f"), Damage, Enemy->GetHealthComponent()->GetMaxHealth()));
		TestTrue(TEXT("A body shot does the gun's damage (not a headshot)"), Damage >= Data->Damage * 0.9f && Damage <= Data->Damage);
		TestFalse(TEXT("The zombie is still alive"), Enemy->IsDead());
		UWorld* World = GetPlayer()->GetWorld();
		// Niagara culls effects in a run with no rendering, so only the setup is checked; the count is for information
		AddInfo(FString::Printf(TEXT("Wound effects playing: %d (before the shot %d)"), CountEffects(World, TEXT("Wound")), State->Wounds));
		// The droplets collide on BloodDroplets, which living bodies' capsules ignore (on WorldStatic they hung on them).
		// The effects the shot spawned are culled in a run with no rendering, so spawn each (not activated) and set it
		// the way the gun does
		const FTHSurfaceImpact* Blood = Data->ImpactSurfaces ? &Data->ImpactSurfaces->GetImpact(SurfaceType7) : nullptr;
		TestTrue(TEXT("Flesh sets the effects' droplet channel to BloodDroplets (setup_impact_decals.py)"),
			Blood && Blood->bSetEffectCollisionChannel && Blood->EffectCollisionChannel == ECC_GameTraceChannel2);
		for (UNiagaraSystem* System : { Blood ? Blood->Effect.Get() : nullptr, Blood ? Blood->ExitEffect.Get() : nullptr })
		{
			UNiagaraComponent* Effect = System ? UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, State->Chest, FRotator::ZeroRotator,
				FVector::OneVector, true, /*bAutoActivate*/ false) : nullptr;
			if (!Effect)
			{
				continue;
			}
			TestTrue(*FString::Printf(TEXT("%s has a CollisionChannel parameter to set"), *System->GetName()),
				UTHImpactSurfaces::SetEffectCollisionChannel(Effect, ECC_GameTraceChannel2));
			for (const FNiagaraVariableWithOffset& Variable : Effect->GetOverrideParameters().ReadParameterVariables())
			{
				if (Variable.GetName() == TEXT("User.CollisionChannel"))
				{
					const int32 Channel = *reinterpret_cast<const int32*>(Effect->GetOverrideParameters().GetParameterData(Variable.Offset, Variable.GetType()));
					TestEqual(*FString::Printf(TEXT("%s's droplets collide on BloodDroplets"), *System->GetName()), Channel, (int32)ECC_GameTraceChannel2);
				}
			}
			Effect->DestroyComponent();
		}
		TestEqual(TEXT("No bullet hole on the body"), CountDecalsNear(World, State->Chest, 40.0f), State->DecalsAtChest);
		const UTHImpactSurfaces* Surfaces = Data->ImpactSurfaces;
		const FTHSurfaceImpact* Flesh = Surfaces ? &Surfaces->GetImpact(SurfaceType7) : nullptr;
		TestTrue(TEXT("Flesh has body-shot and headshot sounds, players' bodies their own, faded with distance (setup_impact_decals.py)"),
			Flesh && !Flesh->Sounds.IsEmpty() && !Flesh->HeadshotSounds.IsEmpty() && !Surfaces->PlayerHitSounds.IsEmpty() && Surfaces->SoundAttenuation);
		TestTrue(TEXT("Flesh has entrance and exit wound effects and blood splatter, and no bullet holes"),
			Flesh && Flesh->Effect && Flesh->ExitEffect && !Flesh->SplatterDecals.IsEmpty() && Flesh->Decals.IsEmpty());
	});

	// A headshot kills it (gun damage x headshot multiplier); it drops as a ragdoll
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.6));
	Step([State]()
	{
		ATHEnemyCharacter* Enemy = State->Enemy.Get();
		State->PelvisZ = Enemy ? Enemy->GetMesh()->GetBoneLocation(TEXT("pelvis")).Z : 0.0;
	});
	// Enemies have 200 health (twice a player's): an M4 headshot does 30 x 4, so it takes two
	for (int32 Shot = 1; Shot <= 2; ++Shot)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the aim trace to be on the zombie's head"), 2.0, [State]()
		{
			ATHEnemyCharacter* Enemy = State->Enemy.Get();
			if (!Enemy || Enemy->IsDead() || DescribeAimTrace().Contains(TEXT("(bone head)")))
			{
				return true;
			}
			AimAtBone(Enemy, TEXT("head"));
			return false;
		}));
		Step([this, State, Shot]()
		{
			const ATHEnemyCharacter* Enemy = State->Enemy.Get();
			if (!Enemy || Enemy->IsDead())
			{
				return;
			}
			const FString Trace = DescribeAimTrace();
			AddInfo(FString::Printf(TEXT("Headshot %d: %s (health %.0f)"), Shot, *Trace, Enemy->GetHealthComponent()->GetHealth()));
			TestTrue(*FString::Printf(TEXT("The aim trace hits the head (%s)"), *Trace), Trace.Contains(TEXT("(bone head)")));
			FireOnce();
		});
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.4));
	}
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("two headshots to kill the zombie"), 3.0, [State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		return Enemy && Enemy->IsDead();
	}));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		if (!Enemy)
		{
			return;
		}
		TestEqual(TEXT("Dead (no downed state for enemies)"), Enemy->GetHealthComponent()->GetHealthState(), ETHHealthState::Dead);
		TestTrue(TEXT("It became a ragdoll"), Enemy->IsRagdoll() && Enemy->GetMesh()->IsSimulatingPhysics());
		TestFalse(TEXT("Its capsule no longer blocks"), Enemy->GetCapsuleComponent()->IsCollisionEnabled());
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(2.0));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		if (!Enemy)
		{
			return;
		}
		const double PelvisZ = Enemy->GetMesh()->GetBoneLocation(TEXT("pelvis")).Z;
		AddInfo(FString::Printf(TEXT("Pelvis from Z %.0f to %.0f"), State->PelvisZ, PelvisZ));
		TestTrue(TEXT("The body fell (pelvis 40 cm lower or more)"), PelvisZ < State->PelvisZ - 40.0);
		const double FloorZ = Enemy->GetActorLocation().Z - Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		TestTrue(TEXT("It lies on the floor, not through it"), PelvisZ > FloorZ - 5.0);
	});

	EndPlay(this);
	return true;
}

// Line-up: one of every zombie and creature body we have (L_EnemyTest's side room, setup_enemies.py) works as an enemy:
// a body with a physics asset, feet on the floor, the idle animation moving it (most play the YI zombie idle on another
// skeleton), bullets hit it on flesh, and a killing hit drops it as a ragdoll onto the floor

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHEnemyLineupTest, "TacticalHorror.Functional.EnemyLineup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHEnemyLineupTest::RunTest(const FString& Parameters)
{
	struct FBody
	{
		TWeakObjectPtr<ATHEnemyCharacter> Enemy;
		FString Name;
		bool bCrawler = false;
		FVector HeadStart = FVector::ZeroVector;   // component space, to see the animation move it
		double PelvisZ = 0.0;
		double FloorZ = 0.0;
	};
	TSharedRef<TArray<FBody>> Bodies = MakeShared<TArray<FBody>>();
	static const FName LineupTag(TEXT("TH_Lineup"));
	constexpr int32 Expected = 21;   // setup_enemies.py's LINEUP

	StartPlay(this, TEXT("/Game/Maps/L_EnemyTest"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(1.0));

	// What each body is made of, and where its feet are
	Step([this, Bodies]()
	{
		for (TActorIterator<ATHEnemyCharacter> It(GetPlayWorld()); It; ++It)
		{
			if (!It->ActorHasTag(LineupTag))
			{
				// The hall's hunting / stalking / ambushing ones stay out of it
				It->SetBehavior(nullptr);
				continue;
			}
			FBody& Entry = Bodies->AddDefaulted_GetRef();
			Entry.Enemy = *It;
			Entry.Name = It->Tags.Num() > 1 ? It->Tags[1].ToString() : It->GetName();
			Entry.bCrawler = Entry.Name.StartsWith(TEXT("M03"));
			const USkeletalMeshComponent* Body = It->GetMesh();
			Entry.HeadStart = Body->GetBoneLocation(TEXT("head"), EBoneSpaces::ComponentSpace);
			Entry.FloorZ = It->GetActorLocation().Z - It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

			const USkeletalMesh* Mesh = Body->GetSkeletalMeshAsset();
			const UPhysicsAsset* Physics = Body->GetPhysicsAsset();
			const FBox Geometry = Mesh ? Mesh->GetImportedBounds().GetBox() : FBox(ForceInit);
			const double FootZ = FMath::Min(Body->GetBoneLocation(TEXT("ball_l")).Z, Body->GetBoneLocation(TEXT("ball_r")).Z);
			AddInfo(FString::Printf(TEXT("%s: mesh %s, physics %s (%d bodies), geometry %.0f to %.0f cm, feet %.0f cm above the floor, head %.0f"),
				*Entry.Name, *GetNameSafe(Mesh), *GetNameSafe(Physics), Physics ? Physics->SkeletalBodySetups.Num() : 0,
				Geometry.Min.Z, Geometry.Max.Z, FootZ - Entry.FloorZ, Body->GetBoneLocation(TEXT("head")).Z - Entry.FloorZ));
			TestNotNull(*(Entry.Name + TEXT(" has a mesh")), Mesh);
			TestTrue(*(Entry.Name + TEXT(" has a physics asset with bodies")), Physics && Physics->SkeletalBodySetups.Num() > 1);
			TestTrue(*(Entry.Name + TEXT(" plays its animation set's idle (UTHEnemyAnimInstance)")),
				Cast<UTHEnemyAnimInstance>(Body->GetAnimInstance()) && It->GetAnimations() && It->GetAnimations()->Idle);
			TestNull(*(Entry.Name + TEXT(" has no behaviour (stands still)")), It->GetBehavior());
			TestTrue(*(Entry.Name + TEXT(": its geometry reaches over 60 cm (a body, not a stray part)")), Geometry.Max.Z > 60.0);
			if (!Entry.bCrawler)
			{
				TestTrue(*(Entry.Name + TEXT(": feet on the floor (balls of the feet -5 to 25 cm above the capsule's bottom)")),
					FootZ > Entry.FloorZ - 5.0 && FootZ < Entry.FloorZ + 25.0);
			}
		}
		TestEqual(TEXT("Every body in the line-up is there"), Bodies->Num(), Expected);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.7));

	// The idle moves it; a bullet trace from in front hits it on flesh
	Step([this, Bodies]()
	{
		UWorld* World = GetPlayWorld();
		for (FBody& Entry : *Bodies)
		{
			ATHEnemyCharacter* Enemy = Entry.Enemy.Get();
			if (!Enemy)
			{
				continue;
			}
			const USkeletalMeshComponent* Body = Enemy->GetMesh();
			const double Moved = FVector::Dist(Body->GetBoneLocation(TEXT("head"), EBoneSpaces::ComponentSpace), Entry.HeadStart);
			TestTrue(*FString::Printf(TEXT("%s: the idle animation moves it (head moved %.2f cm in 0.7 s)"), *Entry.Name, Moved), Moved > 0.2);

			// Proportions: the animated bones' lengths against the body's own reference pose. An animation made for another
			// skeleton can bring that skeleton's bone lengths with it, stretching or squashing the body.
			{
				const FReferenceSkeleton& Reference = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
				const TArray<FTransform> Pose = Enemy->GetMesh()->GetBoneSpaceTransforms();
				double WorstRatio = 1.0;
				FName WorstBone;
				for (const TCHAR* Bone : { TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("neck_01"), TEXT("head"), TEXT("clavicle_l"),
					TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l") })
				{
					const int32 Index = Reference.FindBoneIndex(Bone);
					const double RefLength = Index != INDEX_NONE ? Reference.GetRefBonePose()[Index].GetTranslation().Size() : 0.0;
					if (RefLength < 1.0 || !Pose.IsValidIndex(Index))
					{
						continue;
					}
					const double Ratio = Pose[Index].GetTranslation().Size() / RefLength;
					if (FMath::Abs(Ratio - 1.0) > FMath::Abs(WorstRatio - 1.0))
					{
						WorstRatio = Ratio;
						WorstBone = Bone;
					}
				}
				AddInfo(FString::Printf(TEXT("%s: bone lengths animated / own: worst %.2f (%s)"), *Entry.Name, WorstRatio, *WorstBone.ToString()));
				TestTrue(*FString::Printf(TEXT("%s keeps its own proportions (bone lengths within 10%% of its own; worst %.2f at %s)"), *Entry.Name,
					WorstRatio, *WorstBone.ToString()), FMath::Abs(WorstRatio - 1.0) <= 0.1);
			}

			const FVector Target = BodyCentre(Body, TEXT("spine_02"));
			const FVector Forward = Enemy->GetActorForwardVector();
			FCollisionQueryParams Params(SCENE_QUERY_STAT(THLineupTrace), false);
			Params.bReturnPhysicalMaterial = true;
			FHitResult Hit;
			World->LineTraceSingleByChannel(Hit, Target + Forward * 150.0, Target - Forward * 50.0, ECC_GameTraceChannel1, Params);
			const EPhysicalSurface Surface = Hit.PhysMaterial.IsValid() ? Hit.PhysMaterial->SurfaceType.GetValue() : SurfaceType_Default;
			TestTrue(*FString::Printf(TEXT("%s: a shot at its chest hits it (hit %s, bone %s)"), *Entry.Name, *GetNameSafe(Hit.GetActor()), *Hit.BoneName.ToString()),
				Hit.GetActor() == Enemy && !Hit.BoneName.IsNone());
			TestEqual(*(Entry.Name + TEXT(": the hit is on flesh (surface type 7)")), (int32)Surface, (int32)SurfaceType7);

			// A killing hit along the same line
			Entry.PelvisZ = Body->GetBoneLocation(TEXT("pelvis")).Z;
			UGameplayStatics::ApplyPointDamage(Enemy, 1000.0f, -Forward, Hit, nullptr, nullptr, UDamageType::StaticClass());
		}
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(2.5));

	// Each died, fell as a ragdoll and lies on the floor
	Step([this, Bodies]()
	{
		for (const FBody& Entry : *Bodies)
		{
			const ATHEnemyCharacter* Enemy = Entry.Enemy.Get();
			if (!Enemy)
			{
				continue;
			}
			const double PelvisZ = Enemy->GetMesh()->GetBoneLocation(TEXT("pelvis")).Z;
			AddInfo(FString::Printf(TEXT("%s: pelvis from %.0f to %.0f cm above the floor"), *Entry.Name, Entry.PelvisZ - Entry.FloorZ, PelvisZ - Entry.FloorZ));
			TestTrue(*(Entry.Name + TEXT(" died")), Enemy->IsDead());
			TestTrue(*(Entry.Name + TEXT(" became a ragdoll")), Enemy->IsRagdoll());
			// The M03 half bodies' physics asset has no pelvis (their meshes have no hips or legs): check their chest
			const double LyingZ = Entry.bCrawler ? BodyCentre(Enemy->GetMesh(), TEXT("spine_02")).Z : PelvisZ;
			TestTrue(*FString::Printf(TEXT("%s lies on the floor, not through it (%s %.0f cm above the floor)"), *Entry.Name,
				Entry.bCrawler ? TEXT("chest") : TEXT("pelvis"), LyingZ - Entry.FloorZ), LyingZ > Entry.FloorZ - 5.0);
			if (!Entry.bCrawler)
			{
				TestTrue(*(Entry.Name + TEXT(" fell (pelvis 30 cm lower or more)")), PelvisZ < Entry.PelvisZ - 30.0);
			}
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

// Enemy behaviours, in L_EnemyTest's hall (it has a navigation mesh; its own zombies are removed first): one with no
// behaviour stands still; a hunter that sees the player runs at them and hits them; a hunter facing away hears a shot
// and goes to look; an ambusher ignores a player 7 m away and springs at one 2.5 m away; a stalker the player looks at
// slips away without attacking; a stalker behind a player who looks away strikes (strike delay shortened for the test)

namespace THFunctionalBehavior
{
	static const TCHAR* Behaviors = TEXT("/Game/Enemies/Behaviors/");

	/** The hall's line y = 0 (cells x 3 to 6, 4.004 m each), clear of crates. */
	static constexpr double HallY = 0.0;
	static constexpr double PlayerX = 1300.0;

	struct FState
	{
		TWeakObjectPtr<ATHEnemyCharacter> Enemy;
		FVector Start = FVector::ZeroVector;
		float PlayerHealth = 0.0f;
		float StandingHead = 0.0f;
		float StandingLean = 0.0f;
		float StandingHands = 0.0f;
		bool bSawRetreat = false;
		TArray<TWeakObjectPtr<ATHEnemyCharacter>> Extras;
		double MoanAt = -1.0;
		double HitAt = -1.0;
		TWeakObjectPtr<ATHEnemyCharacter> Second;
		TWeakObjectPtr<AActor> Wall;
		TWeakObjectPtr<class ATHDirector> Director;
		/** A count, and a game time to wait until, for a step that watches for something not happening. */
		int32 Count = 0;
		double Until = 0.0;
		TArray<TWeakObjectPtr<AActor>> Pocket;
	};

	static UTHEnemyBehavior* LoadBehavior(const TCHAR* Name)
	{
		return LoadObject<UTHEnemyBehavior>(nullptr, *FString::Printf(TEXT("%s%s.%s"), Behaviors, Name, Name));
	}

	static ATHEnemyAIController* MindOf(const TSharedRef<FState>& State)
	{
		return State->Enemy.IsValid() ? Cast<ATHEnemyAIController>(State->Enemy->GetController()) : nullptr;
	}

	/** Where the body's pelvis and head are from the actor, and whether the meshes show (developer, 2026-11-13: shot enemies seemed to vanish). */
	static double LogBodyPose(FAutomationTestBase* Test, const ATHEnemyCharacter* Enemy, const TCHAR* When)
	{
		const USkeletalMeshComponent* Mesh = Enemy->GetMesh();
		const FVector At = Enemy->GetActorLocation();
		const FVector Pelvis = Mesh->GetBoneLocation(TEXT("pelvis")) - At;
		const FVector Head = Mesh->GetBoneLocation(TEXT("head")) - At;
		const FBoxSphereBounds Bounds = Mesh->CalcBounds(Mesh->GetComponentTransform());
		Test->AddInfo(FString::Printf(TEXT("Body %s: pelvis %s, head %s, bounds %s +- %s, visible %d hidden-in-game %d"), When, *Pelvis.ToCompactString(),
			*Head.ToCompactString(), *(Bounds.Origin - At).ToCompactString(), *Bounds.BoxExtent.ToCompactString(), Mesh->IsVisible() ? 1 : 0, Mesh->bHiddenInGame ? 1 : 0));
		TInlineComponentArray<USkeletalMeshComponent*> Parts(Enemy);
		for (const USkeletalMeshComponent* Part : Parts)
		{
			if (Part != Mesh && Part->GetSkeletalMeshAsset())
			{
				Test->AddInfo(FString::Printf(TEXT("  part %s: head %s, visible %d"), *Part->GetName(),
					*(Part->GetBoneLocation(TEXT("head")) - At).ToCompactString(), Part->IsVisible() ? 1 : 0));
			}
		}
		return FVector::Dist(Head, Pelvis);
	}

	static ETHEnemyState StateOf(const TSharedRef<FState>& State)
	{
		const ATHEnemyAIController* Mind = MindOf(State);
		return Mind ? Mind->GetEnemyState() : ETHEnemyState::Idle;
	}

	static FString StateName(ETHEnemyState Value)
	{
		return StaticEnum<ETHEnemyState>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	static UTHHealthComponent* PlayerHealth()
	{
		return GetPlayer()->FindComponentByClass<UTHHealthComponent>();
	}

	/** Player at PlayerX on the hall's line, looking along +X (Yaw 0) or back (180), healed; the last enemy removed. */
	static void ResetScene(const TSharedRef<FState>& State, float PlayerYaw)
	{
		if (ATHEnemyCharacter* Old = State->Enemy.Get())
		{
			Old->Destroy();
		}
		APawn* Player = GetPlayer();
		Player->TeleportTo(FVector(PlayerX, HallY, Player->GetActorLocation().Z), FRotator(0.0f, PlayerYaw, 0.0f));
		GetController()->SetControlRotation(FRotator(0.0f, PlayerYaw, 0.0f));
		UTHHealthComponent* Health = PlayerHealth();
		if (Health->GetHealthState() == ETHHealthState::Downed)
		{
			Health->Revive(nullptr);
		}
		Health->Heal(Health->GetMaxHealth());
		State->PlayerHealth = Health->GetHealth();
	}

	/** A BP_Zombie at X on the hall's line facing Yaw, with Behavior (null = none). */
	static void SpawnEnemy(FAutomationTestBase* Test, const TSharedRef<FState>& State, double X, float Yaw, UTHEnemyBehavior* Behavior,
		ETHWaitPose Pose = ETHWaitPose::AsBehavior)
	{
		UClass* Class = LoadClass<ATHEnemyCharacter>(nullptr, THFunctionalEnemy::ZombieClass);
		if (!Test->TestNotNull(TEXT("The zombie Blueprint loads"), Class))
		{
			return;
		}
		const APawn* Player = GetPlayer();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ATHEnemyCharacter* Enemy = Player->GetWorld()->SpawnActor<ATHEnemyCharacter>(Class, FVector(X, HallY, Player->GetActorLocation().Z + 2.0),
			FRotator(0.0f, Yaw, 0.0f), Params);
		if (Test->TestNotNull(TEXT("The enemy spawned"), Enemy))
		{
			Enemy->WaitPose = Pose;
			Enemy->SetBehavior(Behavior);
			Test->TestNotNull(TEXT("An AI controller possesses it"), Cast<ATHEnemyAIController>(Enemy->GetController()));
			State->Enemy = Enemy;
			State->Start = Enemy->GetActorLocation();
		}
	}

	/** Degrees the neck-to-head line leans from upright: how far the head is bowed. */
	static float NeckLean(const ATHEnemyCharacter* Enemy)
	{
		const FVector Line = Enemy->GetMesh()->GetBoneLocation(TEXT("head")) - Enemy->GetMesh()->GetBoneLocation(TEXT("neck_01"));
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Line.GetSafeNormal().Z, -1.0, 1.0)));
	}

	/** Waits until the enemy is in one of these states (up to Timeout s). */
	static void WaitForState(FAutomationTestBase* Test, const TSharedRef<FState>& State, TArray<ETHEnemyState> Wanted, double Timeout, const FString& What)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(Test, What, Timeout, [State, Wanted]() { return Wanted.Contains(StateOf(State)); }));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHEnemyBehaviorsTest, "TacticalHorror.Functional.EnemyBehaviors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHEnemyBehaviorsTest::RunTest(const FString& Parameters)
{
	using namespace THFunctionalBehavior;
	TSharedRef<FState> State = MakeShared<FState>();

	StartPlay(this, TEXT("/Game/Maps/L_EnemyTest"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.5));
	Step([this]()
	{
		for (const TCHAR* Name : { TEXT("DA_Hunter"), TEXT("DA_Stalker"), TEXT("DA_Ambusher") })
		{
			TestNotNull(*FString::Printf(TEXT("%s loads (setup_enemies.py)"), Name), LoadBehavior(Name));
		}
		// Only the test's own enemy in the level
		for (TActorIterator<ATHEnemyCharacter> It(GetPlayWorld()); It; ++It)
		{
			It->Destroy();
		}
	});

	// No behaviour: stands still
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		SpawnEnemy(this, State, PlayerX + 500.0, 180.0f, nullptr);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(2.0));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		TestTrue(TEXT("With no behaviour it stays where it is"), Enemy && FVector::Dist2D(Enemy->GetActorLocation(), State->Start) < 20.0);
		TestEqual(TEXT("With no behaviour it is idle"), StateName(StateOf(State)), StateName(ETHEnemyState::Idle));

		// Standing measurements, for the ambusher's head-down check
		if (const ATHEnemyCharacter* Standing = State->Enemy.Get())
		{
			State->StandingHead = Standing->GetMesh()->GetBoneLocation(TEXT("head")).Z - Standing->GetActorLocation().Z + Standing->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			State->StandingLean = NeckLean(Standing);
		}
		// Random looks (developer, 2026-11-11): a dozen zombies in a row across the hall, checked once they have settled
		UClass* Class = LoadClass<ATHEnemyCharacter>(nullptr, THFunctionalEnemy::ZombieClass);
		for (int32 Index = 0; Index < 12 && Class; ++Index)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			State->Extras.Add(GetPlayer()->GetWorld()->SpawnActor<ATHEnemyCharacter>(Class,
				FVector(PlayerX + 150.0 + Index * 70.0, HallY + 250.0, GetPlayer()->GetActorLocation().Z + 2.0), FRotator(0.0f, 90.0f, 0.0f), Params));
		}

		// The stalker's crouch (borrowed, with the zombie's own arms) is in the zombie's animations
		const ATHEnemyCharacter* Body = State->Enemy.Get();
		TestTrue(TEXT("The zombie's animations have a crouch and a crouch walk"), Body && Body->GetAnimations() && Body->GetAnimations()->Crouch && Body->GetAnimations()->CrouchWalk);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.5));
	Step([this, State]()
	{
		// More than one body, some wearing clothing over the body that follows its animation, and every look takes shots
		TSet<const USkeletalMesh*> Bodies;
		int32 Dressed = 0;
		bool bFollow = true;
		for (const TWeakObjectPtr<ATHEnemyCharacter>& Weak : State->Extras)
		{
			ATHEnemyCharacter* Extra = Weak.Get();
			if (!Extra)
			{
				continue;
			}
			Bodies.Add(Extra->GetMesh()->GetSkeletalMeshAsset());
			Dressed += Extra->GetOutfit().IsEmpty() ? 0 : 1;
			for (const USkeletalMeshComponent* Piece : Extra->GetOutfit())
			{
				bFollow &= Piece && Piece->LeaderPoseComponent.Get() == Extra->GetMesh();
			}
			const FVector Chest = BodyCentre(Extra->GetMesh(), TEXT("spine_02"));
			FHitResult Hit;
			FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(THTestLookHit), true);
			Extra->GetWorld()->LineTraceSingleByChannel(Hit, Chest - FVector(0.0f, 200.0f, 0.0f), Chest + FVector(0.0f, 60.0f, 0.0f), ECC_GameTraceChannel1, TraceParams);
			if (Hit.GetActor() != Extra)
			{
				// What its physics bodies are like
				const USkeletalMeshComponent* Mesh = Extra->GetMesh();
				FString Detail;
				for (const FBodyInstance* Instance : Mesh->Bodies)
				{
					if (Instance && Instance->GetBodySetup() && Detail.Len() < 600)
					{
						const UBodySetup* Setup = Instance->GetBodySetup();
						Detail += FString::Printf(TEXT("%s(c%d r%d shapes%d) "), *Setup->BoneName.ToString(), (int32)Instance->GetCollisionEnabled(),
							(int32)Setup->CollisionReponse.GetValue(), Setup->AggGeom.GetElementCount());
					}
				}
				AddError(FString::Printf(TEXT("A random look can't be shot: %s (physics %s, %d bodies): the trace hit %s. %s"), *GetNameSafe(Mesh->GetSkeletalMeshAsset()),
					*GetNameSafe(Mesh->GetPhysicsAsset()), Mesh->Bodies.Num(), *GetNameSafe(Hit.GetActor()), *Detail));
			}
		}
		AddInfo(FString::Printf(TEXT("12 random zombies: %d different bodies, %d wearing clothing pieces"), Bodies.Num(), Dressed));
		TestTrue(TEXT("Random looks give different bodies"), Bodies.Num() >= 3);
		TestTrue(TEXT("Some wear clothing over the body (the Starved zombies)"), Dressed >= 1);
		TestTrue(TEXT("Clothing follows the body's animation"), bFollow);
		for (const TWeakObjectPtr<ATHEnemyCharacter>& Weak : State->Extras)
		{
			if (Weak.IsValid())
			{
				Weak->Destroy();
			}
		}
		State->Extras.Reset();
	});

	// Hunter in front: sees the player, runs at them, hits them
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		SpawnEnemy(this, State, PlayerX + 900.0, 180.0f, LoadBehavior(TEXT("DA_Hunter")));
	});
	WaitForState(this, State, { ETHEnemyState::Chase, ETHEnemyState::Attack }, 3.0, TEXT("the hunter to see the player and chase"));
	Step([this, State]()
	{
		const ATHEnemyAIController* Mind = MindOf(State);
		TestTrue(TEXT("The hunter is after the player"), Mind && Mind->GetTarget() == GetPlayer());
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the hunter to reach and hit the player"), 10.0, [State]()
	{
		return PlayerHealth()->GetHealth() < State->PlayerHealth;
	}));
	Step([this, State]()
	{
		const UTHEnemyBehavior* Hunter = LoadBehavior(TEXT("DA_Hunter"));
		AddInfo(FString::Printf(TEXT("Hunter hit: player health %.0f -> %.0f"), State->PlayerHealth, PlayerHealth()->GetHealth()));
		TestEqual(TEXT("A blow does the behaviour's attack damage"), State->PlayerHealth - PlayerHealth()->GetHealth(), Hunter ? Hunter->AttackDamage : -1.0f);
		// Its running steps can be heard (developer, 2026-11-13): the player's footstep sounds, not a noise enemies hear
		const UTHFootstepComponent* Steps = State->Enemy.IsValid() ? State->Enemy->FindComponentByClass<UTHFootstepComponent>() : nullptr;
		TestTrue(TEXT("The hunter has footstep sounds (setup_enemy_footsteps.py)"), Steps && (Steps->SurfaceSounds.Num() > 0 || Steps->DefaultSounds.Walk));
		TestTrue(TEXT("Its steps aren't a noise other enemies hear"), Steps && !Steps->bMakesNoise);

		// The player can tell: blood on the screen (and the view jolts; UTHCameraModifier)
		const ATHPlayerController* PC = Cast<ATHPlayerController>(GetController());
		const UTHStatusWidget* Screen = PC ? PC->GetStatusWidget() : nullptr;
		TestTrue(TEXT("The player controller has the view modifier for hits and death"), PC && PC->GetBodyCameraModifier());
		TestTrue(TEXT("Blood splattered on the player's screen"), Screen && Screen->GetVisibleBloodCount() > 0);

		// The music speeds up while an enemy nearby is going for someone
		const ATHEnemyCharacter* Attacker = State->Enemy.Get();
		const UTHAmbienceSubsystem* Ambience = GetPlayer()->GetWorld()->GetSubsystem<UTHAmbienceSubsystem>();
		AddInfo(FString::Printf(TEXT("Music tension while the hunter attacks: %.2f"), Ambience ? Ambience->GetTension() : -1.0f));
		TestTrue(TEXT("The attacking hunter counts as aggressive"), Attacker && Attacker->IsAggressive());
		TestTrue(TEXT("The music's tension has risen (it plays faster)"), Ambience && Ambience->GetTension() > 0.2f);

		// Voices: the hunter's scream (calling others in; 2026-11-14, was its angry alert) as it went for the player, or a chase
		// shout since; the player's hurt cry
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const USoundBase* Voice = Enemy ? Enemy->GetLastVoice() : nullptr;
		AddInfo(FString::Printf(TEXT("Hunter's voice: %s; player's: %s"), *GetNameSafe(Voice), *GetNameSafe(Cast<ATHCharacter>(GetPlayer()) ? Cast<ATHCharacter>(GetPlayer())->GetLastSound() : nullptr)));
		TestTrue(TEXT("The hunter screamed or shouted as it went for the player"), Hunter && Voice
			&& (Hunter->ScreamSounds.Contains(Voice) || Hunter->AlertSounds.Contains(Voice) || Hunter->ChaseSounds.Contains(Voice)));
		const ATHCharacter* Character = Cast<ATHCharacter>(GetPlayer());
		TestTrue(TEXT("The player cried out when hit"), Character && Character->GetLastSound() && Character->HurtSounds.ContainsByPredicate(
			[Character](const TSoftObjectPtr<USoundBase>& Sound) { return Sound.Get() == Character->GetLastSound(); }));
	});

	// Hunter right in front of the player with a crate-high pillar between them (60 cm wide, 1 m tall): it doesn't swing across
	// it; it steps round and hits from where nothing is in the way (developer, 2026-11-13: wait for a clear line)
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		const APawn* Player = GetPlayer();
		const ACharacter* Walker = Cast<ACharacter>(Player);
		const double FloorZ = Player->GetActorLocation().Z - (Walker ? Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Pillar = Player->GetWorld()->SpawnActor<AStaticMeshActor>(FVector(PlayerX + 62.0, HallY, FloorZ + 50.0), FRotator::ZeroRotator, Params);
		if (TestNotNull(TEXT("The test pillar spawns"), Pillar))
		{
			Pillar->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Pillar->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Pillar->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Pillar->SetActorScale3D(FVector(0.25, 0.6, 1.0));
			State->Wall = Pillar;
		}
		SpawnEnemy(this, State, PlayerX + 125.0, 180.0f, LoadBehavior(TEXT("DA_Hunter")));
		State->HitAt = -1.0;
		State->bSawRetreat = false;	// here: a blow landed while something was in the way
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the hunter to get round the pillar and hit the player"), 10.0, [State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		if (PlayerHealth()->GetHealth() < State->PlayerHealth)
		{
			State->HitAt = Enemy ? FMath::Abs(Enemy->GetActorLocation().Y - HallY) : -1.0;
			State->bSawRetreat = Enemy && !Enemy->HasClearReach(GetPlayer());
			return true;
		}
		return false;
	}));
	Step([this, State]()
	{
		AddInfo(FString::Printf(TEXT("Hunter behind a pillar: hit the player from %.0f cm to the side of the pillar's line"), State->HitAt));
		TestTrue(TEXT("It went round the pillar before hitting (not straight across it)"), State->HitAt > 30.0);
		TestFalse(TEXT("Nothing was in the way when the blow landed"), State->bSawRetreat);
		if (AActor* Pillar = State->Wall.Get())
		{
			Pillar->Destroy();
		}
		State->HitAt = -1.0;
	});

	// Shot, a hunter staggers (developer, 2026-11-13): a hit reaction over its body and a moment stopped; not again for
	// its cooldown
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		SpawnEnemy(this, State, PlayerX + 900.0, 180.0f, LoadBehavior(TEXT("DA_Hunter")));
	});
	WaitForState(this, State, { ETHEnemyState::Chase }, 3.0, TEXT("the hunter to come at the player"));
	Step([this, State]()
	{
		ATHEnemyCharacter* Enemy = State->Enemy.Get();
		if (Enemy)
		{
			LogBodyPose(this, Enemy, TEXT("before the shot"));
			UGameplayStatics::ApplyDamage(Enemy, 1.0f, GetController(), GetPlayer(), UDamageType::StaticClass());
		}
		const ATHEnemyAIController* Mind = MindOf(State);
		const UTHEnemyAnimInstance* Anim = Enemy ? Cast<UTHEnemyAnimInstance>(Enemy->GetMesh()->GetAnimInstance()) : nullptr;
		const UTHEnemyBehavior* Hunter = LoadBehavior(TEXT("DA_Hunter"));
		AddInfo(FString::Printf(TEXT("Hunter shot: staggered %d, hit reaction playing %d (stagger %.1f s, %d reactions)"), Mind && Mind->IsStaggered() ? 1 : 0,
			Anim && Anim->IsPlayingHitReaction() ? 1 : 0, Hunter ? Hunter->StaggerTime : -1.0f, Enemy && Enemy->GetAnimations() ? Enemy->GetAnimations()->HitReactions.Num() : -1));
		TestTrue(TEXT("Shot, the hunter staggers"), Mind && Mind->IsStaggered());
		TestTrue(TEXT("and plays a hit reaction"), Anim && Anim->IsPlayingHitReaction());
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.12));
	Step([this, State]()
	{
		const double Upright = State->Enemy.IsValid() ? LogBodyPose(this, State->Enemy.Get(), TEXT("0.12 s into the hit reaction")) : 0.0;
		TestTrue(TEXT("Staggering, its body keeps its shape (head and pelvis 40 cm+ apart; it used to collapse to a point)"), Upright > 40.0);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.13));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		if (Enemy) { LogBodyPose(this, Enemy, TEXT("0.25 s into the hit reaction")); }
		const double Speed = Enemy ? Enemy->GetVelocity().Size2D() : -1.0;
		AddInfo(FString::Printf(TEXT("Hunter 0.25 s after the shot: %.0f cm/s"), Speed));
		TestTrue(TEXT("Staggering, it has (nearly) stopped"), Speed >= 0.0 && Speed < 120.0);
	});
	// Moving again, its flinch is over (2026-11-14: the clip outlasted the stagger and it ran on in it, sliding)
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.4));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const UTHEnemyAnimInstance* Anim = Enemy ? Cast<UTHEnemyAnimInstance>(Enemy->GetMesh()->GetAnimInstance()) : nullptr;
		TestTrue(TEXT("0.65 s after the shot (a 0.4 s stagger) the flinch is over"), Anim && !Anim->IsPlayingHitReaction());
	});
	WaitForState(this, State, { ETHEnemyState::Chase, ETHEnemyState::Attack }, 2.0, TEXT("the hunter to come on again after its stagger"));

	// Hits that matter (developer, 2026-11-13): a shot from behind counts as the back; an arm shot enough comes off (a
	// piece falls, its blows weaken); a leg hurt, it limps; a leg shot off, it crawls
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		UTHEnemyBehavior* Still = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Hunter")), GetTransientPackage());
		if (Still)
		{
			Still->StaggerTime = 0.0f;
			Still->Health = 1000.0f;	// lives through it all
		}
		SpawnEnemy(this, State, PlayerX + 900.0, 180.0f, Still);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.5));
	Step([this, State]()
	{
		ATHEnemyCharacter* Enemy = State->Enemy.Get();
		if (!TestNotNull(TEXT("The wounds test enemy spawned"), Enemy))
		{
			return;
		}
		auto Shoot = [this, Enemy](FName Bone, float Damage, const FVector& Direction)
		{
			FHitResult Hit;
			Hit.BoneName = Bone;
			Hit.ImpactPoint = Enemy->GetMesh()->GetSocketLocation(Bone);
			Hit.Component = Enemy->GetMesh();
			UGameplayStatics::ApplyPointDamage(Enemy, Damage, Direction, Hit, GetController(), GetPlayer(), UDamageType::StaticClass());
		};
		const FVector Forward = Enemy->GetActorForwardVector();
		// From behind: the bullet travels the way it faces
		Shoot(TEXT("spine_02"), 5.0f, Forward);
		TestTrue(TEXT("A shot from behind counts as the back"), Enemy->GetLastHitSide() == ETHHitSide::Back);
		Shoot(TEXT("spine_02"), 5.0f, -Forward);
		TestTrue(TEXT("A shot from in front counts as the front"), Enemy->GetLastHitSide() == ETHHitSide::Front);
		TestTrue(TEXT("Its look has arms and legs that can come off (setup_dismemberment.py)"), Enemy->CanSever(ETHLimb::LeftArm) && Enemy->CanSever(ETHLimb::LeftLeg));
		// An arm: two hits on the forearm add up past its 45 (an M4 limb hit is about 30)
		Shoot(TEXT("lowerarm_l"), 25.0f, -Forward);
		TestFalse(TEXT("One hit doesn't take an arm off"), Enemy->IsLimbSevered(ETHLimb::LeftArm));
		Shoot(TEXT("hand_l"), 25.0f, -Forward);
		TestTrue(TEXT("Hits on the forearm and hand add up and take the arm off"), Enemy->IsLimbSevered(ETHLimb::LeftArm));
		TestEqual(TEXT("The arm falls as a piece"), Enemy->GetSeveredPieceCount(), 1);
		// Dressed as the body (developer, 2026-11-15: limbs fell off in the wrong clothes): each of its slots named as one of
		// the body's ("Body", or "<Model>_Body") has the body's material, and none is empty
		const AActor* Piece = Enemy->GetSeveredPiece(0);
		const USkeletalMeshComponent* Fallen = Piece ? Piece->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
		const USkeletalMeshComponent* Worn = Enemy->GetVisibleBody();
		if (TestNotNull(TEXT("The fallen arm has a mesh"), Fallen) && Worn)
		{
			int32 Shared = 0, Same = 0, Empty = 0;
			for (const FName& Slot : Fallen->GetMaterialSlotNames())
			{
				const UMaterialInterface* Has = Fallen->GetMaterial(Fallen->GetMaterialIndex(Slot));
				Empty += Has ? 0 : 1;
				for (const FName& BodySlot : Worn->GetMaterialSlotNames())
				{
					if (Slot.ToString().Equals(BodySlot.ToString(), ESearchCase::IgnoreCase)
						|| Slot.ToString().EndsWith(TEXT("_") + BodySlot.ToString(), ESearchCase::IgnoreCase))
					{
						const UMaterialInterface* Want = Worn->GetMaterial(Worn->GetMaterialIndex(BodySlot));
						if (const UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Want))
						{
							Want = Dynamic->Parent;
						}
						++Shared;
						Same += Has == Want ? 1 : 0;
						break;
					}
				}
			}
			AddInfo(FString::Printf(TEXT("Fallen arm %s on %s: %d slot(s) shared with the body, %d matching, %d empty"),
				*GetNameSafe(Fallen->GetSkeletalMeshAsset()), *GetNameSafe(Worn->GetSkeletalMeshAsset()), Shared, Same, Empty));
			TestEqual(TEXT("The fallen arm wears the body's materials"), Same, Shared);
			TestEqual(TEXT("The fallen arm has a material in every slot"), Empty, 0);
		}
		TestTrue(TEXT("Its blows weaken without the arm"), Enemy->GetAttackDamageScale() < 0.99f);
		// A leg hurt: it limps
		Shoot(TEXT("calf_r"), 30.0f, -Forward);
		TestTrue(TEXT("A hurt leg makes it limp"), Enemy->GetSpeedScale() < 0.99f && !Enemy->IsCrawling());
		// A leg off: it crawls
		Shoot(TEXT("thigh_l"), 50.0f, -Forward);
		AddInfo(FString::Printf(TEXT("Wounds: left arm off %d, left leg off %d, crawling %d (animations %s, crouched %d), pieces %d, blow x%.2f, limp x%.2f"),
			Enemy->IsLimbSevered(ETHLimb::LeftArm) ? 1 : 0, Enemy->IsLimbSevered(ETHLimb::LeftLeg) ? 1 : 0, Enemy->IsCrawling() ? 1 : 0,
			*GetNameSafe(Enemy->GetAnimations()), Enemy->bIsCrouched ? 1 : 0, Enemy->GetSeveredPieceCount(), Enemy->GetAttackDamageScale(), Enemy->GetSpeedScale()));
		TestTrue(TEXT("A leg shot off and it crawls (the crawler animations, low)"), Enemy->IsCrawling() && Enemy->GetAnimations()
			&& Enemy->GetAnimations()->GetName().Contains(TEXT("Crawler")));
		TestFalse(TEXT("The head stays on without a killing headshot"), Enemy->IsLimbSevered(ETHLimb::Head));
		// The leg's bones drawn shrunk into the hip, not at the engine's hidden-bone point a body's length away (developer,
		// 2026-11-16: a cut leg's skin stretched into a spike)
		const USkeletalMeshComponent* Seen = Enemy->GetVisibleBody();
		const int32 Foot = Seen ? Seen->GetBoneIndex(TEXT("foot_l")) : INDEX_NONE;
		const int32 Hip = Seen ? Seen->GetBoneIndex(TEXT("thigh_l")) : INDEX_NONE;
		const TArray<FTransform>& Pose = Seen ? Seen->GetComponentSpaceTransforms() : TArray<FTransform>();
		const bool bHave = Pose.IsValidIndex(Foot) && Pose.IsValidIndex(Hip);
		AddInfo(FString::Printf(TEXT("Cut leg: %d bones collapsed, foot %.0f cm from the hip, scale %.2f"), Enemy->GetCollapsedBoneCount(),
			bHave ? FVector::Dist(Pose[Foot].GetTranslation(), Pose[Hip].GetTranslation()) : -1.0, bHave ? Pose[Foot].GetScale3D().GetMax() : -1.0));
		TestTrue(TEXT("The cut leg's bones are collapsed into the hip"), bHave && Enemy->GetCollapsedBoneCount() > 0
			&& FVector::Dist(Pose[Foot].GetTranslation(), Pose[Hip].GetTranslation()) < 1.0 && Pose[Foot].GetScale3D().IsNearlyZero());
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(2.0));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const double Speed = Enemy ? Enemy->GetVelocity().Size2D() : -1.0;
		AddInfo(FString::Printf(TEXT("Crawling hunter's speed: %.0f cm/s (crawl clip %.0f cm/s)"), Speed,
			Enemy && Enemy->GetAnimations() ? Enemy->GetAnimations()->WalkAnimSpeed : -1.0f));
		TestTrue(TEXT("Crawling, it is slow"), Speed >= 0.0 && Enemy && Enemy->GetAnimations() && Speed <= Enemy->GetAnimations()->WalkAnimSpeed * 2.0f + 5.0f);
		// Mixamo's crawl travels 2 m a loop in the clip (2026-11-14): held in place, the body stays on its capsule
		const double Off = Enemy ? FVector::Dist2D(Enemy->GetMesh()->GetBoneLocation(TEXT("pelvis")), Enemy->GetActorLocation()) : -1.0;
		AddInfo(FString::Printf(TEXT("Crawling body: pelvis %.0f cm from its capsule (walk travel %s)"), Off,
			Enemy && Enemy->GetAnimations() ? *Enemy->GetAnimations()->WalkTravel.ToCompactString() : TEXT("-")));
		TestTrue(TEXT("Crawling, its body stays on its capsule (within 60 cm)"), Off >= 0.0 && Off < 60.0);
	});

	// Each its own (developer, 2026-11-13: a group never moves in step): two hunters, two paces, within its Variation
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		SpawnEnemy(this, State, PlayerX + 1300.0, 0.0f, LoadBehavior(TEXT("DA_Hunter")));
		State->Second = State->Enemy;
		SpawnEnemy(this, State, PlayerX + 1600.0, 0.0f, LoadBehavior(TEXT("DA_Hunter")));
		const ATHEnemyAIController* One = State->Second.IsValid() ? Cast<ATHEnemyAIController>(State->Second->GetController()) : nullptr;
		const ATHEnemyAIController* Two = MindOf(State);
		const UTHEnemyBehavior* Hunter = LoadBehavior(TEXT("DA_Hunter"));
		const float Spread = Hunter ? Hunter->Variation : 0.0f;
		AddInfo(FString::Printf(TEXT("Two hunters' paces x%.3f and x%.3f, blow rhythm x%.3f and x%.3f (variation %.2f)"), One ? One->GetSpeedVariation() : -1.0f,
			Two ? Two->GetSpeedVariation() : -1.0f, One ? One->GetCooldownVariation() : -1.0f, Two ? Two->GetCooldownVariation() : -1.0f, Spread));
		TestTrue(TEXT("Two hunters move at their own paces"), One && Two && !FMath::IsNearlyEqual(One->GetSpeedVariation(), Two->GetSpeedVariation(), 0.0001f));
		TestTrue(TEXT("Within its Variation"), One && Two && FMath::Abs(One->GetSpeedVariation() - 1.0f) <= Spread + 0.001f && FMath::Abs(Two->GetSpeedVariation() - 1.0f) <= Spread + 0.001f);
		if (State->Second.IsValid())
		{
			State->Second->Destroy();
		}
	});

	// Hunting tactics (developer, 2026-11-13): a hunter facing away is called in by another's shout as its chase starts;
	// with two after the player, the farther one goes round to their side; a hunter aimed at zig-zags. Blows of 1 here
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		UTHEnemyBehavior* Soft = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Hunter")), GetTransientPackage());
		if (Soft)
		{
			Soft->AttackDamage = 1.0f;
			Soft->PaceRadius = 0.0f;
			Soft->WanderRadius = 0.0f;
		}
		SpawnEnemy(this, State, PlayerX + 1250.0, 0.0f, Soft);	// facing away: it can't see the player (inside the hall: past 13 m is off the map)
		State->Second = State->Enemy;
		// Off to one side, so the near one doesn't stand between it and the player
		if (ATHEnemyCharacter* Far = State->Second.Get())
		{
			Far->SetActorLocation(Far->GetActorLocation() + FVector(0.0f, 300.0f, 0.0f));
		}
		SpawnEnemy(this, State, PlayerX + 700.0, 180.0f, Soft);	// facing them: it sees them and shouts
		State->bSawRetreat = false;	// here: the far one flanked
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the far hunter to be called in by the near one's shout"), 4.0, [State]()
	{
		const ATHEnemyCharacter* Far = State->Second.Get();
		const ATHEnemyAIController* Mind = Far ? Cast<ATHEnemyAIController>(Far->GetController()) : nullptr;
		return Mind && Mind->GetTarget() == GetPlayer();
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the far hunter to go round to the player's side"), 4.0, [this, State]()
	{
		const ATHEnemyCharacter* Far = State->Second.Get();
		const ATHEnemyAIController* Mind = Far ? Cast<ATHEnemyAIController>(Far->GetController()) : nullptr;
		State->bSawRetreat |= Mind && Mind->IsFlanking();
		if (!State->bSawRetreat && Far && Mind && FMath::Frac(GameTime()) < 0.02)
		{
			AddInfo(FString::Printf(TEXT("Far hunter: %s, %.0f cm from the player, sees them %d"), *StateName(Mind->GetEnemyState()),
				FVector::Dist2D(Far->GetActorLocation(), GetPlayer()->GetActorLocation()), Mind->CanSee(GetPlayer()) ? 1 : 0));
		}
		return State->bSawRetreat;
	}));
	Step([this, State]()
	{
		AddInfo(FString::Printf(TEXT("Hunters: the far one called in and flanked %d"), State->bSawRetreat ? 1 : 0));
		for (ATHEnemyCharacter* Each : { State->Second.Get(), State->Enemy.Get() })
		{
			if (Each)
			{
				Each->Destroy();
			}
		}
		// Aimed at from 11 m: it zig-zags as it comes
		ResetScene(State, 0.0f);
		UTHEnemyBehavior* Soft = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Hunter")), GetTransientPackage());
		if (Soft)
		{
			Soft->AttackDamage = 1.0f;
		}
		SpawnEnemy(this, State, PlayerX + 1100.0, 180.0f, Soft);
		GetController()->SetControlRotation(FRotator::ZeroRotator);
		State->HitAt = 0.0;	// here: its widest swing off the line
	});
	for (int32 Sample = 0; Sample < 12; ++Sample)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.15));
		Step([State]()
		{
			const ATHEnemyCharacter* Enemy = State->Enemy.Get();
			if (Enemy && FVector::Dist2D(Enemy->GetActorLocation(), GetPlayer()->GetActorLocation()) > 400.0)
			{
				State->HitAt = FMath::Max(State->HitAt, FMath::Abs(Enemy->GetActorLocation().Y - HallY));
			}
		});
	}
	Step([this, State]()
	{
		AddInfo(FString::Printf(TEXT("Hunter aimed at: widest %.0f cm off the line as it came"), State->HitAt));
		TestTrue(TEXT("Aimed at, the hunter zig-zags (over 60 cm off the line)"), State->HitAt > 60.0);
		if (ATHEnemyCharacter* Enemy = State->Enemy.Get())
		{
			Enemy->Destroy();
		}
		State->HitAt = -1.0;
	});

	// Walls muffle sound (developer, 2026-11-13): a walking step 4 m behind a hunter (heard 6 m off in the open) isn't heard
	// through a wall; the same step with the wall gone is
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		UTHEnemyBehavior* Still = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Hunter")), GetTransientPackage());
		if (Still)
		{
			Still->PaceRadius = 0.0f;
			Still->WanderRadius = 0.0f;
		}
		SpawnEnemy(this, State, PlayerX + 900.0, 0.0f, Still);
		const APawn* Player = GetPlayer();
		const ACharacter* Walker = Cast<ACharacter>(Player);
		const double FloorZ = Player->GetActorLocation().Z - (Walker ? Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Wall = Player->GetWorld()->SpawnActor<AStaticMeshActor>(FVector(PlayerX + 700.0, HallY, FloorZ + 200.0), FRotator::ZeroRotator, Params);
		if (TestNotNull(TEXT("The test wall spawns"), Wall))
		{
			Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Wall->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Wall->SetActorScale3D(FVector(0.3, 6.0, 4.0));
			State->Wall = Wall;
		}
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.5));
	Step([this, State]()
	{
		APawn* Player = GetPlayer();
		UAISense_Hearing::ReportNoiseEvent(Player, FVector(PlayerX + 500.0, HallY, Player->GetActorLocation().Z - 80.0), 0.6f, Player, 0.0f, TEXT("Footstep"));
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(1.0));
	Step([this, State]()
	{
		AddInfo(FString::Printf(TEXT("Hunter, a walking step 4 m behind it through a wall: %s"), *StateName(StateOf(State))));
		TestTrue(TEXT("Through a wall, it doesn't hear a step 4 m away"), StateOf(State) == ETHEnemyState::Idle || StateOf(State) == ETHEnemyState::Wander);
		if (AActor* Wall = State->Wall.Get())
		{
			Wall->Destroy();
		}
		APawn* Player = GetPlayer();
		UAISense_Hearing::ReportNoiseEvent(Player, FVector(PlayerX + 500.0, HallY, Player->GetActorLocation().Z - 80.0), 0.6f, Player, 0.0f, TEXT("Footstep"));
	});
	WaitForState(this, State, { ETHEnemyState::Investigate, ETHEnemyState::Chase }, 2.0, TEXT("the hunter to hear the same step with no wall"));

	// A closed door between a hunter and the player who shot it (a doorway: walls either side): it doesn't open it (only
	// the stalker does), it bangs on it; opened, it comes through (developer, 2026-11-13)
	TSharedRef<TArray<TWeakObjectPtr<AActor>>> DoorScene = MakeShared<TArray<TWeakObjectPtr<AActor>>>();
	static constexpr double DoorX = PlayerX + 350.0;
	Step([this, State, DoorScene]()
	{
		ResetScene(State, 0.0f);
		const APawn* Player = GetPlayer();
		const ACharacter* Walker = Cast<ACharacter>(Player);
		const double FloorZ = Player->GetActorLocation().Z - (Walker ? Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UClass* DoorClass = LoadClass<AActor>(nullptr, TEXT("/Game/Template/Interaction/Blueprint/Door/BP_Door.BP_Door_C"));
		AActor* Door = DoorClass ? Player->GetWorld()->SpawnActor<AActor>(DoorClass, FVector(DoorX, HallY, FloorZ), FRotator::ZeroRotator, Params) : nullptr;
		if (!TestNotNull(TEXT("BP_Door spawns"), Door) || !TestNotNull(TEXT("with its door sync"), Door->FindComponentByClass<UTHDoorSyncComponent>()))
		{
			return;
		}
		DoorScene->Add(Door);
		// Walls from the door's sides outwards, so the way is through it
		FVector Centre, Extent;
		Door->GetActorBounds(true, Centre, Extent);
		for (const double Side : { -1.0, 1.0 })
		{
			const double Inner = Side > 0.0 ? Centre.Y + Extent.Y : Centre.Y - Extent.Y;
			AStaticMeshActor* Wall = Player->GetWorld()->SpawnActor<AStaticMeshActor>(FVector(DoorX, Inner + Side * 400.0, FloorZ + 150.0), FRotator::ZeroRotator, Params);
			if (Wall)
			{
				Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
				Wall->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
				Wall->SetActorScale3D(FVector(0.2, 8.0, 3.0));
				DoorScene->Add(Wall);
			}
		}
		AddInfo(FString::Printf(TEXT("Test door: %.0f cm wide, %.0f deep"), Extent.Y * 2.0, Extent.X * 2.0));
		UTHEnemyBehavior* Still = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Hunter")), GetTransientPackage());
		if (Still)
		{
			Still->PaceRadius = 0.0f;
			Still->WanderRadius = 0.0f;
		}
		SpawnEnemy(this, State, PlayerX + 800.0, 180.0f, Still);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.5));
	Step([this, State]()
	{
		TestTrue(TEXT("The hunter behaves as DA_Hunter: bangs on doors, doesn't open them"),
			State->Enemy.IsValid() && State->Enemy->GetBehavior() && State->Enemy->GetBehavior()->bBangsOnDoors && !State->Enemy->GetBehavior()->bOpensDoors);
		APawn* Player = GetPlayer();
		if (ATHEnemyCharacter* Enemy = State->Enemy.Get())
		{
			UGameplayStatics::ApplyDamage(Enemy, 1.0f, Player->GetController(), Player, UDamageType::StaticClass());
		}
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the hunter, shot from behind the closed door, to bang on it twice"), 10.0, [State]()
	{
		return State->Enemy.IsValid() && State->Enemy->GetDoorBangCount() >= 2;
	}));
	Step([this, State, DoorScene]()
	{
		ATHEnemyCharacter* Enemy = State->Enemy.Get();
		AActor* Door = DoorScene->IsEmpty() ? nullptr : (*DoorScene)[0].Get();
		UTHDoorSyncComponent* Sync = Door ? Door->FindComponentByClass<UTHDoorSyncComponent>() : nullptr;
		AddInfo(FString::Printf(TEXT("Hunter at a closed door: %d blows, %.0f cm from the door on its own side, door shut %d"),
			Enemy ? Enemy->GetDoorBangCount() : -1, Enemy ? Enemy->GetActorLocation().X - DoorX : 0.0, Sync && Sync->IsShut() ? 1 : 0));
		TestTrue(TEXT("It didn't open the door"), Sync && Sync->IsShut());
		TestTrue(TEXT("It is still on its own side"), Enemy && Enemy->GetActorLocation().X > DoorX);
		// The player opens it
		if (Sync)
		{
			Sync->HandleRequest(GetPlayer(), true);
		}
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the hunter to come through the opened door"), 8.0, [State]()
	{
		return State->Enemy.IsValid() && State->Enemy->GetActorLocation().X < DoorX - 60.0;
	}));
	Step([DoorScene]()
	{
		for (const TWeakObjectPtr<AActor>& Each : *DoorScene)
		{
			if (AActor* Actor = Each.Get())
			{
				Actor->Destroy();
			}
		}
	});

	// Hunter facing away, out of sight: hears a shot and goes to look (or, turning, sees the player). Not pacing, so it
	// stays facing away
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		UTHEnemyBehavior* Still = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Hunter")), GetTransientPackage());
		if (Still)
		{
			Still->PaceRadius = 0.0f;
			Still->WanderRadius = 0.0f;
		}
		SpawnEnemy(this, State, PlayerX + 900.0, 0.0f, Still);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(1.0));
	Step([this, State]()
	{
		// Every spot light the player carries, and whether the hunter counts itself lit
		FString Lights;
		TArray<AActor*> Carried;
		GetPlayer()->GetAttachedActors(Carried, true, true);
		Carried.Add(GetPlayer());
		for (const AActor* Holder : Carried)
		{
			TInlineComponentArray<USpotLightComponent*> Found(Holder);
			for (const USpotLightComponent* Light : Found)
			{
				Lights += FString::Printf(TEXT("%s.%s(visible %d hidden %d actorHidden %d intensity %.0f) "), *GetNameSafe(Holder), *Light->GetName(),
					Light->IsVisible(), Light->bHiddenInGame, Holder->IsHidden(), Light->Intensity);
			}
		}
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const ATHEnemyAIController* Mind = Enemy ? Cast<ATHEnemyAIController>(Enemy->GetController()) : nullptr;
		AddInfo(FString::Printf(TEXT("Facing-away hunter: %s, lit %d; lights: %s"), *StateName(StateOf(State)), Mind && Mind->IsLitBy(GetPlayer()), *Lights));
		TestTrue(*FString::Printf(TEXT("Facing away, the hunter hasn't noticed the still player (%s)"), *StateName(StateOf(State))),
			StateOf(State) == ETHEnemyState::Idle || StateOf(State) == ETHEnemyState::Wander);
		// Up into the air, so it hits nothing
		GetController()->SetControlRotation(FRotator(60.0f, 0.0f, 0.0f));
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
	Step([]() { THFunctionalEnemy::FireOnce(); });
	WaitForState(this, State, { ETHEnemyState::Investigate, ETHEnemyState::Chase, ETHEnemyState::Attack }, 2.0, TEXT("the hunter to hear the shot"));

	// Hunter with nothing to do: paces a circle about its spot, walking like a person (senses off, to see only that)
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		UTHEnemyBehavior* Blind = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Hunter")), GetTransientPackage());
		if (Blind)
		{
			Blind->SightRadius = 1.0f;
			Blind->LoseSightRadius = 2.0f;
			Blind->HearingRange = 1.0f;
			Blind->bReactsToLight = false;
		}
		SpawnEnemy(this, State, PlayerX + 900.0, 0.0f, Blind);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(3.0));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const UTHEnemyBehavior* Hunter = Enemy ? Enemy->GetBehavior() : nullptr;
		const double FromSpot = Enemy ? FVector::Dist2D(Enemy->GetActorLocation(), State->Start) : 0.0;
		AddInfo(FString::Printf(TEXT("Pacing hunter: %s, %.0f cm from its spot, %.0f cm/s, calm walk %d"), *StateName(StateOf(State)), FromSpot,
			Enemy ? Enemy->GetVelocity().Size2D() : 0.0, Enemy && Enemy->IsCalmWalking()));
		TestEqual(TEXT("With nothing to do, the hunter paces"), StateName(StateOf(State)), StateName(ETHEnemyState::Wander));
		TestTrue(TEXT("Pacing, it walks like a person (calm walk)"), Enemy && Enemy->IsCalmWalking());
		TestTrue(TEXT("It keeps to its circle (moved off its spot, not far)"), Hunter && FromSpot > 100.0 && FromSpot < Hunter->PaceRadius + 150.0);
	});

	// Hunter facing away, standing still, the player silent: a flashlight on it gives the player away
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		UTHEnemyBehavior* Still = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Hunter")), GetTransientPackage());
		if (Still)
		{
			Still->PaceRadius = 0.0f;
			Still->WanderRadius = 0.0f;
		}
		SpawnEnemy(this, State, PlayerX + 900.0, 0.0f, Still);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(1.0));
	Step([this, State]()
	{
		TestTrue(*FString::Printf(TEXT("Facing away, the hunter hasn't noticed the player in the dark (%s)"), *StateName(StateOf(State))),
			StateOf(State) == ETHEnemyState::Idle || StateOf(State) == ETHEnemyState::Wander);
		UTHWeaponComponent* Weapons = GetPlayer()->FindComponentByClass<UTHWeaponComponent>();
		if (Weapons && !Weapons->IsWeaponLightOn())
		{
			Weapons->UnlockWeaponLight();
			if (!Weapons->IsWeaponLightOn())
			{
				Weapons->ToggleWeaponLight();
			}
		}
		TestTrue(TEXT("The weapon light is on, at the hunter"), Weapons && Weapons->IsWeaponLightOn());
	});
	WaitForState(this, State, { ETHEnemyState::Investigate, ETHEnemyState::Chase, ETHEnemyState::Attack }, 2.0, TEXT("the hunter, lit up, to go for the player"));
	Step([]()
	{
		UTHWeaponComponent* Weapons = GetPlayer()->FindComponentByClass<UTHWeaponComponent>();
		if (Weapons && Weapons->IsWeaponLightOn())
		{
			Weapons->ToggleWeaponLight();
		}
	});

	// Charger, seeing the player 11 m away: roars, charges; the player sidesteps; it misses and goes down a long while
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		SpawnEnemy(this, State, PlayerX + 1100.0, 180.0f, LoadBehavior(TEXT("DA_Charger")));
	});
	WaitForState(this, State, { ETHEnemyState::Roar }, 3.0, TEXT("the charger to roar at the player it sees"));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const UTHHealthComponent* Health = Enemy ? Enemy->GetHealthComponent() : nullptr;
		const FString Body = Enemy ? GetNameSafe(Enemy->GetMesh()->GetSkeletalMeshAsset()) : FString();
		AddInfo(FString::Printf(TEXT("Charger: body %s, health %.0f"), *Body, Health ? Health->GetMaxHealth() : 0.0f));
		TestTrue(TEXT("A charger is a mutant (its behaviour's own looks)"), Body.Contains(TEXT("Creature")));
		TestTrue(TEXT("A charger has its behaviour's 400 health"), Health && FMath::IsNearlyEqual(Health->GetMaxHealth(), 400.0f));
		// It moves as a mutant (developer, 2026-11-16: Mixamo's mutant clips), its run held in place
		const UTHEnemyAnimSet* Moves = Enemy ? Enemy->GetAnimations() : nullptr;
		AddInfo(FString::Printf(TEXT("Charger's animations: %s (run travel %s)"), *GetNameSafe(Moves), Moves ? *Moves->RunTravel.ToCompactString() : TEXT("-")));
		TestTrue(TEXT("A charger moves with the mutant animations"), Moves && Moves->GetName() == TEXT("DA_MutantAnims") && !Moves->RunTravel.IsNearlyZero());
	});
	WaitForState(this, State, { ETHEnemyState::Charge }, 3.0, TEXT("the charger to charge after its roar"));
	Step([this, State]()
	{
		// Its warning as it roared: three stomps (developer, 2026-11-13: a scrape before the charge)
		const ATHEnemyAIController* Mind = MindOf(State);
		const UTHEnemyBehavior* Charger = LoadBehavior(TEXT("DA_Charger"));
		AddInfo(FString::Printf(TEXT("Charger's warning as it roared: %d of %d (%d sounds)"), Mind ? Mind->GetChargeTellsPlayed() : -1,
			Charger ? Charger->ChargeTellCount : -1, Charger ? Charger->ChargeTellSounds.Num() : -1));
		TestTrue(TEXT("It warned as it roared (all its stomps)"), Mind && Charger && Mind->GetChargeTellsPlayed() == Charger->ChargeTellCount && Charger->ChargeTellSounds.Num() > 0);
	});
	Step([]()
	{
		// A big sidestep
		APawn* Player = GetPlayer();
		Player->TeleportTo(Player->GetActorLocation() + FVector(0.0f, 350.0f, 0.0f), Player->GetActorRotation());
	});
	WaitForState(this, State, { ETHEnemyState::Stumble }, 4.0, TEXT("the charger to miss the sidestepping player and go down"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.5));
	Step([this, State]()
	{
		if (const ATHEnemyCharacter* Enemy = State->Enemy.Get())
		{
			State->Start = Enemy->GetActorLocation();
			// Its stomps as it ran, each as a foot landed (developer, 2026-11-16: they didn't line up with its feet)
			const UTHFootstepComponent* Steps = Enemy->GetFootsteps();
			AddInfo(FString::Printf(TEXT("Charger's steps as it charged: %d, %d of them a foot landing"), Steps ? Steps->GetStepCount() : -1,
				Steps ? Steps->GetFootLandingCount() : -1));
			TestTrue(TEXT("The charger stomped as it ran"), Steps && Steps->GetStepCount() >= 2);
			TestTrue(TEXT("Most of its steps are its feet landing (not every stride's distance)"), Steps
				&& Steps->GetFootLandingCount() * 2 > Steps->GetStepCount());
		}
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(5.0));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const UTHEnemyAnimInstance* Anim = Enemy ? Cast<UTHEnemyAnimInstance>(Enemy->GetMesh()->GetAnimInstance()) : nullptr;
		AddInfo(FString::Printf(TEXT("Charger 5 s after its miss: %s, moved %.0f cm, player health %.0f"), *StateName(StateOf(State)),
			Enemy ? FVector::Dist2D(Enemy->GetActorLocation(), State->Start) : -1.0, PlayerHealth()->GetHealth()));
		TestEqual(TEXT("After a miss it is still down 5 s later (time to run)"), StateName(StateOf(State)), StateName(ETHEnemyState::Stumble));
		TestTrue(TEXT("Down, it doesn't move"), Enemy && FVector::Dist2D(Enemy->GetActorLocation(), State->Start) < 30.0);
		TestTrue(TEXT("Its body plays the stumble (stunned on its feet)"), Anim && Anim->IsStumbling());
		TestEqual(TEXT("The sidestepping player wasn't hit"), PlayerHealth()->GetHealth(), State->PlayerHealth);
	});

	// Charger, the player standing still: the charge hits for its ChargeDamage. The real one kills outright (developer,
	// 2026-11-12), which would end the test's solo player, so this one is a copy that hits for 30
	Step([this, State]()
	{
		const UTHEnemyBehavior* Real = LoadBehavior(TEXT("DA_Charger"));
		TestTrue(TEXT("DA_Charger's charge kills a player at full health in one hit"), Real && Real->ChargeDamage >= PlayerHealth()->GetMaxHealth());
		ResetScene(State, 0.0f);
		UTHEnemyBehavior* Soft = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Charger")), GetTransientPackage());
		Soft->ChargeDamage = 30.0f;
		SpawnEnemy(this, State, PlayerX + 1100.0, 180.0f, Soft);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the charger's charge to hit the still player"), 8.0, [State]()
	{
		return PlayerHealth()->GetHealth() < State->PlayerHealth;
	}));
	Step([this, State]()
	{
		AddInfo(FString::Printf(TEXT("Charger hit: player health %.0f -> %.0f"), State->PlayerHealth, PlayerHealth()->GetHealth()));
		TestEqual(TEXT("The charge does its ChargeDamage"), State->PlayerHealth - PlayerHealth()->GetHealth(), 30.0f);
	});

	// Charger into a wall: a wall (crate high) drops between it and the player as it charges; it crashes into it at full tilt (no
	// sliding along it), with its crash sound, and goes down
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		SpawnEnemy(this, State, PlayerX + 1100.0, 180.0f, LoadBehavior(TEXT("DA_Charger")));
	});
	WaitForState(this, State, { ETHEnemyState::Charge }, 6.0, TEXT("the charger to charge (wall test)"));
	Step([this, State]()
	{
		const APawn* Player = GetPlayer();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// A crate-high box (80 cm) on the floor: the developer's charger ran into a box with nothing happening
		const ACharacter* Walker = Cast<ACharacter>(Player);
		const double FloorZ = Player->GetActorLocation().Z - (Walker ? Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0);
		AStaticMeshActor* Wall = Player->GetWorld()->SpawnActor<AStaticMeshActor>(FVector(PlayerX + 450.0, HallY, FloorZ + 40.0),
			FRotator::ZeroRotator, Params);
		if (TestNotNull(TEXT("The test wall spawns"), Wall))
		{
			Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Wall->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Wall->SetActorScale3D(FVector(0.6, 5.0, 0.8));
			State->Wall = Wall;
		}
	});
	WaitForState(this, State, { ETHEnemyState::Stumble }, 5.0, TEXT("the charger to crash into the wall and go down"));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const AActor* Wall = State->Wall.Get();
		const UTHEnemyBehavior* Charger = LoadBehavior(TEXT("DA_Charger"));
		const double Gap = Enemy && Wall ? Enemy->GetActorLocation().X - (Wall->GetActorLocation().X + 30.0) - Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius() : -1.0;
		AddInfo(FString::Printf(TEXT("Charger wall crash: %d crash(es), %.0f cm off the wall, %d crash sounds"), Enemy ? Enemy->GetWallCrashCount() : -1, Gap,
			Charger ? Charger->WallCrashSounds.Num() : -1));
		TestEqual(TEXT("It crashed into the wall once"), Enemy ? Enemy->GetWallCrashCount() : -1, 1);
		TestTrue(TEXT("It went down at the wall"), Gap >= -5.0 && Gap < 100.0);
		TestTrue(TEXT("DA_Charger has crash sounds"), Charger && Charger->WallCrashSounds.Num() > 0);
		TestEqual(TEXT("The player behind the wall wasn't hit"), PlayerHealth()->GetHealth(), State->PlayerHealth);
		if (AActor* ToRemove = State->Wall.Get())
		{
			ToRemove->Destroy();
		}
	});

	// Charger and a knee-high crate between it and the player (developer, 2026-11-15: it charged into objects; its clear-run
	// check looked at the waist only): it doesn't charge, and never crashes
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		const APawn* Player = GetPlayer();
		const ACharacter* Walker = Cast<ACharacter>(Player);
		const double FloorZ = Player->GetActorLocation().Z - (Walker ? Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// 55 cm high, centred on the floor: its top below the old check, above the knees' crash check
		AStaticMeshActor* Crate = Player->GetWorld()->SpawnActor<AStaticMeshActor>(FVector(PlayerX + 550.0, HallY, FloorZ), FRotator::ZeroRotator, Params);
		if (TestNotNull(TEXT("The test crate spawns"), Crate))
		{
			Crate->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Crate->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Crate->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Crate->SetActorScale3D(FVector(0.6, 1.5, 1.1));
			State->Wall = Crate;
		}
		SpawnEnemy(this, State, PlayerX + 1100.0, 180.0f, LoadBehavior(TEXT("DA_Charger")));
		State->Count = 0;
		State->Until = GetPlayWorld()->GetTimeSeconds() + 2.5;
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("2.5 s of the charger with a low crate in its way"), 10.0, [State]()
	{
		State->Count |= StateOf(State) == ETHEnemyState::Roar || StateOf(State) == ETHEnemyState::Charge ? 1 : 0;
		return State->Count > 0 || GetPlayWorld()->GetTimeSeconds() >= State->Until;
	}));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		AddInfo(FString::Printf(TEXT("Charger behind a low crate: roared or charged %d, crashes %d, now %s"), State->Count,
			Enemy ? Enemy->GetWallCrashCount() : -1, *StateName(StateOf(State))));
		TestEqual(TEXT("No charge with a crate in the way"), State->Count, 0);
		TestEqual(TEXT("No crash into the crate"), Enemy ? Enemy->GetWallCrashCount() : -1, 0);
		if (AActor* Crate = State->Wall.Get())
		{
			Crate->Destroy();
		}
	});

	// A charger shot in the leg slows (developer, 2026-11-13); no limbs come off a mutant (no pieces for its looks)
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		SpawnEnemy(this, State, PlayerX + 1000.0, 0.0f, LoadBehavior(TEXT("DA_Charger")));
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
	Step([this, State]()
	{
		ATHEnemyCharacter* Enemy = State->Enemy.Get();
		if (!Enemy)
		{
			return;
		}
		FHitResult Hit;
		Hit.BoneName = TEXT("thigh_l");
		Hit.ImpactPoint = Enemy->GetMesh()->GetSocketLocation(Hit.BoneName);
		Hit.Component = Enemy->GetMesh();
		UGameplayStatics::ApplyPointDamage(Enemy, 160.0f, -Enemy->GetActorForwardVector(), Hit, GetController(), GetPlayer(), UDamageType::StaticClass());
		AddInfo(FString::Printf(TEXT("Charger shot in the leg: speed x%.2f, leg off %d"), Enemy->GetSpeedScale(), Enemy->IsLimbSevered(ETHLimb::LeftLeg) ? 1 : 0));
		TestTrue(TEXT("A charger shot in the leg slows"), Enemy->GetSpeedScale() < 0.9f);
		TestFalse(TEXT("A mutant's leg doesn't come off (no pieces)"), Enemy->IsLimbSevered(ETHLimb::LeftLeg));
		Enemy->Destroy();
	});

	// Siren, 7 m away in plain sight: lies there calling (a person's animations), ignoring being seen; the player comes within
	// 2 m and it gets up and goes for them
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		UTHEnemyBehavior* Siren = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Siren")), GetTransientPackage());
		if (Siren)
		{
			Siren->IdleSoundInterval = FVector2D(0.5f, 0.5f);	// calls quickly, so the test can hear one
			Siren->CrawlAnimations = nullptr;	// lying still, to look at it lying (crawling: below)
		}
		SpawnEnemy(this, State, PlayerX + 700.0, 180.0f, Siren);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(3.0));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const UTHEnemyBehavior* Siren = Enemy ? Enemy->GetBehavior() : nullptr;
		const UTHEnemyAnimInstance* Anim = Enemy ? Cast<UTHEnemyAnimInstance>(Enemy->GetMesh()->GetAnimInstance()) : nullptr;
		const USoundBase* Voice = Enemy ? Enemy->GetLastVoice() : nullptr;
		AddInfo(FString::Printf(TEXT("Siren: %s, animations %s, body %s, voice %s"), *StateName(StateOf(State)),
			*GetNameSafe(Enemy ? Enemy->GetAnimations() : nullptr), *GetNameSafe(Enemy ? Enemy->GetMesh()->GetSkeletalMeshAsset() : nullptr), *GetNameSafe(Voice)));
		TestEqual(TEXT("Seen from 7 m, the siren keeps calling"), StateName(StateOf(State)), StateName(ETHEnemyState::Siren));
		TestTrue(TEXT("It waits in one of its poses"), Enemy && Enemy->IsLyingDown() && Anim && Anim->IsStumbling());
		TestTrue(TEXT("It doesn't move"), Enemy && FVector::Dist2D(Enemy->GetActorLocation(), State->Start) < 20.0);
		TestTrue(TEXT("It has a person's animations (its behaviour's own)"), Enemy && Siren && Enemy->GetAnimations() == Siren->Animations);
		TestTrue(TEXT("It calls out"), Siren && Voice && Siren->IdleSounds.Contains(Voice));
		// One of its waiting poses, at random: standing terrified or sitting dazed, never lying (developer, 2026-11-14), and
		// beckoning the player 7 m away in sight
		const int32 Pose = Enemy ? Enemy->GetLyingPose() : INDEX_NONE;
		const float HeadUp = Enemy ? Enemy->GetMesh()->GetBoneLocation(TEXT("head")).Z - (Enemy->GetActorLocation().Z - Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) : -1.0f;
		const FTHEnemyLyingPose* Waiting = Enemy && Enemy->GetAnimations() && Enemy->GetAnimations()->LyingPoses.IsValidIndex(Pose) ? &Enemy->GetAnimations()->LyingPoses[Pose] : nullptr;
		AddInfo(FString::Printf(TEXT("Siren waiting pose %d of %d (%s), head %.0f cm off the floor, beckoning %d"), Pose,
			Enemy && Enemy->GetAnimations() ? Enemy->GetAnimations()->LyingPoses.Num() : 0,
			Waiting ? *GetNameSafe(Waiting->Lying) : TEXT("none"), HeadUp, Enemy && Enemy->IsBeckoning() ? 1 : 0));
		TestTrue(TEXT("It waits in one of its body set's poses, not the one kept for playing dead"), Waiting && !Waiting->bPlayingDeadOnly);
		TestTrue(TEXT("Standing or sitting, not lying (head over 60 cm)"), HeadUp > 60.0f);
		TestTrue(TEXT("It beckons the player in sight 7 m away"), Enemy && Enemy->IsBeckoning());
		// A MetaHuman (developer, 2026-11-12): its body copies the hidden animated body's pose (lying there: heads together)
		const USkeletalMeshComponent* Seen = Enemy ? Enemy->GetVisibleBody() : nullptr;
		const bool bMetaHuman = Seen && Seen != Enemy->GetMesh();
		const float HeadGap = bMetaHuman && Seen->GetBoneIndex(TEXT("head")) != INDEX_NONE
			? FVector::Dist(Seen->GetBoneLocation(TEXT("head")), Enemy->GetMesh()->GetBoneLocation(TEXT("head"))) : -1.0f;
		AddInfo(FString::Printf(TEXT("Siren look: %s, hidden animated body %s, heads %.0f cm apart, %d grooms, %d outfit pieces"),
			*GetNameSafe(Seen ? Seen->GetSkeletalMeshAsset() : nullptr), bMetaHuman && Enemy->GetMesh()->bHiddenInGame ? TEXT("yes") : TEXT("no"),
			HeadGap, Enemy ? Enemy->GetGrooms().Num() : 0, Enemy ? Enemy->GetOutfit().Num() : 0));
		TestTrue(TEXT("The siren wears a MetaHuman over its hidden animated body"), bMetaHuman && Enemy->GetMesh()->bHiddenInGame);
		TestTrue(TEXT("The MetaHuman follows its animation (heads within 25 cm)"), HeadGap >= 0.0f && HeadGap < 25.0f);
		// Optimized MetaHumans keep no strands: their grooms must draw cards up close, or the hair vanishes
		bool bAllCards = Enemy && Enemy->GetGrooms().Num() > 0;
		for (const UGroomComponent* Groom : Enemy ? Enemy->GetGrooms() : TArray<TObjectPtr<UGroomComponent>>())
		{
			// and up close (its forced LOD, or LOD 0) it is a cards LOD (developer, 2026-11-13: use cards alone still lost the hair)
			const UGroomAsset* Asset = Groom ? Groom->GroomAsset.Get() : nullptr;
			bAllCards &= Groom && Groom->bUseCards && Asset
				&& Asset->GetGeometryType(0, FMath::Max(Groom->GetForcedLOD(), 0)) != EGroomGeometryType::Strands;
		}
		TestTrue(TEXT("The MetaHuman's hair draws cards (not missing strands)"), bAllCards);
		APawn* Player = GetPlayer();
		Player->TeleportTo(FVector(PlayerX + 520.0, HallY, Player->GetActorLocation().Z), FRotator::ZeroRotator);
	});
	WaitForState(this, State, { ETHEnemyState::Stumble, ETHEnemyState::Chase, ETHEnemyState::Attack }, 2.0, TEXT("the siren, approached, to turn on the player"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the siren to get up and hit the player"), 10.0, [State]()
	{
		return PlayerHealth()->GetHealth() < State->PlayerHealth;
	}));
	// Up on its feet, bullets hit it (developer, 2026-11-12: shots did nothing to a standing siren)
	Step([this, State]()
	{
		ATHEnemyCharacter* Enemy = State->Enemy.Get();
		if (!Enemy)
		{
			AddError(TEXT("The siren is gone"));
			return;
		}
		// Aimed at the chest of the body you see (developer, 2026-11-13: shots hit the hidden zombie's proportions instead)
		USkeletalMeshComponent* Mesh = Enemy->GetVisibleBody();
		const FName ChestBone = Mesh->GetBoneIndex(TEXT("spine_04")) != INDEX_NONE ? FName(TEXT("spine_04")) : FName(TEXT("spine_02"));
		const FVector Chest = Mesh->GetBoneIndex(ChestBone) != INDEX_NONE ? Mesh->GetBoneLocation(ChestBone) : Enemy->GetActorLocation();
		const FVector Eyes = GetPlayer()->GetPawnViewLocation();
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(THTestSirenHit), true, GetPlayer());
		Enemy->GetWorld()->LineTraceSingleByChannel(Hit, Eyes, Chest + (Chest - Eyes).GetSafeNormal() * 100.0f, ECC_GameTraceChannel1, Params);
		const float HealthBefore = Enemy->GetHealthComponent()->GetHealth();
		if (Hit.GetActor() == Enemy)
		{
			UGameplayStatics::ApplyPointDamage(Enemy, 10.0f, (Chest - Eyes).GetSafeNormal(), Hit, GetPlayer()->GetController(), GetPlayer(), UDamageType::StaticClass());
		}
		AddInfo(FString::Printf(TEXT("Standing siren shot at the chest: hit %s (%s, bone %s), health %.0f -> %.0f, chest %.0f cm above its middle, lying %d"),
			*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), *Hit.BoneName.ToString(), HealthBefore, Enemy->GetHealthComponent()->GetHealth(),
			Chest.Z - Enemy->GetActorLocation().Z, Enemy->IsLyingDown() ? 1 : 0));
		TestTrue(TEXT("A shot at the standing siren's chest hits it"), Hit.GetActor() == Enemy);
		// (Which bone depends on its pose: just after a blow its hand is still in front of its chest)
		TestTrue(TEXT("on the body you see"), Hit.GetComponent() == Mesh);
		TestTrue(TEXT("and hurts it"), Enemy->GetHealthComponent()->GetHealth() < HealthBefore);
	});

	// A siren with a lone player 7 m off stays where it lies, beckoning (developer, 2026-11-13: no crawling, that is for
	// lost legs); they come within its reach and it springs up; shot down to half its health it drops and plays dead; shot
	// again, it is up again
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		SpawnEnemy(this, State, PlayerX + 700.0, 180.0f, LoadBehavior(TEXT("DA_Siren")));
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(3.0));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const double Crept = Enemy ? FVector::Dist2D(Enemy->GetActorLocation(), State->Start) : 0.0;
		AddInfo(FString::Printf(TEXT("Siren with a lone player 7 m off: crawling %d, lying %d, beckoning %d, moved %.0f cm, state %s"), Enemy && Enemy->IsCrawling() ? 1 : 0,
			Enemy && Enemy->IsLyingDown() ? 1 : 0, Enemy && Enemy->IsBeckoning() ? 1 : 0, Crept, *StateName(StateOf(State))));
		TestTrue(TEXT("A siren with a lone player near stays where it waits, beckoning"), Enemy && !Enemy->IsCrawling() && Enemy->IsLyingDown() && Enemy->IsBeckoning() && Crept < 30.0);
		TestEqual(TEXT("Beckoning, it is still the trap"), StateName(StateOf(State)), StateName(ETHEnemyState::Siren));
		APawn* Player = GetPlayer();
		if (Enemy)
		{
			Player->TeleportTo(Enemy->GetActorLocation() - FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		}
	});
	WaitForState(this, State, { ETHEnemyState::Stumble }, 2.0, TEXT("the siren, reached, to get up"));
	WaitForState(this, State, { ETHEnemyState::Chase, ETHEnemyState::Attack }, 5.0, TEXT("the siren, up, to go for the player"));
	Step([this, State]()
	{
		ATHEnemyCharacter* Enemy = State->Enemy.Get();
		TestTrue(TEXT("Up, it isn't crawling"), Enemy && !Enemy->IsCrawling());
		// Shot down to half its health: it drops and plays dead
		if (Enemy)
		{
			UGameplayStatics::ApplyDamage(Enemy, Enemy->GetHealthComponent()->GetMaxHealth() * 0.55f, GetController(), GetPlayer(), UDamageType::StaticClass());
		}
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(1.0));
	Step([this, State]()
	{
		ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const int32 DeadPose = Enemy ? Enemy->GetLyingPose() : INDEX_NONE;
		const bool bDeadPose = Enemy && Enemy->GetAnimations() && Enemy->GetAnimations()->LyingPoses.IsValidIndex(DeadPose) && Enemy->GetAnimations()->LyingPoses[DeadPose].bPlayingDeadOnly;
		const float DeadHead = Enemy ? Enemy->GetMesh()->GetBoneLocation(TEXT("head")).Z - (Enemy->GetActorLocation().Z - Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) : -1.0f;
		AddInfo(FString::Printf(TEXT("Siren shot down: lying %d (pose kept for playing dead %d, head %.0f cm up), state %s, health %.0f"), Enemy && Enemy->IsLyingDown() ? 1 : 0,
			bDeadPose ? 1 : 0, DeadHead, *StateName(StateOf(State)), Enemy ? Enemy->GetHealthComponent()->GetHealth() : -1.0f));
		TestTrue(TEXT("Shot down to half its health, it drops and plays dead"), Enemy && Enemy->IsLyingDown() && StateOf(State) == ETHEnemyState::Siren);
		TestTrue(TEXT("on the floor, in the pose kept for playing dead (head under 45 cm)"), bDeadPose && DeadHead < 45.0f);
		if (Enemy)
		{
			UGameplayStatics::ApplyDamage(Enemy, 1.0f, GetController(), GetPlayer(), UDamageType::StaticClass());
		}
	});
	WaitForState(this, State, { ETHEnemyState::Stumble, ETHEnemyState::Chase, ETHEnemyState::Attack }, 2.0, TEXT("the siren playing dead, shot again, to get up"));
	Step([State]()
	{
		if (ATHEnemyCharacter* Enemy = State->Enemy.Get())
		{
			Enemy->Destroy();
		}
	});

	// Light and dark (developer, 2026-11-13): in the dark long corridor, a hunter 14 m off looking straight at the player can't
	// make them out (it sees in the dark within 10 m); a lamp on them, it can
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		APawn* Player = GetPlayer();
		const FVector Corridor(2.0 * 400.4, 4.0 * 400.4, Player->GetActorLocation().Z);	// build_enemy_test_map.py: cell (2, 4)
		Player->TeleportTo(Corridor, FRotator::ZeroRotator);
		GetController()->SetControlRotation(FRotator::ZeroRotator);
		UTHEnemyBehavior* Still = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Hunter")), GetTransientPackage());
		if (Still)
		{
			Still->PaceRadius = 0.0f;
			Still->WanderRadius = 0.0f;
		}
		UClass* Class = LoadClass<ATHEnemyCharacter>(nullptr, THFunctionalEnemy::ZombieClass);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ATHEnemyCharacter* Enemy = Class ? Player->GetWorld()->SpawnActor<ATHEnemyCharacter>(Class, Corridor + FVector(1400.0, 0.0, 2.0), FRotator(0.0f, 180.0f, 0.0f), Params) : nullptr;
		if (TestNotNull(TEXT("The hunter in the corridor spawned"), Enemy))
		{
			Enemy->SetBehavior(Still);
			State->Enemy = Enemy;
		}
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(1.5));
	Step([this, State]()
	{
		const UTHLightingSubsystem* Lighting = GetPlayer()->GetWorld()->GetSubsystem<UTHLightingSubsystem>();
		const float Light = Lighting ? Lighting->GetLightOn(GetPlayer()) : -1.0f;
		AddInfo(FString::Printf(TEXT("Dark corridor: light on the player %.2f; the hunter 14 m off: %s"), Light, *StateName(StateOf(State))));
		TestTrue(TEXT("The corridor is dark where the player stands (the test needs it)"), Light >= 0.0f && Light < UTHLightingSubsystem::LitAbove);
		TestTrue(TEXT("In the dark, a hunter 14 m off doesn't make the player out"), StateOf(State) != ETHEnemyState::Chase && StateOf(State) != ETHEnemyState::Attack);
		// A lamp on them
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APointLight* Lamp = GetPlayer()->GetWorld()->SpawnActor<APointLight>(GetPlayer()->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f), FRotator::ZeroRotator, Params);
		if (Lamp)
		{
			Lamp->PointLightComponent->SetMobility(EComponentMobility::Movable);
			Lamp->PointLightComponent->SetAttenuationRadius(600.0f);
			Lamp->PointLightComponent->SetIntensity(5000.0f);
			State->Wall = Lamp;
		}
	});
	WaitForState(this, State, { ETHEnemyState::Chase, ETHEnemyState::Attack }, 6.0, TEXT("the hunter to make the lit player out and come"));
	Step([State]()
	{
		for (AActor* Each : { static_cast<AActor*>(State->Wall.Get()), static_cast<AActor*>(State->Enemy.Get()) })
		{
			if (Each)
			{
				Each->Destroy();
			}
		}
	});

	// The director (developer, 2026-11-13: calm stretches building to peaks): calm long enough (cut to 0.3 s here), it brings a
	// wave in at a spawn point out of the player's sight (the side room); the player hurt, intensity rises
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		UWorld* World = GetPlayer()->GetWorld();
		UTHDirectorSettings* Settings = DuplicateObject<UTHDirectorSettings>(LoadObject<UTHDirectorSettings>(nullptr, TEXT("/Game/Enemies/Director/DA_Director.DA_Director")),
			GetTransientPackage());
		if (!TestNotNull(TEXT("DA_Director loads (setup_director.py)"), Settings))
		{
			return;
		}
		Settings->CalmBeforeWave = FVector2D(0.3f, 0.3f);
		Settings->SpawnDistance = FVector2D(500.0f, 6000.0f);
		Settings->MaxAlive = 50;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// The side room (cells x 0-1, y 4-5 of build_enemy_test_map.py), behind walls from the hall
		ATHSpawnPoint* Point = World->SpawnActor<ATHSpawnPoint>(FVector(200.0, 1800.0, 5.0), FRotator::ZeroRotator, Params);
		ATHDirector* Director = World->SpawnActor<ATHDirector>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (Director)
		{
			Director->Settings = Settings;
		}
		State->Wall = Point;
		State->Director = Director;
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the director to bring a wave in"), 4.0, [State]()
	{
		return State->Director.IsValid() && State->Director->GetSpawnedCount() > 0;
	}));
	Step([this, State]()
	{
		ATHDirector* Director = State->Director.Get();
		const AActor* Point = State->Wall.Get();
		int32 Near = 0;
		for (TActorIterator<ATHEnemyCharacter> It(GetPlayer()->GetWorld()); It; ++It)
		{
			Near += Point && FVector::Dist2D(It->GetActorLocation(), Point->GetActorLocation()) < 600.0 ? 1 : 0;
		}
		AddInfo(FString::Printf(TEXT("Director: brought in %d (at the spawn point %d), intensity %.2f"), Director ? Director->GetSpawnedCount() : -1, Near,
			Director ? Director->GetIntensity() : -1.0f));
		TestTrue(TEXT("The wave came in at the spawn point out of sight"), Near > 0);
		UGameplayStatics::ApplyDamage(GetPlayer(), 40.0f, nullptr, nullptr, UDamageType::StaticClass());
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.6));
	Step([this, State]()
	{
		ATHDirector* Director = State->Director.Get();
		AddInfo(FString::Printf(TEXT("Director after the player lost 40 health: intensity %.2f"), Director ? Director->GetIntensity() : -1.0f));
		TestTrue(TEXT("The player hurt, the director's intensity rises"), Director && Director->GetIntensity() > 0.15f);
		// Clear it all away
		if (const AActor* Point = State->Wall.Get())
		{
			for (TActorIterator<ATHEnemyCharacter> It(GetPlayer()->GetWorld()); It; ++It)
			{
				if (FVector::Dist2D(It->GetActorLocation(), Point->GetActorLocation()) < 3000.0)
				{
					It->Destroy();
				}
			}
		}
		for (AActor* Each : { static_cast<AActor*>(State->Wall.Get()), static_cast<AActor*>(Director) })
		{
			if (Each)
			{
				Each->Destroy();
			}
		}
	});

	// Ambushers, a pack of two (6 m apart): playing dead on the floor (developer, 2026-11-13), they ignore a step 7 m away;
	// a shot 4 m from the nearer one (out of its 3 m sight, inside its 5 m hearing) sets it off, up off the floor, and it
	// sets off the other
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		// One lying, one standing: each placed ambusher can choose (Wait Pose; developer, 2026-11-14)
		UTHEnemyBehavior* Ambusher = LoadBehavior(TEXT("DA_Ambusher"));
		SpawnEnemy(this, State, PlayerX + 1300.0, 180.0f, Ambusher, ETHWaitPose::Standing);
		State->Second = State->Enemy;
		SpawnEnemy(this, State, PlayerX + 700.0, 180.0f, Ambusher, ETHWaitPose::Lying);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(2.0));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const ATHEnemyCharacter* Standing = State->Second.Get();
		const float StandingHead = Standing ? Standing->GetMesh()->GetBoneLocation(TEXT("head")).Z - Standing->GetActorLocation().Z + Standing->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
		AddInfo(FString::Printf(TEXT("Ambusher set to wait standing: lying %d, head %.0f cm up"), Standing && Standing->IsLyingDown() ? 1 : 0, StandingHead));
		TestTrue(TEXT("An ambusher set to stand waits standing, head down (not lying)"), Standing && !Standing->IsLyingDown() && StandingHead > 100.0f);
		TestEqual(TEXT("The ambusher waits while the player is 7 m away"), StateName(StateOf(State)), StateName(ETHEnemyState::Ambush));
		TestTrue(TEXT("Waiting, it doesn't move"), Enemy && FVector::Dist2D(Enemy->GetActorLocation(), State->Start) < 20.0);
		const UTHHealthComponent* Health = Enemy ? Enemy->GetHealthComponent() : nullptr;
		TestTrue(TEXT("An ambusher has its behaviour's 90 health (one headshot)"), Health && FMath::IsNearlyEqual(Health->GetMaxHealth(), 90.0f) && FMath::IsNearlyEqual(Health->GetHealth(), 90.0f));
		// Head down, staring at the floor
		const float Head = Enemy ? Enemy->GetMesh()->GetBoneLocation(TEXT("head")).Z - Enemy->GetActorLocation().Z + Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
		const FVector HeadAhead = Enemy ? Enemy->GetMesh()->GetBoneLocation(TEXT("head")) - Enemy->GetActorLocation() : FVector::ZeroVector;
		const float Lean = Enemy ? NeckLean(Enemy) : 0.0f;
		AddInfo(FString::Printf(TEXT("Waiting ambusher: head %.0f cm above the floor (standing %.0f), %.0f cm ahead of its middle; neck to head leans %.0f deg (standing %.0f)"),
			Head, State->StandingHead, FVector::DotProduct(HeadAhead, Enemy ? Enemy->GetActorForwardVector() : FVector::ForwardVector), Lean, State->StandingLean));
		TestTrue(TEXT("Waiting, it plays dead: lying on the floor (head under 45 cm)"), Enemy && Enemy->IsLyingDown() && Head < 45.0f);
		// A step 7 m away: too far to hear
		APawn* Player = GetPlayer();
		UAISense_Hearing::ReportNoiseEvent(Player, Player->GetActorLocation(), 1.0f, Player, 0.0f, TEXT("Footstep"));
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.5));
	Step([this, State]()
	{
		TestEqual(TEXT("A step 7 m away doesn't set it off"), StateName(StateOf(State)), StateName(ETHEnemyState::Ambush));
		APawn* Player = GetPlayer();
		Player->TeleportTo(FVector(PlayerX + 300.0, HallY, Player->GetActorLocation().Z), FRotator::ZeroRotator);
		UAISense_Hearing::ReportNoiseEvent(Player, Player->GetActorLocation(), 4.0f, Player, 0.0f, TEXT("Gunshot"));
	});
	WaitForState(this, State, { ETHEnemyState::Stumble, ETHEnemyState::Chase, ETHEnemyState::Attack }, 2.0, TEXT("the ambusher to spring at a shot 4 m away (up off the floor)"));
	// Up, the get-up's last frame fades out under its moves (2026-11-14: the body jumped from it to the idle in one frame)
	TSharedRef<bool> bEasedOut = MakeShared<bool>(false);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the ambusher's get-up to end"), 4.0, [State, bEasedOut]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const UTHEnemyAnimInstance* Anim = Enemy ? Cast<UTHEnemyAnimInstance>(Enemy->GetMesh()->GetAnimInstance()) : nullptr;
		*bEasedOut = Anim && Anim->IsEasingOutOfStumble();
		return !Anim || !Anim->IsStumbling();
	}));
	Step([this, bEasedOut]()
	{
		TestTrue(TEXT("Up, the ambusher's get-up fades into its moves (no jump to them)"), *bEasedOut);
	});
	WaitForState(this, State, { ETHEnemyState::Chase, ETHEnemyState::Attack }, 3.0, TEXT("the ambusher, up, to go for the player"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the rest of the pack (6 m further) to spring at the same player"), 2.0, [State]()
	{
		const ATHEnemyCharacter* Other = State->Second.Get();
		const ATHEnemyAIController* Mind = Other ? Cast<ATHEnemyAIController>(Other->GetController()) : nullptr;
		return Mind && Mind->GetTarget() == GetPlayer() && Mind->GetEnemyState() != ETHEnemyState::Ambush;
	}));
	Step([State]()
	{
		if (ATHEnemyCharacter* Other = State->Second.Get())
		{
			Other->Destroy();
		}
	});

	// Picked off from 7 m (beyond its hearing) with one killing headshot: the rest of the pack still wakes, and comes for
	// the shooter (developer, 2026-11-13: a pack could be shot one by one, the others none the wiser)
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		UTHEnemyBehavior* Ambusher = LoadBehavior(TEXT("DA_Ambusher"));
		SpawnEnemy(this, State, PlayerX + 1300.0, 180.0f, Ambusher);
		State->Second = State->Enemy;
		SpawnEnemy(this, State, PlayerX + 700.0, 180.0f, Ambusher);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(2.0));
	Step([this, State]()
	{
		ATHEnemyCharacter* Enemy = State->Enemy.Get();
		APawn* Player = GetPlayer();
		if (!Enemy || !Player)
		{
			AddError(TEXT("The ambushers are missing"));
			return;
		}
		FHitResult Hit;
		Hit.BoneName = TEXT("head");
		Hit.ImpactPoint = Enemy->GetActorLocation();
		UGameplayStatics::ApplyPointDamage(Enemy, 200.0f, FVector(1.0, 0.0, 0.0), Hit, Player->GetController(), Player, UDamageType::StaticClass());
		TestTrue(TEXT("One headshot kills the waiting ambusher"), Enemy->IsDead());
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the rest of the pack to wake at the kill"), 1.0, [State]()
	{
		const ATHEnemyCharacter* Other = State->Second.Get();
		const ATHEnemyAIController* Mind = Other ? Cast<ATHEnemyAIController>(Other->GetController()) : nullptr;
		return Mind && Mind->GetEnemyState() != ETHEnemyState::Ambush;
	}));
	// Up off the floor first (a few seconds), then it picks its target
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the woken ambusher, up, to go for the shooter"), 6.0, [State]()
	{
		const ATHEnemyCharacter* Other = State->Second.Get();
		const ATHEnemyAIController* Mind = Other ? Cast<ATHEnemyAIController>(Other->GetController()) : nullptr;
		return Mind && Mind->GetTarget() == GetPlayer() && Mind->GetEnemyState() != ETHEnemyState::Ambush;
	}));
	Step([State]()
	{
		if (ATHEnemyCharacter* Other = State->Second.Get())
		{
			Other->Destroy();
		}
	});

	// A stalker in a dead end, the player 8.5 m off in the way and watching it (developer, 2026-11-15: it sat crouched in a
	// room while the player blocked the doorway, and never attacked): after its Standoff Time it comes for them
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		const APawn* Player = GetPlayer();
		const ACharacter* Walker = Cast<ACharacter>(Player);
		const double FloorZ = Player->GetActorLocation().Z - (Walker ? Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// A pocket open towards the player: a back wall and two sides, 5 m wide and deep (one wall alone it just hid behind)
		State->Pocket.Reset();
		auto AddWall = [&](const FVector& At, const FVector& Scale)
		{
			AStaticMeshActor* Wall = Player->GetWorld()->SpawnActor<AStaticMeshActor>(At, FRotator::ZeroRotator, Params);
			if (TestNotNull(TEXT("A dead-end wall spawns"), Wall))
			{
				Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
				Wall->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
				Wall->SetActorScale3D(Scale);
				State->Pocket.Add(Wall);
			}
		};
		AddWall(FVector(PlayerX + 1100.0, HallY, FloorZ + 200.0), FVector(0.3, 5.3, 4.0));
		AddWall(FVector(PlayerX + 850.0, HallY - 250.0, FloorZ + 200.0), FVector(5.0, 0.3, 4.0));
		AddWall(FVector(PlayerX + 850.0, HallY + 250.0, FloorZ + 200.0), FVector(5.0, 0.3, 4.0));
		UTHEnemyBehavior* Trapped = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Stalker")), GetTransientPackage());
		if (Trapped)
		{
			Trapped->StandoffTime = 2.0f;
			Trapped->bShowsItself = false;
			Trapped->WaylayChance = 0.0f;
			Trapped->LureInterval = FVector2D(100.0f, 100.0f);
		}
		SpawnEnemy(this, State, PlayerX + 850.0, 180.0f, Trapped);
	});
	WaitForState(this, State, { ETHEnemyState::Chase, ETHEnemyState::Attack }, 9.0, TEXT("the stalker in a dead end to come for the player"));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		AddInfo(FString::Printf(TEXT("Stalker in a dead end: %s at %s, %.0f cm from the player"), *StateName(StateOf(State)),
			Enemy ? *(Enemy->GetActorLocation() - GetPlayer()->GetActorLocation()).ToCompactString() : TEXT("-"),
			Enemy ? FVector::Dist2D(Enemy->GetActorLocation(), GetPlayer()->GetActorLocation()) : -1.0));
		for (const TWeakObjectPtr<AActor>& Wall : State->Pocket)
		{
			if (AActor* Each = Wall.Get())
			{
				Each->Destroy();
			}
		}
	});

	// A stalker plays with your lights (developer, 2026-11-13): 3 m off, they flicker; gone, they don't
	Step([this, State]()
	{
		ResetScene(State, 180.0f);
		UTHEnemyBehavior* Watcher = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Stalker")), GetTransientPackage());
		if (Watcher)
		{
			Watcher->bStrikes = false;
		}
		SpawnEnemy(this, State, PlayerX + 300.0, 180.0f, Watcher);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
	Step([this, State]()
	{
		const ATHCharacter* Character = Cast<ATHCharacter>(GetPlayer());
		const UTHEnemyBehavior* Stalker = LoadBehavior(TEXT("DA_Stalker"));
		AddInfo(FString::Printf(TEXT("Stalker 3 m off: the player's lights flicker at %.2f (within %.0f cm)"), Character ? Character->GetLightFlickerStrength() : -1.0f,
			Stalker ? Stalker->LightFlickerDistance : -1.0f));
		TestTrue(TEXT("A stalker close by makes the player's lights flicker"), Character && Character->GetLightFlickerStrength() > 0.3f);
		if (ATHEnemyCharacter* Enemy = State->Enemy.Get())
		{
			Enemy->Destroy();
		}
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.2));
	Step([this]()
	{
		const ATHCharacter* Character = Cast<ATHCharacter>(GetPlayer());
		TestTrue(TEXT("With it gone, the lights are steady again"), Character && Character->GetLightFlickerStrength() <= 0.0f);
	});

	// A stalker out of sight 12 m off, behind a wall, copies a sound to lure the player (developer, 2026-11-13); its next lure
	// brought forward to half a second
	Step([this, State]()
	{
		ResetScene(State, 180.0f);
		const APawn* Player = GetPlayer();
		const ACharacter* Walker = Cast<ACharacter>(Player);
		const double FloorZ = Player->GetActorLocation().Z - (Walker ? Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Wall = Player->GetWorld()->SpawnActor<AStaticMeshActor>(FVector(PlayerX + 600.0, HallY, FloorZ + 200.0), FRotator::ZeroRotator, Params);
		if (TestNotNull(TEXT("The test wall spawns"), Wall))
		{
			Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Wall->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Wall->SetActorScale3D(FVector(0.3, 6.0, 4.0));
			State->Wall = Wall;
		}
		UTHEnemyBehavior* Luring = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Stalker")), GetTransientPackage());
		if (Luring)
		{
			Luring->bStrikes = false;
			Luring->bShowsItself = false;
			Luring->WaylayChance = 0.0f;
			Luring->LureInterval = FVector2D(1.0f, 1.0f);
		}
		SpawnEnemy(this, State, PlayerX + 1200.0, 180.0f, Luring);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the stalker behind the wall to play a lure"), 5.0, [State]()
	{
		const ATHEnemyAIController* Mind = MindOf(State);
		return Mind && Mind->GetLuresPlayed() > 0;
	}));
	Step([this, State]()
	{
		const UTHEnemyBehavior* Stalker = LoadBehavior(TEXT("DA_Stalker"));
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		AddInfo(FString::Printf(TEXT("Stalker lured from %.0f cm (%d lure sounds), now %s"), Enemy ? FVector::Dist2D(Enemy->GetActorLocation(), GetPlayer()->GetActorLocation()) : -1.0,
			Stalker ? Stalker->LureSounds.Num() : -1, *StateName(StateOf(State))));
		TestTrue(TEXT("DA_Stalker has lure sounds (setup_tactics.py)"), Stalker && Stalker->LureSounds.Num() > 0);
		if (AActor* Wall = State->Wall.Get())
		{
			Wall->Destroy();
		}
	});

	// Stalker 10 m behind a silent player, sensing them (through walls too): it is onto them at once, and starts stalking
	// (developer, 2026-11-12: it should always be looking for the player)
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		GetController()->SetControlRotation(FRotator(0.0f, 180.0f, 0.0f));
		SpawnEnemy(this, State, PlayerX + 1000.0, 180.0f, LoadBehavior(TEXT("DA_Stalker")));
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(1.0));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const ATHEnemyAIController* Mind = Enemy ? Cast<ATHEnemyAIController>(Enemy->GetController()) : nullptr;
		const UTHEnemyBehavior* Stalker = LoadBehavior(TEXT("DA_Stalker"));
		AddInfo(FString::Printf(TEXT("Stalker 10 m behind a silent player, sense distance %.0f cm: %s, %s"), Stalker ? Stalker->SenseDistance : -1.0f,
			Mind && Mind->IsStalking() ? TEXT("onto them") : TEXT("unaware"), *StateName(StateOf(State))));
		TestTrue(TEXT("Within its sense distance, the stalker is onto a silent player it can't see"), Mind && Mind->IsStalking());
		TestEqual(TEXT("It didn't attack"), PlayerHealth()->GetHealth(), State->PlayerHealth);
	});

	// Stalker 10 m behind a silent player, not sensing (sense distance 0, to test this alone): unaware, it lies still;
	// the player turns and looks at it, and it notices them and hides without attacking (developer, 2026-11-12: stared
	// at, it used to sit there until they were 3 m away)
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		GetController()->SetControlRotation(FRotator(0.0f, 180.0f, 0.0f));
		UTHEnemyBehavior* Blind = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Stalker")), GetTransientPackage());
		if (Blind)
		{
			Blind->SenseDistance = 0.0f;
		}
		SpawnEnemy(this, State, PlayerX + 1000.0, 180.0f, Blind);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(1.5));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const ATHEnemyAIController* Mind = Enemy ? Cast<ATHEnemyAIController>(Enemy->GetController()) : nullptr;
		TestEqual(TEXT("Unaware, the stalker lurks"), StateName(StateOf(State)), StateName(ETHEnemyState::Lurk));
		TestTrue(TEXT("Unaware of the silent player with their back to it"), Mind && !Mind->IsStalking());
		TestTrue(TEXT("It doesn't move"), Enemy && FVector::Dist2D(Enemy->GetActorLocation(), State->Start) < 20.0);
		TestTrue(TEXT("It lurks crouched"), Enemy && Enemy->bIsCrouched);
		// They turn round and look at it
		GetController()->SetControlRotation(FRotator::ZeroRotator);
	});
	WaitForState(this, State, { ETHEnemyState::Retreat }, 3.0, TEXT("the stalker, looked at, to notice the player and go and hide"));
	// Once it has set off it finishes the run, even when the player stops looking (it used to stop and start)
	Step([]()
	{
		GetController()->SetControlRotation(FRotator(0.0f, 180.0f, 0.0f));
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.4));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const double Speed = Enemy ? Enemy->GetVelocity().Size2D() : 0.0;
		const double Distance = Enemy ? FVector::Dist2D(Enemy->GetActorLocation(), GetPlayer()->GetActorLocation()) : 0.0;
		AddInfo(FString::Printf(TEXT("Stalker after the player looked away: %s, %.0f cm/s, %.0f cm away"), *StateName(StateOf(State)), Speed, Distance));
		TestTrue(TEXT("Hiding, it keeps going when the player looks away"), StateOf(State) == ETHEnemyState::Retreat && Speed > 100.0);
		TestTrue(TEXT("Running to hide, it stays crouched (no getting up and down)"), Enemy && Enemy->bIsCrouched);
		GetController()->SetControlRotation(FRotator::ZeroRotator);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(3.0));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const APawn* Player = GetPlayer();
		const double Distance = Enemy ? FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation()) : 0.0;
		bool bSeen = false;
		if (Enemy)
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(THTestStalker), false, Player);
			Params.AddIgnoredActor(Enemy);
			FHitResult Hit;
			bSeen = !Player->GetWorld()->LineTraceSingleByChannel(Hit, Player->GetPawnViewLocation(), Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f), ECC_Visibility, Params);
		}
		AddInfo(FString::Printf(TEXT("Stalker seen: now %s, %.0f cm from the player (started 1000), %s"), *StateName(StateOf(State)), Distance,
			bSeen ? TEXT("in the open") : TEXT("out of sight")));
		TestTrue(TEXT("It hid: out of the player's sight, or further away"), !bSeen || Distance > 1050.0);
		TestTrue(TEXT("It never came closer than it started"), Distance > 850.0);
		TestEqual(TEXT("It didn't attack"), PlayerHealth()->GetHealth(), State->PlayerHealth);
		const ATHEnemyAIController* Mind = Enemy ? Cast<ATHEnemyAIController>(Enemy->GetController()) : nullptr;
		TestTrue(TEXT("It follows the player now"), Mind && Mind->IsStalking());
		// The player turns their back: after its wait it comes back to stalk them (it used to run off for good)
		GetController()->SetControlRotation(FRotator(0.0f, 180.0f, 0.0f));
	});
	// It may have escaped into another room: creeping back takes a while
	WaitForState(this, State, { ETHEnemyState::Stalk, ETHEnemyState::Strike }, 25.0, TEXT("the stalker, once the player looks away, to come back and stalk them"));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		AddInfo(FString::Printf(TEXT("Stalker back: %s, %.0f cm from the player"), *StateName(StateOf(State)),
			Enemy ? FVector::Dist2D(Enemy->GetActorLocation(), GetPlayer()->GetActorLocation()) : 0.0));
		GetController()->SetControlRotation(FRotator::ZeroRotator);
	});

	// A stalker 6.5 m behind a player who just stands there, back turned: it doesn't flee from them (it used to count
	// its own coming near as being approached, and run off again and again); no strikes, to see only that
	Step([this, State]()
	{
		ResetScene(State, 180.0f);
		UTHEnemyBehavior* Calm = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Stalker")), GetTransientPackage());
		if (Calm)
		{
			Calm->bStrikes = false;
		}
		SpawnEnemy(this, State, PlayerX + 650.0, 180.0f, Calm);
		APawn* Player = GetPlayer();
		UAISense_Hearing::ReportNoiseEvent(Player, Player->GetActorLocation(), 2.0f, Player, 0.0f, TEXT("Footstep"));
		State->bSawRetreat = false;
	});
	for (int32 Sample = 0; Sample < 6; ++Sample)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.5));
		Step([State]()
		{
			State->bSawRetreat |= StateOf(State) == ETHEnemyState::Retreat;
		});
	}
	Step([this, State]()
	{
		AddInfo(FString::Printf(TEXT("Stalker near a still player: now %s"), *StateName(StateOf(State))));
		TestFalse(TEXT("It doesn't run off from a player just standing there with their back turned"), State->bSawRetreat);
	});

	// A stalker 4 m in front of a player looking right at it: within its pounce distance (5 m) it goes for them instead of
	// hiding (developer, 2026-11-13: it was too focused on fleeing)
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		GetController()->SetControlRotation(FRotator::ZeroRotator);
		SpawnEnemy(this, State, PlayerX + 400.0, 180.0f, LoadBehavior(TEXT("DA_Stalker")));
		State->bSawRetreat = false;
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the close stalker to hit the player"), 8.0, [State]()
	{
		State->bSawRetreat |= StateOf(State) == ETHEnemyState::Retreat;
		return PlayerHealth()->GetHealth() < State->PlayerHealth;
	}));
	Step([this, State]()
	{
		const UTHEnemyBehavior* Stalker = LoadBehavior(TEXT("DA_Stalker"));
		AddInfo(FString::Printf(TEXT("Stalker 4 m in front of a player looking at it (pounce distance %.0f cm): health %.0f -> %.0f, %s"),
			Stalker ? Stalker->PounceDistance : -1.0f, State->PlayerHealth, PlayerHealth()->GetHealth(), *StateName(StateOf(State))));
		TestTrue(TEXT("Seen within its pounce distance, it attacked"), PlayerHealth()->GetHealth() < State->PlayerHealth);
		TestFalse(TEXT("It didn't run off to hide first"), State->bSawRetreat);
	});
	// It keeps at them after its blow (developer, 2026-11-13: it attacked once and ran off); then it is taken away
	// before a second blow (solo, that would end the run)
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
	Step([this, State]()
	{
		AddInfo(FString::Printf(TEXT("Stalker just after its blow, the player still close: %s"), *StateName(StateOf(State))));
		TestTrue(TEXT("After its blow it keeps at the player close by (doesn't run off)"),
			StateOf(State) == ETHEnemyState::Chase || StateOf(State) == ETHEnemyState::Attack || StateOf(State) == ETHEnemyState::Strike);
		if (ATHEnemyCharacter* Enemy = State->Enemy.Get())
		{
			Enemy->Destroy();
		}
	});

	// An opening: a player standing still with their back to it, its strike delay a minute off; it strikes anyway, soon
	// after they have stood still 2 s (opportunity delay cut to 1 s) (developer, 2026-11-13: it should hunt, not react)
	Step([this, State]()
	{
		ResetScene(State, 180.0f);
		UTHEnemyBehavior* Patient = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Stalker")), GetTransientPackage());
		if (Patient)
		{
			Patient->StrikeDelay = FVector2D(60.0f, 60.0f);
			Patient->OpportunityDelay = FVector2D(1.0f, 1.0f);
		}
		SpawnEnemy(this, State, PlayerX + 1000.0, 180.0f, Patient);
		State->MoanAt = GameTime();	// when it started, for the info line
	});
	WaitForState(this, State, { ETHEnemyState::Strike, ETHEnemyState::Attack }, 10.0, TEXT("the stalker to strike a player standing still, well before its strike delay"));
	Step([this, State]()
	{
		AddInfo(FString::Printf(TEXT("Stalker at an opening (player still, back turned; strike delay 60 s): striking after %.1f s"), GameTime() - State->MoanAt));
		State->MoanAt = -1.0;
	});

	// Stalker behind a player who looks away: woken by a sound, strikes (its strike delay cut to 1 s), moaning when close
	Step([this, State]()
	{
		ResetScene(State, 180.0f);
		UTHEnemyBehavior* Quick = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Stalker")), GetTransientPackage());
		if (Quick)
		{
			Quick->StrikeDelay = FVector2D(1.0f, 1.0f);
			Quick->IdleSounds.Reset();	// only the moan, so the test can tell
		}
		SpawnEnemy(this, State, PlayerX + 1000.0, 180.0f, Quick);
		APawn* Player = GetPlayer();
		UAISense_Hearing::ReportNoiseEvent(Player, Player->GetActorLocation(), 2.0f, Player, 0.0f, TEXT("Footstep"));
	});
	WaitForState(this, State, { ETHEnemyState::Strike, ETHEnemyState::Attack }, 4.0, TEXT("the stalker to strike from behind"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the stalker's strike to hit the player"), 12.0, [State]()
	{
		// When the moan starts, and when the blow lands
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const UTHEnemyBehavior* Stalker = Enemy ? Enemy->GetBehavior() : nullptr;
		if (State->MoanAt < 0.0 && Stalker && Enemy->GetLastVoice() && Stalker->CloseSounds.Contains(Enemy->GetLastVoice()))
		{
			State->MoanAt = GameTime();
		}
		const bool bHit = PlayerHealth()->GetHealth() < State->PlayerHealth;
		if (bHit)
		{
			State->HitAt = GameTime();
		}
		return bHit;
	}));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Enemy = State->Enemy.Get();
		const UTHEnemyBehavior* Stalker = Enemy ? Enemy->GetBehavior() : nullptr;
		const USoundBase* Voice = Enemy ? Enemy->GetLastVoice() : nullptr;
		AddInfo(FString::Printf(TEXT("Stalker's voice before its blow: %s"), *GetNameSafe(Voice)));
		TestTrue(TEXT("It moaned right behind the player before striking"), Stalker && Voice && Stalker->CloseSounds.Contains(Voice));
		// The whole moan before the blow: time to turn round
		const float MoanLength = Voice ? Voice->GetDuration() : 0.0f;
		AddInfo(FString::Printf(TEXT("Moan %.1f s long; the blow landed %.1f s after it started"), MoanLength, State->HitAt - State->MoanAt));
		TestTrue(TEXT("The blow lands only after the whole moan"), State->MoanAt >= 0.0 && State->HitAt - State->MoanAt >= MoanLength - 0.1);
	});
	// It keeps at them after its blow (developer, 2026-11-13: it attacked once and ran off); then it is taken away
	// before a second blow (solo, that would end the run)
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
	Step([this, State]()
	{
		AddInfo(FString::Printf(TEXT("Stalker just after its blow, the player still close: %s"), *StateName(StateOf(State))));
		TestTrue(TEXT("After its blow it keeps at the player close by (doesn't run off)"),
			StateOf(State) == ETHEnemyState::Chase || StateOf(State) == ETHEnemyState::Attack || StateOf(State) == ETHEnemyState::Strike);
		if (ATHEnemyCharacter* Enemy = State->Enemy.Get())
		{
			Enemy->Destroy();
		}
	});

	// A hunter's scream calls in the hunters near it, but one that was called doesn't pass the call on (2026-11-14: one
	// scream ran on through every hunter within Scream Radius of another). Three in a row, 4 m apart, a 5 m scream: the
	// first sees the player, the second (its back turned) is called, the third (its back turned, 8 m from the first) is not
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		UTHEnemyBehavior* Caller = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Hunter")), GetTransientPackage());
		if (!TestNotNull(TEXT("DA_Hunter loads"), Caller))
		{
			return;
		}
		Caller->PaceRadius = 0.0f;
		Caller->WanderRadius = 0.0f;
		Caller->ScreamRadius = 500.0f;
		Caller->bReactsToLight = false;	// only the call brings the other two
		Caller->bFlanks = false;
		Caller->AttackDamage = 0.0f;	// two hunters reach the player: no harm done
		State->Extras.Reset();
		SpawnEnemy(this, State, PlayerX + 1300.0, 0.0f, Caller);
		State->Extras.Add(State->Enemy);
		SpawnEnemy(this, State, PlayerX + 900.0, 0.0f, Caller);
		State->Second = State->Enemy;
		SpawnEnemy(this, State, PlayerX + 500.0, 180.0f, Caller);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the hunter called by the first one's scream to go for the player"), 6.0, [State]()
	{
		const ATHEnemyAIController* Called = State->Second.IsValid() ? Cast<ATHEnemyAIController>(State->Second->GetController()) : nullptr;
		return Called && Called->GetTarget() == GetPlayer();
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.5));
	Step([this, State]()
	{
		const ATHEnemyCharacter* Called = State->Second.Get();
		const ATHEnemyCharacter* Far = State->Extras.IsEmpty() ? nullptr : State->Extras[0].Get();
		const ATHEnemyAIController* FarMind = Far ? Cast<ATHEnemyAIController>(Far->GetController()) : nullptr;
		const UTHEnemyBehavior* Hunter = Called ? Called->GetBehavior() : nullptr;
		AddInfo(FString::Printf(TEXT("Scream: the called hunter's voice %s; the third hunter %s, target %s"), *GetNameSafe(Called ? Called->GetLastVoice() : nullptr),
			FarMind ? *StateName(FarMind->GetEnemyState()) : TEXT("gone"), *GetNameSafe(FarMind ? FarMind->GetTarget() : nullptr)));
		TestTrue(TEXT("A hunter out of the scream's reach isn't called by the hunter it called"), FarMind && !FarMind->GetTarget()
			&& FarMind->GetEnemyState() == ETHEnemyState::Idle);
		TestTrue(TEXT("A called hunter doesn't scream in turn"), Hunter && !Hunter->ScreamSounds.Contains(Called->GetLastVoice()));
		for (ATHEnemyCharacter* Each : { State->Second.Get(), State->Extras.IsEmpty() ? nullptr : State->Extras[0].Get() })
		{
			if (Each)
			{
				Each->Destroy();
			}
		}
		State->Extras.Reset();
	});

	// A noise it can't get to is given up: a hunter that hears a noise behind a closed door bangs on it, then leaves it
	// (2026-11-14: it banged on for good). Its give-up time cut to 4 s for the test
	TSharedRef<TArray<TWeakObjectPtr<AActor>>> NoiseDoor = MakeShared<TArray<TWeakObjectPtr<AActor>>>();
	TSharedRef<int32> NoiseBlows = MakeShared<int32>(-1);
	Step([this, State, NoiseDoor]()
	{
		ResetScene(State, 0.0f);
		const APawn* Player = GetPlayer();
		const ACharacter* Walker = Cast<ACharacter>(Player);
		const double FloorZ = Player->GetActorLocation().Z - (Walker ? Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UClass* DoorClass = LoadClass<AActor>(nullptr, TEXT("/Game/Template/Interaction/Blueprint/Door/BP_Door.BP_Door_C"));
		AActor* Door = DoorClass ? Player->GetWorld()->SpawnActor<AActor>(DoorClass, FVector(DoorX, HallY, FloorZ), FRotator::ZeroRotator, Params) : nullptr;
		if (!TestNotNull(TEXT("BP_Door spawns (the noise behind a door)"), Door))
		{
			return;
		}
		NoiseDoor->Add(Door);
		FVector Centre, Extent;
		Door->GetActorBounds(true, Centre, Extent);
		for (const double Side : { -1.0, 1.0 })
		{
			const double Inner = Side > 0.0 ? Centre.Y + Extent.Y : Centre.Y - Extent.Y;
			AStaticMeshActor* Wall = Player->GetWorld()->SpawnActor<AStaticMeshActor>(FVector(DoorX, Inner + Side * 400.0, FloorZ + 150.0), FRotator::ZeroRotator, Params);
			if (Wall)
			{
				Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
				Wall->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
				Wall->SetActorScale3D(FVector(0.2, 8.0, 3.0));
				NoiseDoor->Add(Wall);
			}
		}
		UTHEnemyBehavior* Still = DuplicateObject<UTHEnemyBehavior>(LoadBehavior(TEXT("DA_Hunter")), GetTransientPackage());
		if (Still)
		{
			Still->PaceRadius = 0.0f;
			Still->WanderRadius = 0.0f;
			Still->bReactsToLight = false;
			Still->WallMuffle = 1.0f;	// it hears the step through the door
			Still->InvestigateGiveUpTime = 4.0f;
		}
		// Its back to the door, so it doesn't see the player through any gap
		SpawnEnemy(this, State, PlayerX + 800.0, 0.0f, Still);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.5));
	Step([this, State]()
	{
		// A loud noise on the player's side of the door (as a shot: it runs to it)
		APawn* Player = GetPlayer();
		UAISense_Hearing::ReportNoiseEvent(Player, FVector(PlayerX + 100.0, HallY, Player->GetActorLocation().Z - 80.0), 1.0f, Player, 0.0f, TEXT("Gunshot"));
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the hunter that heard a noise behind the closed door to bang on it"), 12.0, [State]()
	{
		return State->Enemy.IsValid() && State->Enemy->GetDoorBangCount() >= 1;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(6.0));
	Step([this, State, NoiseBlows]()
	{
		*NoiseBlows = State->Enemy.IsValid() ? State->Enemy->GetDoorBangCount() : -1;
		AddInfo(FString::Printf(TEXT("Noise behind a door: %d blows by the time it gave up, now %s"), *NoiseBlows, *StateName(StateOf(State))));
		TestTrue(TEXT("It gives the noise up (no longer investigating)"), StateOf(State) != ETHEnemyState::Investigate);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(3.0));
	Step([this, State, NoiseDoor, NoiseBlows]()
	{
		TestTrue(TEXT("Having given up, it bangs on the door no more"), State->Enemy.IsValid() && State->Enemy->GetDoorBangCount() == *NoiseBlows);
		for (const TWeakObjectPtr<AActor>& Each : *NoiseDoor)
		{
			if (AActor* Actor = Each.Get())
			{
				Actor->Destroy();
			}
		}
	});

	// Health comes back by itself (developer, 2026-11-12: no healing items): nothing for RegenDelay (5 s) after a hit,
	// the blood staying on screen meanwhile, then slowly up to full
	Step([this, State]()
	{
		ResetScene(State, 0.0f);
		UGameplayStatics::ApplyDamage(GetPlayer(), 40.0f, nullptr, nullptr, UDamageType::StaticClass());
		State->PlayerHealth = PlayerHealth()->GetHealth();
		State->HitAt = GetPlayer()->GetWorld()->GetTimeSeconds();
		TestEqual(TEXT("40 damage taken"), State->PlayerHealth, PlayerHealth()->GetMaxHealth() - 40.0f);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(3.5));
	Step([this, State]()
	{
		TestEqual(TEXT("No healing within the regeneration delay"), PlayerHealth()->GetHealth(), State->PlayerHealth);
		const ATHPlayerController* PC = Cast<ATHPlayerController>(GetController());
		const UTHStatusWidget* Screen = PC ? PC->GetStatusWidget() : nullptr;
		// The widget doesn't tick headless, so its fading can't be watched here: check what it would hold the blood for
		TestTrue(TEXT("The blood stays on screen until healing starts"), Screen && FMath::IsNearlyEqual(Screen->GetBloodHold(), PlayerHealth()->GetRegenDelay()));
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("health to start coming back"), 4.0, [State]()
	{
		return PlayerHealth()->GetHealth() > State->PlayerHealth;
	}));
	Step([this, State]()
	{
		const double After = GetPlayer()->GetWorld()->GetTimeSeconds() - State->HitAt;
		AddInfo(FString::Printf(TEXT("Healing started %.1f s after the hit"), After));
		TestTrue(TEXT("Healing starts after the regeneration delay"), After >= PlayerHealth()->GetRegenDelay() - 0.15);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("health to come back to full"), 12.0, []()
	{
		return PlayerHealth()->GetHealth() >= PlayerHealth()->GetMaxHealth();
	}));
	Step([this, State]()
	{
		const double After = GetPlayer()->GetWorld()->GetTimeSeconds() - State->HitAt;
		AddInfo(FString::Printf(TEXT("Back to full %.1f s after the hit"), After));
		TestTrue(TEXT("Full again in about 5 s + 40 / 5 a second"), After < PlayerHealth()->GetRegenDelay() + 40.0f / 5.0f + 1.5);
	});

	EndPlay(this);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
