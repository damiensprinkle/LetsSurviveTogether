#include "Debug/THEditorScriptLibrary.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/Skeleton.h"
#include "Engine/Brush.h"

#include "NavMesh/RecastNavMesh.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

#if WITH_EDITOR
#include "Builders/CubeBuilder.h"
#endif

FString UTHEditorScriptLibrary::BuildNavigation(UObject* WorldContext, const TArray<FVector>& Probes)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Navigation)
	{
		return TEXT("no navigation system");
	}
	Navigation->Build();
	FString Result = FString::Printf(TEXT("%d navigation data, %d bounds"), Navigation->NavDataSet.Num(), Navigation->GetNavigationBounds().Num());
	for (const ANavigationData* Data : Navigation->NavDataSet)
	{
		const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(Data);
		Result += FString::Printf(TEXT("; %s: tiles %d"), *GetNameSafe(Data), Recast ? Recast->GetNavMeshTilesCount() : -1);
	}
	for (const FVector& Probe : Probes)
	{
		FNavLocation OnMesh;
		const bool bOnMesh = Navigation->ProjectPointToNavigation(Probe, OnMesh, FVector(30.0, 30.0, 200.0));
		Result += FString::Printf(TEXT("; %s on the mesh %d"), *Probe.ToCompactString(), bOnMesh);
		// From the first probe to each other one
		if (&Probe != &Probes[0])
		{
			const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Probes[0], Probe);
			Result += FString::Printf(TEXT(" path %.0f m%s"), Path ? Path->GetPathLength() / 100.0 : -1.0, Path && Path->IsPartial() ? TEXT(" PARTIAL") : TEXT(""));
		}
	}
	if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(Navigation->GetDefaultNavDataInstance()))
	{
		Result += FString::Printf(TEXT("; cell size %.0f, agent radius %.0f"), Recast->GetCellSize(ENavigationDataResolution::Default), Recast->AgentRadius);
	}
	return Result;
}

bool UTHEditorScriptLibrary::BuildBoxBrush(ABrush* Brush, FVector Size)
{
#if WITH_EDITOR
	if (!Brush || !Brush->GetWorld())
	{
		return false;
	}
	UCubeBuilder* Builder = NewObject<UCubeBuilder>(GetTransientPackage());
	Builder->X = Size.X;
	Builder->Y = Size.Y;
	Builder->Z = Size.Z;
	const bool bBuilt = Builder->Build(Brush->GetWorld(), Brush);
	Brush->PostEditChange();
	return bBuilt;
#else
	return false;
#endif
}

int32 UTHEditorScriptLibrary::KeepOwnBoneLengths(USkeleton* Skeleton, FName PelvisBone)
{
#if WITH_EDITOR
	if (!Skeleton)
	{
		return 0;
	}
	const FReferenceSkeleton& Reference = Skeleton->GetReferenceSkeleton();
	const int32 PelvisIndex = Reference.FindBoneIndex(PelvisBone);
	int32 Changed = 0;
	for (int32 Index = 0; Index < Reference.GetNum(); ++Index)
	{
		const EBoneTranslationRetargetingMode::Type Wanted = Index == 0 ? EBoneTranslationRetargetingMode::Animation
			: Index == PelvisIndex ? EBoneTranslationRetargetingMode::AnimationScaled
			: EBoneTranslationRetargetingMode::Skeleton;
		if (Skeleton->GetBoneTranslationRetargetingMode(Index) != Wanted)
		{
			Skeleton->SetBoneTranslationRetargetingMode(Index, Wanted);
			++Changed;
		}
	}
	if (Changed > 0)
	{
		Skeleton->MarkPackageDirty();
	}
	return Changed;
#else
	return -1;
#endif
}

TArray<FVector> UTHEditorScriptLibrary::GetBonePath(UAnimSequence* Animation, FName Bone, FName RelativeTo, int32 Steps)
{
	TArray<FVector> Path;
#if WITH_EDITOR
	const USkeleton* Skeleton = Animation ? Animation->GetSkeleton() : nullptr;
	if (!Skeleton || Steps < 1)
	{
		return Path;
	}
	const FReferenceSkeleton& Reference = Skeleton->GetReferenceSkeleton();
	const IAnimationDataModel* Model = Animation->GetDataModel();
	const int32 BoneIndex = Reference.FindBoneIndex(Bone);
	const int32 RelativeIndex = RelativeTo.IsNone() ? INDEX_NONE : Reference.FindBoneIndex(RelativeTo);
	if (BoneIndex == INDEX_NONE || (!RelativeTo.IsNone() && RelativeIndex == INDEX_NONE))
	{
		return Path;
	}

	// Component-space position of a bone at a time: its chain of local transforms (reference pose where untracked)
	auto Position = [&](int32 Index, double Time)
	{
		FTransform Result = FTransform::Identity;
		for (; Index != INDEX_NONE; Index = Reference.GetParentIndex(Index))
		{
			FTransform Local = Reference.GetRefBonePose()[Index];
			if (Model && Model->IsValidBoneTrackName(Reference.GetBoneName(Index)))
			{
				Animation->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Index), FAnimExtractContext(Time), true);
			}
			Result = Result * Local;
		}
		return Result.GetLocation();
	};

	const double Length = Animation->GetPlayLength();
	for (int32 Step = 0; Step <= Steps; ++Step)
	{
		const double Time = Length * Step / Steps;
		const FVector Location = Position(BoneIndex, Time);
		Path.Add(RelativeIndex == INDEX_NONE ? Location : Location - Position(RelativeIndex, Time));
	}
#endif
	return Path;
}
