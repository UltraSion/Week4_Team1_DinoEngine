#pragma once
#include "ObjInfo.h"
#include "Mesh.h"
class FStaticMeshRenderData;

class ENGINE_API FObjImporter
{
public:
	// .obj 파싱 -> Raw Data
	static bool ParseObj(const FString& FilePath, FObjInfo& OutInfo);

	// .mtl 파싱 -> MaterialInfo 배열
	static bool ParseMtl(const FString& FilePath, TArray<FObjMaterialInfo>& OutMaterials);

	// Raw > Cooked (FStaticMesh)
	static FStaticMeshRenderData* Cook(const FObjInfo& Info);
};
