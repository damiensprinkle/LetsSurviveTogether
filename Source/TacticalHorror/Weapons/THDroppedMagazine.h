#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "THDroppedMagazine.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USoundBase;

/**
 * A magazine dropped during a reload: a cosmetic physics prop spawned locally on every machine (like shell casings),
 * removed after a while. Collides with the world only, so players never trip on it. Hidden from the one who dropped it
 * until it lands.
 */
UCLASS(NotBlueprintable)
class TACTICALHORROR_API ATHDroppedMagazine : public AActor
{
	GENERATED_BODY()

public:
	ATHDroppedMagazine();

	void Drop(UStaticMesh* Mesh, const FVector& Velocity, float Lifetime, USoundBase* HitSound);

	/** Whether the one who dropped it sees it yet: not while it falls (from the third-person gun, which in first person,
	 * crouched, is just under their eyes), only once it lands. */
	bool IsShownToHolder() const;

private:
	void ShowToHolder();

	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Physics body, sized from the mesh's bounds. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<class UBoxComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> DropSound;

	/** A reload sound: 0.9 since the developer turned reload sounds down by 10% (2026-11-09). */
	static constexpr float DropVolume = 0.9f;

	bool bPlayedSound = false;
};
