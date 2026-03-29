#pragma once
#include "EngineAPI.h"
#include "SWindow.h"
#include "Core/ViewportContext.h"
#include <memory>

class ENGINE_API SViewportWindow : public SWindow
{
	std::unique_ptr<FViewportContext> ViewportContext;

public:
	explicit SViewportWindow(std::unique_ptr<FViewportContext> InViewportContext);
	~SViewportWindow() override;

	FViewportContext* GetViewportContext() const { return ViewportContext.get(); }
	virtual void Tick(FCore* Core, float DeltaTime) override;
	virtual void Draw(FCore* Core, FRenderCommandQueue& CommandQueue) override;
	virtual void OnResize() override;
};

