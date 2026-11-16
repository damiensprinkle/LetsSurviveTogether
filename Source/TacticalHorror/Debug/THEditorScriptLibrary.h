#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "THEditorScriptLibrary.generated.h"

class ABrush;
class UAnimSequence;
class USkeleton;

/** Editor-only helpers for the Python scripts in Scripts/Tools, for things the editor doesn't expose to Python. */
UCLASS()
class TACTICALHORROR_API UTHEditorScriptLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Gives a brush-based actor (a volume such as NavMeshBoundsVolume) a box shape of this size in cm, centred on it.
	 * Volumes spawned from Python have no shape. Editor only; false in a game.
	 */
	UFUNCTION(BlueprintCallable, Category = "TH Editor Scripting")
	static bool BuildBoxBrush(ABrush* Brush, FVector Size);

	/**
	 * Builds the world's navigation now, blocking until done, so a script can save the level with it. Returns a line
	 * describing the result, and whether each probe point (e.g. a room's middle) is on the navigation mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "TH Editor Scripting", meta = (WorldContext = "WorldContext"))
	static FString BuildNavigation(UObject* WorldContext, const TArray<FVector>& Probes);

	/**
	 * Makes a skeleton keep its own bone lengths when it plays animations made for another skeleton (bone translation
	 * retargeting: every bone Skeleton, the pelvis Animation Scaled so the body still crouches and sways, the root
	 * Animation). The usual Unreal setup for sharing animations between differently proportioned bodies. Marks the
	 * skeleton changed; the caller saves it. Returns how many bones changed; -1 in a game.
	 */
	UFUNCTION(BlueprintCallable, Category = "TH Editor Scripting")
	static int32 KeepOwnBoneLengths(USkeleton* Skeleton, FName PelvisBone = TEXT("pelvis"));

	/**
	 * Where a bone is (component space, cm) at Steps + 1 evenly spaced times from the animation's start to its end,
	 * minus where RelativeTo is at the same time (None = not relative). For measuring animations from scripts: how
	 * fast a root-motion walk travels (the pelvis), when an attack's hands are furthest out. Editor only; empty in a game.
	 */
	UFUNCTION(BlueprintCallable, Category = "TH Editor Scripting")
	static TArray<FVector> GetBonePath(UAnimSequence* Animation, FName Bone, FName RelativeTo, int32 Steps = 60);
};
