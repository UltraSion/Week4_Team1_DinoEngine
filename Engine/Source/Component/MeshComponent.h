#pragma once
#include "SceneComponent.h"
#include "Mesh/Mesh.h"
#include "Math/Frustum.h"
#include <memory>
class FMaterial;
struct ID3D11Device;

struct FBoxSphereBounds;
class ENGINE_API UMeshComponent : public USceneComponent
{
	DECLARE_RTTI(UMeshComponent, USceneComponent)

	std::shared_ptr<FMesh> GetMesh() const { return Mesh; }
	void SetMesh(const std::shared_ptr<FMesh>& InMesh) { Mesh = InMesh; }

	FMaterial* GetMaterial(uint32 SlotIndex) const;
	void SetMaterial(uint32 SlotIndex, FMaterial* Mat);
	uint32 GetNumMaterials() const;

	virtual FBoxSphereBounds GetWorldBounds() const;
protected:
	std::shared_ptr<FMesh> Mesh;
	TArray<FMaterial*> OverrideMaterials;
};