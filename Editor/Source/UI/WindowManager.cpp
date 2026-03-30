#include "WindowManager.h"
#include "Core/Viewport.h"
#include "Core/ViewportClient.h"
#include "Core/ViewportContext.h"
#include "Input/InputManager.h"
#include <windowsx.h>

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

bool FWindowManager::HandleMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	if (!Core)
	{
		return false;
	}

	const bool bIsMouseMessage =
		Msg == WM_MOUSEMOVE ||
		Msg == WM_LBUTTONDOWN ||
		Msg == WM_LBUTTONUP ||
		Msg == WM_RBUTTONDOWN ||
		Msg == WM_RBUTTONUP ||
		Msg == WM_MBUTTONDOWN ||
		Msg == WM_MBUTTONUP ||
		Msg == WM_MOUSEWHEEL ||
		Msg == WM_MOUSEHWHEEL;

	if (bIsMouseMessage)
	{
		const FPoint MousePoint(static_cast<float>(GET_X_LPARAM(LParam)), static_cast<float>(GET_Y_LPARAM(LParam)));
		SWindow* TargetWindow = GetWindowAtPoint(MousePoint);
		return TargetWindow ? TargetWindow->HandleMessage(Core, Hwnd, Msg, WParam, LParam) : false;
	}

	for (SWindow* Window : Windows)
	{
		if (Window && Window->HandleMessage(Core, Hwnd, Msg, WParam, LParam))
		{
			return true;
		}
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

void FWindowManager::DrawWindows() const
{
	for (SWindow* Window : Windows)
	{
		if (Window)
		{
			Window->Draw();
		}
	}
}

void FWindowManager::AddWindow(SWindow* NewWindow)
{
	if (!NewWindow)
	{
		return;
	}

	Windows.push_back(NewWindow);
}
