#include "ObjManager.h"
#include "Mesh/StaticMeshRenderData.h"
#include "Object/StaticMesh.h"
#include "Mesh/ObjImporter.h"
#include "Mesh/ObjInfo.h"


TMap<FString, std::shared_ptr<FStaticMeshRenderData>> FObjManager::ObjStaticMeshMap;

FStaticMeshRenderData* FObjManager::LoadObjStaticMeshAsset(const FString& PathFileName)
{
	FString CacheKey = PathFileName;
#if IS_OBJ_VIEWER
	CacheKey += "|";
	CacheKey += FObjImporter::BuildImportAxisMappingKey(FObjImporter::GetImportAxisMapping());
#endif

	auto It = ObjStaticMeshMap.find(CacheKey);
	if (It != ObjStaticMeshMap.end())
		return It->second.get();

	FObjInfo Info;
	if (!FObjImporter::ParseObj(PathFileName, Info))
		return nullptr;

	FStaticMeshRenderData* Cooked = FObjImporter::Cook(Info);
	if (!Cooked) return nullptr;
	Cooked->SetAssetPath(PathFileName);

	ObjStaticMeshMap[CacheKey] = std::shared_ptr<FStaticMeshRenderData>(Cooked);
	return Cooked;
}


void FObjManager::Clear()
{
	ObjStaticMeshMap.clear();
}
