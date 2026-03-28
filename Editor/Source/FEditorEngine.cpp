#include "FEditorEngine.h"

#include "imgui_impl_dx11.h"
#include "UI/EditorViewportClient.h"
#include "UI/PreviewViewportClient.h"
#include "Core/Core.h"
#include "Core/ConsoleVariableManager.h"
#include "World/Level.h"
#include "Actor/Actor.h"

#include "Component/CameraComponent.h"

#include "Component/CubeComponent.h"
#include "Actor/ObjActor.h"
#include "Object/ObjectFactory.h"
#include "Debug/EngineLog.h"
#include "World/World.h"
#include "imgui_impl_win32.h"
#include "Pawn/EditorCameraPawn.h"
#include "Camera/Camera.h"
#include "Actor/SkySphereActor.h"
#include "Core/Paths.h"
#include <commdlg.h>
#include <filesystem>
namespace
{
	constexpr const char* PreviewLevelContextName = "PreviewLevel";

	FString PromptForObjFilePath()
	{
		char FileName[MAX_PATH] = "";
		const FString MeshDir = FPaths::MeshDir().string();

		OPENFILENAMEA Ofn = {};
		Ofn.lStructSize = sizeof(OPENFILENAMEA);
		Ofn.lpstrFilter = "OBJ Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
		Ofn.lpstrFile = FileName;
		Ofn.nMaxFile = MAX_PATH;
		Ofn.lpstrDefExt = "obj";
		Ofn.lpstrInitialDir = MeshDir.c_str();
		Ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		if (GetOpenFileNameA(&Ofn))
		{
			return FString(FileName);
		}

		return "";
	}

	void InitializeDefaultPreviewLevel(FCore* Core)
	{
		if (Core == nullptr)
		{
			return;
		}
		FEditorWorldContext* PreviewContext = Core->GetLevelManager()->CreatePreviewWorldContext(PreviewLevelContextName, 1280, 720);
		if (PreviewContext == nullptr || PreviewContext->World == nullptr)
		{
			return;
		}
		UWorld* PreviewWorld = PreviewContext->World;
		if (PreviewWorld->GetActors().empty())
		{
			AActor* PreviewActor = PreviewWorld->SpawnActor<AActor>("PreviewCube");
			if (PreviewActor)
			{
				UCubeComponent* PreviewComponent = FObjectFactory::ConstructObject<UCubeComponent>(PreviewActor);
				PreviewActor->AddOwnedComponent(PreviewComponent);
				PreviewActor->SetActorLocation({ 0.0f, 0.0f, 0.0f });
			}
		}

		if (UCameraComponent* PreviewCamera = PreviewWorld->GetActiveCameraComponent())
		{
			PreviewCamera->GetCamera()->SetPosition({ -8.0f, -8.0f, 6.0f });
			PreviewCamera->GetCamera()->SetRotation(45.0f, -20.0f);
			PreviewCamera->SetFov(50.0f);
		}
	}
}

bool FEditorEngine::Initialize(HINSTANCE hInstance)
{
	ImGui_ImplWin32_EnableDpiAwareness();

	if (!FEngine::Initialize(hInstance, L"Jungle Editor", 1280, 720))
	{
		return false;
	}

	return true;
}

FEditorEngine::~FEditorEngine()
{
	//Shutdown();
}

void FEditorEngine::Shutdown()
{
	if (Core && Core->GetViewportClient() == PreviewViewportClient.get())
	{
		Core->SetViewportClient(nullptr);
	}

	// EditorPawn은 Level 소속이 아니므로 직접 정리
	if (EditorPawn)
	{
		EditorPawn->Destroy();
		EditorPawn = nullptr;
	}

	PreviewViewportClient.reset();

	// ViewportController가 EnhancedInput을 참조하므로, Engine이 해제하기 전에 정리
	ViewportController.Cleanup();

	FEngine::Shutdown();
}

void FEditorEngine::PreInitialize()
{
	FEngineLog::Get().SetCallback([this](const char* Msg)
		{
			EditorUI.GetConsole().AddLog("%s", Msg);
		});
}

void FEditorEngine::PostInitialize()
{
	InitializeDefaultPreviewLevel(Core.get());
	PreviewViewportClient = std::make_unique<FPreviewViewportClient>(EditorUI, MainWindow, PreviewLevelContextName);

	FConsoleVariableManager& CVM = FConsoleVariableManager::Get();

	// TArray<FString> VariableNames; 삭제
	// CVM.GetAllNames(VariableNames); 삭제

	// 이렇게 람다로 바로 받아서 등록하도록 변경합니다.
	CVM.GetAllNames([this](const FString& Name)
	{
		EditorUI.GetConsole().RegisterCommand(Name.c_str());
	});

	EditorUI.GetConsole().SetCommandHandler([](const char* CommandLine)
		{
			FString Result;
			if (FConsoleVariableManager::Get().Execute(CommandLine, Result))
			{
				FEngineLog::Get().Log("%s", Result.c_str());
			}
			else
			{
				FEngineLog::Get().Log("[error] Unknown command: '%s'", CommandLine);
			}
		});
	// EditorPawn은 Level에 등록하지 않음 — FEditorEngine이 직접 소유
	
	EditorPawn = FObjectFactory::ConstructObject<AEditorCameraPawn>(nullptr, "EditorCameraPawn");
	EditorPawn->Initialize();
	Core->GetActiveWorld()->SetActiveCameraComponent(EditorPawn->GetCameraComponent());
	ViewportController.Initialize(
		EditorPawn->GetCameraComponent(),
		Core->GetInputManager(),
		Core->GetEnhancedInputManager());


	SyncViewportClient();

	//뷰어 모드는 기본적으로 월드축을 끕니다. 참고로 키지도 못합니다.
#if IS_OBJ_VIEWER
	Core->GetViewportClient()->GetShowFlags().SetFlag(EEngineShowFlags::SF_WorldAxis, false);
	Core->GetViewportClient()->GetShowFlags().SetFlag(EEngineShowFlags::SF_Grid, false);
	RunObjViewerStartupTest();
#else
#endif
	UE_LOG("EditorEngine initialized");
}

void FEditorEngine::Tick(float DeltaTime)
{
	// Editor Level에서는 EditorPawn 카메라가 항상 활성화되도록 보장
	// (ClearActors 후 LevelCameraComponent로 폴백된 경우 복원)
	if (EditorPawn && Core && Core->GetLevel() && Core->GetLevel()->IsEditorLevel())
	{
		UCameraComponent* EditorCamera = EditorPawn->GetCameraComponent();
		if (Core->GetActiveWorld()->GetActiveCameraComponent() != EditorCamera)
		{
			Core->GetActiveWorld()->SetActiveCameraComponent(EditorCamera);
		}
	}

	ViewportController.Tick(DeltaTime);
	SyncViewportClient();
}

std::unique_ptr<FViewportClient> FEditorEngine::CreateViewportClient()
{
	return std::make_unique<FEditorViewportClient>(EditorUI, MainWindow);
}

FEditorViewportController* FEditorEngine::GetViewportController()
{
	return &ViewportController;
}

void FEditorEngine::SyncViewportClient()
{
	if (!Core)
	{
		return;
	}

	FViewportClient* TargetViewportClient = ViewportClient.get();
	const FWorldContext* ActiveWorldContext = Core->GetActiveWorldContext();
	if (ActiveWorldContext && ActiveWorldContext->WorldType == ELevelType::Preview && PreviewViewportClient)
	{
		TargetViewportClient = PreviewViewportClient.get();
	}

	if (Core->GetViewportClient() != TargetViewportClient)
	{
		Core->SetViewportClient(TargetViewportClient);
	}
}

void FEditorEngine::RunObjViewerStartupTest()
{
#if IS_OBJ_VIEWER
	if (!Core || !EditorPawn || !Core->GetRenderer())
	{
		return;
	}

	ULevel* Level = Core->GetLevel();
	UWorld* World = Core->GetActiveWorld();
	UCameraComponent* CameraComponent = EditorPawn->GetCameraComponent();
	if (!Level || !World || !CameraComponent)
	{
		return;
	}

	const FString SelectedPath = PromptForObjFilePath();
	if (SelectedPath.empty())
	{
		UE_LOG("[ObjViewerTest] Startup mesh selection canceled");
		return;
	}

	const std::filesystem::path AssetPath = FPaths::ToAbsolutePath(SelectedPath);
	const std::filesystem::path Extension = AssetPath.extension();
	if (!std::filesystem::exists(AssetPath) || (Extension != ".obj" && Extension != ".OBJ"))
	{
		UE_LOG("[ObjViewerTest] Invalid startup mesh selection: %s", SelectedPath.c_str());
		return;
	}

	Level->ClearActors();

	AObjActor* TestActor = Level->SpawnActor<AObjActor>("ObjViewerStartupTest");
	if (!TestActor)
	{
		UE_LOG("[ObjViewerTest] Failed to spawn startup actor");
		return;
	}

	TestActor->SetActorLocation(FVector::ZeroVector);
	TestActor->LoadObj(Core->GetRenderer()->GetDevice(), FPaths::ToRelativePath(SelectedPath));

	FCamera* Camera = CameraComponent->GetCamera();
	if (Camera)
	{
		//viewer 모드에서의 카메라 위치
		Camera->SetPosition({ -6.0f, -6.0f, 5.0f });
		Camera->SetRotation(45.0f, -30.0f);
		Camera->SetFOV(50.0f);
	}

	World->SetActiveCameraComponent(CameraComponent);
	UE_LOG("[ObjViewerTest] Loaded startup mesh: %s", SelectedPath.c_str());
#endif
}
