#include "MeshComponent.h"
#include "Object/Class.h"

#include "PrimitiveComponent.h"
#include "Core/Paths.h"
#include "ThirdParty/stb_image.h"
#include <d3d11.h>


IMPLEMENT_RTTI(UMeshComponent, USceneComponent)
FMaterial* UMeshComponent::GetMaterial(uint32 SlotIndex) const
{
	if (SlotIndex < OverrideMaterials.size() && OverrideMaterials[SlotIndex])
		return OverrideMaterials[SlotIndex];

	if (Mesh)
		return Mesh->GetDefaultMaterial(SlotIndex);
	return nullptr;
}

void UMeshComponent::SetMaterial(uint32 SlotIndex, FMaterial* Mat)
{

	if (SlotIndex >= OverrideMaterials.size())
		OverrideMaterials.resize(SlotIndex + 1, nullptr);
	OverrideMaterials[SlotIndex] = Mat;
}

uint32 UMeshComponent::GetNumMaterials() const
{
	if (Mesh) 
		return Mesh->GetNumMaterialSlots();
	return 0;
}

FBoxSphereBounds UMeshComponent::GetWorldBounds() const
{
	FVector Center = GetWorldLocation();

	if (!Mesh || !Mesh->GetMeshData())
		return { Center, 1.f, FVector(1, 1, 1) };


	FMeshData* MD = Mesh->GetMeshData();
	FVector LocalBoxExtent = (MD->GetMaxCoord() - MD->GetMinCoord()) * 0.5;
	Center = GetWorldTransform().TransformPosition(MD->GetCenterCoord());
	FVector S = GetWorldTransform().GetScaleVector();
	FVector ScaledExtent = FVector::Multiply(LocalBoxExtent, S);
	FMatrix AbsR = FMatrix::Abs(GetWorldTransform().GetRotationMatrix());

	FVector WorldBoxExtent;
	WorldBoxExtent.X = AbsR.M[0][0] * ScaledExtent.X + AbsR.M[1][0] * ScaledExtent.Y + AbsR.M[2][0] * ScaledExtent.Z;
	WorldBoxExtent.Y = AbsR.M[0][1] * ScaledExtent.X + AbsR.M[1][1] * ScaledExtent.Y + AbsR.M[2][1] * ScaledExtent.Z;
	WorldBoxExtent.Z = AbsR.M[0][2] * ScaledExtent.X + AbsR.M[1][2] * ScaledExtent.Y + AbsR.M[2][2] * ScaledExtent.Z;

	return { Center, WorldBoxExtent.Size(), WorldBoxExtent };
}
