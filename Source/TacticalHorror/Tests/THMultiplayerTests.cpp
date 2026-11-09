// Multiplayer tests: a listen server (the host, player 1) and one client (player 2) in a single play session on the
// test map. Checks that what one player does reaches the server and the other player. See TESTING.md.

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Multiplayer/THDoorSyncComponent.h"
#include "Multiplayer/THKeyPickupComponent.h"
#include "Multiplayer/THLockedDoorComponent.h"
#include "Multiplayer/THValveDoorComponent.h"
#include "Player/THCharacter.h"
#include "Range/THTargetButton.h"
#include "Range/THTargetCarrier.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/THTestHelpers.h"
#include "Weapons/THWeaponBase.h"
#include "Weapons/THWeaponComponent.h"
#include "Weapons/THWeaponData.h"

namespace THMultiplayer
{
	using namespace THTestHelpers;

	static UWorld* GetWorld(ENetMode NetMode)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NetMode)
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	static UWorld* ServerWorld() { return GetWorld(NM_ListenServer); }
	static UWorld* ClientWorld() { return GetWorld(NM_Client); }

	/** In World: the pawn played on this machine (bLocal) or the other player's pawn. */
	static APawn* FindPlayer(UWorld* World, bool bLocal)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			if (It->FindComponentByClass<UTHWeaponComponent>() && It->IsLocallyControlled() == bLocal)
			{
				return *It;
			}
		}
		return nullptr;
	}

	static UTHWeaponComponent* Weapons(APawn* Pawn)
	{
		return Pawn ? Pawn->FindComponentByClass<UTHWeaponComponent>() : nullptr;
	}

	/** The host's player on the server; the host's player as the client sees it; the client's player on each. */
	static UTHWeaponComponent* HostOnServer() { return Weapons(FindPlayer(ServerWorld(), true)); }
	static UTHWeaponComponent* HostOnClient() { return Weapons(FindPlayer(ClientWorld(), false)); }
	static UTHWeaponComponent* ClientOnClient() { return Weapons(FindPlayer(ClientWorld(), true)); }
	static UTHWeaponComponent* ClientOnServer() { return Weapons(FindPlayer(ServerWorld(), false)); }

	static ATHWeaponBase* Held(UTHWeaponComponent* Component)
	{
		return Component ? Component->GetCurrentWeapon() : nullptr;
	}

	static bool HasLoadout(const UTHWeaponComponent* Component)
	{
		return Component && Component->GetWeapons().Num() >= 2 && Component->GetCurrentWeapon();
	}

	/** Both players stand on the test map's floor (top at Z = 0) on both machines: none fell through it. */
	static void CheckPlayersOnFloor(FAutomationTestBase* Test)
	{
		Step([Test]()
		{
			for (UWorld* World : { ServerWorld(), ClientWorld() })
			{
				for (TActorIterator<APawn> It(World); It; ++It)
				{
					Test->TestTrue(*FString::Printf(TEXT("%s on the floor (Z %.0f) in the %s's world"), *It->GetName(), It->GetActorLocation().Z,
						World == ServerWorld() ? TEXT("server") : TEXT("client")), It->GetActorLocation().Z > 0.0);
				}
			}
		});
	}
	/** Opens the map (the test map by default) in the editor and starts a listen server with one client. */
	static void StartListenServer(FAutomationTestBase* Test, const TCHAR* Map = TestMap, int32 Players = 2)
	{
		Step([Map, Players]()
		{
			UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
			if (!EditorWorld || EditorWorld->GetOutermost()->GetName() != Map)
			{
				FEditorFileUtils::LoadMap(Map, false, false);
			}

			ULevelEditorPlaySettings* Settings = NewObject<ULevelEditorPlaySettings>();
			Settings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
			Settings->SetPlayNumberOfClients(Players);
			Settings->SetRunUnderOneProcess(true);
			Settings->bLaunchSeparateServer = false;

			FRequestPlaySessionParams Params;
			Params.EditorPlaySettings = Settings;
			GEditor->RequestPlaySession(Params);
			GEditor->StartQueuedPlaySessionRequest();
		});
		ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(Test, TEXT("host and client to spawn with their loadouts, each seeing the other"), 90.0, []()
		{
			return HasLoadout(HostOnServer()) && HasLoadout(ClientOnServer()) && HasLoadout(ClientOnClient()) && Held(HostOnClient());
		}));
		// Both draws
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(1.5));
	}
}

using namespace THMultiplayer;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHListenServerTest, "TacticalHorror.Multiplayer.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHListenServerTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		int32 ServerAmmo = 0;
		bool bLightWasOn = false;
	};
	TSharedRef<FState> State = MakeShared<FState>();

	StartListenServer(this);
	CheckPlayersOnFloor(this);

	// Both players hold the same first weapon on every machine
	Step([this]()
	{
		TestEqual(TEXT("Two players on the server"), ServerWorld()->GetNumPlayerControllers(), 2);
		TestTrue(TEXT("The client sees the host holding their first weapon"),
			Held(HostOnClient()) && Held(HostOnClient())->GetClass() == Held(HostOnServer())->GetClass());
	});

	// The client fires: predicted on the client, confirmed by the server
	Step([this, State]()
	{
		ATHWeaponBase* Mine = Held(ClientOnClient());
		ATHWeaponBase* OnServer = Held(ClientOnServer());
		State->ServerAmmo = OnServer->GetCurrentAmmo();
		const int32 Before = Mine->GetCurrentAmmo();
		Mine->StartFire();
		Mine->StopFire();
		TestEqual(TEXT("Client's own ammo drops right away (prediction)"), Mine->GetCurrentAmmo(), Before - 1);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the server to take the client's shot (ammo - 1)"), 5.0, [State]()
	{
		const ATHWeaponBase* OnServer = Held(ClientOnServer());
		return OnServer && OnServer->GetCurrentAmmo() == State->ServerAmmo - 1;
	}));

	// The client reloads: the server runs it, the full magazine comes back to the client
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
	Step([]() { Held(ClientOnClient())->StartReload(); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the server to start the client's reload"), 5.0, []()
	{
		const ATHWeaponBase* OnServer = Held(ClientOnServer());
		return OnServer && OnServer->IsReloading();
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the client's magazine to be full again on the client"), 15.0, []()
	{
		const ATHWeaponBase* Mine = Held(ClientOnClient());
		return Mine && !Mine->IsReloading() && Mine->GetCurrentAmmo() == Mine->GetWeaponData()->MagazineSize;
	}));
	Step([this]()
	{
		const ATHWeaponBase* OnServer = Held(ClientOnServer());
		TestEqual(TEXT("Server agrees the client's magazine is full"), OnServer->GetCurrentAmmo(), OnServer->GetWeaponData()->MagazineSize);
		TestEqual(TEXT("Client and server agree on the reserve"), Held(ClientOnClient())->GetReserveAmmo(), OnServer->GetReserveAmmo());
	});

	// The host reloads: the client sees the host reloading (third-person reload and magazine)
	Step([]()
	{
		ATHWeaponBase* HostGun = Held(HostOnServer());
		HostGun->StartFire();
		HostGun->StopFire();
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
	Step([]() { Held(HostOnServer())->StartReload(); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the client to see the host reloading"), 5.0, []()
	{
		const ATHWeaponBase* HostGun = Held(HostOnClient());
		return HostGun && HostGun->IsReloading();
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the client to see the host's reload end"), 15.0, []()
	{
		const ATHWeaponBase* HostGun = Held(HostOnClient());
		return HostGun && !HostGun->IsReloading();
	}));

	// The host's weapon light: the client sees the beam come on and go off
	Step([this, State]()
	{
		UTHWeaponComponent* Host = HostOnServer();
		State->bLightWasOn = Host->IsWeaponLightOn();
		if (State->bLightWasOn)
		{
			Host->ToggleWeaponLight();
		}
		Host->ToggleWeaponLight();
		TestTrue(TEXT("Host's weapon light on"), Host->IsWeaponLightOn());
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the client to see the host's weapon light beam"), 5.0, []()
	{
		return IsBeamVisible(Held(HostOnClient()));
	}));
	Step([]() { HostOnServer()->ToggleWeaponLight(); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the client to see the host's weapon light go off"), 5.0, []()
	{
		return !IsBeamVisible(Held(HostOnClient()));
	}));

	// The host switches to the pistol: the client sees the pistol in their hands
	Step([]() { HostOnServer()->EquipWeaponAtIndex(1); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the client to see the host switch weapons"), 5.0, []()
	{
		UTHWeaponComponent* Host = HostOnClient();
		return Host && Host->GetWeapons().Num() >= 2 && Host->GetCurrentWeapon() == Host->GetWeapons()[1];
	}));

	// The host takes out the hand flashlight: the client sees it, and its beam
	Step([]()
	{
		UTHWeaponComponent* Host = HostOnServer();
		Host->GiveHandFlashlight(false);
		Host->EquipWeaponAtIndex(2);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the client to see the host holding the flashlight"), 5.0, []()
	{
		const ATHWeaponBase* Item = Held(HostOnClient());
		return Item && !Item->IsFirearm();
	}));
	Step([this]()
	{
		auto Describe = [](UTHWeaponComponent* Component)
		{
			if (!Component)
			{
				return FString(TEXT("no weapon component"));
			}
			TArray<FString> Names;
			for (const ATHWeaponBase* Weapon : Component->GetWeapons())
			{
				Names.Add(GetNameSafe(Weapon));
			}
			return FString::Printf(TEXT("holds %s of [%s]"), *GetNameSafe(Component->GetCurrentWeapon()), *FString::Join(Names, TEXT(", ")));
		};
		AddInfo(FString::Printf(TEXT("Host on the server %s; host as the client sees them %s"), *Describe(HostOnServer()), *Describe(HostOnClient())));
		if (const APawn* Host = FindPlayer(ServerWorld(), true))
		{
			AddInfo(FString::Printf(TEXT("Host on the server at %s, velocity %s, hidden %d, collision %d, net cull %.0f, dormancy %d"),
				*Host->GetActorLocation().ToCompactString(), *Host->GetVelocity().ToCompactString(), Host->IsHidden(),
				Host->GetActorEnableCollision(), FMath::Sqrt(Host->GetNetCullDistanceSquared()), (int32)Host->NetDormancy));
		}
		if (UWorld* Client = ClientWorld())
		{
			for (TActorIterator<APawn> It(Client); It; ++It)
			{
				AddInfo(FString::Printf(TEXT("Client sees pawn %s: local %d, role %d, controller %s, weapons %s, at %s"), *It->GetName(),
					It->IsLocallyControlled(), (int32)It->GetLocalRole(), *GetNameSafe(It->GetController()),
					*Describe(It->FindComponentByClass<UTHWeaponComponent>()), *It->GetActorLocation().ToCompactString()));
			}
		}
		else
		{
			AddInfo(TEXT("No client world"));
		}
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the client to see the host's flashlight beam"), 5.0, []()
	{
		return IsBeamVisible(Held(HostOnClient()));
	}));
	Step([]() { Held(HostOnServer())->StartFire(); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the client to see the host switch the flashlight off"), 5.0, []()
	{
		return !IsBeamVisible(Held(HostOnClient()));
	}));

	EndPlay(this);
	return true;
}

namespace THMultiplayerDoor
{
	/** The door of this class (name prefix, like BP_Door_C) in this machine's world. */
	static AActor* FindDoor(UWorld* World, const FString& ClassPrefix = TEXT("BP_Door_C"))
	{
		if (World)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->GetClass()->GetName().StartsWith(ClassPrefix))
				{
					return *It;
				}
			}
		}
		return nullptr;
	}

	/** The template door's own state. BP_Door and BP_Outward_Door keep Opening, BP_SlideDoor keeps IsClosed. */
	static bool IsDoorOpen(const AActor* Door)
	{
		if (!Door)
		{
			return false;
		}
		if (const FBoolProperty* Opening = FindFProperty<FBoolProperty>(Door->GetClass(), TEXT("Opening")))
		{
			return Opening->GetPropertyValue_InContainer(Door);
		}
		const FBoolProperty* IsClosed = FindFProperty<FBoolProperty>(Door->GetClass(), TEXT("IsClosed"));
		return IsClosed && !IsClosed->GetPropertyValue_InContainer(Door);
	}

	/** Whether the door's panel has really moved: swinging doors turn their Door mesh, the sliding door moves StaticMesh. */
	static bool IsPanelAway(const AActor* Door)
	{
		if (Door)
		{
			TInlineComponentArray<UStaticMeshComponent*> Meshes(Door);
			for (const UStaticMeshComponent* Mesh : Meshes)
			{
				if (Mesh->GetName() == TEXT("Door"))
				{
					return FMath::Abs(Mesh->GetRelativeRotation().Yaw) > 45.0;
				}
				if (Mesh->GetName() == TEXT("StaticMesh"))
				{
					return Mesh->GetRelativeLocation().Size() > 50.0;
				}
			}
		}
		return false;
	}

	static bool DoorMatches(UWorld* World, bool bOpen, const FString& ClassPrefix = TEXT("BP_Door_C"))
	{
		const AActor* Door = FindDoor(World, ClassPrefix);
		return Door && IsDoorOpen(Door) == bOpen && IsPanelAway(Door) == bOpen;
	}

	static void CallInteract(AActor* Door, APawn* Player)
	{
		UFunction* Interact = Door ? Door->FindFunction(TEXT("Interact")) : nullptr;
		if (!Interact)
		{
			return;
		}
		TArray<uint8> Arguments;
		Arguments.SetNumZeroed(Interact->ParmsSize);
		Interact->InitializeStruct(Arguments.GetData());
		for (TFieldIterator<FObjectProperty> It(Interact); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			if (Player->IsA(It->PropertyClass))
			{
				It->SetObjectPropertyValue_InContainer(Arguments.GetData(), Player);
			}
		}
		Door->ProcessEvent(Interact, Arguments.GetData());
		Interact->DestroyStruct(Arguments.GetData());
	}

	/** Puts a pawn at XY on the machines that hold it (the host's: server only; the client's: both). */
	static void Teleport(bool bHost, const FVector2D& XY)
	{
		const FVector Location(XY.X, XY.Y, -10.0);
		FindPlayer(ServerWorld(), bHost)->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		if (!bHost)
		{
			FindPlayer(ClientWorld(), true)->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	static APawn* LocalPawn(bool bHost)
	{
		return FindPlayer(bHost ? ServerWorld() : ClientWorld(), true);
	}

	/** Aims at the door's panel wherever it has swung to: an open door leaves the doorway empty. */
	static void AimAtDoor(APawn* Pawn)
	{
		APlayerController* Controller = Cast<APlayerController>(Pawn->GetController());
		const AActor* Door = FindDoor(Pawn->GetWorld());
		if (!Controller || !Door)
		{
			return;
		}
		TInlineComponentArray<UStaticMeshComponent*> Meshes(Door);
		for (const UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetName() == TEXT("Door"))
			{
				FVector Eye;
				FRotator Ignored;
				Controller->GetPlayerViewPoint(Eye, Ignored);
				Controller->SetControlRotation((Mesh->Bounds.Origin - Eye).Rotation());
			}
		}
	}

	/** Presses the real interact input on the machine that plays this pawn. */
	static void PressInteract(APawn* Pawn)
	{
		const APlayerController* Controller = Cast<APlayerController>(Pawn->GetController());
		const UInputAction* Action = LoadObject<UInputAction>(nullptr, TEXT("/Game/Blueprint/Input/Actions/IA_Interact.IA_Interact"));
		UEnhancedInputLocalPlayerSubsystem* Input = Controller && Controller->GetLocalPlayer()
			? Controller->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
		if (Input && Action)
		{
			Input->InjectInputForAction(Action, FInputActionValue(true), {}, {});
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHDoorSyncTest, "TacticalHorror.Multiplayer.DoorSync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHDoorSyncTest::RunTest(const FString& Parameters)
{
	using namespace THMultiplayerDoor;

	const auto ExpectDoor = [this](bool bOpen, const FString& What, const FString& ClassPrefix)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, What + TEXT(": the door reaching that state on both machines"), 8.0, [bOpen, ClassPrefix]()
		{
			return DoorMatches(ServerWorld(), bOpen, ClassPrefix) && DoorMatches(ClientWorld(), bOpen, ClassPrefix);
		}));
		// The panels finish moving, so the next press is not ignored as "still moving"
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(3.5));
		Step([this, bOpen, What, ClassPrefix]()
		{
			TestTrue(*(What + TEXT(": still agreed once settled")),
				DoorMatches(ServerWorld(), bOpen, ClassPrefix) && DoorMatches(ClientWorld(), bOpen, ClassPrefix));
		});
	};

	// One real press by one player, with the other out of the way
	const auto Press = [this, ExpectDoor](bool bHost, bool bOpen, const FString& What)
	{
		Step([bHost]()
		{
			Teleport(!bHost, FVector2D(100.0, -300.0));
			Teleport(bHost, FVector2D(-340.0, -170.0)); // close enough to see the open panel at an angle: nearly edge-on it is a thin target
		});
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
		Step([bHost]() { AimAtDoor(LocalPawn(bHost)); });
		// The character's interact trace has to see the panel first
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.5));
		Step([bHost]() { PressInteract(LocalPawn(bHost)); });
		ExpectDoor(bOpen, What, TEXT("BP_Door_C"));
	};

	StartListenServer(this, TEXT("/Game/Maps/L_FiringRange"));
	Step([this]()
	{
		TestNotNull(TEXT("The door is in the server's world"), FindDoor(ServerWorld()));
		TestNotNull(TEXT("The door is in the client's world"), FindDoor(ClientWorld()));
		TestTrue(TEXT("The door starts closed on both machines"), DoorMatches(ServerWorld(), false) && DoorMatches(ClientWorld(), false));
	});

	Press(true, true, TEXT("The host opens it"));
	Press(false, false, TEXT("The client closes it"));
	Press(false, true, TEXT("The client opens it"));

	// Movement is checked by the server: this fails if the door is only open on the client's screen
	Step([]() { Teleport(false, FVector2D(-300.0, -193.0)); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the client to walk through the open door into the room, on the server"), 8.0, []()
	{
		FindPlayer(ClientWorld(), true)->AddMovementInput(FVector(-1.0, 0.0, 0.0));
		return FindPlayer(ServerWorld(), false)->GetActorLocation().X < -520.0;
	}));

	Press(true, false, TEXT("The host closes it"));

	// The server has the last word: a press by a client too far away to reach the door is undone
	Step([]()
	{
		Teleport(false, FVector2D(600.0, -300.0));
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
	Step([]() { CallInteract(FindDoor(ClientWorld()), LocalPawn(false)); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.5));
	Step([this]()
	{
		TestTrue(TEXT("The client's out-of-reach press moved its own door"), IsDoorOpen(FindDoor(ClientWorld())));
		TestFalse(TEXT("The server did not take the out-of-reach press"), IsDoorOpen(FindDoor(ServerWorld())));
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(4.0));
	ExpectDoor(false, TEXT("The client's door goes back after the refused press"), TEXT("BP_Door_C"));

	// The other plain doors, spawned by the server and already opened when the client first hears of them:
	// also what a player joining late sees
	for (const FString& Name : TArray<FString>{ TEXT("BP_Outward_Door"), TEXT("BP_SlideDoor") })
	{
		const FString Prefix = Name + TEXT("_C");
		Step([Name]()
		{
			UClass* DoorClass = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Game/Template/Interaction/Blueprint/Door/%s.%s_C"), *Name, *Name));
			AActor* Door = DoorClass ? ServerWorld()->SpawnActor<AActor>(DoorClass, FVector(100.0, 100.0, -116.0), FRotator::ZeroRotator) : nullptr;
			if (Door)
			{
				CallInteract(Door, LocalPawn(true));
			}
		});
		ExpectDoor(true, Name + TEXT(" opened by the server before the client knew of it"), Prefix);
		Step([Prefix]() { CallInteract(FindDoor(ServerWorld(), Prefix), LocalPawn(true)); });
		ExpectDoor(false, Name + TEXT(" closed by the host"), Prefix);
	}

	EndPlay(this);
	return true;
}

namespace THMultiplayerLocks
{
	using namespace THMultiplayerDoor;

	static UStaticMeshComponent* FindMesh(const AActor* Actor, const TCHAR* Name)
	{
		if (Actor)
		{
			TInlineComponentArray<UStaticMeshComponent*> Meshes(Actor);
			for (UStaticMeshComponent* Mesh : Meshes)
			{
				if (Mesh->GetName() == Name)
				{
					return Mesh;
				}
			}
		}
		return nullptr;
	}

	static float PanelYaw(const AActor* Door)
	{
		const UStaticMeshComponent* Panel = FindMesh(Door, TEXT("Door"));
		return Panel ? Panel->GetRelativeRotation().Yaw : 0.0f;
	}

	// The doors and key these tests use, spawned by the server in the room behind the range door (16 x 16 m: X -1991..-390,
	// Y -440..1161). Doors face X: a player on the east side walks through them going -X.
	static constexpr double FloorZ = -116.0;
	static constexpr double DoorX = -1600.0;
	static constexpr double LockY = -200.0;
	static constexpr double KeypadY = 200.0;
	static constexpr double ValveY = 600.0;
	static const FVector2D KeyXY(-1100.0, 900.0);
	static const FVector2D AtLock(DoorX + 100.0, LockY);
	static const FVector2D AtKeypad(DoorX + 100.0, KeypadY);
	static const FVector2D AtKey(KeyXY.X + 110.0, KeyXY.Y);
	static const FVector2D AwayFromDoors(-800.0, 60.0);

	/** Server: spawns a template interaction Blueprint (e.g. "Door/BP_DoorLock"), its door frame centred on XY, on the floor. */
	static AActor* SpawnTemplateActor(const FString& Name, const FVector2D& XY, double Z = FloorZ)
	{
		const FString Path = FString::Printf(TEXT("/Game/Template/Interaction/Blueprint/%s.%s_C"), *Name, *FPaths::GetCleanFilename(Name));
		UClass* Class = LoadClass<AActor>(nullptr, *Path);
		AActor* Actor = Class ? ServerWorld()->SpawnActor<AActor>(Class, FVector(XY.X, XY.Y, Z), FRotator::ZeroRotator) : nullptr;
		if (const UStaticMeshComponent* Frame = Actor ? FindMesh(Actor, TEXT("DoorFrame")) : nullptr)
		{
			const FBox Box = Frame->Bounds.GetBox();
			Actor->AddActorWorldOffset(FVector(XY.X - Box.GetCenter().X, XY.Y - Box.GetCenter().Y, Z - Box.Min.Z));
		}
		return Actor;
	}

	/** The server spawns the key, and the doors too unless bKeyOnly; waits until every client has them. */
	static void SpawnTestDoors(FAutomationTestBase* Test, bool bKeyOnly)
	{
		Step([Test, bKeyOnly]()
		{
			Test->TestNotNull(TEXT("Key spawned"), SpawnTemplateActor(TEXT("Door/BP_Key"), KeyXY, FloorZ + 10.0));
			if (!bKeyOnly)
			{
				Test->TestNotNull(TEXT("Locked door spawned"), SpawnTemplateActor(TEXT("Door/BP_DoorLock"), FVector2D(DoorX, LockY)));
				Test->TestNotNull(TEXT("Keypad door spawned"), SpawnTemplateActor(TEXT("Door/BP_KeypadDoor"), FVector2D(DoorX, KeypadY)));
				Test->TestNotNull(TEXT("Valve door spawned"), SpawnTemplateActor(TEXT("Valve/BP_ValveDoor"), FVector2D(DoorX, ValveY)));
			}
		});
		ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(Test, TEXT("every client to have the test doors and key"), 10.0, [bKeyOnly]()
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (Context.WorldType != EWorldType::PIE || !World || World->GetNetMode() != NM_Client)
				{
					continue;
				}
				if (!FindDoor(World, TEXT("BP_Key_C")) || (!bKeyOnly && (!FindDoor(World, TEXT("BP_DoorLock_C"))
					|| !FindDoor(World, TEXT("BP_KeypadDoor_C")) || !FindDoor(World, TEXT("BP_ValveDoor_C")))))
				{
					return false;
				}
			}
			return true;
		}));
	}

	/** Stands the player in front of this mesh, on the room's side, and looks at it. Moves the other player away. */
	static void StandAt(bool bHost, const FVector2D& XY)
	{
		Teleport(!bHost, AwayFromDoors);
		Teleport(bHost, XY);
	}

	static void LookAt(const APawn* Pawn, const FVector& Target)
	{
		APlayerController* Controller = Cast<APlayerController>(Pawn->GetController());
		FVector Eye;
		FRotator Ignored;
		Controller->GetPlayerViewPoint(Eye, Ignored);
		Controller->SetControlRotation((Target - Eye).Rotation());
	}

	static void LookAt(bool bHost, const FVector& Target)
	{
		LookAt(LocalPawn(bHost), Target);
	}

	/** Where to look to press the key on this machine. */
	static FVector KeyAim(UWorld* World)
	{
		const UStaticMeshComponent* Key = FindMesh(FindDoor(World, TEXT("BP_Key_C")), TEXT("StaticMesh"));
		return Key ? Key->Bounds.Origin : FVector::ZeroVector;
	}

	static UTHKeyPickupComponent* KeyOn(UWorld* World)
	{
		const AActor* Key = FindDoor(World, TEXT("BP_Key_C"));
		return Key ? Key->FindComponentByClass<UTHKeyPickupComponent>() : nullptr;
	}

	/** Whether the key is out of the world (or in it) as this machine has it: its state, visibility and collision. */
	static bool KeyIsTaken(UWorld* World, bool bTaken)
	{
		const UTHKeyPickupComponent* Key = KeyOn(World);
		return Key && Key->IsTaken() == bTaken && Key->GetOwner()->IsHidden() == bTaken && Key->GetOwner()->GetActorEnableCollision() != bTaken;
	}

	/** The server's pawn for the player with this id. */
	static APawn* ServerPawn(int32 PlayerId)
	{
		for (TActorIterator<APawn> It(ServerWorld()); It; ++It)
		{
			if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerId() == PlayerId)
			{
				return *It;
			}
		}
		return nullptr;
	}

	static UTHLockedDoorComponent* LockOn(UWorld* World)
	{
		const AActor* Door = FindDoor(World, TEXT("BP_DoorLock_C"));
		return Door ? Door->FindComponentByClass<UTHLockedDoorComponent>() : nullptr;
	}

	static UTHValveDoorComponent* ValveOn(UWorld* World)
	{
		const AActor* Door = FindDoor(World, TEXT("BP_ValveDoor_C"));
		return Door ? Door->FindComponentByClass<UTHValveDoorComponent>() : nullptr;
	}

	static void HoldValve(bool bHost, bool bHold)
	{
		const APawn* Pawn = LocalPawn(bHost);
		const ATHCharacter* Character = Cast<ATHCharacter>(Pawn);
		const APlayerController* Controller = Cast<APlayerController>(Pawn->GetController());
		UEnhancedInputLocalPlayerSubsystem* Input = Controller->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		Input->InjectInputForAction(Character->GetValveAction(), FInputActionValue(bHold), {}, {});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHLockedDoorsTest, "TacticalHorror.Multiplayer.LockedDoors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHLockedDoorsTest::RunTest(const FString& Parameters)
{
	using namespace THMultiplayerDoor;
	using namespace THMultiplayerLocks;

	// A real press of the interact key, by one player, looking at what Target returns on their machine
	const auto PressAt = [](bool bHost, const FVector2D& XY, TFunction<FVector(UWorld*)> Target)
	{
		Step([bHost, XY]() { StandAt(bHost, XY); });
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
		Step([bHost, Target]() { LookAt(bHost, Target(LocalPawn(bHost)->GetWorld())); });
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.8));
		Step([bHost]() { PressInteract(LocalPawn(bHost)); });
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.5));
	};

	const auto DoorAim = [](const TCHAR* ClassPrefix)
	{
		const FString Prefix = ClassPrefix;
		return [Prefix](UWorld* World)
		{
			// The frame's top bar: an open panel is nearly edge-on from the room, so a player closing it aims at the frame
			const UStaticMeshComponent* Frame = FindMesh(FindDoor(World, Prefix), TEXT("DoorFrame"));
			return Frame ? Frame->Bounds.Origin + FVector(0.0, 0.0, Frame->Bounds.BoxExtent.Z - 6.0) : FVector::ZeroVector;
		};
	};


	// A press has settled once the swing is over; the lock must agree on both machines
	const auto ExpectLock = [this](bool bOpen, const FString& What)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(3.5));
		Step([this, bOpen, What]()
		{
			const UTHLockedDoorComponent* Server = LockOn(ServerWorld());
			const UTHLockedDoorComponent* Client = LockOn(ClientWorld());
			TestTrue(*(What + TEXT(": the server's state")), Server && Server->IsOpen() == bOpen);
			TestTrue(*(What + TEXT(": the client's state")), Client && Client->IsOpen() == bOpen);
			const float ServerYaw = PanelYaw(FindDoor(ServerWorld(), TEXT("BP_DoorLock_C")));
			const float ClientYaw = PanelYaw(FindDoor(ClientWorld(), TEXT("BP_DoorLock_C")));
			AddInfo(FString::Printf(TEXT("%s: panel yaw server %.0f, client %.0f"), *What, ServerYaw, ClientYaw));
			TestTrue(*(What + TEXT(": the server's panel")), (FMath::Abs(ServerYaw) > 45.0f) == bOpen);
			TestTrue(*(What + TEXT(": the client's panel")), (FMath::Abs(ClientYaw) > 45.0f) == bOpen);
		});
	};

	StartListenServer(this, TEXT("/Game/Maps/L_FiringRange"));
	SpawnTestDoors(this, false);
	Step([this]()
	{
		TestNotNull(TEXT("The locked door has its component on the server"), LockOn(ServerWorld()));
		TestNotNull(TEXT("The locked door has its component on the client"), LockOn(ClientWorld()));
		TestNotNull(TEXT("The valve door has its component on the client"), ValveOn(ClientWorld()));
		TestNotNull(TEXT("The key is on the client"), FindDoor(ClientWorld(), TEXT("BP_Key_C")));
	});

	// Nobody has the key: nobody opens it
	PressAt(true, AtLock, DoorAim(TEXT("BP_DoorLock_C")));
	ExpectLock(false, TEXT("The host presses without a key"));
	PressAt(false, AtLock, DoorAim(TEXT("BP_DoorLock_C")));
	ExpectLock(false, TEXT("The client presses without a key"));

	// The client takes the key: gone for everyone, and only the client holds it
	PressAt(false, AtKey, KeyAim);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the key to be gone on both machines"), 5.0, []()
	{
		return KeyIsTaken(ServerWorld(), true) && KeyIsTaken(ClientWorld(), true);
	}));
	Step([this]()
	{
		TestTrue(TEXT("The client holds the key"), UTHKeyPickupComponent::PlayerHasKey(FindPlayer(ServerWorld(), false)));
		TestFalse(TEXT("The host does not"), UTHKeyPickupComponent::PlayerHasKey(FindPlayer(ServerWorld(), true)));
	});

	// Only the holder can unlock it
	PressAt(true, AtLock, DoorAim(TEXT("BP_DoorLock_C")));
	ExpectLock(false, TEXT("The host still has no key"));
	PressAt(false, AtLock, DoorAim(TEXT("BP_DoorLock_C")));
	ExpectLock(true, TEXT("The client opens it with the key"));

	// Which uses the key up: nobody holds it, and it's gone from every machine, so it can't come back
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the used key to be gone on both machines"), 5.0, []()
	{
		return !FindDoor(ServerWorld(), TEXT("BP_Key_C")) && !FindDoor(ClientWorld(), TEXT("BP_Key_C"));
	}));
	Step([this]()
	{
		TestFalse(TEXT("The client no longer holds the used key"), UTHKeyPickupComponent::PlayerHasKey(FindPlayer(ServerWorld(), false)));
	});

	// Open for all now
	PressAt(true, AtLock, DoorAim(TEXT("BP_DoorLock_C")));
	ExpectLock(false, TEXT("The host, with no key, closes the unlocked door"));
	PressAt(true, AtLock, DoorAim(TEXT("BP_DoorLock_C")));
	ExpectLock(true, TEXT("The host, with no key, opens the unlocked door"));

	// The keypad: as the widget does when the code is right, on the client's machine only
	Step([]()
	{
		AActor* Door = FindDoor(ClientWorld(), TEXT("BP_KeypadDoor_C"));
		FindFProperty<FBoolProperty>(Door->GetClass(), TEXT("CodeCorrect?"))->SetPropertyValue_InContainer(Door, true);
	});
	Step([]() { StandAt(false, AtKeypad); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the server to take the client's correct code"), 5.0, []()
	{
		const AActor* Door = FindDoor(ServerWorld(), TEXT("BP_KeypadDoor_C"));
		return Door && Door->FindComponentByClass<UTHDoorSyncComponent>()->IsUnlockedOnServer();
	}));
	PressAt(false, AtKeypad, DoorAim(TEXT("BP_KeypadDoor_C")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the keypad door to open on both machines"), 8.0, []()
	{
		return DoorMatches(ServerWorld(), true, TEXT("BP_KeypadDoor_C")) && DoorMatches(ClientWorld(), true, TEXT("BP_KeypadDoor_C"));
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(3.5));
	// The host never entered the code, and can use it
	PressAt(true, AtKeypad, DoorAim(TEXT("BP_KeypadDoor_C")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the host, who never entered the code, to close the keypad door on both machines"), 8.0, []()
	{
		return DoorMatches(ServerWorld(), false, TEXT("BP_KeypadDoor_C")) && DoorMatches(ClientWorld(), false, TEXT("BP_KeypadDoor_C"));
	}));

	// The valve: anyone can turn it, and it moves together on both machines
	Step([this]()
	{
		const AActor* Door = FindDoor(ServerWorld(), TEXT("BP_ValveDoor_C"));
		const UBoxComponent* Box = Door->FindComponentByClass<UBoxComponent>();
		AddInfo(FString::Printf(TEXT("Valve box %s, frame at %s, wheel at %s"), *Box->Bounds.GetBox().ToString(),
			*FindMesh(Door, TEXT("DoorFrame"))->Bounds.Origin.ToCompactString(), *FindMesh(Door, TEXT("Valve"))->Bounds.Origin.ToCompactString()));
		const FVector Inside = Box->Bounds.Origin;
		StandAt(false, FVector2D(Inside.X, Inside.Y));

		// Only the wheel answers the interact trace, so only it shows a button prompt; bullets still hit the door
		const auto TraceAt = [Door](const TCHAR* Part, ECollisionChannel Channel)
		{
			const FVector Target = FindMesh(Door, Part)->Bounds.Origin;
			FHitResult Hit;
			Door->GetWorld()->LineTraceSingleByChannel(Hit, Target + FVector(150.0, 0.0, 0.0), Target, Channel);
			return Hit.GetActor() == Door ? Hit.GetComponent()->GetName() : FString(TEXT("nothing of the valve"));
		};
		TestEqual(TEXT("The interact trace finds the wheel"), TraceAt(TEXT("Valve"), ECC_Visibility), FString(TEXT("Valve")));
		TestEqual(TEXT("The interact trace passes the door, so it shows no prompt"), TraceAt(TEXT("Door"), ECC_Visibility), FString(TEXT("nothing of the valve")));
		TestEqual(TEXT("Bullets still hit the door"), TraceAt(TEXT("Door"), ECC_GameTraceChannel1), FString(TEXT("Door")));
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
	Step([]() { HoldValve(false, true); });
	// Injected input lasts a frame: keep holding for a while
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the client's hold to turn the wheel on the server"), 5.0, []()
	{
		HoldValve(false, true);
		return ValveOn(ServerWorld())->GetPosition() > 0.5f;
	}));
	Step([this]()
	{
		const float ServerNow = ValveOn(ServerWorld())->GetPosition();
		const float ClientNow = ValveOn(ClientWorld())->GetPosition();
		AddInfo(FString::Printf(TEXT("Valve while turning: server %.2f, client %.2f"), ServerNow, ClientNow));
		TestTrue(TEXT("The client's wheel turns with the server's"), ClientNow > 0.2f && FMath::Abs(ServerNow - ClientNow) < 0.5f);
	});
	// Letting go stops it, at the same place on both
	Step([]() { HoldValve(false, false); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(1.0));
	Step([this]()
	{
		const float ServerNow = ValveOn(ServerWorld())->GetPosition();
		const float ClientNow = ValveOn(ClientWorld())->GetPosition();
		AddInfo(FString::Printf(TEXT("Valve after letting go: server %.2f, client %.2f, doors %.1f %.1f"), ServerNow, ClientNow,
			PanelYaw(FindDoor(ServerWorld(), TEXT("BP_ValveDoor_C"))), PanelYaw(FindDoor(ClientWorld(), TEXT("BP_ValveDoor_C")))));
		TestTrue(TEXT("Letting go stops the wheel where the server has it"), FMath::IsNearlyEqual(ServerNow, ClientNow, 0.01f));
		TestTrue(TEXT("The wheel stopped when let go (still early in its turn a second later)"), ServerNow < 1.0f);
	});

	// The host takes over and winds it all the way: anyone can, and the door ends up the same on both machines
	Step([this]()
	{
		const AActor* Door = FindDoor(ServerWorld(), TEXT("BP_ValveDoor_C"));
		const FVector Inside = Door->FindComponentByClass<UBoxComponent>()->Bounds.Origin;
		StandAt(true, FVector2D(Inside.X, Inside.Y));
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
	// Injected input lasts a frame, so the hold is repeated every frame throughout
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the host's hold to start the wheel again"), 5.0, []()
	{
		HoldValve(true, true);
		return ValveOn(ServerWorld())->GetPosition() > 1.0f;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the host to wind the wheel to its end"), 40.0, []()
	{
		HoldValve(true, true);
		return !ValveOn(ServerWorld())->IsTurning();
	}));
	Step([]() { HoldValve(true, false); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(1.0));
	Step([this]()
	{
		const float ServerYaw = PanelYaw(FindDoor(ServerWorld(), TEXT("BP_ValveDoor_C")));
		const float ClientYaw = PanelYaw(FindDoor(ClientWorld(), TEXT("BP_ValveDoor_C")));
		AddInfo(FString::Printf(TEXT("Valve fully wound: position server %.2f, client %.2f, door yaw %.0f, %.0f"),
			ValveOn(ServerWorld())->GetPosition(), ValveOn(ClientWorld())->GetPosition(), ServerYaw, ClientYaw));
		TestTrue(TEXT("The valve door opened"), FMath::Abs(ServerYaw) > 45.0f);
		TestTrue(TEXT("Both machines' doors agree when fully open"), FMath::IsNearlyEqual(ServerYaw, ClientYaw, 1.0f));
	});

	// Movement is the server's: the client walks through each door the others opened
	for (const TPair<FString, FVector2D>& Walk : TArray<TPair<FString, FVector2D>>{
		{ TEXT("the door the key opened"), AtLock }, { TEXT("the valve door"), FVector2D(DoorX + 100.0, ValveY) } })
	{
		const FVector2D From = Walk.Value;
		Step([From]() { Teleport(false, From); });
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, FString(TEXT("the client to walk through ")) + Walk.Key + TEXT(", on the server"), 8.0, []()
		{
			FindPlayer(ClientWorld(), true)->AddMovementInput(FVector(-1.0, 0.0, 0.0));
			return FindPlayer(ServerWorld(), false)->GetActorLocation().X < DoorX - 100.0;
		}));
	}

	// A hold at the fully open end turns it back
	Step([]() { Teleport(false, AwayFromDoors); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
	Step([]() { HoldValve(true, true); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("a hold at the end to start turning it back on both machines"), 6.0, []()
	{
		HoldValve(true, true);
		const float End = ValveOn(ServerWorld())->GetLength();
		return ValveOn(ServerWorld())->GetPosition() < End - 0.5f && ValveOn(ClientWorld())->GetPosition() < End - 0.2f;
	}));
	Step([]() { HoldValve(true, false); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.5));

	EndPlay(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHKeyReturnsTest, "TacticalHorror.Multiplayer.KeyReturnsWhenHolderLeaves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHKeyReturnsTest::RunTest(const FString& Parameters)
{
	using namespace THMultiplayerDoor;
	using namespace THMultiplayerLocks;

	// The key as every machine still in the session has it (one that left is on the menu map, with no key)
	const auto KeyEverywhere = [](bool bTaken)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType == EWorldType::PIE && World && World->GetNetMode() != NM_Standalone && !KeyIsTaken(World, bTaken))
			{
				return false;
			}
		}
		return true;
	};
	const auto IdOf = [](const APawn* Pawn) { return Pawn->GetPlayerState()->GetPlayerId(); };
	TSharedRef<int32> LeaverId = MakeShared<int32>(INDEX_NONE);

	// The host, a client who takes the key and leaves, and a client who stays to see it come back
	StartListenServer(this, TEXT("/Game/Maps/L_FiringRange"), 3);
	SpawnTestDoors(this, true);
	Step([this, LeaverId, IdOf]()
	{
		TestEqual(TEXT("Three players on the server"), ServerWorld()->GetNumPlayerControllers(), 3);
		*LeaverId = IdOf(FindPlayer(ServerWorld(), false));
		// The press itself is covered by Multiplayer.LockedDoors
		KeyOn(ServerWorld())->ServerPressed(Cast<ATHCharacter>(ServerPawn(*LeaverId)));
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the key to be out of the world on the server and every client"), 5.0, [KeyEverywhere]()
	{
		return KeyEverywhere(true);
	}));

	// Someone joining now hears the key is taken, though it's a hidden actor with no collision: those aren't sent by default.
	// The wait is longer than a channel to an actor that has stopped being relevant stays open.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(7.0));
	Step([]() { GEditor->RequestLateJoin(); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("a fourth player to join"), 30.0, []()
	{
		return ServerWorld()->GetNumPlayerControllers() == 4;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the player who joined late to see the key is taken"), 10.0, [KeyEverywhere]()
	{
		return KeyEverywhere(true);
	}));

	// The holder quits
	Step([LeaverId, IdOf]()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType == EWorldType::PIE && World && World->GetNetMode() == NM_Client)
			{
				const APawn* Local = FindPlayer(World, true);
				if (Local && Local->GetPlayerState() && IdOf(Local) == *LeaverId)
				{
					GEngine->Exec(World, TEXT("DISCONNECT"));
					return;
				}
			}
		}
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the server to see the holder gone"), 30.0, [LeaverId]()
	{
		return ServerPawn(*LeaverId) == nullptr;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the key to be back on the server and the remaining clients"), 5.0, [KeyEverywhere]()
	{
		return KeyEverywhere(false);
	}));

	// It's a real key again: a client who stayed presses it with the real input and holds it
	Step([IdOf]()
	{
		const FVector Location(AtKey.X, AtKey.Y, -10.0);
		APawn* Pawn = FindPlayer(ClientWorld(), true);
		ServerPawn(IdOf(Pawn))->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		Pawn->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
	// The camera settles a little off a set aim, and the key is small, so aim until the interact trace reaches it
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the interact trace to hit the returned key"), 5.0, []()
	{
		const APawn* Pawn = FindPlayer(ClientWorld(), true);
		LookAt(Pawn, KeyAim(ClientWorld()));
		FVector Eye;
		FRotator Rotation;
		Cast<APlayerController>(Pawn->GetController())->GetPlayerViewPoint(Eye, Rotation);
		FHitResult Hit;
		ClientWorld()->SweepSingleByChannel(Hit, Eye, Eye + Rotation.Vector() * 220.0, FQuat::Identity, ECC_Visibility,
			FCollisionShape::MakeSphere(5.0f), FCollisionQueryParams(NAME_None, false, Pawn));
		return Hit.GetActor() == KeyOn(ClientWorld())->GetOwner();
	}));
	Step([]() { PressInteract(FindPlayer(ClientWorld(), true)); });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, TEXT("the client who stayed to take the returned key"), 5.0, [KeyEverywhere]()
	{
		return KeyEverywhere(true);
	}));
	Step([this, IdOf]()
	{
		TestTrue(TEXT("The client who stayed holds it"), UTHKeyPickupComponent::PlayerHasKey(ServerPawn(IdOf(FindPlayer(ClientWorld(), true)))));
		TestFalse(TEXT("The host does not"), UTHKeyPickupComponent::PlayerHasKey(FindPlayer(ServerWorld(), true)));
	});

	EndPlay(this);
	return true;
}

namespace THMultiplayerRange
{
	using namespace THMultiplayerDoor;

	static AActor* FindNamed(UWorld* World, const FString& Name)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetName() == Name)
			{
				return *It;
			}
		}
		return nullptr;
	}

	static ATHTargetCarrier* CarrierOn(UWorld* World, const FString& Name)
	{
		return Cast<ATHTargetCarrier>(FindNamed(World, Name));
	}

	/** The carrier's paper target (SM_Target): one of its parts, or attached to one. */
	static const AActor* PaperTarget(const ATHTargetCarrier* Carrier)
	{
		for (AActor* Part : Carrier->Parts)
		{
			TArray<AActor*> Candidates{ Part };
			Part->GetAttachedActors(Candidates, false, true);
			for (const AActor* Candidate : Candidates)
			{
				const UStaticMeshComponent* Mesh = Candidate->FindComponentByClass<UStaticMeshComponent>();
				if (Mesh && Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetName() == TEXT("SM_Target"))
				{
					return Candidate;
				}
			}
		}
		return nullptr;
	}

	/** Holds (or, by not calling it, releases) the real interact key for a frame on that player's machine. */
	static void HoldInteract(bool bHost)
	{
		PressInteract(LocalPawn(bHost));
	}

	/** Whether this player's interact trace (as ATHCharacter's) finds the actor with this name. */
	static bool AimsAt(bool bHost, const FString& Name)
	{
		const APawn* Pawn = LocalPawn(bHost);
		const AActor* Target = FindNamed(Pawn->GetWorld(), Name);
		const APlayerController* Controller = Cast<APlayerController>(Pawn->GetController());
		if (!Target || !Controller)
		{
			return false;
		}
		FVector Eye;
		FRotator Rotation;
		Controller->GetPlayerViewPoint(Eye, Rotation);
		Cast<APlayerController>(Pawn->GetController())->SetControlRotation((Target->GetActorLocation() - Eye).Rotation());
		Controller->GetPlayerViewPoint(Eye, Rotation);
		FHitResult Hit;
		Pawn->GetWorld()->SweepSingleByChannel(Hit, Eye, Eye + Rotation.Vector() * 220.0, FQuat::Identity, ECC_Visibility,
			FCollisionShape::MakeSphere(5.0f), FCollisionQueryParams(NAME_None, false, Pawn));
		return Hit.GetActor() == Target;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHTargetCarrierTest, "TacticalHorror.Multiplayer.TargetCarrier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHTargetCarrierTest::RunTest(const FString& Parameters)
{
	using namespace THMultiplayerDoor;
	using namespace THMultiplayerLocks;
	using namespace THMultiplayerRange;

	struct FState
	{
		FString Carrier;
		FString Buttons[2]; // SEND, RETURN
		float Before = 0.0f;
		float TargetStartY = 0.0f; // the paper target's Y at offset 0
	};
	TSharedRef<FState> State = MakeShared<FState>();

	// The two machines' carriers agree: still, at the same place, the target where the carrier says
	const auto ExpectStopped = [this, State](const FString& What)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.6));
		Step([this, State, What]()
		{
			const ATHTargetCarrier* Server = CarrierOn(ServerWorld(), State->Carrier);
			const ATHTargetCarrier* Client = CarrierOn(ClientWorld(), State->Carrier);
			const AActor* ServerTarget = PaperTarget(Server);
			const AActor* ClientTarget = PaperTarget(Client);
			if (!TestNotNull(TEXT("The carrier has a paper target"), ServerTarget) || !TestNotNull(TEXT("The client's carrier has it"), ClientTarget))
			{
				return;
			}
			AddInfo(FString::Printf(TEXT("%s: server %.2f, client %.2f, target %s / %s"), *What, Server->GetOffset(), Client->GetOffset(),
				*ServerTarget->GetActorLocation().ToCompactString(), *ClientTarget->GetActorLocation().ToCompactString()));
			TestFalse(*(What + TEXT(": stopped on the server")), Server->IsMoving());
			TestFalse(*(What + TEXT(": stopped on the client")), Client->IsMoving());
			TestTrue(*(What + TEXT(": both machines have the target in the same place")),
				ServerTarget->GetActorLocation().Equals(ClientTarget->GetActorLocation(), 0.5));
			TestTrue(*(What + TEXT(": the target is where the carrier says")),
				FMath::IsNearlyEqual(ServerTarget->GetActorLocation().Y, State->TargetStartY - Server->GetOffset(), 0.5));
		});
	};

	// One player stands in the booth in front of a button and holds E on it until Done, then lets go
	const auto HoldButton = [this, State](bool bHost, int32 Which, const FString& What, TFunction<bool(float)> Done)
	{
		Step([State, bHost, Which]()
		{
			const FVector Button = FindNamed(ServerWorld(), State->Buttons[Which])->GetActorLocation();
			Teleport(!bHost, AwayFromDoors);
			Teleport(bHost, FVector2D(Button.X - 60.0, -560.0)); // behind the counter, which reaches Y -608
		});
		ADD_LATENT_AUTOMATION_COMMAND(FWaitGameTime(0.3));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, What + TEXT(": the interact trace to find the button"), 5.0, [State, bHost, Which]()
		{
			return AimsAt(bHost, State->Buttons[Which]);
		}));
		Step([State]() { State->Before = CarrierOn(ServerWorld(), State->Carrier)->GetOffset(); });
		ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(this, What, 10.0, [State, bHost, Done]()
		{
			HoldInteract(bHost);
			return Done(CarrierOn(ServerWorld(), State->Carrier)->GetOffset() - State->Before);
		}));
		Step([this, State, What]()
		{
			const ATHTargetCarrier* Server = CarrierOn(ServerWorld(), State->Carrier);
			const ATHTargetCarrier* Client = CarrierOn(ClientWorld(), State->Carrier);
			AddInfo(FString::Printf(TEXT("%s, while held: server %.0f, client %.0f"), *What, Server->GetOffset(), Client->GetOffset()));
			TestTrue(*(What + TEXT(": moving on the client too, close to the server")),
				Client->IsMoving() && FMath::Abs(Server->GetOffset() - Client->GetOffset()) < 100.0f);
		});
	};

	StartListenServer(this, TEXT("/Game/Maps/L_FiringRange"));
	Step([this, State]()
	{
		const ATHTargetCarrier* Carrier = nullptr;
		for (TActorIterator<ATHTargetCarrier> It(ServerWorld()); It && !Carrier; ++It)
		{
			Carrier = *It;
		}
		if (!TestNotNull(TEXT("The range has target carriers (setup_target_carriers.py)"), Carrier))
		{
			return;
		}
		State->Carrier = Carrier->GetName();
		const AActor* Target = PaperTarget(Carrier);
		if (!TestNotNull(TEXT("The carrier moves a paper target"), Target))
		{
			return;
		}
		State->TargetStartY = Target->GetActorLocation().Y + Carrier->GetOffset();
		for (TActorIterator<ATHTargetButton> It(ServerWorld()); It; ++It)
		{
			if (It->GetButton()->Carrier == Carrier)
			{
				State->Buttons[It->GetButton()->Direction > 0 ? 0 : 1] = It->GetName();
			}
		}
		TestFalse(TEXT("It has a SEND button"), State->Buttons[0].IsEmpty());
		TestFalse(TEXT("It has a RETURN button"), State->Buttons[1].IsEmpty());
		TestNotNull(TEXT("The client has the carrier"), CarrierOn(ClientWorld(), State->Carrier));
		TestTrue(TEXT("The paper target is Movable"), Target->GetRootComponent()->Mobility == EComponentMobility::Movable);
	});

	// The client sends the target downrange, the host brings it back: both see it move, and stop where the server stops it
	HoldButton(false, 0, TEXT("the client holding SEND to move the target 1.5 m downrange"), [](float Moved) { return Moved > 150.0f; });
	ExpectStopped(TEXT("The client let go of SEND"));
	HoldButton(true, 1, TEXT("the host holding RETURN to bring it back 1 m"), [](float Moved) { return Moved < -100.0f; });
	ExpectStopped(TEXT("The host let go of RETURN"));

	EndPlay(this);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
