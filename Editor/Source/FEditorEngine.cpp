#include "FEditorEngine.h"

#include "imgui_impl_dx11.h"
#include "UI/EditorViewportClient.h"
#include "UI/PreviewViewportClient.h"
#include "Core/Core.h"
#include "Core/ConsoleVariableManager.h"
#include "Scene/Scene.h"
#include "Actor/Actor.h"

#include "Component/CameraComponent.h"

#include "Component/CubeComponent.h"
#include "Object/ObjectFactory.h"
#include "Debug/EngineLog.h"
#include "World/World.h"
#include "imgui_impl_win32.h"
#include "Pawn/EditorCameraPawn.h"
#include "Camera/Camera.h"
#include "Actor/SkySphereActor.h"
namespace
{
	constexpr const char* PreviewSceneContextName = "PreviewScene";

	void InitializeDefaultPreviewScene(CCore* Core)
	{
		if (Core == nullptr)
		{
			return;
		}
		FEditorWorldContext* PreviewContext = Core->GetSceneManager()->CreatePreviewWorldContext(PreviewSceneContextName, 1280, 720);
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

		//if (UCameraComponent* PreviewCamera = PreviewWorld->GetActiveCameraComponent())
		//{
		//	PreviewCamera->GetCamera()->SetPosition({ -8.0f, -8.0f, 6.0f });
		//	PreviewCamera->GetCamera()->SetRotation(45.0f, -20.0f);
		//	PreviewCamera->SetFov(50.0f);
		//}
	}
}

bool FEditorEngine::Initialize(HINSTANCE hInstance)
{
	ImGui_ImplWin32_EnableDpiAwareness();

	if (!FEngine::Initialize(hInstance, L"Jungle Editor", 1280, 720))
	{
		return false;
	}


	MainViewportClient->TopLeftX = 0;
	MainViewportClient->TopLeftY = 0;
	MainViewportClient->Width = 500;
	MainViewportClient->Height = 500;
	MainViewportClient->Initialize(Core->GetInputManager(), Core->GetEnhancedInputManager());
	Core->AddViewportClient(MainViewportClient.get());
	//AdditionalViewportClients.push_back(std::move(MainViewportClient));

	std::unique_ptr<IViewportClient> Client = std::make_unique<CEditorViewportClient>(EditorUI, MainWindow, SeletedActors, Core.get()->GetEditorWorld());
	Client->TopLeftX = 500;
	Client->TopLeftY = 0;
	Client->Width = 500;
	Client->Height = 500;
	Client->Initialize(Core->GetInputManager(), Core->GetEnhancedInputManager());
	Core->AddViewportClient(Client.get());
	AdditionalViewportClients.push_back(std::move(Client));

	Client = std::make_unique<CEditorViewportClient>(EditorUI, MainWindow, SeletedActors, Core.get()->GetEditorWorld());
	Client->TopLeftX = 0;
	Client->TopLeftY = 500;
	Client->Width = 500;
	Client->Height = 500;
	Client->Initialize(Core->GetInputManager(), Core->GetEnhancedInputManager());
	Core->AddViewportClient(Client.get());
	AdditionalViewportClients.push_back(std::move(Client));

	Client = std::make_unique<CEditorViewportClient>(EditorUI, MainWindow, SeletedActors, Core.get()->GetEditorWorld());
	Client->TopLeftX = 500;
	Client->TopLeftY = 500;
	Client->Width = 500;
	Client->Height = 500;
	Client->Initialize(Core->GetInputManager(), Core->GetEnhancedInputManager());
	Core->AddViewportClient(Client.get());
	AdditionalViewportClients.push_back(std::move(Client));

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
		Core->SetMainViewportClient(nullptr);
	}

	PreviewViewportClient.reset();
	for (std::unique_ptr<IViewportClient>& Client : AdditionalViewportClients)
	{
		if (Client)
		{
			Client->Cleanup();
		}
	}
	AdditionalViewportClients.clear();

	//ViewportController.Cleanup();

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
	InitializeDefaultPreviewScene(Core.get());
	PreviewViewportClient = std::make_unique<CPreviewViewportClient>(EditorUI, MainWindow, PreviewSceneContextName, Core.get()->GetEditorWorld());

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

	SyncViewportClient();
	UE_LOG("EditorEngine initialized");
}

void FEditorEngine::Tick(float DeltaTime)
{
	MainViewportClient->Tick(DeltaTime);
	for (std::unique_ptr<IViewportClient>& Client : AdditionalViewportClients)
	{
		if (Client)
		{
			Client->Tick(DeltaTime);
		}
	}
	//ViewportController.Tick(DeltaTime);
	SyncViewportClient();
}

std::unique_ptr<IViewportClient> FEditorEngine::CreateViewportClient()
{
	return std::make_unique<CEditorViewportClient>(EditorUI, MainWindow, SeletedActors, Core.get()->GetEditorWorld());
}

CEditorViewportController* FEditorEngine::GetViewportController()
{
	return &ViewportController;
}

void FEditorEngine::SyncViewportClient()
{
	if (!Core)
	{
		return;
	}

	IViewportClient* TargetViewportClient = MainViewportClient.get();
	const FWorldContext* ActiveSceneContext = Core->GetActiveWorldContext();
	if (ActiveSceneContext && ActiveSceneContext->WorldType == ESceneType::Preview && PreviewViewportClient)
	{
		TargetViewportClient = PreviewViewportClient.get();
	}

	if (Core->GetViewportClient() != TargetViewportClient)
	{
		Core->SetMainViewportClient(TargetViewportClient);
	}
}
