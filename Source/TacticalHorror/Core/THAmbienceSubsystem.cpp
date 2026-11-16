#include "Core/THAmbienceSubsystem.h"
#include "Settings/THAudioSettingsSubsystem.h"
#include "Enemies/THEnemyCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	constexpr float FadeInTime = 5.0f;
	constexpr float FadeOutTime = 6.0f;

	TSoftObjectPtr<USoundBase> Cue(const FString& Folder, const FString& Name)
	{
		return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), *Folder, *Name, *Name)));
	}
}

UTHAmbienceSubsystem::UTHAmbienceSubsystem()
{
	// The developer's picks (2026-11-11): the Horror Music Free Starter Kit's loops and Horror Ambient SFX's drones
	const FString MusicFolder = TEXT("/Game/ThirdParty/Horror_Music_Free_Starter_Kit/Cue");
	for (const TCHAR* Name : {
		TEXT("WAV_AMBIENCE_LOOP_Dimly_Lit_Basement_Cue"), TEXT("WAV_AMBIENCE_LOOP_Dont_Turn_Around_Cue"),
		TEXT("WAV_AMBIENCE_LOOP_Mirrored_Lunacy_Cue"), TEXT("WAV_AMBIENCE_LOOP_Noises_of_the_Red_Fog_Cue"),
		TEXT("WAV_AMBIENCE_LOOP_The_Park_Maniac_Cue"), TEXT("WAV_ATMOSPHERIC_LOOP_Abandoned_Hill_Cue"),
		TEXT("WAV_ATMOSPHERIC_LOOP_Anxiety_Parabole_Cue"), TEXT("WAV_ATMOSPHERIC_LOOP_Land_of_Bones_Cue"),
		TEXT("WAV_ATMOSPHERIC_LOOP_Mental_Breakdown_Cue"), TEXT("WAV_ATMOSPHERIC_LOOP_Mutations_Inside_the_Asylum_Cue") })
	{
		MusicTracks.Add(Cue(MusicFolder, Name));
	}
	const FString DroneFolder = TEXT("/Game/ThirdParty/HorrorAmbientSFX/HorrorAmbientSFX_cue");
	for (const TCHAR* Name : {
		TEXT("AMB_Drone_AC_Cue"), TEXT("AMB_Drone_Anxiety_Cue"), TEXT("AMB_Drone_Beneath_Cue"), TEXT("AMB_Drone_Corridor_Cue"),
		TEXT("AMB_Drone_Hall_Cue"), TEXT("AMB_Drone_Hum_Cue"), TEXT("AMB_Drone_Ohm_Cue"), TEXT("AMB_Drone_Server_Cue"),
		TEXT("AMB_Drone_ServerBass_Cue"), TEXT("AMB_Drone_Wide_Cue") })
	{
		DroneTracks.Add(Cue(DroneFolder, Name));
	}
	MenuTrack = Cue(MusicFolder, TEXT("WAV_MENU_FULL_Systolic_Menace_Cue"));
}

bool UTHAmbienceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) && Super::ShouldCreateSubsystem(Outer);
}

void UTHAmbienceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	bMenu = !MenuMapName.IsEmpty() && InWorld.GetMapName().Contains(MenuMapName);
	if (bMenu)
	{
		MenuTracks = { MenuTrack };
		Music.Tracks = &MenuTracks;
		Music.Volume = MenuVolume;
		Music.PlayTime = FVector2D(1.0e6f, 1.0e6f);	// until it ends, then again
		Music.Gap = FVector2D(1.0f, 1.0f);
		Music.NextStart = 0.0;
	}
	else
	{
		Music.Tracks = &MusicTracks;
		Music.Volume = MusicVolume;
		Music.NextStart = InWorld.GetTimeSeconds() + 2.0;	// a moment of quiet first
		Drone.Tracks = &DroneTracks;
		Drone.Volume = DroneVolume;
		Drone.PlayTime = FVector2D(90.0f, 180.0f);
		Drone.Gap = FVector2D(10.0f, 30.0f);
		Drone.NextStart = InWorld.GetTimeSeconds();
	}
	bStarted = true;
}

void UTHAmbienceSubsystem::Deinitialize()
{
	for (UAudioComponent* Component : { MusicComponent.Get(), DroneComponent.Get() })
	{
		if (Component)
		{
			Component->Stop();
			Component->DestroyComponent();
		}
	}
	MusicComponent = nullptr;
	DroneComponent = nullptr;
	Super::Deinitialize();
}

TStatId UTHAmbienceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTHAmbienceSubsystem, STATGROUP_Tickables);
}

USoundBase* UTHAmbienceSubsystem::GetMusic() const
{
	return MusicComponent && MusicComponent->IsPlaying() ? MusicComponent->Sound.Get() : nullptr;
}

USoundBase* UTHAmbienceSubsystem::GetDrone() const
{
	return DroneComponent && DroneComponent->IsPlaying() ? DroneComponent->Sound.Get() : nullptr;
}

void UTHAmbienceSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const UWorld* World = GetWorld();
	if (!bStarted || !World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const float Setting = UTHAudioSettingsSubsystem::GetMusicVolumeFor(this);

	// Danger nearby: an enemy close to this machine's player going for someone (checked twice a second)
	if (Now >= NextDangerCheck)
	{
		NextDangerCheck = Now + 0.5;
		bDanger = false;
		const APlayerController* PC = World->GetFirstPlayerController();
		const APawn* Mine = PC ? PC->GetPawn() : nullptr;
		if (Mine)
		{
			for (TActorIterator<ATHEnemyCharacter> It(World); It && !bDanger; ++It)
			{
				bDanger = It->IsAggressive() && !It->IsDead()
					&& FVector::Dist(It->GetActorLocation(), Mine->GetActorLocation()) <= TensionRange;
			}
		}
	}
	Tension = FMath::FInterpConstantTo(Tension, bDanger ? 1.0f : 0.0f, DeltaTime, 1.0f / FMath::Max(bDanger ? TensionRise : TensionFall, 0.05f));
	if (MusicComponent && !bMenu)
	{
		MusicComponent->SetPitchMultiplier(FMath::Lerp(1.0f, TensionPitch, Tension));
	}
	UpdateLayer(Music, MusicComponent, Now, Setting);
	UpdateLayer(Drone, DroneComponent, Now, Setting);
}

void UTHAmbienceSubsystem::UpdateLayer(FLayer& Layer, TObjectPtr<UAudioComponent>& Component, double Now, float Setting)
{
	if (!Layer.Tracks || Layer.Tracks->IsEmpty())
	{
		return;
	}
	const bool bPlaying = Component && Component->IsPlaying();
	if (bPlaying)
	{
		Component->SetVolumeMultiplier(Layer.Volume * Setting * (&Layer == &Music ? FMath::Lerp(1.0f, TensionVolume, Tension) : 1.0f));
		if (!Layer.bStopping && Now >= Layer.StopTime)
		{
			Layer.bStopping = true;
			Component->FadeOut(FadeOutTime, 0.0f);
			Layer.NextStart = Now + FadeOutTime + FMath::FRandRange(Layer.Gap.X, Layer.Gap.Y);
		}
		return;
	}
	// Ended by itself (not looping): the pause starts now
	if (Component && !Layer.bStopping)
	{
		Layer.bStopping = true;
		Layer.NextStart = Now + FMath::FRandRange(Layer.Gap.X, Layer.Gap.Y);
	}
	if (Now < Layer.NextStart)
	{
		return;
	}

	// The next track: not the one just played
	int32 Index = FMath::RandHelper(Layer.Tracks->Num());
	if (Layer.Tracks->Num() > 1 && Index == Layer.LastIndex)
	{
		Index = (Index + 1) % Layer.Tracks->Num();
	}
	USoundBase* Sound = (*Layer.Tracks)[Index].LoadSynchronous();
	Layer.LastIndex = Index;
	Layer.NextStart = Now + 5.0;	// try again in a while if it can't play
	if (!Sound)
	{
		return;
	}
	if (Component)
	{
		Component->DestroyComponent();
		Component = nullptr;
	}
	Component = UGameplayStatics::CreateSound2D(this, Sound, Layer.Volume * Setting, 1.0f, 0.0f, nullptr, false, false);
	if (!Component)
	{
		return;	// no audio device (headless)
	}
	Component->bIsUISound = false;
	Component->FadeIn(FadeInTime, 1.0f);
	Layer.bStopping = false;
	Layer.StopTime = Now + FMath::FRandRange(Layer.PlayTime.X, Layer.PlayTime.Y);
	Layer.NextStart = Layer.StopTime + 1.0e6;
}
