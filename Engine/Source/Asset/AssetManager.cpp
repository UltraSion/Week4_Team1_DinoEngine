#include "AssetManager.h"
#include "AssetRegistry.h"
#include "Object/StaticMesh.h"
#include "Object/ObjectFactory.h"
#include "Mesh/ObjManager.h"
#include "Mesh/StaticMeshRenderData.h"
#include "Debug/EngineLog.h"
#include "AssetCooker.h" 
#include <algorithm>

FAssetManager& FAssetManager::Get()
{
	static FAssetManager Instance;
	return Instance;
}


UStaticMesh* FAssetManager::LoadStaticMesh(ID3D11Device* Device, const FString& AssetName)
{
	// 레지스트리에게 에셋 정보(경로)를 물어봄
	FAssetData AssetData;
	if (!FAssetRegistry::Get().GetAssetByObjectPath(AssetName, AssetData))
	{
		UE_LOG("[AssetManager] Failed to find asset in registry: %s\n", AssetName.c_str());
		return nullptr;
	}

	// 이미 메모리에 캐싱되어 있다면 즉시 반환
	auto It = LoadedMeshes.find(AssetData.AssetPath);
	if (It != LoadedMeshes.end())
	{
		return It->second;
	}

	FString CookedPath = FAssetCooker::GetCookedPath(AssetData.AssetPath);

	FStaticMeshRenderData* RenderData = nullptr;

	//  쿡된 에셋이 유효하면 바이너리에서 로드 (OBJ 파싱 스킵)
	if (!FAssetCooker::NeedsCook(AssetData.AssetPath, CookedPath))
	{
		RenderData = FAssetCooker::LoadCookedStaticMesh(CookedPath);
	}
	if (!RenderData)
	{
		// 쿡 필요 또는 로드 실패 → 기존 파이프라인 (파싱 + 쿡)
		RenderData = FObjManager::LoadObjStaticMeshAsset(AssetData.AssetPath);

		if (!RenderData)
		{
			return nullptr;
		}

		//쿡 결과를 .dasset으로 저장 (다음번에는 바이너리 로드)
		FAssetCooker::SaveCookedStaticMesh(RenderData, AssetData.AssetPath, CookedPath);
	}
	RenderData->SetAssetPath(AssetData.AssetPath);
	//오브젝트 생성 및 GC 보호 플래그 설정
	UStaticMesh* NewMesh = FObjectFactory::ConstructObject<UStaticMesh>(nullptr, AssetData.AssetName);
	NewMesh->SetStaticMeshAsset(RenderData);
	NewMesh->AddFlags(EObjectFlags::Standalone); // 에셋은 GC가 마음대로 죽이지 못하게 보호

	// 5. 캐시에 저장 후 반환
	LoadedMeshes[AssetData.AssetPath] = NewMesh;
	UE_LOG("[AssetManager] Successfully loaded and cached: %s\n", AssetData.AssetPath.c_str());

	return NewMesh;
}