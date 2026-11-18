#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "THSanityData.generated.h"

class UAnimSequence;
class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;
class UTHEnemyAnimSet;
class UTHEnemyAppearance;

/**
 * A sound only the player losing their mind hears (GDD 7.2): one of the enemies' own sounds, or footsteps, from
 * somewhere near them, so they can't tell it from the real thing.
 */
USTRUCT(BlueprintType)
struct FTHHallucinationSound
{
	GENERATED_BODY()

	/** For the logs and tests. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hallucination")
	FName Name;

	/** One picked at random (each step, for footsteps). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hallucination")
	TArray<TObjectPtr<USoundBase>> Sounds;

	/** Heard only at or below this sanity (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hallucination", meta = (ClampMin = 0, ClampMax = 100))
	float BelowSanity = 60.0f;

	/** How often it comes up, relative to the others heard at the time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hallucination", meta = (ClampMin = 0))
	float Weight = 1.0f;

	/** How far away it starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hallucination", meta = (Units = "cm"))
	FVector2D Distance = FVector2D(500.0f, 1200.0f);

	/** From behind (outside the view), else from anywhere around. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hallucination")
	bool bBehind = true;

	/** More than one: footsteps, coming closer a stride each (Step Length), every Step Interval. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hallucination", meta = (ClampMin = 1))
	FIntPoint Steps = FIntPoint(1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hallucination", meta = (Units = "s", ClampMin = 0.05))
	float StepInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hallucination", meta = (Units = "cm"))
	float StepLength = 70.0f;

	/** How far it carries (none: no fall-off). Use the enemies' (SA_EnemyVoice, SA_EnemyFootsteps) so it sounds like them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hallucination")
	TObjectPtr<USoundAttenuation> Attenuation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hallucination", meta = (ClampMin = 0, ClampMax = 4))
	float Volume = 1.0f;
};

/**
 * Sanity (GDD 7.2, developer 2026-11-17), per player, 0-100. Drains in the dark (not a lit room: your own light only
 * slows it) and while alone (only when there is a teammate to be away from: never solo); comes back in lit rooms, near
 * a teammate, and slowly once nothing has drained it for a while. No bar on the HUD: low sanity is heard, as a
 * heartbeat getting faster and louder, and felt, as shakier aim; below that come sounds that aren't there (footsteps,
 * the enemies' voices), and at the bottom a fake enemy rushes the player and vanishes. All of it only on that player's
 * machine. On the player's UTHSanityComponent; made by Scripts/Tools/setup_sanity.py.
 */
UCLASS(BlueprintType)
class TACTICALHORROR_API UTHSanityData : public UDataAsset
{
	GENERATED_BODY()

public:
	// Drains and restores, per second, added together (sanity is 0-100)

	/** Where no lamp lights you (UTHLightingSubsystem: below Lit Above). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Drain", meta = (ClampMin = 0))
	float DarknessDrain = 1.0f;

	/** The darkness drain times this while your own flashlight or weapon light is on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Drain", meta = (ClampMin = 0, ClampMax = 1))
	float OwnLightDarknessScale = 0.5f;

	/** With no living teammate within Alone Distance; never when you have no living teammates (solo). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Drain", meta = (ClampMin = 0))
	float AloneDrain = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Drain", meta = (Units = "cm", ClampMin = 0))
	float AloneDistance = 1500.0f;

	/** Lit by a lamp in the level (not a carried light). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Restore", meta = (ClampMin = 0))
	float LitRestore = 2.0f;

	/** A living teammate within Teammate Distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Restore", meta = (ClampMin = 0))
	float TeammateRestore = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Restore", meta = (Units = "cm", ClampMin = 0))
	float TeammateDistance = 600.0f;

	/** Once nothing has drained it for Calm Delay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Restore", meta = (ClampMin = 0))
	float CalmRestore = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Restore", meta = (Units = "s", ClampMin = 0))
	float CalmDelay = 10.0f;

	// Heartbeat: one beat per sound, at a rate and loudness from Heartbeat Below down to 0

	/** One picked per beat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Heartbeat")
	TArray<TObjectPtr<USoundBase>> HeartbeatSounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Heartbeat", meta = (ClampMin = 0, ClampMax = 100))
	float HeartbeatBelow = 60.0f;

	/** Beats a minute at Heartbeat Below (X) and at 0 (Y). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Heartbeat")
	FVector2D BeatsPerMinute = FVector2D(65.0f, 140.0f);

	/** Loudness at Heartbeat Below (X) and at 0 (Y). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Heartbeat")
	FVector2D HeartbeatVolume = FVector2D(0.15f, 0.9f);

	// Shakier aim: the view drifts on its own, from Sway Below down to 0

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Sway", meta = (ClampMin = 0, ClampMax = 100))
	float SwayBelow = 50.0f;

	/** How far the aim wanders at 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Sway", meta = (Units = "deg", ClampMin = 0))
	float MaxSway = 1.5f;

	/** How fast it wanders (cycles a second, roughly). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Sway", meta = (ClampMin = 0))
	float SwaySpeed = 0.5f;

	// Blacking out: the edges of the view darken, more the lower it goes, swelling with each heartbeat

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Blackout", meta = (ClampMin = 0, ClampMax = 100))
	float BlackoutBelow = 40.0f;

	/** Vignette intensity at Blackout Below (X) and at 0 (Y). The engine's default is 0.4; 2 or so leaves a small clear circle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Blackout")
	FVector2D BlackoutVignette = FVector2D(0.4f, 2.0f);

	/** How much darker each heartbeat makes it for a moment, as a share of the darkening so far (0: steady). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Blackout", meta = (ClampMin = 0))
	float BlackoutPulse = 0.35f;

	// Sounds that aren't there

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Hallucinations", meta = (ClampMin = 0, ClampMax = 100))
	float HallucinationsBelow = 70.0f;

	/** Seconds between them at Hallucinations Below (X) and at 0 (Y), each give or take 30%. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Hallucinations")
	FVector2D HallucinationInterval = FVector2D(60.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Hallucinations")
	TArray<FTHHallucinationSound> Hallucinations;

	// At the bottom: an enemy that isn't there rushes the player and vanishes as it reaches them (no damage)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy", meta = (ClampMin = 0, ClampMax = 100))
	float FakeEnemyBelow = 1.0f;

	/** Seconds at the bottom before the first, and between them after. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy")
	float FakeEnemyDelay = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy")
	FVector2D FakeEnemyInterval = FVector2D(30.0f, 60.0f);

	/** Its looks (one picked at random; looks needing a retargeter, MetaHumans, are skipped) and animations (Run). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy")
	TObjectPtr<UTHEnemyAppearance> FakeEnemyLooks;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy")
	TObjectPtr<UTHEnemyAnimSet> FakeEnemyAnims;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy", meta = (Units = "cm/s", ClampMin = 1))
	float FakeEnemySpeed = 450.0f;

	/** Where it appears: in front of the player (within this angle of where they look), at this distance, in plain sight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy", meta = (Units = "cm"))
	FVector2D FakeEnemyDistance = FVector2D(900.0f, 1500.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy", meta = (Units = "deg", ClampMin = 0, ClampMax = 180))
	float FakeEnemyAngle = 50.0f;

	/** Its shout as it comes (at it), and the sound as it vanishes (in the player's head). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy")
	TArray<TObjectPtr<USoundBase>> FakeEnemySounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy")
	TObjectPtr<USoundAttenuation> FakeEnemyAttenuation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy")
	TArray<TObjectPtr<USoundBase>> FakeEnemyVanishSounds;

	/**
	 * Shot at (developer, 2026-11-17): it dissolves into this effect at its body (spawned facing up), with Shot Sounds.
	 * Placeholder: the developer's NS_ChargerWallDust, until a smoke effect of its own.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy")
	TObjectPtr<UNiagaraSystem> FakeEnemyShotEffect;

	/** Its scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy", meta = (ClampMin = 0.01))
	float FakeEnemyShotEffectScale = 1.5f;

	/** Played at it as it dissolves; empty = the Vanish Sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy")
	TArray<TObjectPtr<USoundBase>> FakeEnemyShotSounds;

	/** Gone within this of the player, or after Fake Enemy Life Span. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy", meta = (Units = "cm"))
	float FakeEnemyVanishDistance = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sanity|Fake Enemy", meta = (Units = "s"))
	float FakeEnemyLifeSpan = 6.0f;
};
