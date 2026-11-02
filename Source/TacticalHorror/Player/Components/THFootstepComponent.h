#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Chaos/ChaosEngineInterface.h"
#include "THFootstepComponent.generated.h"

class ACharacter;
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
 * Plays footsteps from distance travelled on the ground, so they work with any animation and on every machine
 * (simulated proxies included) without RPCs. The surface is read from the physical material under the feet.
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

	/** Landing faster than this (downward) plays a landing sound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps", meta = (Units = "cm/s"))
	float MinLandSpeed = 300.0f;

protected:
	virtual void BeginPlay() override;

private:
	enum class EStep : uint8 { Walk, Sprint, Crouch, Quiet, Land };

	void PlayStep(EStep Kind);
	const FTHFootstepSounds& GetSoundsUnderFeet() const;

	TWeakObjectPtr<ACharacter> Character;
	float DistanceSinceStep = 0.0f;
	bool bWasFalling = false;
	float FallSpeed = 0.0f;
};
