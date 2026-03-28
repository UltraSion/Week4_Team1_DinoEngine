#include "Viewport.h"


FViewport::FViewport(uint32 InTopLeftX, uint32 InTopLeftY, uint32 InWidth, uint32 InHeight) :
	TopLeftX(InTopLeftX),
	TopLeftY(InTopLeftY),
	Width(InWidth),
	Height(InHeight)

{
	EnhancedInput = new FEnhancedInputManager();
}

FViewport::~FViewport()
{
	if (EnhancedInput)
	{
		EnhancedInput->ClearAllMappingContexts();
		delete EnhancedInput;
	}
}

D3D11_VIEWPORT FViewport::GetD3D11Viewport()
{
	D3D11_VIEWPORT Viewport;
	Viewport.TopLeftX = TopLeftX;
	Viewport.TopLeftY = TopLeftY;
	Viewport.Width = Width;
	Viewport.Height = Height;
	Viewport.MaxDepth = 1.f;
	Viewport.MinDepth = 0.f;
	return Viewport;
}