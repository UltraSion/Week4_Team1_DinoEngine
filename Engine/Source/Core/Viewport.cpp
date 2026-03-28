#include "Viewport.h"


FViewport::FViewport()
{
	EnhancedInput = new CEnhancedInputManager();
}

FViewport::~FViewport()
{
	if (EnhancedInput)
	{
		EnhancedInput->ClearAllMappingContexts();
		delete EnhancedInput;
	}
}
