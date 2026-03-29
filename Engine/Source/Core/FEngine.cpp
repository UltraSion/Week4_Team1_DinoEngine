#include "FEngine.h"
#include "Platform/Windows/Window.h"
#include "Core/ViewportClient.h"
#include "Platform/Windows/WindowApplication.h"
#include "Object/ObjectGlobals.h"
#include "World/World.h"
#include "Core/ViewportContext.h"
#include <windowsx.h>

FEngine* GEngine = nullptr;

FEngine::~FEngine()
{
	Shutdown();
}

namespace
{
	bool IsMouseMessage(UINT Msg)
	{
		switch (Msg)
		{
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
			return true;
		default:
			return false;
		}
	}

	bool IsKeyboardMessage(UINT Msg)
	{
		switch (Msg)
		{
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_CHAR:
		case WM_SYSCHAR:
		case WM_UNICHAR:
			return true;
		default:
			return false;
		}
	}
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

	InitializeViewportContexts(4);
	ViewportLayoutOriginX = 0;
	ViewportLayoutOriginY = 0;
	ViewportLayoutWidth = static_cast<uint32>((std::max)(MainWindow->GetWidth(), 0));
	ViewportLayoutHeight = static_cast<uint32>((std::max)(MainWindow->GetHeight(), 0));
	UpdateViewportLayout(MainWindow->GetWidth(), MainWindow->GetHeight());

	if (!Viewports.empty())
	{
		SetActiveViewportContext(&Viewports[0]);
	}


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
	if (!bHasCustomViewportLayoutBounds)
	{
		ViewportLayoutOriginX = 0;
		ViewportLayoutOriginY = 0;
		ViewportLayoutWidth = static_cast<uint32>((std::max)(Width, 0));
		ViewportLayoutHeight = static_cast<uint32>((std::max)(Height, 0));
		UpdateViewportLayout(Width, Height);
	}

	if (Core)
	{
		Core->OnResize(Width, Height);
	}
}

void FEngine::SetViewportLayout(EViewportLayout InLayout)
{
	ViewportLayout = InLayout;
	if (bHasCustomViewportLayoutBounds)
	{
		UpdateViewportLayout(static_cast<int32>(ViewportLayoutWidth), static_cast<int32>(ViewportLayoutHeight));
	}
	else if (MainWindow)
	{
		UpdateViewportLayout(MainWindow->GetWidth(), MainWindow->GetHeight());
	}
}

void FEngine::SetViewportLayoutBounds(int32 InTopLeftX, int32 InTopLeftY, uint32 InWidth, uint32 InHeight)
{
	ViewportLayoutOriginX = InTopLeftX;
	ViewportLayoutOriginY = InTopLeftY;
	ViewportLayoutWidth = InWidth;
	ViewportLayoutHeight = InHeight;
	bHasCustomViewportLayoutBounds = true;
	UpdateViewportLayout(static_cast<int32>(InWidth), static_cast<int32>(InHeight));
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

	FViewportContext* InputViewportContext = GetInputOwnerViewportContext();
	if (!Core || !InputViewportContext)
	{
		return;
	}

	FViewportClient* ViewportClient = InputViewportContext->GetViewportClient();
	if (ViewportClient)
	{
		ViewportClient->ProcessCameraInput(Core.get(), DeltaTime);
	}
}

void FEngine::ProcessInput(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	if (InputManager)
	{
		InputManager->ProcessMessage(Hwnd, Msg, WParam, LParam);
	}

	if (Viewports.empty())
	{
		return;
	}

	if (IsMouseMessage(Msg))
	{
		POINT MousePoint = { static_cast<LONG>(GET_X_LPARAM(LParam)), static_cast<LONG>(GET_Y_LPARAM(LParam)) };
		if ((Msg == WM_MOUSEWHEEL || Msg == WM_MOUSEHWHEEL) && MainWindow)
		{
			::ScreenToClient(MainWindow->GetHwnd(), &MousePoint);
		}

		UpdateViewportInteractionState(MousePoint.x, MousePoint.y);
		if (Msg == WM_LBUTTONDOWN || Msg == WM_RBUTTONDOWN || Msg == WM_MBUTTONDOWN)
		{
			FViewportContext* HoveredViewportContext = FindHoveredViewportContext(MousePoint.x, MousePoint.y);
			if (HoveredViewportContext)
			{
				SetActiveViewportContext(HoveredViewportContext);
				SetCapturingViewportContext(HoveredViewportContext);
			}
		}
		else if ((Msg == WM_LBUTTONUP || Msg == WM_RBUTTONUP || Msg == WM_MBUTTONUP) && !AreAnyMouseButtonsDown())
		{
			SetCapturingViewportContext(nullptr);
		}
	}

	RouteInputToViewport(Hwnd, Msg, WParam, LParam);
}

void FEngine::Tick(float DeltaTime)
{
	TickViewportContexts(DeltaTime);
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

	CleanupViewportContexts();
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
	ActiveViewportContext = nullptr;
	HoveredViewportContext = nullptr;
	CapturingViewportContext = nullptr;
	Viewports.clear();
}

void FEngine::Render()
{
	if (!Core)
		return;

	if (!GRenderer || GRenderer->IsOccluded())
	{
		return;
	}

	GRenderer->BeginFrame();
	RenderAllViewports();
	GRenderer->EndFrame();
}


//Imgui Viewport 크기에 따라 조절
void FEngine::UpdateViewportLayout(int32 Width, int32 Height)
{
	if (Viewports.empty())
	{
		return;
	}

	const uint32 SafeWidth = static_cast<uint32>((std::max)(Width, 0));
	const uint32 SafeHeight = static_cast<uint32>((std::max)(Height, 0));
	const int32 LayoutOriginX = ViewportLayoutOriginX;
	const int32 LayoutOriginY = ViewportLayoutOriginY;
	struct FViewportRect
	{
		int32 X;
		int32 Y;
		uint32 Width;
		uint32 Height;
	};

	TArray<FViewportRect> Rects;
	Rects.reserve(Viewports.size());

	switch (ViewportLayout)
	{
	case EViewportLayout::Single:
		Rects.push_back({ 0, 0, SafeWidth, SafeHeight });
		break;
	case EViewportLayout::LeftRight:
	{
		const uint32 LeftWidth = SafeWidth / 2;
		const uint32 RightWidth = SafeWidth - LeftWidth;
		Rects.push_back({ 0, 0, LeftWidth, SafeHeight });
		Rects.push_back({ static_cast<int32>(LeftWidth), 0, RightWidth, SafeHeight });
		break;
	}
	case EViewportLayout::TopBottom:
	{
		const uint32 TopHeight = SafeHeight / 2;
		const uint32 BottomHeight = SafeHeight - TopHeight;
		Rects.push_back({ 0, 0, SafeWidth, TopHeight });
		Rects.push_back({ 0, static_cast<int32>(TopHeight), SafeWidth, BottomHeight });
		break;
	}
	case EViewportLayout::Quad:
	default:
	{
		const uint32 LeftWidth = SafeWidth / 2;
		const uint32 RightWidth = SafeWidth - LeftWidth;
		const uint32 TopHeight = SafeHeight / 2;
		const uint32 BottomHeight = SafeHeight - TopHeight;
		Rects.push_back({ 0, 0, LeftWidth, TopHeight });
		Rects.push_back({ 0, static_cast<int32>(TopHeight), LeftWidth, BottomHeight });
		Rects.push_back({ static_cast<int32>(LeftWidth), 0, RightWidth, TopHeight });
		Rects.push_back({ static_cast<int32>(LeftWidth), static_cast<int32>(TopHeight), RightWidth, BottomHeight });
		break;
	}
	}

	const size_t ViewportCount = (std::min)(Viewports.size(), Rects.size());
	for (size_t Index = 0; Index < ViewportCount; ++Index)
	{
		Viewports[Index].ApplyLayout(
			LayoutOriginX + Rects[Index].X,
			LayoutOriginY + Rects[Index].Y,
			Rects[Index].Width,
			Rects[Index].Height,
			Rects[Index].X,
			Rects[Index].Y);
	}

	for (size_t Index = ViewportCount; Index < Viewports.size(); ++Index)
	{
		Viewports[Index].ApplyLayout(0, 0, 0, 0, 0, 0);
	}
}

void FEngine::UpdateViewportInteractionState(int32 WindowMouseX, int32 WindowMouseY)
{
	HoveredViewportContext = FindHoveredViewportContext(WindowMouseX, WindowMouseY);
	for (FViewportContext& ViewportContext : Viewports)
	{
		const bool bFocused = (&ViewportContext == ActiveViewportContext);
		const bool bCapturing = (&ViewportContext == CapturingViewportContext);
		ViewportContext.UpdateInteractionState(WindowMouseX, WindowMouseY, bFocused, bCapturing);
	}
}

FViewportContext* FEngine::FindHoveredViewportContext(int32 WindowMouseX, int32 WindowMouseY)
{
	for (FViewportContext& ViewportContext : Viewports)
	{
		if (ViewportContext.AcceptsInput() && ViewportContext.ContainsPoint(WindowMouseX, WindowMouseY))
		{
			return &ViewportContext;
		}
	}

	return nullptr;
}

void FEngine::RouteInputToViewport(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	FViewportContext* TargetViewportContext = ResolveInputViewportContext(Msg);
	if (!TargetViewportContext)
	{
		return;
	}

	TargetViewportContext->HandleMessage(Core.get(), Hwnd, Msg, WParam, LParam);
}

FViewportContext* FEngine::ResolveInputViewportContext(UINT Msg) const
{
	if (IsKeyboardMessage(Msg))
	{
		return ActiveViewportContext;
	}

	if (!IsMouseMessage(Msg))
	{
		return ActiveViewportContext;
	}

	if (Msg == WM_MOUSEWHEEL || Msg == WM_MOUSEHWHEEL)
	{
		return CapturingViewportContext ? CapturingViewportContext : ActiveViewportContext;
	}

	if (CapturingViewportContext)
	{
		return CapturingViewportContext;
	}

	if (HoveredViewportContext)
	{
		return HoveredViewportContext;
	}

	return ActiveViewportContext;
}

FViewportContext* FEngine::GetInputOwnerViewportContext() const
{
	auto IsValidInputOwner = [](FViewportContext* ViewportContext)
	{
		return ViewportContext != nullptr && ViewportContext->AcceptsInput() && ViewportContext->GetViewportClient() != nullptr;
	};

	if (IsValidInputOwner(CapturingViewportContext))
	{
		return CapturingViewportContext;
	}

	if (IsValidInputOwner(ActiveViewportContext))
	{
		return ActiveViewportContext;
	}

	if (IsValidInputOwner(HoveredViewportContext))
	{
		return HoveredViewportContext;
	}

	return nullptr;
}

FViewportContext FEngine::CreateViewportContext(size_t Index)
{
	FViewportContext Context(std::make_unique<FViewport>(0, 0, 0, 0), CreateViewportClient());
	ConfigureViewportContext(Index, Context);
	return Context;
}

void FEngine::InitializeViewportContexts(size_t Count)
{
	Viewports.clear();
	Viewports.reserve(Count);
	for (size_t Index = 0; Index < Count; ++Index)
	{
		Viewports.push_back(CreateViewportContext(Index));
		Viewports.back().Initialize(Core.get(), InputManager, EnhancedInput);
	}
}

void FEngine::CleanupViewportContexts()
{
	for (FViewportContext& ViewportContext : Viewports)
	{
		ViewportContext.Cleanup();
	}
}

void FEngine::SetActiveViewportContext(FViewportContext* InViewportContext)
{
	if (ActiveViewportContext == InViewportContext)
	{
		if (ActiveViewportContext)
		{
			ActiveViewportContext->SetActive(true);
		}
		return;
	}

	FViewportContext* PreviousActiveContext = ActiveViewportContext;
	if (ActiveViewportContext)
	{
		ActiveViewportContext->SetActive(false);
	}

	ActiveViewportContext = InViewportContext;
	if (ActiveViewportContext)
	{
		ActiveViewportContext->SetActive(true);
	}

	OnActiveViewportContextChanged(ActiveViewportContext, PreviousActiveContext);
}

void FEngine::SetCapturingViewportContext(FViewportContext* InViewportContext)
{
	if (CapturingViewportContext == InViewportContext)
	{
		return;
	}

	if (CapturingViewportContext)
	{
		CapturingViewportContext->SetCapturing(false);
	}

	CapturingViewportContext = InViewportContext;
	if (CapturingViewportContext)
	{
		CapturingViewportContext->SetCapturing(true);
	}
}

void FEngine::ConfigureViewportContext(size_t Index, FViewportContext& Context)
{
	(void)Index;
	(void)Context;
}

void FEngine::TickViewportContexts(float DeltaTime)
{
	for (FViewportContext& ViewportContext : Viewports)
	{
		ViewportContext.Tick(Core.get(), DeltaTime);
	}
}

void FEngine::RenderAllViewports()
{
	for (FViewportContext& Context : Viewports)
	{
		Context.Render(Core.get(), CommandQueue);
	}
}

bool FEngine::AreAnyMouseButtonsDown() const
{
	if (!InputManager)
	{
		return false;
	}

	return InputManager->IsMouseButtonDown(FInputManager::MOUSE_LEFT) ||
		InputManager->IsMouseButtonDown(FInputManager::MOUSE_RIGHT) ||
		InputManager->IsMouseButtonDown(FInputManager::MOUSE_MIDDLE);
}
