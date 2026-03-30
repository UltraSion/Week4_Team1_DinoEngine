#include "AssetManager.h"
#include "AssetRegistry.h"
#include "Object/StaticMesh.h"
#include "Object/ObjectFactory.h"
#include "Mesh/ObjManager.h"
#include "Debug/EngineLog.h"
#include <algorithm>

FAssetManager& FAssetManager::Get()
{
	static FAssetManager Instance;
	return Instance;
}


UStaticMesh* FAssetManager::LoadStaticMesh(ID3D11Device* Device, const FString& AssetName)
{
	// 1. 레지스트리에게 에셋 정보(경로)를 물어봄
	FAssetData AssetData;
	if (!FAssetRegistry::Get().GetAssetByObjectPath(AssetName, AssetData))
	{
		UE_LOG("[AssetManager] Failed to find asset in registry: %s\n", AssetName.c_str());
		return nullptr;
	}

	// 2. 이미 메모리에 캐싱되어 있다면 즉시 반환 (지연 로딩의 핵심!)
	auto It = LoadedMeshes.find(AssetData.AssetPath);
	if (It != LoadedMeshes.end())
	{
		return It->second;
	}

	// 3. 캐시에 없으므로 파일 로드 및 파싱 (Cook)
	FStaticMeshRenderData* RenderData = FObjManager::LoadObjStaticMeshAsset(AssetData.AssetPath);
	if (!RenderData)
	{
		return nullptr;
	}

	// 4. 오브젝트 생성 및 GC 보호 플래그 설정
	UStaticMesh* NewMesh = FObjectFactory::ConstructObject<UStaticMesh>(nullptr, AssetData.AssetName);
	NewMesh->SetStaticMeshAsset(RenderData);
	NewMesh->AddFlags(EObjectFlags::Standalone); // 에셋은 GC가 마음대로 죽이지 못하게 보호

	// 5. 캐시에 저장 후 반환
	LoadedMeshes[AssetData.AssetPath] = NewMesh;
	UE_LOG("[AssetManager] Successfully loaded and cached: %s\n", AssetData.AssetPath.c_str());

	return NewMesh;
}