// Shared pieces of the play-session tests (THFunctionalTests.cpp, THMultiplayerTests.cpp). See TESTING.md.
#pragma once

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace THTestHelpers
{
	/** Made by Scripts/Tools/create_test_map.py: a floor, player starts and a light. */
	inline const TCHAR* TestMap = TEXT("/Game/Tests/Maps/L_FunctionalTests");

	/** The first play-session world (the server's, in a multiplayer session). */
	inline UWorld* GetPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World())
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	inline double GameTime()
	{
		const UWorld* World = GetPlayWorld();
		return World ? World->GetTimeSeconds() : 0.0;
	}

	/** The middle of a bone's physics body, where a shot at that bone should aim (the bone's own point is its joint:
	 *  the head's sits at the neck). The bone's point if it has no body. */
	inline FVector BodyCentre(const USkeletalMeshComponent* Mesh, FName Bone)
	{
		if (!Mesh)
		{
			return FVector::ZeroVector;
		}
		const FBodyInstance* Body = Mesh->GetBodyInstance(Bone);
		return Body && Body->IsValidBodyInstance() ? Body->GetBodyBounds().GetCenter() : Mesh->GetBoneLocation(Bone);
	}

	/** Whether any spot light on the actor is shown (weapon light beam, flashlight beam). */
	inline bool IsBeamVisible(const AActor* Actor)
	{
		if (!Actor)
		{
			return false;
		}
		TInlineComponentArray<USpotLightComponent*> Lights(Actor);
		for (const USpotLightComponent* Light : Lights)
		{
			if (Light->IsVisible())
			{
				return true;
			}
		}
		return false;
	}

	/** Waits until Condition is true; after Timeout seconds (real time) it records an error and moves on. */
	class FWaitUntil : public IAutomationLatentCommand
	{
	public:
		FWaitUntil(FAutomationTestBase* InTest, FString InWhat, double InTimeout, TFunction<bool()> InCondition)
			: Test(InTest), What(MoveTemp(InWhat)), Timeout(InTimeout), Condition(MoveTemp(InCondition)) {}

		virtual bool Update() override
		{
			if (Condition())
			{
				return true;
			}
			if (GetCurrentRunTime() > Timeout)
			{
				Test->AddError(FString::Printf(TEXT("Timed out after %.0f s waiting for: %s"), Timeout, *What));
				return true;
			}
			return false;
		}

	private:
		FAutomationTestBase* Test;
		FString What;
		double Timeout;
		TFunction<bool()> Condition;
	};

	/** Waits this many seconds of game time. */
	class FWaitGameTime : public IAutomationLatentCommand
	{
	public:
		explicit FWaitGameTime(double InSeconds) : Seconds(InSeconds) {}

		virtual bool Update() override
		{
			if (EndTime < 0.0)
			{
				EndTime = GameTime() + Seconds;
			}
			return GameTime() >= EndTime || GetCurrentRunTime() > Seconds + 30.0;
		}

	private:
		double Seconds;
		double EndTime = -1.0;
	};

	/** Runs Function once, in order with the other latent commands. */
	inline void Step(TFunction<void()> Function)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Function]() { Function(); return true; }));
	}

	/** Ends the play session and waits until it's gone, so the next test starts clean. */
	inline void EndPlay(FAutomationTestBase* Test)
	{
		Step([]() { GEditor->RequestEndPlayMap(); });
		ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(Test, TEXT("the play session to end"), 30.0, []() { return GetPlayWorld() == nullptr; }));
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
