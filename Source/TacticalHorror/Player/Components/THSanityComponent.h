#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "THSanityComponent.generated.h"

class ATHFakeEnemy;
class UTHSanityData;

/**
 * A player's sanity (GDD 7.2; tuning and rules on UTHSanityData, DA_Sanity). The server works it out from the light on
 * the player and where their teammates are, and sends it to that player only. Its effects play on that player's own
 * machine, for them alone: the heartbeat, the aim drifting, sounds that aren't there, and at the bottom a fake enemy.
 * Frozen while downed or dead (no effects when dead).
 */
UCLASS(ClassGroup = (TacticalHorror), meta = (BlueprintSpawnableComponent))
class TACTICALHORROR_API UTHSanityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr float MaxSanity = 100.0f;

	UTHSanityComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Sanity")
	float GetSanity() const { return Sanity; }

	/** Server: sets it (tests, the Sanity console command). */
	void SetSanity(float Value);

	/** Server: per second right now, drains as negative (for tests and th.SanityDebug). */
	float GetRate() const { return Rate; }

	/** What the last update found (server). */
	bool IsInDarkness() const { return bInDarkness; }
	bool IsAlone() const { return bAlone; }
	bool IsNearTeammate() const { return bNearTeammate; }

	/** The owner's machine: what has played so far (tests). */
	int32 GetHeartbeatCount() const { return HeartbeatCount; }
	int32 GetHallucinationCount() const { return HallucinationCount; }
	FName GetLastHallucination() const { return LastHallucination; }
	int32 GetFakeEnemyCount() const { return FakeEnemyCount; }
	ATHFakeEnemy* GetFakeEnemy() const { return FakeEnemy.Get(); }
	/** The view's drift from sanity right now (pitch, yaw), degrees. */
	FVector2D GetSway() const { return AppliedSway; }
	/** The view's edges darkening from sanity right now: vignette intensity, and how much it replaces the camera's own (0: none). */
	float GetBlackoutVignette() const { return BlackoutVignette; }
	float GetBlackoutWeight() const { return BlackoutWeight; }

	/** Owner's machine: plays one hallucination now (Name: that one; none: one picked as usual), ignoring the timer. */
	bool PlayHallucination(FName Name = NAME_None);

	/** Owner's machine: sends a fake enemy now, if there is room for one. */
	bool SendFakeEnemy();

	const UTHSanityData* GetData() const;

	UPROPERTY(EditDefaultsOnly, Category = "Sanity")
	TSoftObjectPtr<UTHSanityData> Data = TSoftObjectPtr<UTHSanityData>(FSoftObjectPath(TEXT("/Game/Blueprint/Character/DA_Sanity.DA_Sanity")));

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdateSanity(float DeltaTime, const UTHSanityData& Tuning);
	void UpdateEffects(float DeltaTime, const UTHSanityData& Tuning);
	void UpdateSway(float DeltaTime, const UTHSanityData& Tuning, bool bActive);
	void PlayStep(TSharedRef<struct FTHPhantomWalk> Walk);

	/** 0 at Threshold and above, 1 at 0 sanity. */
	float Depth(float Threshold) const;

	UPROPERTY(Replicated)
	float Sanity = MaxSanity;

	UPROPERTY(Transient)
	mutable TObjectPtr<const UTHSanityData> LoadedData;

	float Rate = 0.0f;
	bool bInDarkness = false;
	bool bAlone = false;
	bool bNearTeammate = false;
	double LastDrainTime = -1.0e9;
	float SinceUpdate = 0.0f;

	double NextBeatTime = 0.0;
	double NextHallucinationTime = -1.0;
	double NextFakeEnemyTime = -1.0;
	FName LastHallucination;
	int32 HeartbeatCount = 0;
	int32 HallucinationCount = 0;
	int32 FakeEnemyCount = 0;
	TWeakObjectPtr<ATHFakeEnemy> FakeEnemy;

	FDelegateHandle ShotHandle;
	float SwayTime = 0.0f;
	FVector2D AppliedSway = FVector2D::ZeroVector;
	float SwaySeed = 0.0f;
	double LastBeatTime = -1.0e9;
	float BlackoutDepth = 0.0f;
	float BlackoutVignette = 0.0f;
	float BlackoutWeight = 0.0f;
};
