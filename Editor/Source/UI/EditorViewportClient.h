#pragma once

#include "Core/ViewportClient.h"
#include "Gizmo/Gizmo.h"
#include "Picking/Picker.h"
#include "Types/CoreTypes.h"

class FEditorUI;
class FWindow;
class AActor;
class ULevel;
class UWorld;
struct FRenderCommandQueue;
class SViewportWindow;

enum class ERenderMode
{
	Lighting,
	NoLighting,
	Wireframe,
	SolidWireframe,
	UV,
	Normals,
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
	FEditorViewportClient(FEditorUI& InEditorUI, FWindow* InMainWindow, EEditorViewportType InViewportType, ELevelType InWorldType);

	void Attach(FCore* Core) override;
	void Detach() override;
	void HandleMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam) override;
	UWorld* ResolveWorld(FCore* Core) const override;
	const char* GetViewportLabel() const override;
	EGizmoMode GetGizmoMode() const { return Gizmo.GetMode(); }
	void SetGizmoMode(EGizmoMode InMode) { Gizmo.SetMode(InMode); }
	ERenderMode GetRenderMode() const { return RenderMode; }
	void SetRenderMode(ERenderMode InRenderMode) { RenderMode = InRenderMode; }
	EEditorViewportType GetViewportType() const { return ViewportType; }
	void SetViewportType(EEditorViewportType InViewportType);
	bool SupportsEditingTools() const { return WorldType == ELevelType::Editor; }
	virtual void DrawUI() override;
	void DrawCameraOption(FCamera* Camera);

	void HandleFileDoubleClick(const FString& FilePath);
	void HandleFileDropOnViewport(const FString& FilePath);
	void BuildRenderCommands(TArray<AActor*>& InActors, FRenderCommandQueue& OutQueue) override;
	void PostRender(FCore* Core, FRenderer* Renderer) override;
	void Tick(float DeltaTime) override;
	void ProcessCameraInput(FCore* Core, float DeltaTime) override;
	float GetGridSize() const { return GridSize; }
	void SetGridSize(float InSize);
	float GetLineThickness() const { return LineThickness; }
	void SetLineThickness(float InThickness);
	bool IsGridVisible() const;
	void SetGridVisible(bool bVisible);
	void SaveInitialCameraState();
	void ResetCameraToInitialState();

private:
	void ConfigureDefaultView();
	bool CanUseEditingTools(FCore* Core, ULevel*& OutLevel, UWorld*& OutWorld) const;
	void HandleEditorHotkeys(WPARAM WParam, bool bRightMouseDown);
	void HandleSelectionClick(FCore* Core, UWorld* World, AActor* SelectedActor);
	void HandleMouseMove(AActor* SelectedActor);
	void HandleMouseRelease();
	AActor* GetSelectedActor() const;
	AActor* GetGizmoTarget() const;
	void CreateGridResource(FRenderer* Renderer);
	void CreateViewerDebugMaterials(FRenderer* Renderer);
	void ApplyViewerNoCull(FMaterial* Material) const;
	void ShowViewOptionPanel();

	FEditorUI& EditorUI;
	FWindow* MainWindow = nullptr;
	FPicker Picker;
	mutable FGizmo Gizmo;

	ERenderMode RenderMode = ERenderMode::Lighting;
	const FString WireframeMaterialName = "M_Wireframe";
	const FString SolidWireframeFillMaterialName = "M_SolidWireframeFill";
	const FString SolidWireframeLineMaterialName = "M_SolidWireframeLine";
	std::shared_ptr<FMaterial> WireFrameMaterial = nullptr;
	std::shared_ptr<FMaterial> SolidWireFrameFillMaterial = nullptr;
	std::shared_ptr<FMaterial> SolidWireFrameLineMaterial = nullptr;
	std::shared_ptr<FMaterial> ViewerUVMaterial = nullptr;
	std::shared_ptr<FMaterial> ViewerNormalMaterial = nullptr;

	std::unique_ptr<FMeshData> GridMesh;
	std::shared_ptr<FMaterial> GridMaterial;
	float GridSize = 10.0f;
	float LineThickness = 1.0f;
	bool bShowGrid = true;
	FVector InitialCameraPosition = FVector::ZeroVector;
	float InitialCameraYaw = 0.0f;
	float InitialCameraPitch = 0.0f;
	float InitialCameraFOV = 45.0f;
	bool bHasInitialCameraState = false;
	FVector ResetAnimationStartPosition = FVector::ZeroVector;
	float ResetAnimationStartYaw = 0.0f;
	float ResetAnimationStartPitch = 0.0f;
	float ResetAnimationStartFOV = 45.0f;
	float ResetAnimationElapsed = 0.0f;
	float ResetAnimationDuration = 0.25f;
	bool bResetCameraAnimating = false;
	EEditorViewportType ViewportType = EEditorViewportType::Perspective;
};
