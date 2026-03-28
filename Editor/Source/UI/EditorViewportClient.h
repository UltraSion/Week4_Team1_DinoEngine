#pragma once

#include "Core/ViewportClient.h"
#include "Gizmo/Gizmo.h"
#include "Picking/Picker.h"
#include "Types/CoreTypes.h"

class CEditorUI;
class CWindow;
class FFrustum;
struct FRenderCommandQueue;

enum class ERenderMode
{
	Lighting,
	NoLighting,
	Wireframe,
};

class CEditorViewportClient : public IViewportClient
{
public:
	CEditorViewportClient(CEditorUI& InEditorUI, CWindow* InMainWindow, TArray<AActor*>& InSeletedActors, UWorld* InWorld);

	void Attach(CCore* Core, CRenderer* Renderer) override;
	void Detach(CCore* Core, CRenderer* Renderer) override;
	void Tick(CCore* Core, float DeltaTime) override;
	void HandleMessage(CCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam) override;
	EGizmoMode GetGizmoMode() const { return Gizmo.GetMode(); }
	void SetGizmoMode(EGizmoMode InMode) { Gizmo.SetMode(InMode); }
	ERenderMode GetRenderMode() { return RenderMode; }
	void SetRenderMode(ERenderMode InRenderMode) { RenderMode = InRenderMode; }

	void HandleFileDoubleClick(const FString& FilePath) override;
	void HandleFileDropOnViewport(const FString& FilePath) override;
	void BuildRenderCommands(TArray<AActor*>& InActors, FRenderCommandQueue& OutQueue) override;
	float GetGridSize() const { return GridSize; }
	void SetGridSize(float InSize);
	float GetLineThickness() const { return LineThickness; }
	void SetLineThickness(float InThickness);
	bool IsGridVisible() const { return bShowGrid; }
	void SetGridVisible(bool bVisible) { bShowGrid = bVisible; }
	void SetSelection(TArray<AActor*>& SeletedActorArrayPtr);
	void SetupInputBindings() override;

private:
	FInputMappingContext* CameraContext = nullptr; // 소멸자에서 정리

	// Action 정의 (포인터 아닌 값으로 소유)
	FInputAction MoveForwardAction{ "MoveForward", EInputActionValueType::Float };
	FInputAction MoveRightAction{ "MoveRight",   EInputActionValueType::Float };
	FInputAction MoveUpAction{ "MoveUp",      EInputActionValueType::Float };
	FInputAction LookXAction{ "LookX",       EInputActionValueType::Float };
	FInputAction LookYAction{ "LookY",       EInputActionValueType::Float };

	CEditorUI& EditorUI;
	CWindow* MainWindow = nullptr;
	CPicker Picker;
	mutable CGizmo Gizmo;
	TArray<AActor*>& SeletedActors;

	ERenderMode RenderMode = ERenderMode::Lighting;
	const FString WireframeMaterialName = "M_Wireframe";
	std::shared_ptr<FMaterial> WireFrameMaterial = nullptr;

	int32 ScreenWidth = 0;
	int32 ScreenHeight = 0;
	int32 ScreenMouseX = 0;
	int32 ScreenMouseY = 0;

	// 그리드 렌더링용
	std::unique_ptr<FMeshData> GridMesh;
	std::shared_ptr<FMaterial> GridMaterial;
	void CreateGridResource(CRenderer* Renderer);
	float GridSize = 10.0f;
	float LineThickness = 1.0f;
	bool bShowGrid = true;
};
