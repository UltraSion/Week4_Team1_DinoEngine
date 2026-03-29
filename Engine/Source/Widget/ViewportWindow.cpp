#include "ViewportWindow.h"

SViewportWindow::SViewportWindow(std::unique_ptr<FViewportContext> InViewportContext)
	: ViewportContext(std::move(InViewportContext))
{
}

SViewportWindow::~SViewportWindow()
{
	if (ViewportContext)
	{
		ViewportContext->Cleanup();
	}
}

void SViewportWindow::Tick(FCore* Core, float DeltaTime)
{
	if (ViewportContext)
	{
		ViewportContext->Tick(Core, DeltaTime);
	}
}

void SViewportWindow::Draw(FCore* Core, FRenderCommandQueue& CommandQueue)
{
	if (ViewportContext)
	{
		ViewportContext->Render(Core, CommandQueue);
	}
}

void SViewportWindow::OnResize()
{
	if (!ViewportContext)
	{
		return;
	}

	ViewportContext->SetRect(GetRect());
}
