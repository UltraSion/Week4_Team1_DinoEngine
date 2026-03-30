
#include "ObjImporter.h"
#include "ObjInfo.h"
#include "Core/Paths.h"
#include "StaticMeshRenderData.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include "Asset/AssetRegistry.h"
#include "Asset/AssetManager.h"
// ParseObj — 기존 PrimitiveObj::LoadObj에서 변환
bool FObjImporter::ParseObj(const FString& FilePath, FObjInfo& OutInfo)
{
	std::ifstream File(std::filesystem::path(FPaths::ToAbsolutePath(FilePath)).wstring());

	if (!File.is_open()) return false;
	OutInfo.ObjDirectory = std::filesystem::path(FPaths::ToAbsolutePath(FilePath)).parent_path().string();
	std::string Line;
	while (std::getline(File, Line))
	{
		std::stringstream SS(Line);
		std::string Type;
		SS >> Type;

		if (Type == "v")
		{
			FVector Pos;
			SS >> Pos.X >> Pos.Y >> Pos.Z;
			OutInfo.Positions.push_back(Pos);
		}
		else if (Type == "vt")
		{
			FVector2 UV;
			SS >> UV.X >> UV.Y;
			UV.Y = 1.0f - UV.Y;
			OutInfo.UVs.push_back(UV);
		}
		else if (Type == "vn")
		{
			FVector N;
			SS >> N.X >> N.Y >> N.Z;
			OutInfo.Normals.push_back(N);
		}
		else if (Type == "usemtl")
		{
			FString MatName;
			SS >> MatName;
			OutInfo.MaterialNames.push_back(MatName);
			OutInfo.SectionStartIndices.push_back(
				(uint32)OutInfo.PositionIndices.size());
		}
		else if (Type == "mtllib")
		{
			std::string MtlFileName;
			// 띄어쓰기가 있는 파일명을 위해 안전하게 파싱
			std::getline(SS >> std::ws, MtlFileName);
			FAssetData MtlData;
			FString MtlPath;
			// AssetManager에게 전체 프로젝트에서 .mtl을 찾아달라고 요청
			if (FAssetRegistry::Get().GetAssetByObjectPath(MtlFileName, MtlData))
			{
				// 찾았다면 AssetData 안의 상대경로를 절대경로로 변환
				MtlPath = FPaths::ToAbsolutePath(MtlData.AssetPath);
			}
			else
			{
				// 못 찾았다면 기존 폴백 로직
				std::filesystem::path ObjDir = std::filesystem::path(FPaths::ToAbsolutePath(FilePath)).parent_path();
				MtlPath = (ObjDir / MtlFileName).string();
			}
			ParseMtl(MtlPath, OutInfo.Materials);
		}

		else if (Type == "f")
		{
		
			std::string VStr;

			//  Normal 인덱스도 파싱 (v/vt/vn 형식 지원)
			struct FIndex
			{
				uint32 PosIdx = 0;
				uint32 UVIdx = 0;
				uint32 NrmIdx = 0;
				bool bHasUV = false;
				bool bHasNrm = false;
			};

			std::vector<FIndex> Face;

			while (SS >> VStr)
			{
				std::stringstream VSS(VStr);
				std::string S0, S1, S2;

				std::getline(VSS, S0, '/');

				FIndex Idx{};
				Idx.PosIdx = std::stoi(S0) - 1;

				if (std::getline(VSS, S1, '/'))
				{
					if (!S1.empty()) { Idx.UVIdx = std::stoi(S1) - 1; Idx.bHasUV = true; }
					if (std::getline(VSS, S2, '/'))
					{
						if (!S2.empty()) { Idx.NrmIdx = std::stoi(S2) - 1; Idx.bHasNrm = true; }
					}
				}
				Face.push_back(Idx);
			}

			// Fan triangulation
			for (size_t i = 1; i + 1 < Face.size(); ++i)
			{
				FIndex Tri[3] = { Face[0], Face[i], Face[i + 1] };

				for (int j = 0; j < 3; ++j)
				{
					OutInfo.PositionIndices.push_back(Tri[j].PosIdx);
					OutInfo.UVIndices.push_back(Tri[j].bHasUV ? Tri[j].UVIdx : UINT32_MAX);
					OutInfo.NormalIndices.push_back(Tri[j].bHasNrm ? Tri[j].NrmIdx : UINT32_MAX);
				}
			}
		}
	}
	return true;
}
// FObjImporter::ParseMtl
bool FObjImporter::ParseMtl(const FString& FilePath, TArray<FObjMaterialInfo>& OutMaterials)
{
	std::ifstream File(std::filesystem::path(FPaths::ToAbsolutePath(FilePath)).wstring());
	if (!File.is_open()) return false;

	FObjMaterialInfo* Current = nullptr;

	std::string Line;
	while (std::getline(File, Line))
	{
		std::stringstream SS(Line);
		std::string Type;
		SS >> Type;

		if (Type == "newmtl")
		{
			OutMaterials.push_back({});
			Current = &OutMaterials.back();
			SS >> Current->Name;
		}
		else if (!Current) continue;
		else if (Type == "Kd")  // Diffuse Color
		{
			SS >> Current->DiffuseColor.X
				>> Current->DiffuseColor.Y
				>> Current->DiffuseColor.Z;
		}
		else if (Type == "map_Kd")  // Diffuse Texture
		{
			std::getline(SS >> std::ws, Current->DiffuseTexturePath);
		}
	}
	return true;
}

FStaticMeshRenderData* FObjImporter::Cook(const FObjInfo& Info)
{
	auto MeshData = std::make_shared<FMeshData>();

	uint32 TriCount = (uint32)Info.PositionIndices.size();
	for (uint32 i = 0; i < TriCount; ++i)
	{
		FPrimitiveVertex V{};
		V.Position = Info.Positions[Info.PositionIndices[i]];

		// Normal: 없으면 면 노말 계산
		if (Info.NormalIndices[i] != UINT32_MAX)
		{
			V.Normal = Info.Normals[Info.NormalIndices[i]];
		}
		else
		{
			// 삼각형 단위 (i는 3의 배수 기준)
			uint32 Base = (i / 3) * 3;
			FVector P0 = Info.Positions[Info.PositionIndices[Base]];
			FVector P1 = Info.Positions[Info.PositionIndices[Base + 1]];
			FVector P2 = Info.Positions[Info.PositionIndices[Base + 2]];
			FVector E1 = P1 - P0;
			FVector E2 = P2 - P0;
			FVector N = FVector::CrossProduct(E1, E2);
			float Len = N.Size();
			V.Normal = (Len > 0.0001f) ? N / Len : FVector(0, 0, 1);
		}

		// UV
		V.UV = (Info.UVIndices[i] != UINT32_MAX)
			? Info.UVs[Info.UVIndices[i]] : FVector2(0, 0);

		V.Color = FVector4(1, 1, 1, 1);

		MeshData->Vertices.push_back(V);
		MeshData->Indices.push_back(i);
	}

	// Section 생성
	for (uint32 s = 0; s < Info.SectionStartIndices.size(); ++s)
	{
		FMeshSection Sec;
		Sec.FirstIndex = Info.SectionStartIndices[s];
		Sec.IndexCount = (s + 1 < Info.SectionStartIndices.size())
			? Info.SectionStartIndices[s + 1] - Sec.FirstIndex
			: TriCount - Sec.FirstIndex;
		Sec.MaterialIndex = s;
		MeshData->Sections.push_back(Sec);
	}

	// Section 없으면 전체를 1개로
	if (MeshData->Sections.empty() && !MeshData->Indices.empty())
	{
		FMeshSection Sec;
		Sec.FirstIndex = 0;
		Sec.IndexCount = TriCount;
		Sec.MaterialIndex = 0;
		MeshData->Sections.push_back(Sec);
	}

	MeshData->Topology = EMeshTopology::EMT_TriangleList;
	MeshData->UpdateLocalBound();

	FStaticMeshRenderData* Mesh = new FStaticMeshRenderData();
	Mesh->SetMeshData(MeshData);

	std::filesystem::path Dir(Info.ObjDirectory);

	for (uint32 s = 0; s < MeshData->Sections.size(); ++s)
	{
		FString TexPath = "";
		FVector DiffuseColor(1.0f, 1.0f, 1.0f);
		if (s < Info.MaterialNames.size())
		{
			FString MatName = Info.MaterialNames[s]; // 현재 섹션이 요구하는 머티리얼 이름
			for (const auto& Mtl : Info.Materials)
			{
				// 이름이 일치하고, 텍스처(map_Kd)가 존재한다면
				for (const auto& Mtl : Info.Materials)
				{
					if (Mtl.Name == MatName)
					{
						// 일단 색상은 무조건
						DiffuseColor = Mtl.DiffuseColor;

						// 텍스처가 있으면 경로 파싱
						if (!Mtl.DiffuseTexturePath.empty())
						{
							FAssetData TexData;
							if (FAssetRegistry::Get().GetAssetByObjectPath(Mtl.DiffuseTexturePath, TexData))
							{
								TexPath = TexData.AssetPath;
							}
							else
							{
								TexPath = (Dir / Mtl.DiffuseTexturePath).string();
							}
						}
						break;
					}
				}
			}
		}
		Mesh->ImportedTexturePaths.push_back(TexPath);
		Mesh->ImportedDiffuseColors.push_back(DiffuseColor);
	}
	return Mesh;
}
