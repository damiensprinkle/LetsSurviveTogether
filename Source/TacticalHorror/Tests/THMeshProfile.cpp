// A tool, not a check: describes a skeletal mesh's shape as text, for finding points on it (e.g. a gun's sights) without
// looking at it. Does nothing and passes unless TH_PROFILE_MESHES is set to a ;-separated list of asset paths.
// Output: Saved/MeshProfiles/<name>.txt. See TESTING.md.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Engine/SkeletalMesh.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "ReferenceSkeleton.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHMeshProfile, "TacticalHorror.Tools.ProfileMeshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHMeshProfile::RunTest(const FString& Parameters)
{
	TArray<FString> Paths;
	FPlatformMisc::GetEnvironmentVariable(TEXT("TH_PROFILE_MESHES")).ParseIntoArray(Paths, TEXT(";"));
	for (const FString& Path : Paths)
	{
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Path);
		const FSkeletalMeshModel* Model = Mesh ? Mesh->GetImportedModel() : nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("Mesh %s"), *Path), Model) || Model->LODModels.IsEmpty())
		{
			continue;
		}
		FString Text = FString::Printf(TEXT("%s, mesh space (component space at the reference pose)\n\n=== BONES\n"), *Path);
		const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
		TArray<FTransform> ComponentSpace;
		ComponentSpace.SetNum(Skeleton.GetNum());
		for (int32 Bone = 0; Bone < Skeleton.GetNum(); ++Bone)
		{
			const int32 Parent = Skeleton.GetParentIndex(Bone);
			ComponentSpace[Bone] = Parent == INDEX_NONE ? Skeleton.GetRefBonePose()[Bone] : Skeleton.GetRefBonePose()[Bone] * ComponentSpace[Parent];
			const FTransform& T = ComponentSpace[Bone];
			Text += FString::Printf(TEXT("%-24s parent %-20s at %s rot %s\n"), *Skeleton.GetBoneName(Bone).ToString(),
				Parent == INDEX_NONE ? TEXT("-") : *Skeleton.GetBoneName(Parent).ToString(), *T.GetLocation().ToCompactString(), *T.Rotator().ToCompactString());
		}

		const FSkeletalMeshLODModel& LOD = Model->LODModels[0];
		Text += TEXT("\n=== SECTIONS (material slot, vertices, bounds, most-weighted bones)\n");
		for (const FSkelMeshSection& Section : LOD.Sections)
		{
			const FName Slot = Mesh->GetMaterials().IsValidIndex(Section.MaterialIndex) ? Mesh->GetMaterials()[Section.MaterialIndex].MaterialSlotName : NAME_None;
			FBox Box(ForceInit);
			TMap<FName, int32> BoneCounts;
			for (const FSoftSkinVertex& Vertex : Section.SoftVertices)
			{
				Box += FVector(Vertex.Position);
				const FBoneIndexType Bone = Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[0]) ? Section.BoneMap[Vertex.InfluenceBones[0]] : 0;
				BoneCounts.FindOrAdd(Skeleton.GetBoneName(Bone))++;
			}
			BoneCounts.ValueSort([](int32 A, int32 B) { return A > B; });
			FString Bones;
			int32 Shown = 0;
			for (const TPair<FName, int32>& Pair : BoneCounts)
			{
				if (Shown++ < 4)
				{
					Bones += FString::Printf(TEXT(" %s(%d)"), *Pair.Key.ToString(), Pair.Value);
				}
			}
			Text += FString::Printf(TEXT("slot %d %-28s %6d verts  min %s  max %s  bones%s\n"), Section.MaterialIndex, *Slot.ToString(),
				Section.SoftVertices.Num(), *Box.Min.ToCompactString(), *Box.Max.ToCompactString(), *Bones);
		}

		// Per section, along each axis in 1 cm slices: the extreme of the other two axes, so a gun's top edge (sights) shows up
		for (const FSkelMeshSection& Section : LOD.Sections)
		{
			const FName Slot = Mesh->GetMaterials().IsValidIndex(Section.MaterialIndex) ? Mesh->GetMaterials()[Section.MaterialIndex].MaterialSlotName : NAME_None;
			for (int32 Along = 0; Along < 3; ++Along)
			{
				TMap<int32, FBox> Slices;
				for (const FSoftSkinVertex& Vertex : Section.SoftVertices)
				{
					const FVector P(Vertex.Position);
					Slices.FindOrAdd(FMath::FloorToInt(P[Along]), FBox(ForceInit)) += P;
				}
				Slices.KeySort([](int32 A, int32 B) { return A < B; });
				Text += FString::Printf(TEXT("\n=== PROFILE slot %d %s along %c: slice -> min .. max of the whole slice\n"), Section.MaterialIndex, *Slot.ToString(), TEXT("XYZ")[Along]);
				for (const TPair<int32, FBox>& Slice : Slices)
				{
					Text += FString::Printf(TEXT("%4d: %s .. %s\n"), Slice.Key, *Slice.Value.Min.ToCompactString(), *Slice.Value.Max.ToCompactString());
				}
			}
		}
		const FString File = FPaths::ProjectSavedDir() / TEXT("MeshProfiles") / FPaths::GetBaseFilename(Path) + TEXT(".txt");
		TestTrue(*FString::Printf(TEXT("Wrote %s"), *File), FFileHelper::SaveStringToFile(Text, *File));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
