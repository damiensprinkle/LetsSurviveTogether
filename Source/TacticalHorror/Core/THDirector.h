#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "THDirector.generated.h"

class APawn;
class ATHEnemyCharacter;
class UBillboardComponent;
class UTHEnemyBehavior;

/** One kind of enemy the director can send, and how many at once. */
USTRUCT(BlueprintType)
struct FTHDirectorWave
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Director")
	TObjectPtr<UTHEnemyBehavior> Behavior;

	/** How many come (random between the two). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Director")
	FIntPoint Count = FIntPoint(1, 2);

	/** How often this wave comes up, relative to the others. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Director", meta = (ClampMin = 0))
	float Weight = 1.0f;
};

/**
 * How the director paces a level (developer, 2026-11-13: calm stretches building to peaks, so it never plays the same).
 * Intensity (0..1) rises with what happens to the players and falls away on its own; a long calm brings a wave, a peak
 * brings a breather.
 */
UCLASS(BlueprintType)
class TACTICALHORROR_API UTHDirectorSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Intensity for each point of damage a player takes (100 = a whole health bar: 0.6). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intensity", meta = (ClampMin = 0))
	float PerDamage = 0.006f;

	/** Intensity for each shot a player fires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intensity", meta = (ClampMin = 0))
	float PerShot = 0.03f;

	/** Intensity a second for each enemy going for a player within Near Distance of one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intensity", meta = (ClampMin = 0))
	float PerAggressiveEnemy = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intensity", meta = (Units = "cm"))
	float NearDistance = 2000.0f;

	/** Intensity lost a second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intensity", meta = (ClampMin = 0))
	float Decay = 0.05f;

	/** Below this it counts as calm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pacing", meta = (ClampMin = 0, ClampMax = 1))
	float CalmBelow = 0.25f;

	/** This calm for this long (random between the two): a wave. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pacing", meta = (Units = "s"))
	FVector2D CalmBeforeWave = FVector2D(45.0f, 75.0f);

	/** At this a peak: no waves, and stalkers hold off, for Relax Time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pacing", meta = (ClampMin = 0, ClampMax = 1))
	float PeakAbove = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pacing", meta = (Units = "s"))
	float RelaxTime = 25.0f;

	/** No wave while this many enemies (of any kind) are alive in the level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves", meta = (ClampMin = 0))
	int32 MaxAlive = 12;

	/** A wave comes from a spawn point out of every player's sight, this far (between the two) from the nearest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves", meta = (Units = "cm"))
	FVector2D SpawnDistance = FVector2D(1500.0f, 3500.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves")
	TArray<FTHDirectorWave> Waves;

	/** The enemy Blueprint waves are made of (BP_Zombie). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves")
	TSoftClassPtr<ATHEnemyCharacter> EnemyClass;
};

/** Where the director may bring enemies in: placed in the level, out of the way (a dark corner, a far room). */
UCLASS()
class TACTICALHORROR_API ATHSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ATHSpawnPoint();

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif
};

/** Where the director's pacing is. */
UENUM(BlueprintType)
enum class ETHDirectorPhase : uint8
{
	/** Quiet: counting down to a wave. */
	Calm,
	/** Something is happening. */
	Active,
	/** After a peak: no waves, stalkers hold off. */
	Relax
};

/**
 * The director (developer, 2026-11-13): placed once in a level, on the server it watches the players (damage, shots,
 * enemies on them) as Intensity, and paces the level: a long calm brings a wave from a spawn point out of sight (hunters
 * sent towards the players), a peak brings a breather. Its tuning is a UTHDirectorSettings data asset.
 */
UCLASS()
class TACTICALHORROR_API ATHDirector : public AActor
{
	GENERATED_BODY()

public:
	ATHDirector();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Director")
	float GetIntensity() const { return Intensity; }

	UFUNCTION(BlueprintPure, Category = "Director")
	ETHDirectorPhase GetPhase() const { return Phase; }

	/** Enemies it has brought in (tests). */
	int32 GetSpawnedCount() const { return SpawnedCount; }

	/** Server: a wave now, ignoring the calm (tests, scripting). False if there was nowhere to bring it in. */
	bool SpawnWave();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Director")
	TObjectPtr<UTHDirectorSettings> Settings;

protected:
	virtual void BeginPlay() override;

private:
	void Measure(float DeltaSeconds);
	/** A spawn point out of every player's sight at the right distance; null if none. */
	ATHSpawnPoint* PickSpawnPoint() const;
	TArray<APawn*> GetPlayers() const;

	float Intensity = 0.0f;
	ETHDirectorPhase Phase = ETHDirectorPhase::Calm;
	double CalmSince = 0.0;
	double WaveAt = 0.0;
	double RelaxUntil = 0.0;
	int32 SpawnedCount = 0;
	bool bScheduled = false;

	/** What it last saw of each player: their health, their gun's last shot. */
	TMap<TWeakObjectPtr<APawn>, float> LastHealth;
	TMap<TWeakObjectPtr<APawn>, float> LastShot;
};
