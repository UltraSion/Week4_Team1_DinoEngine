#include "WindowManager.h"
#include "Core/Viewport.h"
#include "Core/ViewportClient.h"
#include "Core/ViewportContext.h"
#include "Input/InputManager.h"
#include "ViewportWindow.h"
#include <windowsx.h>

namespace
{
	bool IsMouseMessage(UINT Msg)
	{
		switch (Msg)
		{
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONDBLCLK:
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
		case WM_IME_CHAR:
			return true;
		default:
			return false;
		}
	}

	bool IsMouseButtonDownMessage(UINT Msg)
	{
		return
			Msg == WM_LBUTTONDOWN ||
			Msg == WM_LBUTTONDBLCLK ||
			Msg == WM_RBUTTONDOWN ||
			Msg == WM_RBUTTONDBLCLK ||
			Msg == WM_MBUTTONDOWN ||
			Msg == WM_MBUTTONDBLCLK;
	}

	bool IsMouseButtonUpMessage(UINT Msg)
	{
		return
			Msg == WM_LBUTTONUP ||
			Msg == WM_RBUTTONUP ||
			Msg == WM_MBUTTONUP;
	}

	bool IsMouseWheelMessage(UINT Msg)
	{
		return Msg == WM_MOUSEWHEEL || Msg == WM_MOUSEHWHEEL;
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
	HoveredWindow = nullptr;
	PressedWindow = nullptr;
	MouseCaptureWindow = nullptr;
	KeyboardFocusWindow = nullptr;
	ActiveViewportWindow = nullptr;
	for (SWindow* Window : PendingDestroyWindows)
	{
		delete Window;
	}
	PendingDestroyWindows.clear();
	for (SWindow* Window : Windows)
	{
		delete Window;
	}
	Windows.clear();
	InputManager = nullptr;
	EnhancedInputManager = nullptr;
}

void FWindowManager::SetRootRect(const FRect& InRect)
{
	if (Windows.empty() || !Windows[0])
	{
		return;
	}

	Windows[0]->SetRect(InRect);
}

void FWindowManager::CheckParent()
{
	for(int i = 0; i < Windows.size(); ++i)
	{
		if (!Windows[i])
			continue;

		if(!Windows[i]->GetParent())
			continue;

		Windows[i] = Windows[i]->GetParent();
	}
}

FPoint FWindowManager::ScreenToClient(HWND Hwnd, const FPoint& ScreenPoint) const
{
	POINT ClientPoint
	{
		static_cast<LONG>(ScreenPoint.X),
		static_cast<LONG>(ScreenPoint.Y)
	};

	if (Hwnd)
	{
		::ScreenToClient(Hwnd, &ClientPoint);
	}

	return FPoint(static_cast<float>(ClientPoint.x), static_cast<float>(ClientPoint.y));
}

bool FWindowManager::TryGetClientMousePoint(HWND Hwnd, UINT Msg, LPARAM LParam, FPoint& OutPoint) const
{
	if (IsMouseWheelMessage(Msg))
	{
		const FPoint ScreenPoint(static_cast<float>(GET_X_LPARAM(LParam)), static_cast<float>(GET_Y_LPARAM(LParam)));
		OutPoint = ScreenToClient(Hwnd, ScreenPoint);
		return true;
	}

	if (!IsMouseMessage(Msg))
	{
		return false;
	}

	OutPoint = FPoint(static_cast<float>(GET_X_LPARAM(LParam)), static_cast<float>(GET_Y_LPARAM(LParam)));
	return true;
}

SViewportWindow* FWindowManager::FindViewportWindow(SWindow* Window) const
{
	SWindow* Current = Window;
	while (Current)
	{
		if (SViewportWindow* ViewportWindow = dynamic_cast<SViewportWindow*>(Current))
		{
			return ViewportWindow;
		}

		Current = Current->GetParent();
	}

	return nullptr;
}

void FWindowManager::SetHoveredWindow(SWindow* NewHoveredWindow)
{
	if (HoveredWindow == NewHoveredWindow)
	{
		return;
	}

	if (SViewportWindow* PreviousViewportWindow = FindViewportWindow(HoveredWindow))
	{
		if (FViewportContext* PreviousContext = PreviousViewportWindow->GetViewportContext())
		{
			if (FViewport* PreviousViewport = PreviousContext->GetViewport())
			{
				PreviousViewport->SetHovered(false);
			}
		}
	}

	HoveredWindow = NewHoveredWindow;

	if (SViewportWindow* HoveredViewportWindow = FindViewportWindow(HoveredWindow))
	{
		if (FViewportContext* HoveredContext = HoveredViewportWindow->GetViewportContext())
		{
			if (FViewport* HoveredViewport = HoveredContext->GetViewport())
			{
				HoveredViewport->SetHovered(true);
			}
		}
	}
}

void FWindowManager::SetPressedWindow(SWindow* NewPressedWindow)
{
	PressedWindow = NewPressedWindow;
}

void FWindowManager::SetMouseCaptureWindow(HWND Hwnd, SWindow* NewMouseCaptureWindow)
{
	if (MouseCaptureWindow == NewMouseCaptureWindow)
	{
		return;
	}

	if (SViewportWindow* PreviousViewportWindow = FindViewportWindow(MouseCaptureWindow))
	{
		if (FViewportContext* PreviousContext = PreviousViewportWindow->GetViewportContext())
		{
			PreviousContext->SetCapturing(false);
		}
	}

	MouseCaptureWindow = NewMouseCaptureWindow;

	if (MouseCaptureWindow && Hwnd)
	{
		::SetCapture(Hwnd);
	}
	else if (!MouseCaptureWindow && ::GetCapture() == Hwnd)
	{
		::ReleaseCapture();
	}

	if (SViewportWindow* CaptureViewportWindow = FindViewportWindow(MouseCaptureWindow))
	{
		if (FViewportContext* CaptureContext = CaptureViewportWindow->GetViewportContext())
		{
			CaptureContext->SetCapturing(true);
		}
	}
}

void FWindowManager::ReleaseMouseCapture(HWND Hwnd)
{
	SetMouseCaptureWindow(Hwnd, nullptr);
}

void FWindowManager::SetKeyboardFocusWindow(SWindow* NewKeyboardFocusWindow)
{
	if (KeyboardFocusWindow == NewKeyboardFocusWindow)
	{
		return;
	}

	if (SViewportWindow* PreviousViewportWindow = FindViewportWindow(KeyboardFocusWindow))
	{
		if (FViewportContext* PreviousContext = PreviousViewportWindow->GetViewportContext())
		{
			if (FViewport* PreviousViewport = PreviousContext->GetViewport())
			{
				PreviousViewport->SetFocused(false);
			}
		}
	}

	KeyboardFocusWindow = NewKeyboardFocusWindow;

	if (SViewportWindow* FocusViewportWindow = FindViewportWindow(KeyboardFocusWindow))
	{
		if (FViewportContext* FocusContext = FocusViewportWindow->GetViewportContext())
		{
			if (FViewport* FocusViewport = FocusContext->GetViewport())
			{
				FocusViewport->SetFocused(true);
			}
		}
	}
}

void FWindowManager::SetActiveViewportWindow(SViewportWindow* NewActiveViewportWindow)
{
	if (ActiveViewportWindow == NewActiveViewportWindow)
	{
		return;
	}

	if (ActiveViewportWindow)
	{
		if (FViewportContext* PreviousContext = ActiveViewportWindow->GetViewportContext())
		{
			PreviousContext->SetActive(false);
		}
	}

	ActiveViewportWindow = NewActiveViewportWindow;
	if (ActiveViewportWindow)
	{
		if (FViewportContext* NewContext = ActiveViewportWindow->GetViewportContext())
		{
			NewContext->SetActive(true);
		}
	}
}

bool FWindowManager::HasAnyMouseButtonPressed() const
{
	return
		InputManager &&
		(
			InputManager->IsMouseButtonDown(FInputManager::MOUSE_LEFT) ||
			InputManager->IsMouseButtonDown(FInputManager::MOUSE_RIGHT) ||
			InputManager->IsMouseButtonDown(FInputManager::MOUSE_MIDDLE)
		);
}

bool FWindowManager::IsInWindowSubtree(SWindow* Window, SWindow* CandidateAncestor) const
{
	SWindow* Current = Window;
	while (Current)
	{
		if (Current == CandidateAncestor)
		{
			return true;
		}

		Current = Current->GetParent();
	}

	return false;
}

void FWindowManager::FlushPendingDestroyWindows()
{
	for (SWindow* WindowToDestroy : PendingDestroyWindows)
	{
		if (!WindowToDestroy)
		{
			continue;
		}

		if (IsInWindowSubtree(HoveredWindow, WindowToDestroy))
		{
			HoveredWindow = nullptr;
		}
		if (IsInWindowSubtree(PressedWindow, WindowToDestroy))
		{
			PressedWindow = nullptr;
		}
		if (IsInWindowSubtree(MouseCaptureWindow, WindowToDestroy))
		{
			MouseCaptureWindow = nullptr;
		}
		if (IsInWindowSubtree(KeyboardFocusWindow, WindowToDestroy))
		{
			KeyboardFocusWindow = nullptr;
		}
		if (IsInWindowSubtree(ActiveViewportWindow, WindowToDestroy))
		{
			ActiveViewportWindow = nullptr;
		}

		delete WindowToDestroy;
	}

	PendingDestroyWindows.clear();
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

bool FWindowManager::RouteMouseMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	FPoint ClientPoint;
	if (!TryGetClientMousePoint(Hwnd, Msg, LParam, ClientPoint))
	{
		return false;
	}

	const LPARAM RoutedLParam = MAKELPARAM(
		static_cast<short>(ClientPoint.X),
		static_cast<short>(ClientPoint.Y));
	SWindow* HitWindow = GetWindowAtPoint(ClientPoint);
	SetHoveredWindow(HitWindow);

	if (IsMouseButtonDownMessage(Msg))
	{
		if (!HitWindow)
		{
			return false;
		}

		SetPressedWindow(HitWindow);
		SetKeyboardFocusWindow(HitWindow);
		SetMouseCaptureWindow(Hwnd, HitWindow);
		if (SViewportWindow* TargetViewportWindow = FindViewportWindow(HitWindow))
		{
			SetActiveViewportWindow(TargetViewportWindow);
		}

		return HitWindow->HandleMessage(Core, Hwnd, Msg, WParam, RoutedLParam);
	}

	if (IsMouseButtonUpMessage(Msg))
	{
		SWindow* TargetWindow = MouseCaptureWindow ? MouseCaptureWindow : HitWindow;
		if (!TargetWindow)
		{
			return false;
		}

		const bool bHandled = TargetWindow->HandleMessage(Core, Hwnd, Msg, WParam, RoutedLParam);
		SetPressedWindow(nullptr);
		if (!HasAnyMouseButtonPressed())
		{
			ReleaseMouseCapture(Hwnd);
		}
		return bHandled;
	}

	if (IsMouseWheelMessage(Msg))
	{
		return HitWindow ? HitWindow->HandleMessage(Core, Hwnd, Msg, WParam, RoutedLParam) : false;
	}

	SWindow* TargetWindow = MouseCaptureWindow ? MouseCaptureWindow : HitWindow;
	return TargetWindow ? TargetWindow->HandleMessage(Core, Hwnd, Msg, WParam, RoutedLParam) : false;
}

bool FWindowManager::RouteKeyboardMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	return KeyboardFocusWindow ? KeyboardFocusWindow->HandleMessage(Core, Hwnd, Msg, WParam, LParam) : false;
}

bool FWindowManager::HandleMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	if (!Core)
	{
		return false;
	}

	if (IsMouseMessage(Msg))
	{
		return RouteMouseMessage(Core, Hwnd, Msg, WParam, LParam);
	}

	if (IsKeyboardMessage(Msg))
	{
		return RouteKeyboardMessage(Core, Hwnd, Msg, WParam, LParam);
	}

	return false;
}

void FWindowManager::Tick(float DeltaTime)
{
	for (SWindow* Window : Windows)
	{
		if (Window)
		{
			Window->Tick(DeltaTime);
		}
	}
}

void FWindowManager::RenderWindows() const
{
	for (SWindow* Window : Windows)
	{
		if (Window)
		{
			Window->Render();
		}
	}
}

void FWindowManager::DrawWindows() const
{
	for (SWindow* Window : Windows)
	{
		if (Window)
		{
			Window->Draw();
		}
	}

	const_cast<FWindowManager*>(this)->FlushPendingDestroyWindows();
}

void FWindowManager::AddWindow(SWindow* NewWindow)
{
	if (!NewWindow)
	{
		return;
	}

	Windows.push_back(NewWindow);
}

void FWindowManager::ReplaceWindow(SWindow* OldWindow, SWindow* NewWindow)
{
	if (OldWindow == nullptr || OldWindow == NewWindow)
	{
		return;
	}

	for (SWindow*& Window : Windows)
	{
		if (Window == OldWindow)
		{
			Window = NewWindow;
		}
	}
}

void FWindowManager::QueueDestroyWindow(SWindow* Window)
{
	if (!Window)
	{
		return;
	}

	PendingDestroyWindows.push_back(Window);
}
