#include "ViewportWindow.h"
#include "Core/FEngine.h"

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

void SViewportWindow::Tick(float DeltaTime)
{
	if (ViewportContext && GEngine)
	{
		ViewportContext->Tick(GEngine->GetCore(), DeltaTime);
	}
}

void SViewportWindow::Draw()
{
	if (ViewportContext && GEngine)
	{
		ViewportContext->Render(GEngine->GetCore(), GEngine->GetCommandQueue());
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
