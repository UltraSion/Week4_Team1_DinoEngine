#pragma once

#include "EngineAPI.h"

enum class EEngineLaunchMode : unsigned char
{
	Default,
	ObjViewer
};

class ENGINE_API FLaunchOptions
{
public:
	static void SetLaunchMode(EEngineLaunchMode InLaunchMode);
	static EEngineLaunchMode GetLaunchMode();
	static bool IsObjViewerMode();
};
