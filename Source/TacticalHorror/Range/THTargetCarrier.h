#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "THTargetCarrier.generated.h"

/** What a carrier is doing, sent from the server. */
USTRUCT()
struct FTHCarrierState
{
	GENERATED_BODY()

	/** 1 moving away (downrange), -1 coming back, 0 still. */
	UPROPERTY()
	int8 Direction = 0;

	/** Where it was when this started, cm along Axis from where the parts were placed. */
	UPROPERTY()
	float Offset = 0.0f;
};

/**
 * A firing-range target carrier: moves the actors in Parts (a paper target and its hanger) along its lane's rail while
 * a player holds one of its buttons (ATHTargetButton). The server decides; every machine moves its own copy of the
 * parts from the replicated state, so all players see the target at the same distance. Placed by
 * Scripts/Tools/setup_target_carriers.py, one per lane with a target. The parts must be Movable.
 */
UCLASS()
class TACTICALHORROR_API ATHTargetCarrier : public AActor
{
	GENERATED_BODY()

public:
	ATHTargetCarrier();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: start moving away (1) or back (-1) for this player. Ignored while someone else is moving it. */
	void ServerStartMoving(int8 Direction, const AActor* By);

	/** Server: this player let go. */
	void ServerStopMoving(const AActor* By);

	/** This machine's offset from where the parts were placed, cm along Axis. */
	float GetOffset() const { return Offset; }

	bool IsMoving() const { return State.Direction != 0; }

	/** The actors moved together, e.g. SM_Target and SM_Target_Top. */
	UPROPERTY(EditInstanceOnly, Category = "Carrier")
	TArray<TObjectPtr<AActor>> Parts;

	/** Downrange, in world space. */
	UPROPERTY(EditAnywhere, Category = "Carrier")
	FVector Axis = FVector(0.0, -1.0, 0.0);

	/** How far it can come towards the shooter from where it was placed (negative), cm along Axis. */
	UPROPERTY(EditInstanceOnly, Category = "Carrier", meta = (Units = "cm"))
	float MinOffset = -500.0f;

	/** How far it can go downrange from where it was placed, cm along Axis. */
	UPROPERTY(EditInstanceOnly, Category = "Carrier", meta = (Units = "cm"))
	float MaxOffset = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Carrier", meta = (ClampMin = 1, Units = "cm/s"))
	float Speed = 250.0f;

private:
	UFUNCTION()
	void OnRep_State();

	void ApplyOffset();
	void StopOnServer();

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FTHCarrierState State;

	/** This machine's position, moved every frame while State says so. */
	float Offset = 0.0f;

	TArray<FVector> StartLocations;

	/** Server: who is holding a button. */
	TWeakObjectPtr<const AActor> Mover;
};
