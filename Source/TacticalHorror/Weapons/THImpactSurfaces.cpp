#include "Weapons/THImpactSurfaces.h"

#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

bool UTHImpactSurfaces::SetEffectCollisionChannel(UNiagaraComponent* Component, ECollisionChannel Channel)
{
	static const FName UserName(TEXT("User.CollisionChannel"));
	if (!Component)
	{
		return false;
	}
	FNiagaraUserRedirectionParameterStore& Store = Component->GetOverrideParameters();
	for (const FNiagaraVariableWithOffset& Variable : Store.ReadParameterVariables())
	{
		if (Variable.GetName() == UserName && Variable.GetSizeInBytes() == sizeof(int32))
		{
			const int32 Value = static_cast<int32>(Channel);
			Store.SetParameterValue(Value, FNiagaraVariable(Variable.GetType(), Variable.GetName()));
			return true;
		}
	}
	return false;
}

const FTHSurfaceImpact& UTHImpactSurfaces::GetImpact(EPhysicalSurface Surface) const
{
	const FTHSurfaceImpact* Impact = Surfaces.Find(Surface);
	return Impact ? *Impact : Default;
}

UMaterialInterface* UTHImpactSurfaces::PickDecal(EPhysicalSurface Surface) const
{
	return PickRandom(GetImpact(Surface).Decals);
}
