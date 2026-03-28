#pragma once
#include "PrimitiveComponent.h"
#include "Mesh/Mesh.h"
#include "Math/Frustum.h"
#include <memory>
class FMaterial;
struct ID3D11Device;

struct FBoxSphereBounds;
class ENGINE_API UMeshComponent : public UPrimitiveComponent
{
	DECLARE_RTTI(UMeshComponent, UPrimitiveComponent)

	virtual FMeshData* GetMeshData() const { return nullptr; }
	virtual const TArray<FMeshSection>& GetSections() const;
	virtual uint32                    GetNumMaterials() const { return 0; }


	virtual FMaterial* GetMaterial(uint32 SlotIndex) const;
	void SetMaterial(uint32 SlotIndex, FMaterial* Mat);


	virtual FBoxSphereBounds GetWorldBounds() const;
protected:

	TArray<FMaterial*> OverrideMaterials;
};