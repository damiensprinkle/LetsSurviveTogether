#include "Debug/THEditorScriptLibrary.h"

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
