#include "Debug/THAutoTestSubsystem.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Weapons/THWeaponBase.h"
#include "Weapons/THWeaponComponent.h"
#include "Player/THCharacterMovementComponent.h"
#include "Components/ActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"
#include "TacticalHorror.h"

static TAutoConsoleVariable<int32> CVarAutoWalk(
	TEXT("th.AutoWalk"), 0, TEXT("Local player's pawn walks forward on its own: 1 = normal, 2 = walking quietly, 3 = crouched."));
static TAutoConsoleVariable<float> CVarAutoShotInterval(
	TEXT("th.AutoShotInterval"), 0.0f, TEXT("Seconds between automatic screenshots (0 = off)."));
static TAutoConsoleVariable<float> CVarAutoQuitAfter(
	TEXT("th.AutoQuitAfter"), 0.0f, TEXT("Quit this many seconds after the local player has a pawn (0 = off)."));
static TAutoConsoleVariable<FString> CVarPreviewMesh(
	TEXT("th.PreviewMesh"), TEXT(""), TEXT("Object path of a skeletal mesh to show in front of the camera."));
static TAutoConsoleVariable<FString> CVarPreviewAnim(
	TEXT("th.PreviewAnim"), TEXT(""), TEXT("Object path of an animation to loop on the preview mesh."));
static TAutoConsoleVariable<float> CVarPreviewSpin(
	TEXT("th.PreviewSpin"), 0.0f, TEXT("Preview mesh rotation speed in degrees per second."));
static TAutoConsoleVariable<FString> CVarPreviewAttach(
	TEXT("th.PreviewAttach"), TEXT(""), TEXT("\"x y z pitch yaw roll\": hold the preview mesh at this transform relative to the camera, like a held weapon, instead of floating it in view."));
static TAutoConsoleVariable<float> CVarAutoFireAfter(
	TEXT("th.AutoFireAfter"), 0.0f, TEXT("Hold the trigger of the held weapon from this many seconds in (0 = off)."));
static TAutoConsoleVariable<FString> CVarHoldOffset(
	TEXT("th.HoldOffset"), TEXT(""), TEXT("\"x y z pitch yaw roll\" override for the held weapon's attach transform, for tuning viewmodels."));
static TAutoConsoleVariable<int32> CVarAutoEquip(
	TEXT("th.AutoEquip"), -1, TEXT("Equip this loadout slot (0 = primary, 1 = secondary) once the weapons exist (-1 = off)."));
static TAutoConsoleVariable<bool> CVarBlockCameraShakes(
	TEXT("th.BlockCameraShakes"), false, TEXT("Stop every camera shake on the local player each frame."));
static TAutoConsoleVariable<int32> CVarThirdPerson(
	TEXT("th.ThirdPerson"), 0,
	TEXT("1 = watch your own character from outside, as other players see it (weapon in hand, body animation). 0 = normal view. Camera placement: th.ThirdPersonView."));
static TAutoConsoleVariable<FString> CVarThirdPersonView(
	TEXT("th.ThirdPersonView"), TEXT("200 60 20 40"),
	TEXT("Camera for th.ThirdPerson: \"distance angle height [lookheight]\" - distance (cm), angle around the character (0 = in front, 90 = its right, 180 = behind), height, and the point it looks at above the character's centre (default 20)."));
static TAutoConsoleVariable<float> CVarAutoWalkAngle(
	TEXT("th.AutoWalkAngle"), 0.0f, TEXT("Direction th.AutoWalk moves in, degrees from facing (90 = strafe right, 180 = backwards)."));
static TAutoConsoleVariable<float> CVarAutoLookPitch(
	TEXT("th.AutoLookPitch"), 0.0f, TEXT("Hold the local player's view at this pitch in degrees (0 = off; use 0.01 for level)."));
static TAutoConsoleVariable<float> CVarAutoReloadAfter(
	TEXT("th.AutoReloadAfter"), 0.0f, TEXT("Reload the held weapon once, this many seconds in (0 = off)."));
static TAutoConsoleVariable<FString> CVarStripComponents(
	TEXT("th.StripComponents"), TEXT(""), TEXT("Comma-separated name fragments; matching components are destroyed on the local pawn. For bisecting Blueprint issues."));

bool UTHAutoTestSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UTHAutoTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTHAutoTestSubsystem, STATGROUP_Tickables);
}

void UTHAutoTestSubsystem::Tick(float DeltaTime)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	// Startup -ExecCmds apply a few frames after the pawn spawns, so re-strip whenever the setting changes too
	const FString StripSetting = CVarStripComponents.GetValueOnGameThread();
	if (StrippedPawn != Pawn || StrippedSetting != StripSetting)
	{
		StrippedPawn = Pawn;
		StrippedSetting = StripSetting;
		StripComponents(Pawn);
	}

	Elapsed += DeltaTime;

	TickPreview(PC, DeltaTime);

	const int32 EquipSlot = CVarAutoEquip.GetValueOnGameThread();
	if (EquipSlot >= 0 && EquippedPawn != Pawn)
	{
		UTHWeaponComponent* Weapons = Pawn->FindComponentByClass<UTHWeaponComponent>();
		if (Weapons && Weapons->GetWeapons().IsValidIndex(EquipSlot))
		{
			Weapons->EquipWeaponAtIndex(EquipSlot);
			EquippedPawn = Pawn;
		}
	}

	// "x y z pitch yaw roll": override the held weapon's transform relative to what it's attached to
	TArray<FString> Hold;
	CVarHoldOffset.GetValueOnGameThread().ParseIntoArrayWS(Hold);
	if (Hold.Num() == 6)
	{
		TArray<AActor*> Attached;
		Pawn->GetAttachedActors(Attached, true, true);
		for (AActor* Actor : Attached)
		{
			if (!Actor->IsHidden())
			{
				Actor->SetActorRelativeTransform(FTransform(
					FRotator(FCString::Atof(*Hold[3]), FCString::Atof(*Hold[4]), FCString::Atof(*Hold[5])),
					FVector(FCString::Atof(*Hold[0]), FCString::Atof(*Hold[1]), FCString::Atof(*Hold[2]))));
			}
		}
	}

	if (CVarBlockCameraShakes.GetValueOnGameThread() && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StopAllCameraShakes(true);
	}

	const float FireAfter = CVarAutoFireAfter.GetValueOnGameThread();
	if (FireAfter > 0.0f && Elapsed >= FireAfter)
	{
		TArray<AActor*> Attached;
		Pawn->GetAttachedActors(Attached, true, true);
		for (AActor* Actor : Attached)
		{
			ATHWeaponBase* Weapon = Cast<ATHWeaponBase>(Actor);
			if (Weapon && !Weapon->IsHidden() && !bAutoFiring)
			{
				Weapon->StartFire();
				bAutoFiring = true;
			}
		}
	}

	if (const int32 AutoWalk = CVarAutoWalk.GetValueOnGameThread())
	{
		if (UTHCharacterMovementComponent* Movement = Cast<UTHCharacterMovementComponent>(Pawn->GetMovementComponent()))
		{
			Movement->SetWalkQuietly(AutoWalk == 2);
			Movement->bWantsToCrouch = AutoWalk == 3 || AutoWalk == 4;
		}
		// 4 = crouched in place
		if (AutoWalk != 4)
		{
			Pawn->AddMovementInput(Pawn->GetActorForwardVector().RotateAngleAxis(CVarAutoWalkAngle.GetValueOnGameThread(), FVector::UpVector), 1.0f);
		}
	}

	if (const float LookPitch = CVarAutoLookPitch.GetValueOnGameThread(); LookPitch != 0.0f)
	{
		FRotator Control = PC->GetControlRotation();
		Control.Pitch = LookPitch;
		PC->SetControlRotation(Control);
	}

	const float ReloadAfter = CVarAutoReloadAfter.GetValueOnGameThread();
	if (ReloadAfter > 0.0f && Elapsed >= ReloadAfter && !bAutoReloaded)
	{
		bAutoReloaded = true;
		if (UTHWeaponComponent* Weapons = Pawn->FindComponentByClass<UTHWeaponComponent>(); Weapons && Weapons->GetCurrentWeapon())
		{
			Weapons->GetCurrentWeapon()->StartReload();
		}
	}

	TickThirdPersonView(PC, Pawn);

	const float ShotInterval = CVarAutoShotInterval.GetValueOnGameThread();
	if (ShotInterval > 0.0f && Elapsed >= NextShotTime)
	{
		NextShotTime = Elapsed + ShotInterval;
		FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("AutoShot_%02d.png"), ShotIndex++), true, false);
	}

	const float QuitAfter = CVarAutoQuitAfter.GetValueOnGameThread();
	if (QuitAfter > 0.0f && Elapsed >= QuitAfter)
	{
		PC->ConsoleCommand(TEXT("quit"));
	}
}

void UTHAutoTestSubsystem::TickPreview(APlayerController* PC, float DeltaTime)
{
	const FString MeshPath = CVarPreviewMesh.GetValueOnGameThread();
	if (MeshPath.IsEmpty() || !PC->PlayerCameraManager)
	{
		return;
	}

	if (!PreviewActor.IsValid())
	{
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
		if (!Mesh)
		{
			UE_LOG(LogTacticalHorror, Error, TEXT("AutoTest: preview mesh not found: %s"), *MeshPath);
			IConsoleManager::Get().FindConsoleVariable(TEXT("th.PreviewMesh"))->Set(TEXT(""));
			return;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASkeletalMeshActor* Actor = GetWorld()->SpawnActor<ASkeletalMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		Actor->GetSkeletalMeshComponent()->SetSkeletalMeshAsset(Mesh);

		const FString AnimPath = CVarPreviewAnim.GetValueOnGameThread();
		if (UAnimSequenceBase* Anim = AnimPath.IsEmpty() ? nullptr : LoadObject<UAnimSequenceBase>(nullptr, *AnimPath))
		{
			Actor->GetSkeletalMeshComponent()->PlayAnimation(Anim, true);
			UE_LOG(LogTacticalHorror, Warning, TEXT("AutoTest: playing %s (%.2fs)"), *Anim->GetName(), Anim->GetPlayLength());
		}

		PreviewActor = Actor;
		PreviewYaw = 0.0f;
	}

	ASkeletalMeshActor* Actor = PreviewActor.Get();
	USkeletalMeshComponent* MeshComp = Actor->GetSkeletalMeshComponent();

	TArray<FString> Attach;
	CVarPreviewAttach.GetValueOnGameThread().ParseIntoArrayWS(Attach);
	if (Attach.Num() == 6)
	{
		const FTransform Offset(
			FRotator(FCString::Atof(*Attach[3]), FCString::Atof(*Attach[4]), FCString::Atof(*Attach[5])),
			FVector(FCString::Atof(*Attach[0]), FCString::Atof(*Attach[1]), FCString::Atof(*Attach[2])));
		const FTransform Camera(PC->PlayerCameraManager->GetCameraRotation(), PC->PlayerCameraManager->GetCameraLocation());
		Actor->SetActorTransform(Offset * Camera);
		return;
	}

	PreviewYaw += CVarPreviewSpin.GetValueOnGameThread() * DeltaTime;

	// Rotate about the mesh's own bounds centre (viewmodels are often authored far from their pivot),
	// then keep that centre a fitting distance in front of the camera
	const FBoxSphereBounds LocalBounds = MeshComp->GetSkeletalMeshAsset()->GetBounds();
	const FRotator Rotation(0.0f, PreviewYaw, 0.0f);
	const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Forward = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const FVector Target = Camera + Forward * FMath::Max(LocalBounds.SphereRadius * 1.8f, 30.0f);
	Actor->SetActorLocationAndRotation(Target - Rotation.RotateVector(LocalBounds.Origin), Rotation);
}

void UTHAutoTestSubsystem::TickThirdPersonView(APlayerController* PC, APawn* Pawn)
{
	TArray<FString> Values;
	CVarThirdPersonView.GetValueOnGameThread().ParseIntoArrayWS(Values);
	if (Values.Num() != 3 && Values.Num() != 4)
	{
		Values = { TEXT("200"), TEXT("60"), TEXT("20"), TEXT("40") };
	}
	if (CVarThirdPerson.GetValueOnGameThread() == 0)
	{
		if (ACameraActor* Camera = ThirdPersonCamera.Get())
		{
			PC->SetViewTarget(Pawn);
			Camera->Destroy();
		}
		return;
	}

	ACameraActor* Camera = ThirdPersonCamera.Get();
	if (!Camera)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Camera = GetWorld()->SpawnActor<ACameraActor>(Pawn->GetActorLocation(), FRotator::ZeroRotator, Params);
		ThirdPersonCamera = Camera;
		ThirdPersonYaw = Pawn->GetActorRotation().Yaw;
	}

	// Other players see the body and the weapon in its hand; the view target decides which copy is drawn
	const float Distance = FCString::Atof(*Values[0]);
	const float Angle = FCString::Atof(*Values[1]);
	const float Height = FCString::Atof(*Values[2]);
	const float TargetHeight = Values.Num() == 4 ? FCString::Atof(*Values[3]) : 20.0f;
	const FVector Target = Pawn->GetActorLocation() + FVector(0.0f, 0.0f, TargetHeight);
	const FVector Direction = Pawn->GetActorForwardVector().RotateAngleAxis(Angle, FVector::UpVector);
	const FVector Location = Target + Direction * Distance + FVector(0.0f, 0.0f, Height);
	Camera->SetActorLocationAndRotation(Location, (Target - Location).Rotation());
	if (PC->GetViewTarget() != Camera)
	{
		PC->SetViewTarget(Camera);
	}

	// The camera manager points the view at this camera; keep the character facing the way it did when watching
	// started (the body and the weapon in its hand follow the actor's yaw)
	Pawn->SetActorRotation(FRotator(0.0f, ThirdPersonYaw, 0.0f));
}

void UTHAutoTestSubsystem::StripComponents(APawn* Pawn)
{
	TArray<FString> Fragments;
	CVarStripComponents.GetValueOnGameThread().ParseIntoArray(Fragments, TEXT(","), true);
	if (Fragments.IsEmpty())
	{
		return;
	}

	TInlineComponentArray<UActorComponent*> Components(Pawn);
	for (UActorComponent* Component : Components)
	{
		for (const FString& Fragment : Fragments)
		{
			// "anim:<name>" keeps the skeletal mesh but drops its animation Blueprint (reference pose)
			FString Name = Fragment.TrimStartAndEnd();
			if (Name.RemoveFromStart(TEXT("anim:")))
			{
				if (USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(Component); SkelMesh && SkelMesh->GetName().Contains(Name))
				{
					UE_LOG(LogTacticalHorror, Warning, TEXT("AutoTest: stripping animation from %s"), *SkelMesh->GetName());
					SkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
				}
				continue;
			}

			if (Component->GetName().Contains(Name))
			{
				UE_LOG(LogTacticalHorror, Warning, TEXT("AutoTest: stripping %s"), *Component->GetName());
				Component->DestroyComponent();
				break;
			}
		}
	}
}
