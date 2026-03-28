#include "ViewportContext.h"
#include "Core.h"

FViewportContext::FViewportContext() = default;

FViewportContext::FViewportContext(std::unique_ptr<FViewport> InViewport, std::unique_ptr<FViewportClient> InViewportClient)
	: Viewport(std::move(InViewport))
	, ViewportClient(std::move(InViewportClient))
{
}

FViewportContext::FViewportContext(FViewportContext&& Other) noexcept = default;

FViewportContext& FViewportContext::operator=(FViewportContext&& Other) noexcept = default;

FViewportContext::~FViewportContext() = default;

bool FViewportContext::IsValid() const
{
	return Viewport != nullptr && ViewportClient != nullptr;
}

bool FViewportContext::IsEnabled() const
{
	return bEnabled && IsValid();
}

void FViewportContext::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
}

bool FViewportContext::AcceptsInput() const
{
	return bAcceptsInput && IsEnabled();
}

void FViewportContext::SetAcceptsInput(bool bInAcceptsInput)
{
	bAcceptsInput = bInAcceptsInput;
}

FViewport* FViewportContext::GetViewport() const
{
	return Viewport.get();
}

FViewportClient* FViewportContext::GetViewportClient() const
{
	return ViewportClient.get();
}

//bool FViewportContext::IsInteractive() const
//{
//	return AcceptsInput() && ViewportClient && ViewportClient->IsViewportInteractive();
//}
//
//void FViewportContext::Tick(FCore* Core, float DeltaTime)
//{
//	if (!Core || !AcceptsInput() || ViewportClient == nullptr)
//	{
//		return;
//	}
//
//	Core->TickViewport(*ViewportClient, DeltaTime);
//}

//void FViewportContext::HandleMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
//{
//	if (!Core || !AcceptsInput() || ViewportClient == nullptr)
//	{
//		return;
//	}
//
//	Core->ProcessInput(*ViewportClient, Hwnd, Msg, WParam, LParam);
//}


void FViewportContext::Cleanup()
{
	if (ViewportClient)
	{
		ViewportClient->Cleanup();
	}
}
