#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "THAutoTestSubsystem.generated.h"

/**
 * Unattended play testing for any pawn, driven by console variables so it can run from the command line:
 *   UnrealEditor.exe <uproject> /Game/Demo -game -RenderOffScreen
 *     -ExecCmds="th.AutoWalk 1, th.AutoShotInterval 0.5, th.AutoQuitAfter 5"
 * Screenshots land in Saved/Screenshots/WindowsEditor/AutoShot_NN.png.
 */
UCLASS()
class TACTICALHORROR_API UTHAutoTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void StripComponents(APawn* Pawn);
	void TickPreview(APlayerController* PC, float DeltaTime);
	void TickThirdPersonView(APlayerController* PC, APawn* Pawn);

	TWeakObjectPtr<class ASkeletalMeshActor> PreviewActor;
	TWeakObjectPtr<class ACameraActor> ThirdPersonCamera;
	float ThirdPersonYaw = 0.0f;
	bool bAutoReloaded = false;
	bool bAutoAimed = false;
	bool bAutoSwitched = false;
	float PreviewYaw = 0.0f;
	bool bAutoFiring = false;

	float Elapsed = 0.0f;
	float NextShotTime = 0.0f;
	int32 ShotIndex = 0;
	TWeakObjectPtr<APawn> StrippedPawn;
	TWeakObjectPtr<APawn> EquippedPawn;
	FString StrippedSetting;
	TSet<int32> RanCommands;
};
