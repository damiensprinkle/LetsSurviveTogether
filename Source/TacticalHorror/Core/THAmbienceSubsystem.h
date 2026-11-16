#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "THAmbienceSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * Background music and ambience, on each machine with a player (not a dedicated server). In a mission map: a music
 * layer (Horror Music Free Starter Kit loops) and a drone layer (Horror Ambient SFX), each picking a random track,
 * fading in, playing a few minutes, fading out, then a pause before the next. In the main menu: the menu theme, looped.
 * Volume: the player's Music & ambience setting (UTHAudioSettingsSubsystem). Lists in DefaultGame.ini can replace the
 * defaults ([/Script/TacticalHorror.THAmbienceSubsystem] +MusicTracks=...).
 */
UCLASS(Config = Game)
class TACTICALHORROR_API UTHAmbienceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UTHAmbienceSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** What each layer is playing now (for tests); null when silent. */
	USoundBase* GetMusic() const;
	USoundBase* GetDrone() const;

	UPROPERTY(Config)
	TArray<TSoftObjectPtr<USoundBase>> MusicTracks;

	UPROPERTY(Config)
	TArray<TSoftObjectPtr<USoundBase>> DroneTracks;

	UPROPERTY(Config)
	TSoftObjectPtr<USoundBase> MenuTrack;

	/** Maps whose name contains this play the menu theme instead. */
	UPROPERTY(Config)
	FString MenuMapName = TEXT("MainMenu");

	UPROPERTY(Config)
	float MusicVolume = 0.225f;	// Halved 2026-11-13 (developer: overwhelmed scenes; set theirs below 50%)

	UPROPERTY(Config)
	float DroneVolume = 0.175f;

	UPROPERTY(Config)
	float MenuVolume = 0.3f;

	/**
	 * Danger (developer, 2026-11-12): while an enemy within TensionRange of this machine's player is going for someone
	 * (chasing, attacking, roaring, charging, striking), the music plays this much faster (and higher) and louder, eased
	 * in over TensionRise seconds and out over TensionFall.
	 */
	UPROPERTY(Config)
	float TensionPitch = 1.15f;

	UPROPERTY(Config)
	float TensionVolume = 1.3f;

	UPROPERTY(Config)
	float TensionRange = 2500.0f;

	UPROPERTY(Config)
	float TensionRise = 1.5f;

	UPROPERTY(Config)
	float TensionFall = 4.0f;

	/** 0 calm .. 1 in danger, as the music plays it now (for tests). */
	float GetTension() const { return Tension; }

private:
	struct FLayer
	{
		const TArray<TSoftObjectPtr<USoundBase>>* Tracks = nullptr;
		float Volume = 1.0f;
		FVector2D PlayTime = FVector2D(120.0f, 240.0f);
		FVector2D Gap = FVector2D(15.0f, 45.0f);
		double StopTime = 0.0;
		double NextStart = 0.0;
		int32 LastIndex = INDEX_NONE;
		bool bStopping = false;
	};

	void UpdateLayer(FLayer& Layer, TObjectPtr<UAudioComponent>& Component, double Now, float Setting);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> DroneComponent;

	FLayer Music;
	FLayer Drone;
	TArray<TSoftObjectPtr<USoundBase>> MenuTracks;
	bool bMenu = false;
	bool bStarted = false;
	float Tension = 0.0f;
	bool bDanger = false;
	double NextDangerCheck = 0.0;
};
