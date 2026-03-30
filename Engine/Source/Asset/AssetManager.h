#pragma once
#include "CoreMinimal.h"
#include "EngineAPI.h"
#include <string>
#include <filesystem>


class UStaticMesh;
struct ID3D11Device;

class ENGINE_API FAssetManager
{
public:
	static FAssetManager& Get();


	// 에셋 이름("sword.obj" 등)을 받아서, 메모리에 로드된 메쉬를 반환 (없으면 로드함)
	UStaticMesh* LoadStaticMesh(ID3D11Device* Device, const FString& AssetName);

private:
	FAssetManager() = default;
	~FAssetManager() = default;

	FAssetManager(const FAssetManager&) = delete;
	FAssetManager& operator=(const FAssetManager&) = delete;

	// 메모리에 올라간 실제 오브젝트들을 들고 있음 (메모리 캐싱)
	TMap<std::string, UStaticMesh*> LoadedMeshes;


};