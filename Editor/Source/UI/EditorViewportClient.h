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

enum class EEditorViewportType : uint8_t
{
	Perspective,
	Top,
	Front,
	Right
};

class FEditorViewportClient : public FViewportClient
{
public:
	FEditorViewportClient(FEditorUI& InEditorUI, FWindow* InMainWindow, TArray<AActor*>& InSeletedActors, EEditorViewportType InViewportType, ELevelType InWorldType);

	void Attach(FCore* Core) override;
	void Detach() override;
	void HandleMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam) override;
	UWorld* ResolveWorld(FCore* Core) const override;
	const char* GetViewportLabel() const override;
	EGizmoMode GetGizmoMode() const { return Gizmo.GetMode(); }
	void SetGizmoMode(EGizmoMode InMode) { Gizmo.SetMode(InMode); }
	ERenderMode GetRenderMode() { return RenderMode; }
	void SetRenderMode(ERenderMode InRenderMode) { RenderMode = InRenderMode; }
	EEditorViewportType GetViewportType() const { return ViewportType; }
	bool SupportsEditingTools() const { return WorldType == ELevelType::Editor; }

	void HandleFileDoubleClick(const FString& FilePath) override;
	void HandleFileDropOnViewport(const FString& FilePath) override;
	void BuildRenderCommands(TArray<AActor*>& InActors, FRenderCommandQueue& OutQueue) override;
	void PostRender(FCore* Core, FRenderer* Renderer) override;
	void ProcessCameraInput(FCore* Core, float DeltaTime) override;
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

	std::unique_ptr<FMeshData> GridMesh;
	std::shared_ptr<FMaterial> GridMaterial;
	void ConfigureDefaultView();
	void CreateGridResource(FRenderer* Renderer);
	float GridSize = 10.0f;
	float LineThickness = 1.0f;
	bool bShowGrid = true;
	EEditorViewportType ViewportType = EEditorViewportType::Perspective;
};
