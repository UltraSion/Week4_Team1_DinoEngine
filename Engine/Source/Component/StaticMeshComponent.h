#pragma once
#include"MeshComponent.h"

class ENGINE_API UStaticMeshComponent : public UMeshComponent
{
public:
	DECLARE_RTTI(UStaticMeshComponent, UMeshComponent)

	void Initialize();

	void LoadStaticMesh(ID3D11Device* Device, const FString& FilePath);
	FString GetStaticMeshAsset() const;

	void LoadTexture(ID3D11Device* Device, const FString& FilePath);
private:
	std::unique_ptr<FMaterial> DynamicMaterialOwner;
};