#pragma once
#include "CoreMinimal.h"
#include "Windows.h"
#include "SWindow.h"
#include <functional>
#include <memory>

class FViewportClient;
class FViewportContext;
class FInputManager;
class FEnhancedInputManager;
class FCore;
class SViewportWindow;

class FWindowManager
{
	TArray<SWindow*> Windows;
	TArray<SWindow*> PendingDestroyWindows;
	FInputManager* InputManager = nullptr;
	FEnhancedInputManager* EnhancedInputManager = nullptr;
	SWindow* HoveredWindow = nullptr;
	SWindow* PressedWindow = nullptr;
	SWindow* MouseCaptureWindow = nullptr;
	SWindow* KeyboardFocusWindow = nullptr;
	SViewportWindow* ActiveViewportWindow = nullptr;

	FPoint ScreenToClient(HWND Hwnd, const FPoint& ScreenPoint) const;
	bool TryGetClientMousePoint(HWND Hwnd, UINT Msg, LPARAM LParam, FPoint& OutPoint) const;
	SViewportWindow* FindViewportWindow(SWindow* Window) const;
	void SetHoveredWindow(SWindow* NewHoveredWindow);
	void SetPressedWindow(SWindow* NewPressedWindow);
	void SetMouseCaptureWindow(HWND Hwnd, SWindow* NewMouseCaptureWindow);
	void ReleaseMouseCapture(HWND Hwnd);
	void SetKeyboardFocusWindow(SWindow* NewKeyboardFocusWindow);
	void SetActiveViewportWindow(SViewportWindow* NewActiveViewportWindow);
	bool HasAnyMouseButtonPressed() const;
	bool IsInWindowSubtree(SWindow* Window, SWindow* CandidateAncestor) const;
	void FlushPendingDestroyWindows();
	bool RouteMouseMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	bool RouteKeyboardMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);

public:
	~FWindowManager();

	void Initialize(FInputManager* InInputManager, FEnhancedInputManager* InEnhancedInputManager);
	void Shutdown();
	void SetRootRect(const FRect& InRect);
	void CheckParent();
	SWindow* GetWindowAtPoint(const FPoint& Point) const;
	bool HandleMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	void Tick(float DeltaTime);
	void RenderWindows() const;
	void DrawWindows() const;
	void AddWindow(SWindow* NewWindow);
	void ReplaceWindow(SWindow* OldWindow, SWindow* NewWindow);
	void QueueDestroyWindow(SWindow* Window);
};
