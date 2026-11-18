#include "Player/THCameraModifier.h"
#include "Player/Components/THHealthComponent.h"
#include "Player/Components/THSanityComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UTHCameraModifier::AddHit(float Strength)
{
	// A new blow while one is still shaking takes the stronger of the two, restarted
	const float Remaining = HitTime < HitDuration ? HitStrength * (1.0f - HitTime / HitDuration) : 0.0f;
	HitStrength = FMath::Clamp(FMath::Max(Strength, Remaining), 0.0f, 1.5f);
	HitTime = 0.0f;
	HitSeed = FMath::FRandRange(0.0f, 100.0f);
}

bool UTHCameraModifier::IsOwnerDead() const
{
	const APlayerController* PC = CameraOwner ? CameraOwner->GetOwningPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UTHHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UTHHealthComponent>() : nullptr;
	return Health && Health->GetHealthState() == ETHHealthState::Dead;
}

bool UTHCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);

	// Death: from where the eyes were, down to the floor and over onto the side (whatever the body does meanwhile)
	if (IsOwnerDead())
	{
		if (!bFalling)
		{
			bFalling = true;
			FallTime = 0.0f;
			FallStart = InOutPOV.Location;
			FallStartRotation = InOutPOV.Rotation;
			// The floor under the eyes; ignore pawns (their own body, a corpse)
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(THDeathCamera), false);
			FCollisionObjectQueryParams Objects;
			Objects.AddObjectTypesToQuery(ECC_WorldStatic);
			Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
			const FVector Down = FallStart - FVector(0.0f, 0.0f, 300.0f);
			const double FloorZ = GetWorld()->LineTraceSingleByObjectType(Hit, FallStart, Down, Objects, Params) ? Hit.ImpactPoint.Z : Down.Z;
			// Slumping a little forward and to the side as it goes
			const FVector Forward = FallStartRotation.Vector().GetSafeNormal2D();
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
			FallEnd = FVector(FallStart.X, FallStart.Y, FloorZ + FloorEyeHeight) + Forward * 25.0f + Right * 15.0f;
		}
		FallTime += DeltaTime;
		const float FallAlpha = FMath::Clamp(FallTime / FMath::Max(FallDuration, 0.05f), 0.0f, 1.0f);
		// Accelerating like a fall
		const float Drop = FallAlpha * FallAlpha;
		InOutPOV.Location = FMath::Lerp(FallStart, FallEnd, Drop);
		FRotator Rotation = FallStartRotation;
		Rotation.Pitch = FMath::Lerp(FallStartRotation.Pitch, -8.0f, Drop);
		Rotation.Roll = FMath::Lerp(0.0f, FallRoll, FMath::SmoothStep(0.0f, 1.0f, FallAlpha));
		InOutPOV.Rotation = Rotation;
		return false;
	}
	bFalling = false;

	// Hit: a sharp jolt that dies away
	if (HitStrength > 0.0f && HitTime < HitDuration)
	{
		HitTime += DeltaTime;
		const float Fade = FMath::Square(1.0f - FMath::Clamp(HitTime / FMath::Max(HitDuration, 0.05f), 0.0f, 1.0f));
		const float Shake = FMath::Sin((HitTime + HitSeed) * 38.0f);
		InOutPOV.Rotation.Roll += HitRoll * HitStrength * Fade * Shake;
		InOutPOV.Rotation.Pitch -= HitPitch * HitStrength * Fade;
		InOutPOV.Rotation.Yaw += HitRoll * 0.4f * HitStrength * Fade * FMath::Sin((HitTime + HitSeed) * 27.0f);
	}
	return false;
}

void UTHCameraModifier::ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings)
{
	Super::ModifyPostProcess(DeltaTime, PostProcessBlendWeight, PostProcessSettings);
	const APlayerController* PC = CameraOwner ? CameraOwner->GetOwningPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UTHSanityComponent* Sanity = Pawn ? Pawn->FindComponentByClass<UTHSanityComponent>() : nullptr;
	if (Sanity && Sanity->GetBlackoutWeight() > 0.0f)
	{
		PostProcessBlendWeight = Sanity->GetBlackoutWeight();
		PostProcessSettings.bOverride_VignetteIntensity = true;
		PostProcessSettings.VignetteIntensity = Sanity->GetBlackoutVignette();
	}
}
