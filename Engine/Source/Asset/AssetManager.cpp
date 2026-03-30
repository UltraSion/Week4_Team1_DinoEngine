#include "AssetManager.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include <algorithm>

FAssetManager& FAssetManager::Get()
{
	static FAssetManager Instance;
	return Instance;
}

void FAssetManager::Initialize(const FString& RootAssetPath)
{
	namespace fs = std::filesystem;
	if (!fs::exists(RootAssetPath))
	{
		UE_LOG("[AssetManager] Root asset path does not exist: %s\n", RootAssetPath.c_str());
		return;
	}

	AssetPathRegistry.clear();

	// recursive_directory_iterator를 사용해 하위 폴더까지 싹 다 뒤집니다.
	for (const auto& entry : fs::recursive_directory_iterator(RootAssetPath))
	{
		if (entry.is_regular_file())
		{
			std::string FileName = entry.path().filename().string();
			// 프로젝트 루트 기준의 상대 경로로 저장
			std::string RelPath = fs::relative(entry.path(), FPaths::ProjectRoot()).generic_string();

			// 이름 충돌 정책: 가장 먼저 찾은 것을 등록 (또는 폴더 깊이별 우선순위 지정 가능)
			if (AssetPathRegistry.find(FileName) == AssetPathRegistry.end())
			{
				AssetPathRegistry[FileName] = RelPath;
			}
		}
	}
	UE_LOG("[AssetManager] Initialized. Registered %zu assets.\n", AssetPathRegistry.size());
}

FString FAssetManager::FindAssetPath(const FString& FileName) const
{
	namespace fs = std::filesystem;
	// 만약 입력값에 경로가 섞여 있다면 순수 파일명만 추출 ("Textures/wood.png" -> "wood.png")
	std::string PureFileName = fs::path(FileName).filename().string();

	auto It = AssetPathRegistry.find(PureFileName);
	if (It != AssetPathRegistry.end())
	{
		return It->second; // 찾은 실제 경로 반환
	}

	UE_LOG("[AssetManager] Failed to find asset in registry: %s\n", PureFileName.c_str());
	return "";
}

TArray<FString> FAssetManager::GetAssetsByExtension(const FString& Extension) const
{
	TArray<FString> Result;
	std::string LowerExt = Extension;
	std::transform(LowerExt.begin(), LowerExt.end(), LowerExt.begin(), ::tolower);

	for (const auto& Pair : AssetPathRegistry)
	{
		namespace fs = std::filesystem;
		std::string Ext = fs::path(Pair.second).extension().string();
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::tolower);

		if (Ext == LowerExt)
		{
			Result.push_back(Pair.second);
		}
	}
	return Result;
}