#pragma once
#include"MeshComponent.h"
#include"Mesh/StaticMesh.h"
class ENGINE_API UStaticMeshComponent : public UMeshComponent
{
public:
	DECLARE_RTTI(UStaticMeshComponent, UMeshComponent)

	void Initialize();

	void LoadStaticMesh(ID3D11Device* Device, const FString& FilePath);
	FString GetStaticMeshAsset() const;

	void LoadTexture(ID3D11Device* Device, const FString& FilePath);
	void SetStaticMesh(FStaticMesh* InMesh);
	FStaticMesh* GetStaticMesh() const { return StaticMesh; }
	// UMeshComponent 인터페이스 override
	FMeshData* GetMeshData() const override;
	const TArray<FMeshSection>& GetSections() const ;
	FMaterial* GetMaterial(uint32 SlotIndex) const override;
	uint32 GetNumMaterials() const override;

private:
	std::unique_ptr<FMaterial> DynamicMaterialOwner;
	FStaticMesh* StaticMesh = nullptr;
};