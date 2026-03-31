#include "SceneSerializer.h"
#include "ThirdParty/nlohmann/json.hpp"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Component/CameraComponent.h"
#include "Camera/Camera.h"
#include "Core/Paths.h"
#include "Actor/Actor.h"
#include "Actor/AttachTestActor.h"
#include "Renderer/Renderer.h" /
#include "Component/PrimitiveComponent.h"
#include "World/Level.h"
#include "Object/ObjectFactory.h" 
#include "Serializer/Archive.h"
#include "Object/Class.h"
#include "Actor/StaticMeshActor.h"
#include "Component/StaticMeshComponent.h"
#include <iomanip>
#include <filesystem>
#include <fstream>
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
			auto& P = Cam["Position"];
			Camera->SetPosition({ P[0].get<float>(), P[1].get<float>(), P[2].get<float>() });
		}
		if (Cam.contains("Rotation"))
		{
			auto& R = Cam["Rotation"];
			Camera->SetRotation(R[0].get<float>(), R[1].get<float>());
		}
	}

	if (PerspectiveCamera && Json.contains("PerspectiveCamera"))
	{
		auto& PerspectiveCameraJson = Json["PerspectiveCamera"];
		if (PerspectiveCameraJson.contains("Location"))
		{
			auto& Location = PerspectiveCameraJson["Location"];
			PerspectiveCamera->SetPosition({
				Location[0].get<float>(),
				Location[1].get<float>(),
				Location[2].get<float>() });
		}
		if (PerspectiveCameraJson.contains("Rotation"))
		{
			auto& Rotation = PerspectiveCameraJson["Rotation"];
			PerspectiveCamera->SetRotation(
				Rotation[0].get<float>(),
				Rotation[1].get<float>());
		}
		if (PerspectiveCameraJson.contains("FOV"))
		{
			PerspectiveCamera->SetFOV(PerspectiveCameraJson["FOV"].get<float>());
		}
		if (PerspectiveCameraJson.contains("NearClip"))
		{
			PerspectiveCamera->SetNearClip(PerspectiveCameraJson["NearClip"].get<float>());
		}
		if (PerspectiveCameraJson.contains("FarClip"))
		{
			PerspectiveCamera->SetFarClip(PerspectiveCameraJson["FarClip"].get<float>());
		}
	}

	int32 ActorIndex = 0;
	for (auto& [Key, Value] : Json["Primitives"].items())
	{
		FString ClassName = Value.value("Class","");
		UClass* ActorClass = UClass::FindClass(ClassName);
		if (!ActorClass)
		{
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
