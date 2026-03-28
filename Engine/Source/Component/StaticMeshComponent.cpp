#include "StaticMeshComponent.h"
#include "Mesh/StaticMesh.h"
#include "Core/Paths.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Object/Class.h"
#include "ThirdParty/stb_image.h"
#include <filesystem>
#include <d3d11.h>


IMPLEMENT_RTTI(UStaticMeshComponent, UMeshComponent)
void UStaticMeshComponent::Initialize()
{
}

void UStaticMeshComponent::LoadStaticMesh(ID3D11Device* Device, const FString& FilePath)
{
	auto SM = FStaticMesh::GetOrLoad(FilePath);
	if (!SM) return;

	StaticMesh = SM.get();

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
				StaticMesh->SetDefaultMaterial(i, DefaultMat.get());  
		}
	}
}

FString UStaticMeshComponent::GetStaticMeshAsset() const
{
	if (StaticMesh) return StaticMesh->GetAssetPath();
	return "";
}

void UStaticMeshComponent::LoadTexture(ID3D11Device* Device, const FString& FilePath)
{
	if (!DynamicMaterialOwner)
	{
		std::shared_ptr<FMaterial> BaseMat = FMaterialManager::Get().FindByName("M_StaticMesh");
		if (!BaseMat) 
			BaseMat = FMaterialManager::Get().FindByName("M_Default_Texture");
		if (!BaseMat) 
			return;
		DynamicMaterialOwner = BaseMat->CreateDynamicMaterial();
	}

	int width = 0, height = 0, channels = 0;
	unsigned char* data = stbi_load(
		FPaths::ToAbsolutePath(FilePath).c_str(),
		&width, &height, &channels, STBI_rgb_alpha);
	if (!data) return;


	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // ⭐ diffuse면 SRGB 추천
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = data;
	initData.SysMemPitch = width * 4;


	ID3D11Texture2D* texture = nullptr;
	HRESULT hr = Device->CreateTexture2D(&desc, &initData, &texture);
	if (FAILED(hr)) { stbi_image_free(data); return; }

	ID3D11ShaderResourceView* srv = nullptr;
	hr = Device->CreateShaderResourceView(texture, nullptr, &srv);

	// Texture는 SRV 만들었으면 바로 버려도 됨
	texture->Release();
	if (FAILED(hr)) { stbi_image_free(data); return; }
	stbi_image_free(data);

	auto MT = std::make_shared<FMaterialTexture>();
	MT->TextureSRV = srv;
	DynamicMaterialOwner->SetMaterialTexture(MT);

	if (StaticMesh)
	{
		for (uint32 i = 0; i < StaticMesh->GetNumMaterialSlots(); ++i)
			StaticMesh->SetDefaultMaterial(i, DynamicMaterialOwner.get());
	}
}

FMeshData* UStaticMeshComponent::GetMeshData() const
{
	if (StaticMesh)
		return StaticMesh->GetMeshData();
	return nullptr;
}

const TArray<FMeshSection>& UStaticMeshComponent::GetSections() const
{
	if (StaticMesh)
		return StaticMesh->GetSections();
	static TArray<FMeshSection> Empty;
	return Empty;
}

FMaterial* UStaticMeshComponent::GetMaterial(uint32 SlotIndex) const
{
	if (SlotIndex < OverrideMaterials.size() && OverrideMaterials[SlotIndex])
		return OverrideMaterials[SlotIndex];
	if (StaticMesh)
		return StaticMesh->GetDefaultMaterial(SlotIndex);
	return nullptr;
}

uint32 UStaticMeshComponent::GetNumMaterials() const
{
	if (StaticMesh)
		return StaticMesh->GetNumMaterialSlots();
	return 0;
}
