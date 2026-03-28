#pragma once
#include "CoreMinimal.h"
#include "EngineAPI.h"
#include "Input/InputAction.h"
#include "Input/InputManager.h"
#include "Input/EnhancedInputManager.h"

class ENGINE_API FViewport
{
public:
	FViewport();
	virtual ~FViewport();

	bool GetMousePositionInViewport(int32 WindowMouseX, int32 WindowMouseY, int32& OutViewportX, int32& OutViewportY, int32& OutWidth, int32& OutHeight) const;
	bool IsHovered() const { return bHovered; }
	bool IsFocused() const { return bFocused; }
	bool IsVisible() const { return bVisible; }

private:

	uint32 OffscreenWidth = 0;
	uint32 OffscreenHeight = 0;
	int32 TopLeftX = 0;
	int32 TopLeftY = 0;
	bool bHovered = false;
	bool bFocused = false;
	bool bVisible = false;

	CInputManager* InputManager = nullptr;
	CEnhancedInputManager* EnhancedInput = nullptr;
	FInputMappingContext* CameraContext = nullptr; // 소멸자에서 정리

	FInputAction MoveForwardAction{ "MoveForward", EInputActionValueType::Float };
	FInputAction MoveRightAction{ "MoveRight",   EInputActionValueType::Float };
	FInputAction MoveUpAction{ "MoveUp",      EInputActionValueType::Float };
	FInputAction LookXAction{ "LookX",       EInputActionValueType::Float };
	FInputAction LookYAction{ "LookY",       EInputActionValueType::Float };

};

