#include "StaticMesh.h"
#include "Object/Class.h"
#include "Mesh/StaticMeshRenderData.h"

IMPLEMENT_RTTI(UStaticMesh, UObject)
const FString& UStaticMesh::GetAssetPathFileName() const
{
	if (StaticMeshAsset)
		return StaticMeshAsset->GetAssetPath();
	static FString Empty;
	return Empty;
}
