#include "Viewport.h"


FViewport::FViewport()
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
