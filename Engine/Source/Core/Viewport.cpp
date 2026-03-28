#include "Viewport.h"

FViewport::FViewport(uint32 InTopLeftX, uint32 InTopLeftY, uint32 InWidth, uint32 InHeight)
{
	SetRect(static_cast<int32>(InTopLeftX), static_cast<int32>(InTopLeftY), InWidth, InHeight);
}

FViewport::~FViewport() = default;

bool FViewport::GetMousePositionInViewport(int32 WindowMouseX, int32 WindowMouseY, int32& OutViewportX, int32& OutViewportY, int32& OutWidth, int32& OutHeight) const
{
	if (!ContainsPoint(WindowMouseX, WindowMouseY))
	{
		return false;
	}

	OutViewportX = WindowMouseX - TopLeftX;
	OutViewportY = WindowMouseY - TopLeftY;
	OutWidth = static_cast<int32>(Width);
	OutHeight = static_cast<int32>(Height);
	return true;
}

bool FViewport::ContainsPoint(int32 WindowMouseX, int32 WindowMouseY) const
{
	if (!bVisible || Width == 0 || Height == 0)
	{
		return false;
	}

	if (WindowMouseX < TopLeftX || WindowMouseY < TopLeftY)
	{
		return false;
	}

	const int32 Right = TopLeftX + static_cast<int32>(Width);
	const int32 Bottom = TopLeftY + static_cast<int32>(Height);
	return WindowMouseX < Right && WindowMouseY < Bottom;
}

void FViewport::SetRect(int32 InTopLeftX, int32 InTopLeftY, uint32 InWidth, uint32 InHeight)
{
	TopLeftX = InTopLeftX;
	TopLeftY = InTopLeftY;
	RenderTopLeftX = InTopLeftX;
	RenderTopLeftY = InTopLeftY;
	Width = InWidth;
	Height = InHeight;
	bVisible = (Width > 0 && Height > 0);

	if (!bVisible)
	{
		bHovered = false;
		bFocused = false;
	}
}

void FViewport::SetRenderOffset(int32 InRenderTopLeftX, int32 InRenderTopLeftY)
{
	RenderTopLeftX = InRenderTopLeftX;
	RenderTopLeftY = InRenderTopLeftY;
}

D3D11_VIEWPORT FViewport::GetD3D11Viewport() const
{
	D3D11_VIEWPORT Viewport = {};
	Viewport.TopLeftX = static_cast<float>(RenderTopLeftX);
	Viewport.TopLeftY = static_cast<float>(RenderTopLeftY);
	Viewport.Width = static_cast<float>(Width);
	Viewport.Height = static_cast<float>(Height);
	Viewport.MaxDepth = 1.f;
	Viewport.MinDepth = 0.f;
	return Viewport;
}
