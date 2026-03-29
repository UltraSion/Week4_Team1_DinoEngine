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
struct FRenderCommandQueue;
class SViewportWindow;

class ENGINE_API FWindowManager
{
	TArray<SWindow*> Windows;
	FInputManager* InputManager = nullptr;
	FEnhancedInputManager* EnhancedInputManager = nullptr;
	SViewportWindow* ActiveViewportWindow = nullptr;
	SViewportWindow* HoveredViewportWindow = nullptr;
	SViewportWindow* CapturingViewportWindow = nullptr;
	std::function<void(FViewportContext*, FViewportContext*)> OnActiveViewportChanged;

public:
	~FWindowManager();

	void Initialize(FInputManager* InInputManager, FEnhancedInputManager* InEnhancedInputManager);
	void Shutdown();
	SViewportWindow* CreateViewportWindow(FCore* Core, std::unique_ptr<FViewportClient> ViewportClient, const FRect& InRect);
	void SetRootRect(const FRect& InRect);
	FViewportContext* GetActiveViewportContext() const;
	SWindow* GetWindowAtPoint(const FPoint& Point) const;
	void SetOnActiveViewportContextChanged(std::function<void(FViewportContext*, FViewportContext*)> InCallback)
	{
		OnActiveViewportChanged = std::move(InCallback);
	}
	void HandleMessage(FCore* Core, HWND MainWindowHwnd, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	void ProcessCameraInput(FCore* Core, float DeltaTime);
	void Tick(FCore* Core, float DeltaTime);
	void DrawWindows(FCore* Core, FRenderCommandQueue& CommandQueue) const;

private:
	void ClearViewportState();
	void UpdateViewportInteractionState(int32 WindowMouseX, int32 WindowMouseY);
	SViewportWindow* FindHoveredViewportWindow(int32 WindowMouseX, int32 WindowMouseY) const;
	SViewportWindow* ResolveInputViewportWindow(UINT Msg) const;
	SViewportWindow* GetInputOwnerViewportWindow() const;
	void SetActiveViewportWindow(SViewportWindow* InViewportWindow);
	void SetCapturingViewportWindow(SViewportWindow* InViewportWindow);
	bool AreAnyMouseButtonsDown() const;

	template <typename T, typename... Args>
	T* CreateSWindow(Args&&... args)
	{
		T* NewWindow = new T(std::forward<Args>(args)...);
		Windows.push_back(NewWindow);
		return NewWindow;
	}
};
