#pragma once
#include "CoreMinimal.h"

#include"MeshComponent.h"
#include"Mesh/StaticMeshRenderData.h"
class FDynamicMaterial;
class ENGINE_API UStaticMeshComponent : public UMeshComponent
{
public:
	DECLARE_RTTI(UStaticMeshComponent, UMeshComponent)

	void Initialize();

	void LoadStaticMesh(ID3D11Device* Device, const FString& FilePath);
	FString GetStaticMeshAsset() const;

	void LoadTexture(ID3D11Device* Device, const FString& FilePath);
	void SetStaticMeshData(ID3D11Device* Device, FStaticMeshRenderData* InMesh);
	void LoadTextureToSlot(ID3D11Device* Device, const FString& FilePath, uint32 SlotIndex);
	FStaticMeshRenderData* GetStaticMesh() const { return StaticMeshRenderData; }
	// UMeshComponent 인터페이스 override
	FMeshData* GetMeshData() const override;
	const TArray<FMeshSection>& GetSections() const override;
	FMaterial* GetMaterial(uint32 SlotIndex) const override;
	uint32 GetNumMaterials() const override;

private:
	TMap<uint32, std::shared_ptr<FDynamicMaterial>> DynamicMaterialOwners;
	FStaticMeshRenderData* StaticMeshRenderData = nullptr;
};