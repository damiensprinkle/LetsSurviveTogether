#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Chaos/ChaosEngineInterface.h"
#include "THFootstepComponent.generated.h"

class ACharacter;
class USkeletalMeshComponent;
class USoundAttenuation;
class USoundBase;

/** Footstep sounds for one kind of surface. */
USTRUCT(BlueprintType)
struct FTHFootstepSounds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps")
	TObjectPtr<USoundBase> Walk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps")
	TObjectPtr<USoundBase> Sprint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps")
	TObjectPtr<USoundBase> Crouch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps")
	TObjectPtr<USoundBase> Land;
};

/**
 * Plays footsteps from distance travelled on the ground, or as a foot lands (FootBones), so they work with any animation
 * and on every machine (simulated proxies included) without RPCs. The surface is read from the physical material under the feet.
 */
UCLASS(ClassGroup = (TacticalHorror), meta = (BlueprintSpawnableComponent))
class TACTICALHORROR_API UTHFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTHFootstepComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Sounds per surface type (Project Settings > Physics > Physical Surface). Missing surfaces use DefaultSounds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps")
	TMap<TEnumAsByte<EPhysicalSurface>, FTHFootstepSounds> SurfaceSounds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps")
	FTHFootstepSounds DefaultSounds;

	/** Applied to other players' footsteps so they are positioned in the world. Your own play unattenuated. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps")
	TObjectPtr<USoundAttenuation> Attenuation;

	/** Your own footsteps are quieter than other players' so they don't mask the world. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = 0, ClampMax = 2))
	float LocalVolume = 0.6f;

	/** Distance per step at the slow and fast end of SpeedRange; in between it scales with speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps", meta = (Units = "cm"))
	FVector2D StrideRange = FVector2D(80.0f, 180.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps", meta = (Units = "cm/s"))
	FVector2D SpeedRange = FVector2D(150.0f, 550.0f);

	/** Faster than this uses the sprint sounds (there is no sprint input; this is for being pushed or future movement). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps", meta = (Units = "cm/s"))
	float SprintSpeed = 320.0f;

	/**
	 * Standing up and slower than this counts as walking quietly (Shift, or the slow aim-down-sights walk):
	 * the crouch sound at QuietVolume. Speed rather than input, so it works for other players' characters too.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps", meta = (Units = "cm/s"))
	float QuietSpeed = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = 0, ClampMax = 1))
	float QuietVolume = 0.3f;

	/** Crouch-walking is the slowest and quietest way to move. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = 0, ClampMax = 1))
	float CrouchVolume = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps", meta = (Units = "cm/s"))
	float MinStepSpeed = 20.0f;

	/**
	 * How loud each kind of step is to enemies: heard within this many times their hearing range
	 * (UTHEnemyBehavior::HearingRange). Walking quietly (Shift, aiming) and crouching are much harder to hear.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps|Enemies hear", meta = (ClampMin = 0))
	float SprintNoise = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps|Enemies hear", meta = (ClampMin = 0))
	float WalkNoise = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps|Enemies hear", meta = (ClampMin = 0))
	float QuietNoise = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps|Enemies hear", meta = (ClampMin = 0))
	float CrouchNoise = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps|Enemies hear", meta = (ClampMin = 0))
	float LandNoise = 0.8f;

	/** Steps are noises enemies hear (players). Off for enemies' own steps. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps|Enemies hear")
	bool bMakesNoise = true;

	/** Landing faster than this (downward) plays a landing sound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps", meta = (Units = "cm/s"))
	float MinLandSpeed = 300.0f;

	/**
	 * Feet (bones of SetFootMesh's mesh): a step plays as one lands, so it lines up with the animation (developer, 2026-11-16:
	 * the charger's stomps didn't), rather than every stride's distance. Feet that barely lift (a shuffle) still step once
	 * well past a stride. Empty, or no such bones: by distance only (the players: no body of their own to watch).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps|Feet")
	TArray<FName> FootBones;

	/** A foot this far above the lowest it has been lately is off the floor... */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps|Feet", meta = (Units = "cm", ClampMin = 1))
	float FootLiftHeight = 8.0f;

	/** ...and back down within this of it, it has landed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps|Feet", meta = (Units = "cm", ClampMin = 0))
	float FootPlantHeight = 3.0f;

	void SetFootMesh(USkeletalMeshComponent* Mesh);

	/** Steps of its own, whatever the floor (a charger's stomp), one at random per walk or run step; empty = by surface. */
	void SetStepSoundOverride(const TArray<TObjectPtr<USoundBase>>& Sounds, float Volume = 1.0f) { StepSoundOverride = Sounds; StepOverrideVolume = Volume; }

	/** Steps played so far, and how many of them a foot landing set off (tests). */
	int32 GetStepCount() const { return StepCount; }
	int32 GetFootLandingCount() const { return FootLandingCount; }

protected:
	virtual void BeginPlay() override;

private:
	enum class EStep : uint8 { Walk, Sprint, Crouch, Quiet, Land };

	void PlayStep(EStep Kind);
	const FTHFootstepSounds& GetSoundsUnderFeet() const;
	/** Follows the feet; true when one has just landed. */
	bool UpdateFeet(float DeltaTime);

	struct FFoot
	{
		FName Bone;
		float Floor = 0.0f;	// cm above the capsule's bottom: the lowest lately (it creeps up, for slopes and stairs)
		bool bHasFloor = false;
		bool bLifted = false;
	};
	TArray<FFoot> TrackedFeet;
	TWeakObjectPtr<USkeletalMeshComponent> FootMesh;
	double LastStepTime = -1.0;
	int32 StepCount = 0;
	int32 FootLandingCount = 0;

	TWeakObjectPtr<ACharacter> Character;
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> StepSoundOverride;
	float StepOverrideVolume = 1.0f;
	float DistanceSinceStep = 0.0f;
	bool bWasFalling = false;
	float FallSpeed = 0.0f;
};
