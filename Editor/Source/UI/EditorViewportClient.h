#pragma once

#include "Core/ViewportClient.h"
#include "Gizmo/Gizmo.h"
#include "Picking/Picker.h"
#include "Types/CoreTypes.h"

class FEditorUI;
class FWindow;
class FFrustum;
struct FRenderCommandQueue;

enum class ERenderMode
{
	Lighting,
	NoLighting,
	Wireframe,
};

class FEditorViewportClient : public FViewportClient
{
public:
	FEditorViewportClient(FEditorUI& InEditorUI, FWindow* InMainWindow, TArray<AActor*>& InSeletedActors);

	void Attach(FCore* Core) override;
	void Detach() override;
	void HandleMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam) override;
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
	FEditorUI& EditorUI;
	FWindow* MainWindow = nullptr;
	FPicker Picker;
	mutable FGizmo Gizmo;
	TArray<AActor*>& SeletedActors;

	ERenderMode RenderMode = ERenderMode::Lighting;
	const FString WireframeMaterialName = "M_Wireframe";
	std::shared_ptr<FMaterial> WireFrameMaterial = nullptr;

	int32 ScreenWidth = 0;
	int32 ScreenHeight = 0;
	int32 ScreenMouseX = 0;
	int32 ScreenMouseY = 0;

	std::unique_ptr<FMeshData> GridMesh;
	std::shared_ptr<FMaterial> GridMaterial;
	void CreateGridResource(FRenderer* Renderer);
	float GridSize = 10.0f;
	float LineThickness = 1.0f;
	bool bShowGrid = true;
};
