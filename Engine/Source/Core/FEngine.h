#pragma once
#include "CoreMinimal.h"
#include "World/LevelTypes.h"
#include "Windows.h"
#include "Core/Core.h"
#include "Core/ViewportContext.h"
#include "ViewportClient.h"
#include <memory>

class FWindowApplication;
class FWindow;

enum class EViewportLayout : uint8_t
{
	Single,
	Quad,
	LeftRight,
	TopBottom
};

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
	FWindowApplication* GetApp() const { return App; }
	FViewportContext* GetActiveViewportContext() const { return ActiveViewportContext; }
	FViewportContext* GetHoveredViewportContext() const { return HoveredViewportContext; }
	FViewportContext* GetCapturingViewportContext() const { return CapturingViewportContext; }
	EViewportLayout GetViewportLayout() const { return ViewportLayout; }
	void SetViewportLayout(EViewportLayout InLayout);
	void SetViewportLayoutBounds(int32 InTopLeftX, int32 InTopLeftY, uint32 InWidth, uint32 InHeight);

protected:
	virtual void PreInitialize() {}
	virtual void PostInitialize() {}
	virtual void Input(float DeltaTime);
	virtual void ProcessInput(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	virtual void Tick(float DeltaTime);
	virtual void Render();
	virtual ELevelType GetStartupLevelType() const { return ELevelType::Game; }
	virtual std::unique_ptr<FViewportClient> CreateViewportClient();
	virtual void ConfigureViewportContext(size_t Index, FViewportContext& Context);
	virtual void UpdateViewportLayout(int32 Width, int32 Height);
	virtual void OnActiveViewportContextChanged(FViewportContext* NewActiveContext, FViewportContext* PreviousActiveContext) {}

	FWindowApplication* App = nullptr;
	FWindow* MainWindow = nullptr;
	std::unique_ptr<FCore> Core;
	TArray<FViewportContext> Viewports;
	FRenderCommandQueue CommandQueue;
	FInputManager* InputManager = nullptr;
	FEnhancedInputManager* EnhancedInput = nullptr;
	EViewportLayout ViewportLayout = EViewportLayout::Quad;
	FViewportContext* ActiveViewportContext = nullptr;
	FViewportContext* HoveredViewportContext = nullptr;
	FViewportContext* CapturingViewportContext = nullptr;
	int32 ViewportLayoutOriginX = 0;
	int32 ViewportLayoutOriginY = 0;
	uint32 ViewportLayoutWidth = 0;
	uint32 ViewportLayoutHeight = 0;
	bool bHasCustomViewportLayoutBounds = false;

private:
	FViewportContext CreateViewportContext(size_t Index);
	void InitializeViewportContexts(size_t Count);
	void CleanupViewportContexts();
	void SetActiveViewportContext(FViewportContext* InViewportContext);
	void SetCapturingViewportContext(FViewportContext* InViewportContext);
	void UpdateViewportInteractionState(int32 WindowMouseX, int32 WindowMouseY);
	FViewportContext* FindHoveredViewportContext(int32 WindowMouseX, int32 WindowMouseY);
	FViewportContext* GetInputOwnerViewportContext() const;
	FViewportContext* ResolveInputViewportContext(UINT Msg) const;
	void RouteInputToViewport(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	void TickViewportContexts(float DeltaTime);
	void RenderAllViewports();
	bool AreAnyMouseButtonsDown() const;
	bool OnInput(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	void OnResize(int32 Width, int32 Height);
};
