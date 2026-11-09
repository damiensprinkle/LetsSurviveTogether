// A tool, not a check: writes a Blueprint's graphs as text, because the .uasset is binary. Does nothing and passes unless
// TH_DUMP_BLUEPRINTS is set to a ;-separated list of asset paths. Output: Saved/BlueprintGraphs/<name>.txt. See TESTING.md.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Curves/CurveFloat.h"
#include "Engine/Blueprint.h"
#include "Engine/TimelineTemplate.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTHBlueprintDump, "TacticalHorror.Tools.DumpBlueprintGraphs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTHBlueprintDump::RunTest(const FString& Parameters)
{
	TArray<FString> Paths;
	FPlatformMisc::GetEnvironmentVariable(TEXT("TH_DUMP_BLUEPRINTS")).ParseIntoArray(Paths, TEXT(";"));

	const auto Title = [](const UEdGraphNode* Node)
	{
		return Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"), TEXT(" "));
	};

	for (const FString& Path : Paths)
	{
		const UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
		if (!TestNotNull(*FString::Printf(TEXT("Blueprint %s"), *Path), Blueprint))
		{
			continue;
		}
		FString Text;
		TArray<UEdGraph*> Graphs = Blueprint->UbergraphPages;
		Graphs.Append(Blueprint->FunctionGraphs);
		// Interface functions with a return value (e.g. GetButtonPromptInfo) are graphs of their own
		for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
		{
			Graphs.Append(Interface.Graphs);
		}
		for (const UEdGraph* Graph : Graphs)
		{
			Text += FString::Printf(TEXT("\n=== GRAPH %s\n"), *Graph->GetName());
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				Text += FString::Printf(TEXT("[%s#%u] %s\n"), *Node->GetClass()->GetName().Replace(TEXT("K2Node_"), TEXT("")), Node->GetUniqueID(), *Title(Node));
				for (const UEdGraphPin* Pin : Node->Pins)
				{
					const bool bExec = Pin->PinType.PinCategory == TEXT("exec");
					// Data is listed from the side that reads it
					if (Pin->bHidden || (Pin->Direction == EGPD_Output && !bExec) || (bExec && Pin->LinkedTo.Num() == 0))
					{
						continue;
					}
					FString Line;
					for (const UEdGraphPin* Linked : Pin->LinkedTo)
					{
						Line += FString::Printf(TEXT(" -> %s#%u.%s"), *Title(Linked->GetOwningNode()), Linked->GetOwningNode()->GetUniqueID(), *Linked->PinName.ToString());
					}
					if (Pin->LinkedTo.Num() == 0)
					{
						// Text literals are kept apart from the other defaults
						Line = FString::Printf(TEXT(" = %s%s%s"), *Pin->DefaultValue, Pin->DefaultObject ? *Pin->DefaultObject->GetName() : TEXT(""),
							Pin->DefaultTextValue.IsEmpty() ? TEXT("") : *(TEXT("\"") + Pin->DefaultTextValue.ToString() + TEXT("\"")));
					}
					Text += FString::Printf(TEXT("    %s %s%s\n"), bExec ? TEXT("EXEC") : TEXT("in"), *Pin->PinName.ToString(), *Line);
				}
			}
		}
		// Timelines' curves, which the graphs only name
		for (const UTimelineTemplate* Timeline : Blueprint->Timelines)
		{
			Text += FString::Printf(TEXT("\n=== TIMELINE %s (length %.2f s)\n"), *Timeline->GetVariableName().ToString(), Timeline->TimelineLength);
			for (const FTTFloatTrack& Track : Timeline->FloatTracks)
			{
				FString Keys;
				if (Track.CurveFloat)
				{
					for (const FRichCurveKey& Key : Track.CurveFloat->FloatCurve.GetConstRefOfKeys())
					{
						Keys += FString::Printf(TEXT(" (%.2f s: %.3f)"), Key.Time, Key.Value);
					}
				}
				Text += FString::Printf(TEXT("    float %s:%s\n"), *Track.GetTrackName().ToString(), *Keys);
			}
		}
		const FString File = FPaths::ProjectSavedDir() / TEXT("BlueprintGraphs") / FPaths::GetBaseFilename(Path) + TEXT(".txt");
		TestTrue(*FString::Printf(TEXT("Wrote %s"), *File), FFileHelper::SaveStringToFile(Text, *File));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
