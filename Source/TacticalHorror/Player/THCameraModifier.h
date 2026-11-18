#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "THCameraModifier.generated.h"

/**
 * The local player's view reacting to their body: a jolt when hit (AddHit), and falling to the floor and rolling onto
 * its side when they die, and the edges darkening as their sanity runs out. Added by ATHPlayerController to its
 * camera manager. Works on the final view, so it doesn't care how the template's camera is attached.
 */
UCLASS()
class TACTICALHORROR_API UTHCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	/** A hit: Strength 1 is a heavy blow (about 40 damage). */
	void AddHit(float Strength);

	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
	/** Low sanity: the edges of the view darken (UTHSanityComponent). */
	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings) override;

	/** Jolt at strength 1: degrees of roll / pitch, and how long it lasts. */
	UPROPERTY(EditAnywhere, Category = "Hit")
	float HitRoll = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Hit")
	float HitPitch = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Hit", meta = (Units = "s"))
	float HitDuration = 0.45f;

	/** Death: time to reach the floor, the eye's height above it there, and the roll onto the side. */
	UPROPERTY(EditAnywhere, Category = "Death", meta = (Units = "s"))
	float FallDuration = 0.9f;

	UPROPERTY(EditAnywhere, Category = "Death", meta = (Units = "cm"))
	float FloorEyeHeight = 18.0f;

	UPROPERTY(EditAnywhere, Category = "Death")
	float FallRoll = 75.0f;

private:
	bool IsOwnerDead() const;

	float HitStrength = 0.0f;
	float HitTime = 0.0f;
	float HitSeed = 0.0f;

	bool bFalling = false;
	float FallTime = 0.0f;
	FVector FallStart = FVector::ZeroVector;
	FRotator FallStartRotation = FRotator::ZeroRotator;
	FVector FallEnd = FVector::ZeroVector;
};
