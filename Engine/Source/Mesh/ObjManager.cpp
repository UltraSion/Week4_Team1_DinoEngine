#include "ObjManager.h"
#include "Mesh/StaticMeshRenderData.h"
#include "Object/StaticMesh.h"
#include "Mesh/ObjImporter.h"
#include "Mesh/ObjInfo.h"
#include "Object/ObjectIterator.h"
#include "Object/ObjectFactory.h"

TMap<FString, std::shared_ptr<FStaticMeshRenderData>> FObjManager::ObjStaticMeshMap;

FStaticMeshRenderData* FObjManager::LoadObjStaticMeshAsset(const FString& PathFileName)
{
	auto It = ObjStaticMeshMap.find(PathFileName);
	if (It != ObjStaticMeshMap.end())
		return It->second.get();

	FObjInfo Info;
	if (!FObjImporter::ParseObj(PathFileName, Info))
		return nullptr;

	FStaticMeshRenderData* Cooked = FObjImporter::Cook(Info);
	if (!Cooked) return nullptr;
	Cooked->SetAssetPath(PathFileName);

	ObjStaticMeshMap[PathFileName] = std::shared_ptr<FStaticMeshRenderData>(Cooked);
	return Cooked;
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName)
{
	for (TObjectIterator<UStaticMesh> It; It; ++It)
	{
		if ((*It)->GetAssetPathFileName() == PathFileName)
			return *It;
	}

	FStaticMeshRenderData* Asset = LoadObjStaticMeshAsset(PathFileName);
	if (!Asset) return nullptr;

	UStaticMesh* NewObj = FObjectFactory::ConstructObject<UStaticMesh>();
	NewObj->SetStaticMeshAsset(Asset);
	return NewObj;
}

void FObjManager::Clear()
{
	ObjStaticMeshMap.clear();
}
