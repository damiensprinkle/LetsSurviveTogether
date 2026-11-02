#include "Player/THCharacterMovementComponent.h"
#include "Player/THCharacter.h"

namespace
{
	/** Adds Walk Quietly to the moves the client sends, so the server replays them at the same speed. */
	class FTHSavedMove : public FSavedMove_Character
	{
	public:
		using Super = FSavedMove_Character;

		virtual void Clear() override
		{
			Super::Clear();
			bSavedWalkQuietly = false;
		}

		virtual uint8 GetCompressedFlags() const override
		{
			uint8 Flags = Super::GetCompressedFlags();
			if (bSavedWalkQuietly)
			{
				Flags |= FLAG_Custom_0;
			}
			return Flags;
		}

		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
		{
			return bSavedWalkQuietly == static_cast<const FTHSavedMove*>(NewMove.Get())->bSavedWalkQuietly
				&& Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
		}

		virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
		{
			Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
			bSavedWalkQuietly = static_cast<UTHCharacterMovementComponent*>(Character->GetCharacterMovement())->bWantsToWalkQuietly;
		}

		virtual void PrepMoveFor(ACharacter* Character) override
		{
			Super::PrepMoveFor(Character);
			static_cast<UTHCharacterMovementComponent*>(Character->GetCharacterMovement())->bWantsToWalkQuietly = bSavedWalkQuietly;
		}

	private:
		uint8 bSavedWalkQuietly : 1 = false;
	};

	class FTHNetworkPredictionData_Client : public FNetworkPredictionData_Client_Character
	{
	public:
		explicit FTHNetworkPredictionData_Client(const UCharacterMovementComponent& Movement)
			: FNetworkPredictionData_Client_Character(Movement)
		{
		}

		virtual FSavedMovePtr AllocateNewMove() override
		{
			return FSavedMovePtr(new FTHSavedMove());
		}
	};
}

float UTHCharacterMovementComponent::GetMaxSpeed() const
{
	const bool bStandingWalk = (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking) && !IsCrouching();
	const float BaseSpeed = bStandingWalk
		? StandingWalkSpeed * (bWantsToWalkQuietly ? QuietWalkMultiplier : 1.0f)
		: Super::GetMaxSpeed();

	const ATHCharacter* Character = Cast<ATHCharacter>(CharacterOwner);
	return Character ? BaseSpeed * Character->GetMovementSpeedMultiplier() : BaseSpeed;
}

void UTHCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToWalkQuietly = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

FNetworkPredictionData_Client* UTHCharacterMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UTHCharacterMovementComponent* MutableThis = const_cast<UTHCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FTHNetworkPredictionData_Client(*this);
	}
	return ClientPredictionData;
}
