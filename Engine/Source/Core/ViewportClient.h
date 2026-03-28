#pragma once

#include "EngineAPI.h"
#include "Windows.h"
#include "Types/String.h"
#include "ShowFlags.h"
#include "Renderer/RenderCommand.h"
#include "Scene/RenderCollector.h"
#include "Input/InputManager.h"
#include "Camera/Camera.h"
#include "Input/InputAction.h"
#include "Input/InputManager.h"
#include "Input/EnhancedInputManager.h"

class CCore;
class CRenderer;
class UScene;
class FFrustum;
class UPrimitiveComponent;
struct FRenderCommandQueue;
struct CCamera;

class UWorld;
class ENGINE_API IViewportClient
{
public:
	IViewportClient(UWorld* InWorld) : World(InWorld) {}
	virtual ~IViewportClient() = default;

	virtual void Attach(CCore* Core, CRenderer* Renderer);
	virtual void Detach(CCore* Core, CRenderer* Renderer);
	virtual void Tick(CCore* Core, float DeltaTime);
	virtual void HandleMessage(CCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	virtual UScene* ResolveScene(CCore* Core) const;
	virtual UWorld* ResolveWorld(CCore* Core) const;
	FShowFlags& GetShowFlags() { return ShowFlags; }
	const FShowFlags& GetShowFlags() const { return ShowFlags; }
	virtual void BuildRenderCommands(TArray<AActor*>& InActor, FRenderCommandQueue& OutQueue);
	/** 입력 처리는 원래 Viewport 에서 처리하는게 맞는데 구조상 여기다 넣음 */
	virtual void HandleFileDoubleClick(const FString& FilePath);
	virtual void HandleFileDropOnViewport(const FString& FilePath);
	CCamera* GetCamera() { return &CameraTransform; }
protected:
	CCamera CameraTransform;
	FShowFlags ShowFlags;
	FSceneRenderCollector RenderCollector;
	const UWorld* World;


public:
	//// ← EnhancedInput 포인터 추가
	//void Initialize(CInputManager* InInput, CEnhancedInputManager* InEnhancedInput);
	//void Cleanup();

	void Tick(float DeltaTime);

	int32 TopLeftX = 0, TopLeftY = 0, Width = 0, Height = 0;

public:
	virtual void Initialize(CInputManager* InInput, CEnhancedInputManager* InEnhancedInput);
	virtual void SetupInputBindings(); // 기존 ProcessCameraInput 대체
	virtual void Cleanup();
	CInputManager* InputManager = nullptr;
	CEnhancedInputManager* EnhancedInput = nullptr;
	FInputMappingContext* CameraContext = nullptr; // 소멸자에서 정리

	FInputAction MoveForwardAction{ "MoveForward", EInputActionValueType::Float };
	FInputAction MoveRightAction{ "MoveRight",   EInputActionValueType::Float };
	FInputAction MoveUpAction{ "MoveUp",      EInputActionValueType::Float };
	FInputAction LookXAction{ "LookX",       EInputActionValueType::Float };
	FInputAction LookYAction{ "LookY",       EInputActionValueType::Float };



	float CurrentDeltaTime = 0.0f; // 콜백에서 DeltaTime 쓰기 위해 보관
};

class ENGINE_API CGameViewportClient : public IViewportClient
{
public:
	CGameViewportClient(UWorld* InWorld) : IViewportClient(InWorld) {}
	void Attach(CCore* Core, CRenderer* Renderer) override;
	void Detach(CCore* Core, CRenderer* Renderer) override;
};
