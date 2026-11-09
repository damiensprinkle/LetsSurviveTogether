#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "THEditorScriptLibrary.generated.h"

class ABrush;

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
};
