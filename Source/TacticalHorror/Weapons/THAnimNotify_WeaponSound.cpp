#include "Weapons/THAnimNotify_WeaponSound.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/THAudioSettingsSubsystem.h"
#include "Sound/SoundBase.h"

void UTHAnimNotify_WeaponSound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!Sound || !MeshComp)
	{
		return;
	}

	// The mesh belongs to the weapon actor, whose owner is the pawn holding it. No pawn = animation editor preview.
	const AActor* Weapon = MeshComp->GetOwner();
	const APawn* Holder = Weapon ? Cast<APawn>(Weapon->GetOwner()) : nullptr;
	const float Volume = VolumeMultiplier * UTHAudioSettingsSubsystem::GetGunplayVolumeFor(MeshComp);
	if (!Holder || Holder->IsLocallyControlled())
	{
		UGameplayStatics::PlaySound2D(MeshComp, Sound, Volume, PitchMultiplier);
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(MeshComp, Sound, MeshComp->GetComponentLocation(), Volume, PitchMultiplier, 0.0f, Attenuation);
	}
}

FString UTHAnimNotify_WeaponSound::GetNotifyName_Implementation() const
{
	return Sound ? Sound->GetName() : TEXT("TH Weapon Sound");
}
