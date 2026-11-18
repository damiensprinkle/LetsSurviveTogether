#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "THImpactSurfaces.generated.h"

class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;

/** What a shot leaves on one kind of surface. */
USTRUCT(BlueprintType)
struct FTHSurfaceImpact
{
	GENERATED_BODY()

	/** Its effects and decals are blood (flesh): a player with Blood off in Settings sees none of them, only hears the sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	bool bIsBlood = false;

	/** Bullet-hole decals; one is picked at random per shot. Empty: no hole (flesh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TArray<TObjectPtr<UMaterialInterface>> Decals;

	/** Particles at the hit, facing out of the surface (a wound's spray). Empty: the gun's ImpactEffect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TObjectPtr<UNiagaraSystem> Effect;

	/** Particles at the hit, facing along the shot (blood leaving the far side). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TObjectPtr<UNiagaraSystem> ExitEffect;

	/**
	 * Float user parameters set on Effect and ExitEffect when they spawn (name without "User."). The YI blood droplets
	 * take e.g. EmitterDurationCoefficient (how long the spray keeps spawning) and ParticleLifeTimeCoefficient (how
	 * long each droplet lives), both 1 = as authored. A name the system doesn't have is ignored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TMap<FName, float> EffectParameters;

	/**
	 * Sets the effects' "CollisionChannel" user parameter (the channel the YI blood droplets' collision traces use). Their
	 * own WorldStatic is blocked by living bodies' invisible capsules, so droplets landed on them and hung in the air;
	 * BloodDroplets (DefaultEngine.ini) is ignored by capsules.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	bool bSetEffectCollisionChannel = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (EditCondition = "bSetEffectCollisionChannel"))
	TEnumAsByte<ECollisionChannel> EffectCollisionChannel = ECC_WorldStatic;

	/** Decals on whatever is behind the hit along the shot, within SplatterDistance (blood on the wall behind); one picked at random. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TArray<TObjectPtr<UMaterialInterface>> SplatterDecals;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (Units = "cm", ClampMin = 0))
	float SplatterDistance = 250.0f;

	/** Width of a splatter decal; it grows with the distance from the hit (half again at SplatterDistance). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (Units = "cm", ClampMin = 1))
	float SplatterSize = 60.0f;

	/** Played at the hit, one at random. Empty: the gun's ImpactSound. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact|Sound")
	TArray<TObjectPtr<USoundBase>> Sounds;

	/** Instead of Sounds when the shot hits a head bone (the gun's HeadBoneNames). Empty: Sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact|Sound")
	TArray<TObjectPtr<USoundBase>> HeadshotSounds;
};

/**
 * Shot impacts by surface, shared by all guns (UTHWeaponData::ImpactSurfaces): bullet holes, and for flesh, blood. The
 * surface is the Surface Type of the hit's Physical Material (Project Settings, Physics, Physical Surface). Surfaces not
 * listed use Default. Set up by Scripts/Tools/setup_impact_decals.py (holes and blood).
 */
UCLASS(BlueprintType)
class TACTICALHORROR_API UTHImpactSurfaces : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	FTHSurfaceImpact Default;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TMap<TEnumAsByte<EPhysicalSurface>, FTHSurfaceImpact> Surfaces;

	/** Instead of the surface's sounds when the shot hits a player's body (friendly fire): a teammate being hit sounds different from an enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact|Sound")
	TArray<TObjectPtr<USoundBase>> PlayerHitSounds;

	/** How the surfaces' sounds fade with distance (the sounds themselves may have none). Empty: the sound's own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact|Sound")
	TObjectPtr<USoundAttenuation> SoundAttenuation;

	/** This surface's entry, or Default if it isn't listed. */
	const FTHSurfaceImpact& GetImpact(EPhysicalSurface Surface) const;

	/** A random bullet-hole decal for this surface, or null if it has none. */
	UMaterialInterface* PickDecal(EPhysicalSurface Surface) const;

	/** A random entry, or null if empty. */
	template <typename T>
	static T* PickRandom(const TArray<TObjectPtr<T>>& Items)
	{
		return Items.IsEmpty() ? nullptr : Items[FMath::RandHelper(Items.Num())].Get();
	}

	/**
	 * Sets a not-yet-activated effect's "CollisionChannel" user parameter (FTHSurfaceImpact::EffectCollisionChannel).
	 * In the YI droplets it is an ECollisionChannel enum parameter, which SetVariableInt (int type) doesn't reach, so it
	 * is found by name. False if the effect has no such parameter.
	 */
	static bool SetEffectCollisionChannel(UNiagaraComponent* Component, ECollisionChannel Channel);
};
