#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "THFakeEnemy.generated.h"

class APawn;
class USkeletalMeshComponent;
class UTHSanityData;

/**
 * An enemy that isn't there (sanity at the bottom, GDD 7.2): spawned only on the machine of the player losing their
 * mind, never replicated. A zombie look from the sanity data's Fake Enemy Looks, running straight at the player along
 * the floor with its shout; it vanishes with a sound as it reaches them, when its way is blocked, or after its life
 * span. No collision: bullets and bodies pass through it, and it does no harm.
 */
UCLASS(NotBlueprintable)
class TACTICALHORROR_API ATHFakeEnemy : public AActor
{
	GENERATED_BODY()

public:
	ATHFakeEnemy();

	/** Dresses it, faces the player, starts the run and its shout. */
	void Start(APawn* InVictim, const UTHSanityData* InData);

	/** Why it went: "reached", "blocked", "timed out", or empty while it runs. */
	const FString& GetEndReason() const { return EndReason; }

	USkeletalMeshComponent* GetBody() const { return Body; }

	/** A shot from From along Direction: if it passes through the body (nothing solid first), it dissolves. True if so. */
	bool TakeShot(const FVector& From, const FVector& Direction);

	/** Effects it left when shot (tests: Niagara plays nothing with no rendering). */
	int32 GetDissolveEffectCount() const { return DissolveEffectCount; }

	virtual void Tick(float DeltaSeconds) override;

private:
	void Vanish(const TCHAR* Reason);

	/** Faces where it runs; the body under it is turned to match the enemies' meshes, which face +Y. */
	UPROPERTY(VisibleAnywhere, Category = "Fake Enemy")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Fake Enemy")
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> Outfit;

	UPROPERTY(Transient)
	TObjectPtr<const UTHSanityData> Data;

	TWeakObjectPtr<APawn> Victim;
	float Age = 0.0f;
	int32 DissolveEffectCount = 0;
	FString EndReason;
};
