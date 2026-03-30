#include "StaticMeshComponent.h"
#include "Mesh/StaticMeshRenderData.h"
#include "Core/Paths.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Object/Class.h"
#include "ThirdParty/stb_image.h"
#include "Renderer/Material.h"
#include "Mesh/ObjManager.h"
#include <filesystem>
#include <d3d11.h>


IMPLEMENT_RTTI(UStaticMeshComponent, UMeshComponent)
void UStaticMeshComponent::Initialize()
{
}



FString UStaticMeshComponent::GetStaticMeshAsset() const
{
	if (StaticMeshRenderData) return StaticMeshRenderData->GetAssetPath();
	return "";
}

void UStaticMeshComponent::LoadTexture(ID3D11Device* Device, const FString& FilePath)
{
	for (uint32 i = 0; i < GetNumMaterials(); ++i)
	{
		LoadTextureToSlot(Device, FilePath, i);
	}
}

void UStaticMeshComponent::SetStaticMeshData(ID3D11Device* Device, FStaticMeshRenderData* InMesh)
{
	StaticMeshRenderData = InMesh;

	// 이전 메쉬가 쓰던 다이내믹 매테리얼 찌꺼기 초기화
	DynamicMaterialOwners.clear();

	if (!StaticMeshRenderData || !Device) return;

	// LoadStaticMesh에 있던 자동 매핑 로직 이식
	uint32 NumSlots = GetNumMaterials();
	for (uint32 i = 0; i < NumSlots; ++i)
	{
		bool bTextureLoaded = false;

		if (i < StaticMeshRenderData->ImportedTexturePaths.size())
		{
			FString TexPath = StaticMeshRenderData->ImportedTexturePaths[i];
			if (!TexPath.empty())
			{
				//  fs::exists 대신, 우리가 새로 만든 안전한 ToWide()를 사용하여 파일 존재 여부 확인
				std::wstring WidePath = FPaths::ToWide(FPaths::ToAbsolutePath(TexPath));
				DWORD dwAttrib = GetFileAttributesW(WidePath.c_str());
				bool bExists = (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));

				if (bExists)
				{
					LoadTextureToSlot(Device, TexPath, i);
					bTextureLoaded = true;
				}
			}
		}

		// 텍스처가 없을 경우 기본 매테리얼 적용
		if (!bTextureLoaded)
		{
			std::shared_ptr<FMaterial> BaseMat = FMaterialManager::Get().FindByName("M_StaticMesh");
			if (!BaseMat) BaseMat = FMaterialManager::Get().FindByName("M_Default");

			if (BaseMat)
			{
				// 원본을 건드리지 않기 위해 다이내믹 인스턴스로 복제
				std::shared_ptr<FDynamicMaterial> DynamicMat = BaseMat->CreateDynamicMaterial();

				// 배열에서 색상(Kd) 꺼내오기
				FVector MatColor(1.0f, 1.0f, 1.0f);
				if (i < StaticMeshRenderData->ImportedDiffuseColors.size())
				{
					MatColor = StaticMeshRenderData->ImportedDiffuseColors[i];
				}
				DynamicMat->SetVectorParameter("ColorTint", FVector4(MatColor.X, MatColor.Y, MatColor.Z, 1.0f));

				// 머티리얼에 색상 전달
				// DynamicMat->SetColor(MatColor);
				DynamicMat->SetMaterialTexture(GDefaultWhiteTexture);

				SetMaterial(i, DynamicMat.get());
				DynamicMaterialOwners[i] = DynamicMat; // 찌꺼기 방지 맵에 저장
			}
		}
	}
}

// 덤으로 LoadStaticMesh도 깔끔하게 정리할 수 있습니다.
void UStaticMeshComponent::LoadStaticMesh(ID3D11Device* Device, const FString& FilePath)
{
	FStaticMeshRenderData* LoadedData = FObjManager::LoadObjStaticMeshAsset(FilePath);
	SetStaticMeshData(Device, LoadedData);
}
void UStaticMeshComponent::LoadTextureToSlot(ID3D11Device* Device, const FString& FilePath, uint32 SlotIndex)
{
	if (SlotIndex >= GetNumMaterials()) return; // 유효하지 않은 슬롯 방어

	// 1. Map에서 해당 슬롯의 다이내믹 머티리얼 찾기
	std::shared_ptr<FDynamicMaterial> DynamicMat;
	auto It = DynamicMaterialOwners.find(SlotIndex);

	if (It != DynamicMaterialOwners.end())
	{
		// 이미 이 슬롯에 할당된 머티리얼이 있으면 재사용
		DynamicMat = It->second;
	}
	else
	{
		// 없으면 기본 머티리얼을 찾아 복제 후 새로 생성
		std::shared_ptr<FMaterial> BaseMat = FMaterialManager::Get().FindByName("M_StaticMesh");
		if (!BaseMat)
			BaseMat = FMaterialManager::Get().FindByName("M_Default_Texture");
		if (!BaseMat)
			return;

		DynamicMat = BaseMat->CreateDynamicMaterial();
		DynamicMaterialOwners[SlotIndex] = DynamicMat; // Map에 저장
	}


	int width = 0, height = 0, channels = 0;
	FILE* f = nullptr;

	//std::filesystem::path를 거치지 않고, 안전한 FPaths::ToWide를 직접 사용
	std::wstring WidePath = FPaths::ToWide(FPaths::ToAbsolutePath(FilePath));
	_wfopen_s(&f, WidePath.c_str(), L"rb");

	unsigned char* data = nullptr;
	if (f)
	{
		data = stbi_load_from_file(f, &width, &height, &channels, STBI_rgb_alpha);
		fclose(f);
	}

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

	// 3. 머티리얼 텍스처 세팅
	auto MT = std::make_shared<FMaterialTexture>();
	MT->TextureSRV = srv;
	DynamicMat->SetMaterialTexture(MT);

	// ⭐ 4. 전체 반복문 대신, 인자로 받은 특정 SlotIndex에만 덮어쓰기!
	SetMaterial(SlotIndex, DynamicMat.get());
}

FMeshData* UStaticMeshComponent::GetMeshData() const
{
	if (StaticMeshRenderData)
		return StaticMeshRenderData->GetMeshData();
	return nullptr;
}

const TArray<FMeshSection>& UStaticMeshComponent::GetSections() const
{
	if (StaticMeshRenderData)
		return StaticMeshRenderData->GetSections();
	static TArray<FMeshSection> Empty;
	return Empty;
}

FMaterial* UStaticMeshComponent::GetMaterial(uint32 SlotIndex) const
{
	if (SlotIndex < OverrideMaterials.size() && OverrideMaterials[SlotIndex])
		return OverrideMaterials[SlotIndex];
	if (StaticMeshRenderData)
		return StaticMeshRenderData->GetDefaultMaterial(SlotIndex);
	return nullptr;
}

uint32 UStaticMeshComponent::GetNumMaterials() const
{
	return static_cast<uint32>(GetSections().size());
}
