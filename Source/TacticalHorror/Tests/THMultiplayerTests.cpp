// Multiplayer tests: a listen server (the host, player 1) and one client (player 2) in a single play session on the
// test map. Checks that what one player does reaches the server and the other player. See TESTING.md.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerController.h"
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
	/** Opens the test map in the editor and starts a listen server with one client. */
	static void StartListenServer(FAutomationTestBase* Test)
	{
		Step([]()
		{
			UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
			if (!EditorWorld || EditorWorld->GetOutermost()->GetName() != TestMap)
			{
				FEditorFileUtils::LoadMap(TestMap, false, false);
			}

			ULevelEditorPlaySettings* Settings = NewObject<ULevelEditorPlaySettings>();
			Settings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
			Settings->SetPlayNumberOfClients(2);
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

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
