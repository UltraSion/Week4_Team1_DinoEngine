#include "StaticMesh.h"
#include "Core/Paths.h"
#include "Primitive/PrimitiveObj.h"
#include <fstream>
TMap<FString, std::shared_ptr<FStaticMesh>> FStaticMesh::Cache;

bool FStaticMesh::LoadFromOBJ(const FString& FilePath)
{
	AssetPath = FilePath;

	// CPrimitiveBase의 캐시를 먼저 확인
	auto Cached = CPrimitiveBase::GetCached(FilePath);
	if (Cached)
	{
		MeshData = Cached;
	}
	else
	{
		FPrimitiveObj Loader(FilePath);
		MeshData = CPrimitiveBase::GetCached(FilePath);
		if (!MeshData) return false;
	}

	// DefaultMaterials 슬롯 확보
	uint32 MaxSlot = 0;
	for (const auto& S : MeshData->Sections)
		if (S.MaterialIndex > MaxSlot) MaxSlot = S.MaterialIndex;
	DefaultMaterials.resize(MaxSlot + 1, nullptr);

	return true;
}

std::shared_ptr<FStaticMesh> FStaticMesh::GetOrLoad(const FString& FilePath)
{
	// 캐시에 있으면 바로 반환
	auto It = Cache.find(FilePath);
	if (It != Cache.end())
		return It->second;

	// 없으면 새로 로드
	auto SM = std::make_shared<FStaticMesh>();
	if (SM->LoadFromOBJ(FilePath))
	{
		Cache[FilePath] = SM;
		return SM;
	}

	// 로드 실패
	return nullptr;
}

std::shared_ptr<FStaticMesh> FStaticMesh::GetCached(const FString& Key)
{
	auto It = Cache.find(Key);
	if (It != Cache.end())
		return It->second;
	return nullptr;
}

void FStaticMesh::ClearCache()
{
	Cache.clear();
}

TArray<FString> FStaticMesh::GetCachedKeys()
{
	TArray<FString> Keys;
	for (const auto& Pair : Cache)
		Keys.push_back(Pair.first);
	return Keys;
}
