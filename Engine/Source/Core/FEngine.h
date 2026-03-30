#pragma once
#include "CoreMinimal.h"
#include "World/LevelTypes.h"
#include "Windows.h"
#include "Core/Core.h"
#include "Core/ViewportContext.h"
#include "ViewportClient.h"
#include <memory>
#include "UI/WindowManager.h"
#include "Math/Rect.h"

class FWindowApplication;
class FWindow;
class FWindowManager;
class FRect;

class ENGINE_API FEngine
{
public:
	FEngine() = default;
	virtual ~FEngine();

	FEngine(const FEngine&) = delete;
	FEngine& operator=(const FEngine&) = delete;

	bool Initialize(HINSTANCE hInstance, const wchar_t* Title, int32 Width, int32 Height);
	void Run();
	virtual void Shutdown();

	FCore* GetCore() const { return Core.get(); }
	FRenderCommandQueue& GetCommandQueue() { return CommandQueue; }
	FWindowApplication* GetApp() const { return App; }
	FWindowManager& GetWindowManager() { return WindowManager; }
	void SetViewportLayoutBounds(int32 InTopLeftX, int32 InTopLeftY, uint32 InWidth, uint32 InHeight);
	FViewportContext* CreateContext(FRect InRect);

protected:
	virtual void PreInitialize() {}
	virtual void PostInitialize() {}
	virtual void Input(float DeltaTime);
	virtual void ProcessInput(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	virtual void Tick(float DeltaTime);
	virtual void Render();
	virtual ELevelType GetStartupLevelType() const { return ELevelType::Game; }
	virtual FViewportClient* CreateViewportClient() = 0;
	virtual void OnActiveViewportContextChanged(FViewportContext* NewActiveContext, FViewportContext* PreviousActiveContext) {}

	FWindowApplication* App = nullptr;
	FWindow* MainWindow = nullptr;
	std::unique_ptr<FCore> Core;
	FRenderCommandQueue CommandQueue;
	FInputManager* InputManager = nullptr;
	FEnhancedInputManager* EnhancedInput = nullptr;
	FWindowManager WindowManager;

private:
	bool OnInput(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	void OnResize(int32 Width, int32 Height);
};
