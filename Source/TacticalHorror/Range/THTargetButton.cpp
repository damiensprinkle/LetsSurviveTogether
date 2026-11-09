#include "Range/THTargetButton.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Player/THCharacter.h"
#include "Range/THTargetCarrier.h"

void UTHTargetButtonComponent::ServerPressed(ATHCharacter* By)
{
	if (Carrier)
	{
		Carrier->ServerStartMoving(Direction, By);
	}
}

void UTHTargetButtonComponent::ServerReleased(ATHCharacter* By)
{
	if (Carrier)
	{
		Carrier->ServerStopMoving(By);
	}
}

ATHTargetButton::ATHTargetButton()
{
	bReplicates = true;

	PressArea = CreateDefaultSubobject<UBoxComponent>(TEXT("PressArea"));
	PressArea->SetBoxExtent(FVector(4.0, 10.0, 10.0));
	PressArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PressArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	PressArea->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SetRootComponent(PressArea);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(PressArea);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(PressArea);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(3.0f);
	Label->SetTextRenderColor(FColor::White);

	Button = CreateDefaultSubobject<UTHTargetButtonComponent>(TEXT("Button"));
}
