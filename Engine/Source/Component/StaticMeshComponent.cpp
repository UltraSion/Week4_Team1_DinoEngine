#include "StaticMeshComponent.h"
#include "Mesh/StaticMesh.h"
#include "Core/Paths.h"
#include "Renderer/MaterialManager.h"
#include <filesystem>
#include <fstream>
void UStaticMeshComp::Initialize()
{
}

void UStaticMeshComp::LoadStaticMesh(ID3D11Device* Device, const FString& FilePath)
{
	auto SM = FStaticMesh::GetOrLoad(FilePath);
	if (!SM) return;

	Mesh = SM;

	// 텍스처 로드 (.obj → .png)
	std::filesystem::path PngPath = FilePath;
	PngPath.replace_extension(".png");
	if (std::filesystem::exists(FPaths::ToAbsolutePath(PngPath.string())))
	{
		LoadTexture(Device, PngPath.string());
	}
	else
	{
		// 텍스처 없으면 기본 머티리얼
		std::shared_ptr<FMaterial> DefaultMat = FMaterialManager::Get().FindByName("M_StaticMesh");
		if (!DefaultMat) DefaultMat = FMaterialManager::Get().FindByName("M_Default");
		if (DefaultMat)
		{
			for (uint32 i = 0; i < GetNumMaterials(); ++i)
				Mesh->SetDefaultMaterial(i, DefaultMat.get());
		}
	}
}

FString UStaticMeshComp::GetStaticMeshAsset() const
{
	return FString();
}

void UStaticMeshComp::LoadTexture(ID3D11Device* Device, const FString& FilePath)
{
}
