#include "FEngine.h"
#include "Platform/Windows/Window.h"
#include "Platform/Windows/WindowApplication.h"
#include "Object/ObjectGlobals.h"
#include "Math/Rect.h"

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
	WindowManager.Initialize(InputManager, EnhancedInput);
	PostInitialize();

	App->AddMessageFilter(std::bind(&FEngine::OnInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	App->SetOnResizeCallback(std::bind(&FEngine::OnResize, this, std::placeholders::_1, std::placeholders::_2));
	App->ShowWindow();

	return true;
}

FViewportContext* FEngine::CreateContext(FRect InRect)
{
	auto ViewportClient = CreateViewportClient();
	auto Viewport = new FViewport(InRect);
	
	FViewportContext* ViewportContext = new FViewportContext(Viewport, ViewportClient);
	ViewportContext->Initialize(Core.get(), InputManager, EnhancedInput);
	return ViewportContext;
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
	}
}

bool FEngine::OnInput(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	ProcessInput(Hwnd, Msg, WParam, LParam);
	return false;
}

void FEngine::OnResize(int32 Width, int32 Height)
{
	WindowManager.SetRootRect(FRect(
		0.0f,
		0.0f,
		static_cast<float>((std::max)(Width, 0)),
		static_cast<float>((std::max)(Height, 0))));

	if (Core)
	{
		Core->OnResize(Width, Height);
	}
}

void FEngine::SetViewportLayoutBounds(int32 InTopLeftX, int32 InTopLeftY, uint32 InWidth, uint32 InHeight)
{
	WindowManager.SetRootRect(FRect(
		static_cast<float>(InTopLeftX),
		static_cast<float>(InTopLeftY),
		static_cast<float>(InWidth),
		static_cast<float>(InHeight)));
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

	WindowManager.HandleMessage(Core.get(), Hwnd, Msg, WParam, LParam);
}

void FEngine::Tick(float DeltaTime)
{
	Input(Core->GetTimer().GetDeltaTime());
	WindowManager.Tick(DeltaTime);
	WindowManager.CheckParent();
	Render();
}

void FEngine::Shutdown()
{
	GEngine = nullptr;
	WindowManager.Shutdown();

	if (Core)
	{
		Core->Release();
		Core.reset();
	}

	delete EnhancedInput;
	EnhancedInput = nullptr;
	delete InputManager;
	InputManager = nullptr;

	if (App)
	{
		App->Destroy();
		App = nullptr;
	}

	MainWindow = nullptr;
}

void FEngine::Render()
{
	if (!Core)
	{
		return;
	}

	if (!GRenderer || GRenderer->IsOccluded())
	{
		return;
	}

	GRenderer->BeginFrame();
	WindowManager.DrawWindows();
	GRenderer->EndFrame();
}
