#include "WindowManager.h"
#include "ViewportWindow.h"
#include "Core/Viewport.h"
#include "Core/ViewportClient.h"
#include "Core/ViewportContext.h"
#include "Input/InputManager.h"
#include <windowsx.h>

namespace
{
	void VisitViewportWindows(SWindow* Window, const std::function<void(SViewportWindow*)>& Visitor)
	{
		if (!Window)
		{
			return;
		}

		if (SViewportWindow* ViewportWindow = dynamic_cast<SViewportWindow*>(Window))
		{
			Visitor(ViewportWindow);
			return;
		}

		if (SSplitter* Splitter = dynamic_cast<SSplitter*>(Window))
		{
			VisitViewportWindows(Splitter->GetSideLT(), Visitor);
			VisitViewportWindows(Splitter->GetSideRB(), Visitor);
		}
	}

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

FWindowManager::~FWindowManager()
{
	Shutdown();
}

void FWindowManager::Initialize(FInputManager* InInputManager, FEnhancedInputManager* InEnhancedInputManager)
{
	InputManager = InInputManager;
	EnhancedInputManager = InEnhancedInputManager;
}

void FWindowManager::Shutdown()
{
	for (SWindow* Window : Windows)
	{
		delete Window;
	}
	Windows.clear();
	ClearViewportState();
	InputManager = nullptr;
	EnhancedInputManager = nullptr;
	OnActiveViewportChanged = nullptr;
}

SViewportWindow* FWindowManager::CreateViewportWindow(FCore* Core, std::unique_ptr<FViewportClient> ViewportClient, const FRect& InRect)
{
	if (!ViewportClient)
	{
		return nullptr;
	}

	std::unique_ptr<FViewportContext> ViewportContext = std::make_unique<FViewportContext>(
		std::make_unique<FViewport>(InRect, ViewportClient.get()),
		std::move(ViewportClient));
	ViewportContext->Initialize(Core, InputManager, EnhancedInputManager);

	SViewportWindow* ViewportWindow = CreateSWindow<SViewportWindow>(std::move(ViewportContext));
	ViewportWindow->SetRect(InRect);
	if (!ActiveViewportWindow)
	{
		SetActiveViewportWindow(ViewportWindow);
	}

	return ViewportWindow;
}

void FWindowManager::SetRootRect(const FRect& InRect)
{
	if (Windows.empty() || !Windows[0])
	{
		return;
	}

	Windows[0]->SetRect(InRect);
}

FViewportContext* FWindowManager::GetActiveViewportContext() const
{
	return ActiveViewportWindow ? ActiveViewportWindow->GetViewportContext() : nullptr;
}

SWindow* FWindowManager::GetWindowAtPoint(const FPoint& Point) const
{
	for (SWindow* Window : Windows)
	{
		if (Window && Window->ISHover(Point))
		{
			return Window->GetWindow(Point);
		}
	}
	return nullptr;
}

void FWindowManager::HandleMessage(FCore* Core, HWND MainWindowHwnd, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	if (!Core)
	{
		return;
	}

	if (IsMouseMessage(Msg))
	{
		POINT MousePoint = { static_cast<LONG>(GET_X_LPARAM(LParam)), static_cast<LONG>(GET_Y_LPARAM(LParam)) };
		if ((Msg == WM_MOUSEWHEEL || Msg == WM_MOUSEHWHEEL) && MainWindowHwnd)
		{
			::ScreenToClient(MainWindowHwnd, &MousePoint);
		}

		UpdateViewportInteractionState(MousePoint.x, MousePoint.y);
		if (Msg == WM_LBUTTONDOWN || Msg == WM_RBUTTONDOWN || Msg == WM_MBUTTONDOWN)
		{
			SetActiveViewportWindow(FindHoveredViewportWindow(MousePoint.x, MousePoint.y));
			SetCapturingViewportWindow(ActiveViewportWindow);
		}
		else if ((Msg == WM_LBUTTONUP || Msg == WM_RBUTTONUP || Msg == WM_MBUTTONUP) && !AreAnyMouseButtonsDown())
		{
			SetCapturingViewportWindow(nullptr);
		}
	}

	SViewportWindow* TargetViewportWindow = ResolveInputViewportWindow(Msg);
	if (!TargetViewportWindow)
	{
		return;
	}

	FViewportContext* ViewportContext = TargetViewportWindow->GetViewportContext();
	if (!ViewportContext)
	{
		return;
	}

	ViewportContext->HandleMessage(Core, Hwnd, Msg, WParam, LParam);
}

void FWindowManager::ProcessCameraInput(FCore* Core, float DeltaTime)
{
	if (!Core)
	{
		return;
	}

	SViewportWindow* InputOwner = GetInputOwnerViewportWindow();
	if (!InputOwner)
	{
		return;
	}

	FViewportContext* ViewportContext = InputOwner->GetViewportContext();
	FViewportClient* ViewportClient = ViewportContext ? ViewportContext->GetViewportClient() : nullptr;
	if (ViewportClient)
	{
		ViewportClient->ProcessCameraInput(Core, DeltaTime);
	}
}

void FWindowManager::Tick(FCore* Core, float DeltaTime)
{
	for (SWindow* Window : Windows)
	{
		if (Window)
		{
			Window->Tick(Core, DeltaTime);
		}
	}
}

void FWindowManager::DrawWindows(FCore* Core, FRenderCommandQueue& CommandQueue) const
{
	for (SWindow* Window : Windows)
	{
		if (Window)
		{
			Window->Draw(Core, CommandQueue);
		}
	}
}

void FWindowManager::ClearViewportState()
{
	ActiveViewportWindow = nullptr;
	HoveredViewportWindow = nullptr;
	CapturingViewportWindow = nullptr;
}

void FWindowManager::UpdateViewportInteractionState(int32 WindowMouseX, int32 WindowMouseY)
{
	HoveredViewportWindow = FindHoveredViewportWindow(WindowMouseX, WindowMouseY);

	for (SWindow* Window : Windows)
	{
		VisitViewportWindows(
			Window,
			[this, WindowMouseX, WindowMouseY](SViewportWindow* ViewportWindow)
			{
				FViewportContext* ViewportContext = ViewportWindow->GetViewportContext();
				if (!ViewportContext)
				{
					return;
				}

				ViewportContext->UpdateInteractionState(
					WindowMouseX,
					WindowMouseY,
					ViewportWindow == ActiveViewportWindow,
					ViewportWindow == CapturingViewportWindow);
			});
	}
}

SViewportWindow* FWindowManager::FindHoveredViewportWindow(int32 WindowMouseX, int32 WindowMouseY) const
{
	SWindow* Window = GetWindowAtPoint(FPoint(static_cast<float>(WindowMouseX), static_cast<float>(WindowMouseY)));
	if (!Window)
	{
		return nullptr;
	}

	SViewportWindow* ViewportWindow = dynamic_cast<SViewportWindow*>(Window);
	if (!ViewportWindow)
	{
		return nullptr;
	}

	FViewportContext* ViewportContext = ViewportWindow->GetViewportContext();
	return (ViewportContext && ViewportContext->AcceptsInput()) ? ViewportWindow : nullptr;
}

SViewportWindow* FWindowManager::ResolveInputViewportWindow(UINT Msg) const
{
	if (IsKeyboardMessage(Msg))
	{
		return ActiveViewportWindow;
	}

	if (!IsMouseMessage(Msg))
	{
		return ActiveViewportWindow;
	}

	if (Msg == WM_MOUSEWHEEL || Msg == WM_MOUSEHWHEEL)
	{
		return CapturingViewportWindow ? CapturingViewportWindow : ActiveViewportWindow;
	}

	if (CapturingViewportWindow)
	{
		return CapturingViewportWindow;
	}

	if (HoveredViewportWindow)
	{
		return HoveredViewportWindow;
	}

	return ActiveViewportWindow;
}

SViewportWindow* FWindowManager::GetInputOwnerViewportWindow() const
{
	auto IsValidInputOwner = [](SViewportWindow* ViewportWindow)
	{
		FViewportContext* ViewportContext = ViewportWindow ? ViewportWindow->GetViewportContext() : nullptr;
		return ViewportContext != nullptr && ViewportContext->AcceptsInput() && ViewportContext->GetViewportClient() != nullptr;
	};

	if (IsValidInputOwner(CapturingViewportWindow))
	{
		return CapturingViewportWindow;
	}

	if (IsValidInputOwner(ActiveViewportWindow))
	{
		return ActiveViewportWindow;
	}

	if (IsValidInputOwner(HoveredViewportWindow))
	{
		return HoveredViewportWindow;
	}

	return nullptr;
}

void FWindowManager::SetActiveViewportWindow(SViewportWindow* InViewportWindow)
{
	if (ActiveViewportWindow == InViewportWindow)
	{
		if (FViewportContext* ViewportContext = GetActiveViewportContext())
		{
			ViewportContext->SetActive(true);
		}
		return;
	}

	FViewportContext* PreviousContext = GetActiveViewportContext();
	if (PreviousContext)
	{
		PreviousContext->SetActive(false);
	}

	ActiveViewportWindow = InViewportWindow;
	if (FViewportContext* NewContext = GetActiveViewportContext())
	{
		NewContext->SetActive(true);
	}

	if (OnActiveViewportChanged)
	{
		OnActiveViewportChanged(GetActiveViewportContext(), PreviousContext);
	}
}

void FWindowManager::SetCapturingViewportWindow(SViewportWindow* InViewportWindow)
{
	if (CapturingViewportWindow == InViewportWindow)
	{
		return;
	}

	if (CapturingViewportWindow)
	{
		if (FViewportContext* ViewportContext = CapturingViewportWindow->GetViewportContext())
		{
			ViewportContext->SetCapturing(false);
		}
	}

	CapturingViewportWindow = InViewportWindow;
	if (CapturingViewportWindow)
	{
		if (FViewportContext* ViewportContext = CapturingViewportWindow->GetViewportContext())
		{
			ViewportContext->SetCapturing(true);
		}
	}
}

bool FWindowManager::AreAnyMouseButtonsDown() const
{
	if (!InputManager)
	{
		return false;
	}

	return InputManager->IsMouseButtonDown(FInputManager::MOUSE_LEFT) ||
		InputManager->IsMouseButtonDown(FInputManager::MOUSE_RIGHT) ||
		InputManager->IsMouseButtonDown(FInputManager::MOUSE_MIDDLE);
}
