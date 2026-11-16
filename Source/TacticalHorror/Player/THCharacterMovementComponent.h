#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "THCharacterMovementComponent.generated.h"

/**
 * Counter-Strike style ground speed: StandingWalkSpeed is the unarmed speed, the held weapon scales it down
 * (pistol a little, rifle more), and holding Walk Quietly (Shift) roughly halves it. There is no sprint.
 * Crouching keeps the engine path (MaxWalkSpeedCrouched), still scaled by the weapon.
 *
 * Walk Quietly travels in the saved-move flags so the server simulates the same speed as the client.
 * The weapon/aim/downed multiplier comes from replicated state, so a client may get small corrections
 * while that state is in flight; move it into flags too if that becomes noticeable.
 */
UCLASS()
class TACTICALHORROR_API UTHCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual float GetMaxSpeed() const override;
	/** A downed player stays down: crouched (low view, low body) whatever the crouch key says. */
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	/** Called from input on the owning client. */
	void SetWalkQuietly(bool bWalkQuietly) { bWantsToWalkQuietly = bWalkQuietly; }

	UFUNCTION(BlueprintPure, Category = "Character Movement: Walking")
	bool IsWalkingQuietly() const { return bWantsToWalkQuietly && IsMovingOnGround() && !IsCrouching(); }

	/** Unarmed standing speed. Replaces MaxWalkSpeed, so nothing in the template Blueprint can turn it into a sprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement: Walking", meta = (ClampMin = 0, Units = "cm/s"))
	float StandingWalkSpeed = 260.0f;

	/** Speed scale while walking quietly (CS shift-walk is about half). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement: Walking", meta = (ClampMin = 0.1, ClampMax = 1))
	float QuietWalkMultiplier = 0.52f;

	bool bWantsToWalkQuietly = false;
};
