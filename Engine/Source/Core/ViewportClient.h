#pragma once

#include "EngineAPI.h"
#include "Windows.h"
#include "Types/String.h"
#include "ShowFlags.h"
#include "Renderer/RenderCommand.h"
#include "World/RenderCollector.h"
#include "Input/InputManager.h"
#include "Camera/Camera.h"
#include "Input/InputAction.h"
#include "Input/EnhancedInputManager.h"

class FCore;
class FRenderer;
class ULevel;
class FFrustum;
class UPrimitiveComponent;
struct FRenderCommandQueue;
class UWorld;
struct FInputMappingContext;

class ENGINE_API FViewportClient
{
public:
	FViewportClient() = default;
	virtual ~FViewportClient() = default;

	virtual void Attach(FCore* Core);
	virtual void Detach();
	virtual void HandleMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	virtual ULevel* ResolveLevel(FCore* Core) const;
	virtual UWorld* ResolveWorld(FCore* Core) const;
	FShowFlags& GetShowFlags() { return ShowFlags; }
	const FShowFlags& GetShowFlags() const { return ShowFlags; }
	virtual void BuildRenderCommands(TArray<AActor*>& InActors, FRenderCommandQueue& OutQueue);
	virtual void HandleFileDoubleClick(const FString& FilePath);
	virtual void HandleFileDropOnViewport(const FString& FilePath);
	FCamera* GetCamera() { return &CameraTransform; }

	virtual void Initialize(FInputManager* InInput, FEnhancedInputManager* InEnhancedInput);
	virtual void SetupInputBindings();
	virtual void Cleanup();
	virtual void Tick(float DeltaTime);

	int32 TopLeftX = 0;
	int32 TopLeftY = 0;
	int32 Width = 0;
	int32 Height = 0;

protected:
	FCamera CameraTransform;
	FShowFlags ShowFlags;
	FLevelRenderCollector RenderCollector;
	FInputManager* InputManager = nullptr;
	FEnhancedInputManager* EnhancedInput = nullptr;
	FInputMappingContext* CameraContext = nullptr;

	FInputAction MoveForwardAction{ "MoveForward", EInputActionValueType::Float };
	FInputAction MoveRightAction{ "MoveRight", EInputActionValueType::Float };
	FInputAction MoveUpAction{ "MoveUp", EInputActionValueType::Float };
	FInputAction LookXAction{ "LookX", EInputActionValueType::Float };
	FInputAction LookYAction{ "LookY", EInputActionValueType::Float };

	float CurrentDeltaTime = 0.0f;
};

class ENGINE_API FGameViewportClient : public FViewportClient
{
public:
	void Attach(FCore* Core) override;
	void Detach() override;
};
