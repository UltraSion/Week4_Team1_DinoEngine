#include "FEditorEngine.h"

#include "imgui_impl_dx11.h"
#include "UI/EditorViewportClient.h"
#include "Core/Core.h"
#include "Core/ConsoleVariableManager.h"
#include "World/Level.h"
#include "Debug/EngineLog.h"
#include "imgui_impl_win32.h"

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
	EditorUI.DetachFromRenderer();
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

	EditorUI.Initialize(Core.get());
	EditorUI.SetupWindow(MainWindow);
	EditorUI.AttachToRenderer();

	RefreshEditorViewportClients();
	UE_LOG("EditorEngine initialized");
}

void FEditorEngine::Tick(float DeltaTime)
{
	FEngine::Tick(DeltaTime);
}

void FEditorEngine::Render()
{
	FEngine::Render();
}

std::unique_ptr<FViewportClient> FEditorEngine::CreateViewportClient()
{
	return std::make_unique<FEditorViewportClient>(EditorUI, MainWindow, EEditorViewportType::Perspective, ELevelType::Editor);
}

void FEditorEngine::ConfigureViewportContext(size_t Index, FViewportContext& Context)
{
	const EEditorViewportType ViewportTypes[] =
	{
		EEditorViewportType::Perspective,
		EEditorViewportType::Top,
		EEditorViewportType::Front,
		EEditorViewportType::Right
	};

	if (Index < std::size(ViewportTypes))
	{
		Context.ViewportClient = std::make_unique<FEditorViewportClient>(EditorUI, MainWindow, ViewportTypes[Index], ELevelType::Editor);
	}
}

void FEditorEngine::OnActiveViewportContextChanged(FViewportContext* NewActiveContext, FViewportContext* PreviousActiveContext)
{
	(void)PreviousActiveContext;
	EditorUI.SetActiveViewportClient(ResolveEditorViewportClient(NewActiveContext));
}

void FEditorEngine::RefreshEditorViewportClients()
{
	EditorUI.SetActiveViewportClient(ResolveEditorViewportClient(GetActiveViewportContext()));
}

FEditorViewportClient* FEditorEngine::ResolveEditorViewportClient(FViewportContext* ViewportContext) const
{
	return ViewportContext ? dynamic_cast<FEditorViewportClient*>(ViewportContext->GetViewportClient()) : nullptr;
}
