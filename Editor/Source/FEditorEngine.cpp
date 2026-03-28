#include "FEditorEngine.h"

#include "imgui_impl_dx11.h"
#include "UI/EditorViewportClient.h"
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

	return true;
}

FEditorEngine::~FEditorEngine()
{
}

void FEditorEngine::Shutdown()
{
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
	FEngine::Tick(DeltaTime);

	SyncViewportClient();
}

void FEditorEngine::Render()
{
	FEngine::Render();
}

std::unique_ptr<FViewportClient> FEditorEngine::CreateViewportClient()
{
	return std::make_unique<FEditorViewportClient>(EditorUI, MainWindow, SeletedActors);
}

//FEditorViewportController* FEditorEngine::GetViewportController()
//{
//	return &ViewportController;
//}

void FEditorEngine::SyncViewportClient()
{
	if (!Core || Viewports.empty())
	{
		return;
	}

	FViewportClient* TargetViewportClient = Viewports[0].GetViewportClient();
	if (!TargetViewportClient)
	{
		return;
	}

	if (FEditorViewportClient* EditorViewportClient = dynamic_cast<FEditorViewportClient*>(TargetViewportClient))
	{
		EditorViewportClient->SetSelection(SeletedActors);
	}

	if (ActiveViewportClient == TargetViewportClient)
	{
		return;
	}

	if (ActiveViewportClient)
	{
		ActiveViewportClient->Detach();
	}

	TargetViewportClient->Attach(Core.get());
	ActiveViewportClient = TargetViewportClient;
}
