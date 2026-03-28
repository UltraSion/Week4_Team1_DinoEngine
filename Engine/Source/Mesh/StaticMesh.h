#pragma once
#include "Mesh.h"

class ENGINE_API FStaticMesh : public FMesh
{
public:
	FStaticMesh() = default;

	bool LoadFromOBJ(const FString& FilePath);

	static std::shared_ptr<FStaticMesh> GetOrLoad(const FString& FilePath);
	static std::shared_ptr<FStaticMesh> GetCached(const FString& Key);
	static void ClearCache();
	static TArray<FString> GetCachedKeys();

private:
	static TMap<FString, std::shared_ptr<FStaticMesh>> Cache;
};