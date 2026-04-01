#include "LaunchOptions.h"

namespace
{
	EEngineLaunchMode GLaunchMode = EEngineLaunchMode::Default;
}

void FLaunchOptions::SetLaunchMode(EEngineLaunchMode InLaunchMode)
{
	GLaunchMode = InLaunchMode;
}

EEngineLaunchMode FLaunchOptions::GetLaunchMode()
{
	return GLaunchMode;
}

bool FLaunchOptions::IsObjViewerMode()
{
	return GLaunchMode == EEngineLaunchMode::ObjViewer;
}
