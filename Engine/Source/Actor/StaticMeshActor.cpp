#include "StaticMeshActor.h"
#include "Object/Class.h"
#include "Component/RandomColorComponent.h"
#include "Component/StaticMeshComponent.h"

IMPLEMENT_RTTI(AStaticMeshActor, AActor)
void AStaticMeshActor::PostSpawnInitialize()
{
	StaticMeshComponent = FObjectFactory::ConstructObject<UStaticMeshComponent>(this);
	AddOwnedComponent(StaticMeshComponent);
	if (bUseRandomColor)
	{
		RandomColorComponent = FObjectFactory::ConstructObject<URandomColorComponent>(this);
		AddOwnedComponent(RandomColorComponent);
	}
	AActor::PostSpawnInitialize();
}
void AStaticMeshActor::LoadStaticMesh(ID3D11Device* Device, const FString& FilePath)
{
	if (StaticMeshComponent && Device)
		StaticMeshComponent->LoadStaticMesh(Device, FilePath);
}
