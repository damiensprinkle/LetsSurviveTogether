#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "THAnimNotify_WeaponSound.generated.h"

class USoundAttenuation;
class USoundBase;

/**
 * Weapon foley timed in an animation (magazine out/in, bolt). The holder hears it unattenuated;
 * everyone else hears it positioned at the weapon, so a teammate's reload isn't played in your ears.
 */
UCLASS(meta = (DisplayName = "TH Weapon Sound"))
class TACTICALHORROR_API UTHAnimNotify_WeaponSound : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = 0))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = 0))
	float PitchMultiplier = 1.0f;

	/** Used for other players' weapons. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundAttenuation> Attenuation;
};
