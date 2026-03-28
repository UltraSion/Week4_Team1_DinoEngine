#include "FEngine.h"
#include "Platform/Windows/Window.h"
#include "Core/ViewportClient.h"
#include "Platform/Windows/WindowApplication.h"
#include "Object/ObjectGlobals.h"
#include "World/World.h"
#include "Core/ViewportContext.h"

FEngine* GEngine = nullptr;

FEngine::~FEngine()
{
	Shutdown();
}

bool FEngine::Initialize(HINSTANCE hInstance, const wchar_t* Title, int32 Width, int32 Height)
{
	App = &FWindowApplication::Get();
	if (!App->Create(hInstance))
	{
		return false;
	}

	if (!App->CreateMainWindow(Title, Width, Height))
	{
		return false;
	}

	GEngine = this;


	MainWindow = App->GetMainWindow();
	if (!MainWindow)
	{
		return false;
	}

	PreInitialize();

	Core = std::make_unique<FCore>();
	if (!Core->Initialize(MainWindow->GetHwnd(), MainWindow->GetWidth(), MainWindow->GetHeight(), GetStartupLevelType()))
	{
		return false;
	}
	InputManager = new FInputManager();
	EnhancedInput = new FEnhancedInputManager();


	Viewports.push_back({std::make_unique<FViewport>(0, 0, 750, 750), CreateViewportClient()});
	Viewports.push_back({ std::make_unique<FViewport>(0, 750, 750, 750), CreateViewportClient() });
	Viewports.push_back({ std::make_unique<FViewport>(750, 0, 750, 750), CreateViewportClient() });
	Viewports.push_back({ std::make_unique<FViewport>(750, 750, 750, 750), CreateViewportClient() });

	Viewports[0].GetViewportClient()->Initialize(InputManager, EnhancedInput);
	Viewports[1].GetViewportClient()->Initialize(InputManager, EnhancedInput);
	Viewports[2].GetViewportClient()->Initialize(InputManager, EnhancedInput);
	Viewports[3].GetViewportClient()->Initialize(InputManager, EnhancedInput);


	PostInitialize();

	App->AddMessageFilter(std::bind(&FEngine::OnInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	App->SetOnResizeCallback(std::bind(&FEngine::OnResize, this, std::placeholders::_1, std::placeholders::_2));
	App->ShowWindow();

	return true;
}

void FEngine::Run()
{
	while (App->PumpMessages())
	{
		if (Core)
		{
			Tick(Core->GetTimer().GetDeltaTime());
			Core->Tick();
		}
		Input(Core->GetTimer().GetDeltaTime());
		Render();
	}
}

bool FEngine::OnInput(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	ProcessInput(Hwnd, Msg, WParam, LParam);
	return false;
}

void FEngine::OnResize(int32 Width, int32 Height)
{
	if (Core)
	{
		Core->OnResize(Width, Height);
	}
}

void FEngine::Input(float DeltaTime)
{
	if (InputManager)
	{
		InputManager->Tick();
	}

	if (EnhancedInput && InputManager)
	{
		EnhancedInput->ProcessInput(InputManager, DeltaTime);
	}
}

void FEngine::ProcessInput(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	if (InputManager)
	{
		InputManager->ProcessMessage(Hwnd, Msg, WParam, LParam);
	}

	Viewports[0].GetViewportClient()->HandleMessage(Core.get(), Hwnd, Msg, WParam, LParam);
}

void FEngine::Tick(float DeltaTime)
{
	for (auto& Client : Viewports)
	{
		Client.GetViewportClient()->Tick(DeltaTime);
	}
}

std::unique_ptr<FViewportClient> FEngine::CreateViewportClient()
{
	return std::make_unique<FGameViewportClient>();
}

void FEngine::Shutdown()
{
	GEngine = nullptr;

	if (Core)
	{
		Core->Release();
		Core.reset();
	}

	for (auto& ViewportContext : Viewports)
		ViewportContext.Cleanup();

	if (App)
	{
		App->Destroy();
		App = nullptr;
	}

	MainWindow = nullptr;

	for (auto& Client : Viewports)
	{
		Client.Cleanup();
	}
	Viewports.clear();
}

void FEngine::Render()
{
	if (!Core)
		return;

	ULevel* Level = Core->GetActiveLevel();
	if (!GRenderer || !Level || GRenderer->IsOccluded())
	{
		return;
	}

	GRenderer->BeginFrame();

	UWorld* ActiveWorld = Core->GetActiveWorld();
	if (!ActiveWorld)
	{
		GRenderer->EndFrame();
		return;
	}

	for (const FViewportContext& Context : Viewports)
	{
		CommandQueue.Clear();
		CommandQueue.Reserve(GRenderer->GetPrevCommandCount());

		TArray<AActor*> Actors = ActiveWorld->GetAllActors();
		Context.GetViewportClient()->BuildRenderCommands(Actors, CommandQueue);

		D3D11_VIEWPORT VP = Context.GetViewport()->GetD3D11Viewport();
		GRenderer->SetViewport(&VP);

		GRenderer->SubmitCommands(CommandQueue);
		GRenderer->ExecuteCommands();
		const FShowFlags& ShowFlags = Context.GetViewportClient()->GetShowFlags();
		Core.get()->GetDebugDrawManager().Flush(GRenderer, ShowFlags, ActiveWorld);
	}

	GRenderer->EndFrame();
}