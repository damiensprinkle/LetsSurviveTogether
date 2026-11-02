#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "THShellCasing.generated.h"

class UStaticMeshComponent;
class UTHWeaponData;

/**
 * Spent casing thrown from a weapon's ejection port. Purely cosmetic: spawned locally on each machine, not replicated,
 * physics only (no queries, so bullets and interaction traces ignore it), and it passes through pawns.
 */
UCLASS(NotBlueprintable)
class TACTICALHORROR_API ATHShellCasing : public AActor
{
	GENERATED_BODY()

public:
	ATHShellCasing();

	/** Sets mesh, sound and lifetime from the weapon data, then starts simulating with the given world velocity. */
	void Launch(const UTHWeaponData* Data, const FVector& Velocity, const FVector& AngularVelocityDegrees);

private:
	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<const UTHWeaponData> WeaponData;

	bool bPlayedBounceSound = false;
};
