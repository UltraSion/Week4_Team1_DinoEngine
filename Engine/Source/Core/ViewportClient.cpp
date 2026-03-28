#include "ViewportClient.h"
#include "World/World.h"
#include "Core/Core.h"
#include "Camera/Camera.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Material.h"
#include "World/Level.h"
#include "Debug/EngineLog.h"
#include "Component/UUIDBillboardComponent.h"
#include "Component/SubUVComponent.h"
#include "Core/FEngine.h"
#include "Component/TextComponent.h"

void FViewportClient::Attach(FCore* Core, FRenderer* Renderer)
{
}

void FViewportClient::Detach(FCore* Core, FRenderer* Renderer)
{
}

void FViewportClient::Tick(FCore* Core, float DeltaTime)
{
	Tick(DeltaTime);
}

void FViewportClient::Initialize(FInputManager* InInput, FEnhancedInputManager* InEnhancedInput)
{
	InputManager = InInput;
	EnhancedInput = InEnhancedInput;
	SetupInputBindings();
}

void FViewportClient::Cleanup()
{
	if (EnhancedInput && CameraContext)
	{
		EnhancedInput->RemoveMappingContext(CameraContext);
	}
	delete CameraContext;
	CameraContext = nullptr;
	EnhancedInput = nullptr;
}

void FViewportClient::Tick(float DeltaTime)
{
	CurrentDeltaTime = DeltaTime;
}

void FViewportClient::SetupInputBindings()
{
}

void FViewportClient::HandleMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
}

ULevel* FViewportClient::ResolveLevel(FCore* Core) const
{
	return Core ? Core->GetActiveLevel() : nullptr;
}

UWorld* FViewportClient::ResolveWorld(FCore* Core) const
{
	return Core ? Core->GetActiveWorld() : nullptr;
}

void FViewportClient::BuildRenderCommands(TArray<AActor*>& InActors, FRenderCommandQueue& OutQueue)
{
	FFrustum Frustum;
	const FMatrix ViewProjection = CameraTransform.GetViewMatrix() * CameraTransform.GetProjectionMatrix();
	Frustum.ExtractFromVP(ViewProjection);

	OutQueue.ViewMatrix = CameraTransform.GetViewMatrix();
	OutQueue.ProjectionMatrix = CameraTransform.GetProjectionMatrix();
	RenderCollector.CollectRenderCommands(InActors, Frustum, ShowFlags, OutQueue);
}

void FViewportClient::HandleFileDoubleClick(const FString& FilePath)
{
}

void FViewportClient::HandleFileDropOnViewport(const FString& FilePath)
{
}

void FGameViewportClient::Attach(FCore* Core, FRenderer* Renderer)
{
	if (Renderer)
	{
		Renderer->ClearViewportCallbacks();
	}
}

void FGameViewportClient::Detach(FCore* Core, FRenderer* Renderer)
{
	if (Renderer)
	{
		Renderer->ClearViewportCallbacks();
	}
}
