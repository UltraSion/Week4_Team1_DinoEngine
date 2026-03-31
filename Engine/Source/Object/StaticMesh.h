#pragma once
#include "Object/Object.h"

class FStaticMeshRenderData;

class ENGINE_API UStaticMesh : public UObject
{
public:
	DECLARE_RTTI(UStaticMesh, UObject)
	virtual ~UStaticMesh() override;
	void SetStaticMeshAsset(FStaticMeshRenderData* InAsset) { StaticMeshAsset = InAsset; }
	FStaticMeshRenderData* GetStaticMeshAsset() const { return StaticMeshAsset; }
	const FString& GetAssetPathFileName() const;

private:
	FStaticMeshRenderData* StaticMeshAsset = nullptr;
};
