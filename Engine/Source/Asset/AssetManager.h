#pragma once
#include "CoreMinimal.h"

#include <string>
#include <filesystem>


class UStaticMesh;
class FTexture;

class ENGINE_API FAssetManager
{
public:
	static FAssetManager& Get();

	// 엔진 초기화 시 Assets 폴더 전체 스캔
	void Initialize(const FString& RootAssetPath);

	// 파일 이름으로 실제 상대/절대 경로를 찾아 반환
	FString FindAssetPath(const FString& FileName) const;

	TArray<FString> GetAssetsByExtension(const FString& Extension) const;

private:
	FAssetManager() = default;
	~FAssetManager() = default;

	FAssetManager(const FAssetManager&) = delete;
	FAssetManager& operator=(const FAssetManager&) = delete;

	// Asset Registry: [파일명(확장자 포함)] -> [프로젝트 기준 상대 경로]
	TMap<std::string, std::string> AssetPathRegistry;

	// std::unordered_map<std::string, UStaticMesh*> MeshCache;
};