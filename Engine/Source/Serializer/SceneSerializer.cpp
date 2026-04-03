#include "SceneSerializer.h"
#include "ThirdParty/nlohmann/json.hpp"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Component/CameraComponent.h"
#include "Camera/Camera.h"
#include "Core/Paths.h"
#include "Actor/Actor.h"
#include "Actor/AttachTestActor.h"
#include "Renderer/Renderer.h"
#include "Component/PrimitiveComponent.h"
#include "World/Level.h"
#include "Object/ObjectFactory.h"
#include "Serializer/Archive.h"
#include "Object/Class.h"
#include "Actor/StaticMeshActor.h"
#include "Component/StaticMeshComponent.h"
#include "Debug/EngineLog.h"
#include <iomanip>
#include <filesystem>
#include <fstream>

namespace
{
	float ReadJsonFloat(const nlohmann::json& Value, float DefaultValue = 0.0f)
	{
		if (Value.is_array())
		{
			if (!Value.empty())
			{
				return Value[0].get<float>();
			}

			return DefaultValue;
		}

		if (Value.is_number())
		{
			return Value.get<float>();
		}

		return DefaultValue;
	}

	bool ReadJsonVector3(const nlohmann::json& ParentJson, const char* Key, FVector& OutVector)
	{
		if (!ParentJson.contains(Key))
		{
			return false;
		}

		const nlohmann::json& Value = ParentJson[Key];
		if (!Value.is_array() || Value.size() < 3)
		{
			return false;
		}

		OutVector = {
			Value[0].get<float>(),
			Value[1].get<float>(),
			Value[2].get<float>()
		};
		return true;
	}

	FString ResolveLegacyStaticMeshPath(const FString& LegacyPath)
	{
		if (LegacyPath.empty())
		{
			return LegacyPath;
		}

		if (!LegacyPath.starts_with("Data/"))
		{
			return LegacyPath;
		}

		return "Assets/JungleApples/" + LegacyPath.substr(5);
	}

	bool LoadLegacyStaticMeshPrimitive(ULevel* Level, const nlohmann::json& PrimitiveJson, ID3D11Device* Device, int32 ActorIndex)
	{
		AStaticMeshActor* Actor = static_cast<AStaticMeshActor*>(
			FObjectFactory::ConstructObject(AStaticMeshActor::StaticClass(), Level, "AStaticMeshActor_" + std::to_string(ActorIndex)));
		if (!Actor)
		{
			UE_LOG("[SceneSerializer] Failed to create legacy static mesh actor for primitive %d\n", ActorIndex);
			return false;
		}

		Level->RegisterActor(Actor);
		Actor->PostSpawnInitialize();

		UStaticMeshComponent* StaticMeshComponent = Actor->GetStaticMeshComponent();
		if (!StaticMeshComponent)
		{
			UE_LOG("[SceneSerializer] Legacy primitive %d has no static mesh component\n", ActorIndex);
			return false;
		}

		FVector Location = FVector::ZeroVector;
		FVector RotationEuler = FVector::ZeroVector;
		FVector Scale = FVector::OneVector;

		const bool bHasLocation = ReadJsonVector3(PrimitiveJson, "Location", Location);
		const bool bHasRotation = ReadJsonVector3(PrimitiveJson, "Rotation", RotationEuler);
		const bool bHasScale = ReadJsonVector3(PrimitiveJson, "Scale", Scale);
		if (bHasLocation || bHasRotation || bHasScale)
		{
			FTransform Transform(
				FRotator::MakeFromEuler(RotationEuler),
				Location,
				Scale);
			StaticMeshComponent->SetRelativeTransform(Transform);
		}

		const FString LegacyMeshPath = PrimitiveJson.value("ObjStaticMeshAsset", "");
		const FString ResolvedMeshPath = ResolveLegacyStaticMeshPath(LegacyMeshPath);
		Actor->LoadStaticMesh(Device, ResolvedMeshPath);
		if (!StaticMeshComponent->GetStaticMeshAsset().empty())
		{
			return true;
		}

		if (ResolvedMeshPath != LegacyMeshPath)
		{
			Actor->LoadStaticMesh(Device, LegacyMeshPath);
			if (!StaticMeshComponent->GetStaticMeshAsset().empty())
			{
				UE_LOG("[SceneSerializer] Legacy primitive %d fell back to original mesh path: %s\n", ActorIndex, LegacyMeshPath.c_str());
				return true;
			}
		}

		UE_LOG("[SceneSerializer] Failed to load legacy mesh for primitive %d: %s\n", ActorIndex, LegacyMeshPath.c_str());
		return false;
	}
}

void FSceneSerializer::Save(ULevel* Level, const FString& FilePath, const FCamera* PerspectiveCamera)
{
	nlohmann::json Json;
	FCamera* Camera = Level->GetCamera();
	if (Camera)
	{
		const FVector Position = Camera->GetPosition();
		Json["Camera"]["Position"] = { Position.X, Position.Y, Position.Z };
		Json["Camera"]["Rotation"] = { Camera->GetYaw(), Camera->GetPitch() };
	}

	if (PerspectiveCamera && PerspectiveCamera->GetProjectionMode() == ECameraProjectionMode::Perspective)
	{
		const FVector Location = PerspectiveCamera->GetPosition();
		Json["PerspectiveCamera"]["Location"] = { Location.X, Location.Y, Location.Z };
		Json["PerspectiveCamera"]["Rotation"] = { PerspectiveCamera->GetYaw(), PerspectiveCamera->GetPitch() };
		Json["PerspectiveCamera"]["FOV"] = PerspectiveCamera->GetFOV();
		Json["PerspectiveCamera"]["NearClip"] = PerspectiveCamera->GetNearClip();
		Json["PerspectiveCamera"]["FarClip"] = PerspectiveCamera->GetFarClip();
	}

	// Materials (로드된 Material 파일 경로를 프로젝트 루트 기준 상대 경로로 저장)
	TArray<FString> LoadedPaths = FMaterialManager::Get().GetLoadedPaths();
	if (!LoadedPaths.empty())
	{
		nlohmann::json Materials = nlohmann::json::array();
		FString Root = FPaths::ProjectRoot().string();
		for (const FString& AbsPath : LoadedPaths)
		{
			// 절대 경로 → 프로젝트 루트 기준 상대 경로
			std::filesystem::path Rel = std::filesystem::relative(AbsPath, Root);
			Materials.push_back(Rel.generic_string());
		}
		Json["Materials"] = Materials;
	}

	// Primitives
	nlohmann::json Primitives;
	int32 Index = 0;
	for (AActor* Actor : Level->GetActors())
	{
		if (!Actor || Actor->IsPendingDestroy())
			continue;
		if (!Actor->GetRootComponent())
			continue;
		FArchive Ar(true);
		Actor->Serialize(Ar);
		nlohmann::json& ActorJson 
			= *static_cast<nlohmann::json*>(Ar.GetRawJson());
		Primitives[std::to_string(Index)] = ActorJson;
		Index++;
	}

	Json["Primitives"] = Primitives;
	Json["NextUUID"] = FObjectFactory::GetLastUUID();
	std::ofstream File(std::filesystem::path(FilePath).wstring());
	if (File.is_open())
	{
		File << std::setw(2) << Json << std::endl;
	}
}

bool FSceneSerializer::Load(ULevel* Level, const FString& FilePath, ID3D11Device* Device, FCamera* PerspectiveCamera)
{
	std::ifstream File(std::filesystem::path(FilePath).wstring());
	if (!File.is_open())
	{
		return false;
	}

	nlohmann::json Json;

	try
	{
		File >> Json;
	}
	catch (const std::exception& e)
	{
		return false;
	}

	if (!Json.contains("Primitives"))
		return false;
	if (Json.contains("Materials"))//매테리얼 프리로드
	{
		for (const auto& MatPath : Json["Materials"])
		{
			FString FullPath = (FPaths::ProjectRoot() / MatPath.get<std::string>()).string();
			if (GRenderer && GRenderer->GetRenderStateManager())
			{
				FMaterialManager::Get().LoadFromFile(Device, GRenderer->GetRenderStateManager().get(), FullPath);
			}
		}
	}
	FCamera* Camera = Level->GetCamera();
	if (Camera && Json.contains("Camera"))
	{
		auto& Cam = Json["Camera"];
		if (Cam.contains("Position"))
		{
			const auto& P = Cam["Position"];
			Camera->SetPosition({ P[0].get<float>(), P[1].get<float>(), P[2].get<float>() });
		}
		if (Cam.contains("Rotation"))
		{
			const auto& R = Cam["Rotation"];
			Camera->SetRotation(R[0].get<float>(), R[1].get<float>());
		}
	}

	if (PerspectiveCamera && Json.contains("PerspectiveCamera"))
	{
		auto& PerspectiveCameraJson = Json["PerspectiveCamera"];
		if (PerspectiveCameraJson.contains("Location"))
		{
			const auto& Location = PerspectiveCameraJson["Location"];
			PerspectiveCamera->SetPosition({
				Location[0].get<float>(),
				Location[1].get<float>(),
				Location[2].get<float>() });
		}
		if (PerspectiveCameraJson.contains("Rotation"))
		{
			const auto& Rotation = PerspectiveCameraJson["Rotation"];
			PerspectiveCamera->SetRotation(
				Rotation[0].get<float>(),
				Rotation[1].get<float>());
		}
		if (PerspectiveCameraJson.contains("FOV"))
		{
			PerspectiveCamera->SetFOV(ReadJsonFloat(PerspectiveCameraJson["FOV"]));
		}
		if (PerspectiveCameraJson.contains("NearClip"))
		{
			PerspectiveCamera->SetNearClip(ReadJsonFloat(PerspectiveCameraJson["NearClip"]));
		}
		//if (PerspectiveCameraJson.contains("FarClip"))
		//{
		//	PerspectiveCamera->SetFarClip(PerspectiveCameraJson["FarClip"].get<float>());
		//} 아무도 나를 막을 수 없음
	}

	int32 ActorIndex = 0;
	for (auto& [Key, Value] : Json["Primitives"].items())
	{
		if (Value.contains("Class"))
		{
			FString ClassName = Value.value("Class", "");
			UClass* ActorClass = UClass::FindClass(ClassName);
			if (!ActorClass)
			{
				UE_LOG("[SceneSerializer] Unknown actor class while loading primitive %s: %s\n", Key.c_str(), ClassName.c_str());
				ActorIndex++;
				continue;
			}

			const FString ActorName = ClassName + "_" + std::to_string(ActorIndex);
			AActor* Actor = static_cast<AActor*>(FObjectFactory::ConstructObject(ActorClass, Level, ActorName));
			if (!Actor)
			{
				ActorIndex++;
				continue;
			}

			Level->RegisterActor(Actor);
			Actor->PostSpawnInitialize();
			FArchive Ar(false);// loading
			*static_cast<nlohmann::json*>(Ar.GetRawJson()) = Value;
			Actor->Serialize(Ar);

			++ActorIndex;
			continue;
		}

		const FString LegacyType = Value.value("Type", "");
		if (LegacyType == "StaticMeshComp" || Value.contains("ObjStaticMeshAsset"))
		{
			LoadLegacyStaticMeshPrimitive(Level, Value, Device, ActorIndex);
			++ActorIndex;
			continue;
		}

		UE_LOG("[SceneSerializer] Skipping unsupported primitive format: %s\n", Key.c_str());
	}
	if (Json.contains("NextUUID"))
	{
		uint32 Saved = Json["NextUUID"].get<uint32>();
		if (Saved > FObjectFactory::GetLastUUID())
		{
			FObjectFactory::SetLastUUID(Saved);
		}
	}

	return true;
}
