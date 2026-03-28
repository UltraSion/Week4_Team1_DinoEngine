#pragma once
#include"MeshComponent.h"
#include"Mesh/StaticMeshRenderData.h"
class ENGINE_API UStaticMeshComponent : public UMeshComponent
{
public:
	DECLARE_RTTI(UStaticMeshComponent, UMeshComponent)

	void Initialize();

	void LoadStaticMesh(ID3D11Device* Device, const FString& FilePath);
	FString GetStaticMeshAsset() const;

	void LoadTexture(ID3D11Device* Device, const FString& FilePath);
	void SetStaticMeshData(FStaticMeshRenderData* InMesh);
	FStaticMeshRenderData* GetStaticMesh() const { return StaticMeshRenderData; }
	// UMeshComponent 인터페이스 override
	FMeshData* GetMeshData() const override;
	const TArray<FMeshSection>& GetSections() const override;
	FMaterial* GetMaterial(uint32 SlotIndex) const override;
	uint32 GetNumMaterials() const override;

private:
	std::unique_ptr<FMaterial> DynamicMaterialOwner;
	FStaticMeshRenderData* StaticMeshRenderData = nullptr;
};