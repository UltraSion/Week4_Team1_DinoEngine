#include "FEditorEngine.h"

#include "imgui_impl_dx11.h"
#include "UI/EditorViewportClient.h"
#include "UI/PreviewViewportClient.h"
#include "Core/Core.h"
#include "Core/ConsoleVariableManager.h"
#include "World/Level.h"
#include "Actor/Actor.h"
#include "Component/CubeComponent.h"
#include "Object/ObjectFactory.h"
#include "Debug/EngineLog.h"
#include "World/World.h"
#include "imgui_impl_win32.h"

namespace
{
	constexpr const char* PreviewLevelContextName = "PreviewLevel";

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
	}
}

bool FEditorEngine::Initialize(HINSTANCE hInstance)
{
	ImGui_ImplWin32_EnableDpiAwareness();

	if (!FEngine::Initialize(hInstance, L"Jungle Editor", 1280, 720))
	{
		return false;
	}

	ViewportClient->TopLeftX = 0;
	ViewportClient->TopLeftY = 0;
	ViewportClient->Width = 500;
	ViewportClient->Height = 500;
	ViewportClient->Initialize(Core->GetInputManager(), Core->GetEnhancedInputManager());
	Core->AddViewportClient(ViewportClient.get());

	std::unique_ptr<FViewportClient> Client = std::make_unique<FEditorViewportClient>(EditorUI, MainWindow, SeletedActors);
	Client->TopLeftX = 500;
	Client->TopLeftY = 0;
	Client->Width = 500;
	Client->Height = 500;
	Client->Initialize(Core->GetInputManager(), Core->GetEnhancedInputManager());
	Core->AddViewportClient(Client.get());
	AdditionalViewportClients.push_back(std::move(Client));

	Client = std::make_unique<FEditorViewportClient>(EditorUI, MainWindow, SeletedActors);
	Client->TopLeftX = 0;
	Client->TopLeftY = 500;
	Client->Width = 500;
	Client->Height = 500;
	Client->Initialize(Core->GetInputManager(), Core->GetEnhancedInputManager());
	Core->AddViewportClient(Client.get());
	AdditionalViewportClients.push_back(std::move(Client));

	Client = std::make_unique<FEditorViewportClient>(EditorUI, MainWindow, SeletedActors);
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
}

void FEditorEngine::Shutdown()
{
	if (Core && Core->GetViewportClient() == PreviewViewportClient.get())
	{
		Core->SetViewportClient(nullptr);
	}

	PreviewViewportClient.reset();

	for (std::unique_ptr<FViewportClient>& Client : AdditionalViewportClients)
	{
		if (Client)
		{
			Client->Cleanup();
		}
	}
	AdditionalViewportClients.clear();

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
	if (ViewportClient)
	{
		ViewportClient->Tick(DeltaTime);
	}

	for (std::unique_ptr<FViewportClient>& Client : AdditionalViewportClients)
	{
		if (Client)
		{
			Client->Tick(DeltaTime);
		}
	}

	SyncViewportClient();
}

std::unique_ptr<FViewportClient> FEditorEngine::CreateViewportClient()
{
	return std::make_unique<FEditorViewportClient>(EditorUI, MainWindow, SeletedActors);
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
