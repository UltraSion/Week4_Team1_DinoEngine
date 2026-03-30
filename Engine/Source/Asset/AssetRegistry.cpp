#include "AssetRegistry.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include <filesystem>
FAssetRegistry& FAssetRegistry::Get()
{
	static FAssetRegistry Instance;
	return Instance;
    // TODO: insert return statement here
}

void FAssetRegistry::SearchAllAssets(const FString& RootDir)
{
	namespace fs = std::filesystem;
	if (!fs::exists(RootDir))
	{
		UE_LOG("[AssetRegistry] Root dir does not exist: %s\n", RootDir.c_str());
		return;
	}
	RegistryData.clear();
	for (const auto& entry : fs::recursive_directory_iterator(RootDir))
	{
		if (entry.is_regular_file())
		{
			FAssetData Data;
			Data.AssetName = entry.path().filename().string(); // 예: "wood.png"

			// 프로젝트 기준 상대 경로로 통일 (슬래시 변환)
			Data.AssetPath = fs::relative(entry.path(), FPaths::ProjectRoot()).generic_string();
			std::replace(Data.AssetPath.begin(), Data.AssetPath.end(), '\\', '/');

			// 확장자를 보고 AssetClass(타입) 분류
			std::string Ext = entry.path().extension().string();
			std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::tolower);

			if (Ext == ".obj") Data.AssetClass = "StaticMesh";
			else if (Ext == ".png" || Ext == ".jpg" || Ext == ".jpeg") Data.AssetClass = "Texture";
			else if (Ext == ".json") Data.AssetClass = "Material";
			else Data.AssetClass = "Unknown";

			// 대소문자 무시 검색을 위해 Key를 소문자로 저장
			std::string SearchKey = Data.AssetName;
			std::transform(SearchKey.begin(), SearchKey.end(), SearchKey.begin(), ::tolower);

			RegistryData[SearchKey] = Data;
		}
	}
	UE_LOG("[AssetRegistry] Scanned %zu assets.\n", RegistryData.size());
}
bool FAssetRegistry::GetAssetByObjectPath(const FString& ObjectName, FAssetData& OutData) const
{
	namespace fs = std::filesystem;
	// 입력값에 경로가 섞여 있어도 순수 파일명만 추출
	std::string SearchKey = fs::path(ObjectName).filename().string();
	std::transform(SearchKey.begin(), SearchKey.end(), SearchKey.begin(), ::tolower);

	auto It = RegistryData.find(SearchKey);
	if (It != RegistryData.end())
	{
		OutData = It->second;
		return true;
	}
	return false;
}

TArray<FAssetData> FAssetRegistry::GetAssetsByClass(const FString& AssetClass) const
{
	TArray<FAssetData> Result;
	for (const auto& Pair : RegistryData)
	{
		if (Pair.second.AssetClass == AssetClass)
		{
			Result.push_back(Pair.second);
		}
	}
	return Result;
}