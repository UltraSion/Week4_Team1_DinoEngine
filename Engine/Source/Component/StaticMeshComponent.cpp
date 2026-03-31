#include "StaticMeshComponent.h"
#include "Mesh/StaticMeshRenderData.h"
#include "Object/StaticMesh.h"
#include "Asset/AssetManager.h"
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
	if (StaticMesh) return StaticMesh->GetAssetPathFileName();
	return "";
}


// 덤으로 LoadStaticMesh도 깔끔하게 정리할 수 있습니다.
void UStaticMeshComponent::LoadStaticMesh(ID3D11Device* Device, const FString& AssetName)
{
	UStaticMesh* LoadedMesh = FAssetManager::Get().LoadStaticMesh(Device, AssetName);

	// 2. 방금 위에서 훌륭하게 작성하신 SetStaticMeshData를 호출하여 컴포넌트에 적용합니다.
	SetStaticMeshData(Device, LoadedMesh);
}
void UStaticMeshComponent::SetStaticMeshData(ID3D11Device* Device, UStaticMesh* InMesh)
{
	StaticMesh = InMesh;

	// 이전 메쉬가 쓰던 다이내믹 매테리얼 찌꺼기 초기화
	DynamicMaterialOwners.clear();

	if (!StaticMesh) return;

	FStaticMeshRenderData* RenderData = StaticMesh->GetStaticMeshAsset();
	if (!RenderData) return;
	// LoadStaticMesh에 있던 자동 매핑 로직 이식
	uint32 NumSlots = GetNumMaterials();
	for (uint32 i = 0; i < NumSlots; ++i)
	{
		bool bTextureLoaded = false;

		if (Device && i < RenderData->ImportedTexturePaths.size())
		{
			FString TexPath = RenderData->ImportedTexturePaths[i];
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
				FVector MatColor(0.5f, 0.5f, 0.5f);
				if (i < RenderData->ImportedDiffuseColors.size())
				{
					MatColor = RenderData->ImportedDiffuseColors[i];
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


void UStaticMeshComponent::LoadTexture(ID3D11Device* Device, const FString& FilePath)
{
	for (uint32 i = 0; i < GetNumMaterials(); ++i)
	{
		LoadTextureToSlot(Device, FilePath, i);
	}
}
void UStaticMeshComponent::LoadTextureToSlot(ID3D11Device* Device, const FString& FilePath, uint32 SlotIndex)
{
	if (SlotIndex >= GetNumMaterials()) return;

	// 1. 다이내믹 머티리얼 준비
	std::shared_ptr<FDynamicMaterial> DynamicMat;
	auto It = DynamicMaterialOwners.find(SlotIndex);

	if (It != DynamicMaterialOwners.end())
	{
		DynamicMat = It->second;
	}
	else
	{
		std::shared_ptr<FMaterial> BaseMat = FMaterialManager::Get().FindByName("M_StaticMesh");
		if (!BaseMat) BaseMat = FMaterialManager::Get().FindByName("M_Default_Texture");
		if (!BaseMat) return;

		DynamicMat = BaseMat->CreateDynamicMaterial();
		DynamicMaterialOwners[SlotIndex] = DynamicMat;
	}

	//AssetManager를 통해 텍스처 로드 (여기서 중복 파일 읽기가 완벽히 차단)
	ID3D11ShaderResourceView* srv = FAssetManager::Get().LoadTexture(Device, FilePath);
	if (!srv) return;

	//머티리얼 텍스처 세팅
	auto MT = std::make_shared<FMaterialTexture>();
	MT->TextureSRV = srv; // MT가 소멸될 때 srv->Release()를 부르지만, AssetManager가 원본을 쥐고 있으니 안전

	DynamicMat->SetMaterialTexture(MT);
	SetMaterial(SlotIndex, DynamicMat.get());
}

FMeshData* UStaticMeshComponent::GetMeshData() const
{
	if (StaticMesh && StaticMesh->GetStaticMeshAsset())
		return StaticMesh->GetStaticMeshAsset()->GetMeshData();
	return nullptr;
}

const TArray<FMeshSection>& UStaticMeshComponent::GetSections() const
{
	if (StaticMesh && StaticMesh->GetStaticMeshAsset())
		return StaticMesh->GetStaticMeshAsset()->GetSections();
	static TArray<FMeshSection> Empty;
	return Empty;
}

FMaterial* UStaticMeshComponent::GetMaterial(uint32 SlotIndex) const
{
	// 1순위: 오버라이드 매테리얼
	if (SlotIndex < OverrideMaterials.size() && OverrideMaterials[SlotIndex])
		return OverrideMaterials[SlotIndex];

	// 2순위: 렌더 데이터 기본 매테리얼
	if (StaticMesh && StaticMesh->GetStaticMeshAsset())
		return StaticMesh->GetStaticMeshAsset()->GetDefaultMaterial(SlotIndex);
	return nullptr;
}

uint32 UStaticMeshComponent::GetNumMaterials() const
{
	return static_cast<uint32>(GetSections().size());
}
