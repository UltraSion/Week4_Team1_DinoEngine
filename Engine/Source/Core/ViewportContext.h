#pragma once

#include "CoreMinimal.h"
#include "Viewport.h"
#include "ViewportClient.h"
#include <memory>

class FCore;

struct ENGINE_API FViewportContext
{
	std::unique_ptr<FViewport> Viewport;
	std::unique_ptr<FViewportClient> ViewportClient;
	bool bEnabled = true;
	bool bAcceptsInput = true;

	FViewportContext();
	FViewportContext(std::unique_ptr<FViewport> InViewport, std::unique_ptr<FViewportClient> InViewportClient);
	FViewportContext(FViewportContext&& Other) noexcept;
	FViewportContext& operator=(FViewportContext&& Other) noexcept;
	~FViewportContext();

	bool IsValid() const;
	bool IsEnabled() const;
	void SetEnabled(bool bInEnabled);
	bool AcceptsInput() const;
	void SetAcceptsInput(bool bInAcceptsInput);
	FViewport* GetViewport() const;
	FViewportClient* GetViewportClient() const;
	bool IsInteractive() const;
	void Tick(FCore* Core, float DeltaTime);
	void HandleMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	void PrepareView();
	void Render(FCore* Core);
	void Cleanup();
};
