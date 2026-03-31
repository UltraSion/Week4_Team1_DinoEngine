#include "EditorViewportClient.h"

#include "EditorUI.h"
#include "FEditorEngine.h"
#include "Actor/Actor.h"
#include "Actor/SkySphereActor.h"
#include "Actor/StaticMeshActor.h"
#include "Component/MeshComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Core/Core.h"
#include "Core/FEngine.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include "Math/MathUtility.h"
#include "Platform/Windows/Window.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/ShaderMap.h"
#include "Serializer/SceneSerializer.h"
#include "ViewportWindow.h"
#include "World/Level.h"
#include "imgui.h"

#include <cmath>
#include <filesystem>

namespace
{
	const char* GetViewportTypeLabel(EEditorViewportType ViewportType)
	{
		switch (ViewportType)
		{
		case EEditorViewportType::Top:
			return "Top";
		case EEditorViewportType::Front:
			return "Front";
		case EEditorViewportType::Right:
			return "Right";
		case EEditorViewportType::Perspective:
		default:
			return "Perspective";
		}
	}

	float LerpFloat(float A, float B, float Alpha)
	{
		return A + (B - A) * Alpha;
	}

	float LerpAngleDegrees(float Start, float End, float Alpha)
	{
		float Delta = std::fmod(End - Start, 360.0f);
		if (Delta > 180.0f)
		{
			Delta -= 360.0f;
		}
		else if (Delta < -180.0f)
		{
			Delta += 360.0f;
		}

		return Start + Delta * Alpha;
	}

	FViewportContext* CreateEditorViewportContext(EEditorViewportType ViewportType)
	{
		FEditorEngine* EditorEngine = dynamic_cast<FEditorEngine*>(GEngine);
		return EditorEngine ? EditorEngine->CreateEditorViewportContext(FRect(), ViewportType) : nullptr;
	}

	SViewportWindow* CreateEditorViewportWindow(EEditorViewportType ViewportType)
	{
		FViewportContext* Context = CreateEditorViewportContext(ViewportType);
		return Context ? new SViewportWindow(FRect(), Context) : nullptr;
	}
}

FEditorViewportClient::FEditorViewportClient(FEditorUI& InEditorUI, FWindow* InMainWindow, EEditorViewportType InViewportType, ELevelType InWorldType)
	: EditorUI(InEditorUI)
	, MainWindow(InMainWindow)
	, ViewportType(InViewportType)
{
	SetWorldType(InWorldType);
	SetGridVisible(bShowGrid);
}

void FEditorViewportClient::Attach(FCore* Core)
{
	if (!Core || !GRenderer || !MainWindow)
	{
		return;
	}

	if (!EditorUI.HasActiveViewportClient())
	{
		EditorUI.SetActiveViewportClient(this);
	}

	WireFrameMaterial = FMaterialManager::Get().FindByName(WireframeMaterialName);
	SolidWireFrameFillMaterial = FMaterialManager::Get().FindByName(SolidWireframeFillMaterialName);
	SolidWireFrameLineMaterial = FMaterialManager::Get().FindByName(SolidWireframeLineMaterialName);
	CreateGridResource(GRenderer);
#if IS_OBJ_VIEWER
	CreateViewerDebugMaterials(GRenderer);
#endif
}

void FEditorViewportClient::Detach()
{
	Gizmo.EndDrag();
	GridMesh.reset();
	GridMaterial.reset();
	WireFrameMaterial.reset();
	SolidWireFrameFillMaterial.reset();
	SolidWireFrameLineMaterial.reset();
	ViewerUVMaterial.reset();
	ViewerNormalMaterial.reset();

	if (EditorUI.GetActiveViewportClient() == this)
	{
		EditorUI.SetActiveViewportClient(nullptr);
	}
}

void FEditorViewportClient::CreateGridResource(FRenderer* Renderer)
{
	ID3D11Device* Device = Renderer ? Renderer->GetDevice() : nullptr;
	if (!Device)
	{
		return;
	}

	GridMesh = std::make_unique<FMeshData>();
	GridMesh->Topology = EMeshTopology::EMT_TriangleList;
	for (int i = 0; i < 18; ++i)
	{
		FPrimitiveVertex Vertex;
		GridMesh->Vertices.push_back(Vertex);
		GridMesh->Indices.push_back(i);
	}
	GridMesh->CreateVertexAndIndexBuffer(Device);

	const std::wstring ShaderDir = FPaths::ShaderDir();
	auto VertexShader = FShaderMap::Get().GetOrCreateVertexShader(Device, (ShaderDir + L"AxisVertexShader.hlsl").c_str());
	auto PixelShader = FShaderMap::Get().GetOrCreatePixelShader(Device, (ShaderDir + L"AxisPixelShader.hlsl").c_str());

	GridMaterial = std::make_shared<FMaterial>();
	GridMaterial->SetOriginName("M_EditorGrid");
	GridMaterial->SetVertexShader(VertexShader);
	GridMaterial->SetPixelShader(PixelShader);

	FRasterizerStateOption RasterizerOption;
	RasterizerOption.FillMode = D3D11_FILL_SOLID;
	RasterizerOption.CullMode = D3D11_CULL_NONE;
	GridMaterial->SetRasterizerOption(RasterizerOption);
	GridMaterial->SetRasterizerState(Renderer->GetRenderStateManager()->GetOrCreateRasterizerState(RasterizerOption));

	FDepthStencilStateOption DepthStencilOption;
	DepthStencilOption.DepthEnable = true;
	DepthStencilOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	GridMaterial->SetDepthStencilOption(DepthStencilOption);
	GridMaterial->SetDepthStencilState(Renderer->GetRenderStateManager()->GetOrCreateDepthStencilState(DepthStencilOption));

	const int32 SlotIndex = GridMaterial->CreateConstantBuffer(Device, 32);
	if (SlotIndex >= 0)
	{
		GridMaterial->RegisterParameter("GridSize", SlotIndex, 12, 4);
		GridMaterial->RegisterParameter("LineThickness", SlotIndex, 16, 4);
		GridMaterial->SetParameterData("GridSize", &GridSize, 4);
		GridMaterial->SetParameterData("LineThickness", &LineThickness, 4);
	}
}

void FEditorViewportClient::CreateViewerDebugMaterials(FRenderer* Renderer)
{
	ID3D11Device* Device = Renderer ? Renderer->GetDevice() : nullptr;
	FRenderStateManager* RenderStateManager = Renderer ? Renderer->GetRenderStateManager().get() : nullptr;
	if (!Device || !RenderStateManager)
	{
		return;
	}

	const std::wstring ShaderDir = FPaths::ShaderDir();
	auto VertexShader = FShaderMap::Get().GetOrCreateVertexShader(Device, (ShaderDir + L"StaticMeshVertexShader.hlsl").c_str());
	auto UVPixelShader = FShaderMap::Get().GetOrCreatePixelShader(Device, (ShaderDir + L"ObjViewerUVPixelShader.hlsl").c_str());
	auto NormalPixelShader = FShaderMap::Get().GetOrCreatePixelShader(Device, (ShaderDir + L"ObjViewerNormalPixelShader.hlsl").c_str());

	auto CreateDebugMaterial = [&](const char* MaterialName, const std::shared_ptr<FPixelShader>& PixelShader)
	{
		if (!VertexShader || !PixelShader)
		{
			return std::shared_ptr<FMaterial>();
		}

		auto Material = std::make_shared<FMaterial>();
		Material->SetOriginName(MaterialName);
		Material->SetVertexShader(VertexShader);
		Material->SetPixelShader(PixelShader);

		FRasterizerStateOption RasterizerOption;
		RasterizerOption.FillMode = D3D11_FILL_SOLID;
		RasterizerOption.CullMode = D3D11_CULL_NONE;
		Material->SetRasterizerOption(RasterizerOption);
		Material->SetRasterizerState(RenderStateManager->GetOrCreateRasterizerState(RasterizerOption));

		FDepthStencilStateOption DepthStencilOption;
		DepthStencilOption.DepthEnable = true;
		DepthStencilOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		Material->SetDepthStencilOption(DepthStencilOption);
		Material->SetDepthStencilState(RenderStateManager->GetOrCreateDepthStencilState(DepthStencilOption));

		const int32 SlotIndex = Material->CreateConstantBuffer(Device, 16);
		if (SlotIndex >= 0)
		{
			Material->RegisterParameter("ColorTint", SlotIndex, 0, 16);
			const FVector4 White(1.0f, 1.0f, 1.0f, 1.0f);
			Material->SetParameterData("ColorTint", &White, 16);
		}

		return Material;
	};

	ViewerUVMaterial = CreateDebugMaterial("M_ObjViewer_UV", UVPixelShader);
	ViewerNormalMaterial = CreateDebugMaterial("M_ObjViewer_Normal", NormalPixelShader);
}

void FEditorViewportClient::ApplyViewerNoCull(FMaterial* Material) const
{
	if (!Material || !GRenderer)
	{
		return;
	}

	const FRasterizerStateOption& CurrentOption = Material->GetRasterizerOption();
	if (CurrentOption.CullMode == D3D11_CULL_NONE)
	{
		return;
	}

	FRasterizerStateOption UpdatedOption = CurrentOption;
	UpdatedOption.CullMode = D3D11_CULL_NONE;
	Material->SetRasterizerOption(UpdatedOption);
	Material->SetRasterizerState(GRenderer->GetRenderStateManager()->GetOrCreateRasterizerState(UpdatedOption));
}

void FEditorViewportClient::ShowViewOptionPanel()
{
	if (!ImGui::CollapsingHeader("View"))
	{
		return;
	}

	FShowFlags& ShowFlags = GetShowFlags();
	auto ShowFlagCheckbox = [&ShowFlags](const char* Label, EEngineShowFlags Flag)
	{
		bool bValue = ShowFlags.HasFlag(Flag);
		if (ImGui::Checkbox(Label, &bValue))
		{
			ShowFlags.SetFlag(Flag, bValue);
		}
	};

	ImGui::SeparatorText(GetViewportLabel());

	int RenderModeIndex = static_cast<int>(GetRenderMode());
	const char* RenderModeLabels[] =
	{
		"Lighting",
		"No Lighting",
		"Wireframe",
		"Solid Wireframe",
		"UV",
		"Normals"
	};

	if (ImGui::Combo("Render Mode", &RenderModeIndex, RenderModeLabels, IM_ARRAYSIZE(RenderModeLabels)))
	{
		SetRenderMode(static_cast<ERenderMode>(RenderModeIndex));
	}

	ShowFlagCheckbox("Primitives", EEngineShowFlags::SF_Primitives);
	ShowFlagCheckbox("UUID", EEngineShowFlags::SF_UUID);
	ShowFlagCheckbox("Debug Draw", EEngineShowFlags::SF_DebugDraw);
	ShowFlagCheckbox("Grid", EEngineShowFlags::SF_Grid);
	ShowFlagCheckbox("Collision", EEngineShowFlags::SF_Collision);

	float LocalGridSize = GetGridSize();
	if (ImGui::SliderFloat("Grid Size", &LocalGridSize, 1.0f, 100.0f, "%.1f"))
	{
		SetGridSize(LocalGridSize);
	}

	float LocalThickness = GetLineThickness();
	if (ImGui::SliderFloat("Line Thickness", &LocalThickness, 0.1f, 5.0f, "%.2f"))
	{
		SetLineThickness(LocalThickness);
	}
}

UWorld* FEditorViewportClient::ResolveWorld(FCore* Core) const
{
	return FViewportClient::ResolveWorld(Core);
}

const char* FEditorViewportClient::GetViewportLabel() const
{
	return GetViewportTypeLabel(ViewportType);
}

void FEditorViewportClient::HandleMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	(void)Hwnd;

	if (EditorUI.GetActiveViewportClient() != this)
	{
		EditorUI.SetActiveViewportClient(this);
	}

	ULevel* Level = nullptr;
	UWorld* World = nullptr;
	const bool bCanUseEditingTools = CanUseEditingTools(Core, Level, World);
	AActor* SelectedActor = bCanUseEditingTools ? GetSelectedActor() : nullptr;
	const bool bRightMouseDown = InputManager && InputManager->IsMouseButtonDown(FInputManager::MOUSE_RIGHT);

	switch (Msg)
	{
	case WM_KEYDOWN:
		HandleEditorHotkeys(WParam, bRightMouseDown);
		OnKeyDown(WParam, LParam);
		return;
	case WM_KEYUP:
		OnKeyUp(WParam, LParam);
		return;
#if IS_OBJ_VIEWER
	case WM_LBUTTONDBLCLK:
		ResetCameraToInitialState();
		return;
#endif
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONDBLCLK:
		OnMouseButtonDown(Msg, WParam, LParam);
		if (Msg == WM_LBUTTONDOWN && bCanUseEditingTools)
		{
			HandleSelectionClick(Core, World, SelectedActor);
		}
		return;
	case WM_MOUSEMOVE:
		OnMouseMove(WParam, LParam);
		if (bCanUseEditingTools)
		{
			HandleMouseMoveForTools(SelectedActor);
		}
		return;
	case WM_MOUSEWHEEL:
		OnMouseWheel(static_cast<float>(GET_WHEEL_DELTA_WPARAM(WParam)) / static_cast<float>(WHEEL_DELTA), WParam, LParam);
		return;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
		OnMouseButtonUp(Msg, WParam, LParam);
		if (Msg == WM_LBUTTONUP && bCanUseEditingTools)
		{
			HandleMouseReleaseForTools();
		}
		return;
	default:
		return;
	}
}

void FEditorViewportClient::Tick(float DeltaTime)
{
	FViewportClient::Tick(DeltaTime);

#if IS_OBJ_VIEWER
	if (!bResetCameraAnimating || !bHasInitialCameraState)
	{
		return;
	}

	ResetAnimationElapsed += DeltaTime;
	const float NormalizedTime = FMath::Clamp(ResetAnimationElapsed / ResetAnimationDuration, 0.0f, 1.0f);
	const float SmoothAlpha = NormalizedTime * NormalizedTime * (3.0f - 2.0f * NormalizedTime);

	const FVector NewPosition = ResetAnimationStartPosition + (InitialCameraPosition - ResetAnimationStartPosition) * SmoothAlpha;
	const float NewYaw = LerpAngleDegrees(ResetAnimationStartYaw, InitialCameraYaw, SmoothAlpha);
	const float NewPitch = LerpFloat(ResetAnimationStartPitch, InitialCameraPitch, SmoothAlpha);
	const float NewFOV = LerpFloat(ResetAnimationStartFOV, InitialCameraFOV, SmoothAlpha);

	CameraTransform.SetPosition(NewPosition);
	CameraTransform.SetRotation(NewYaw, NewPitch);
	CameraTransform.SetFOV(NewFOV);

	if (NormalizedTime >= 1.0f)
	{
		bResetCameraAnimating = false;
		ResetAnimationElapsed = 0.0f;
	}
#else
	(void)DeltaTime;
#endif
}

void FEditorViewportClient::SaveInitialCameraState()
{
	InitialCameraPosition = CameraTransform.GetPosition();
	InitialCameraYaw = CameraTransform.GetYaw();
	InitialCameraPitch = CameraTransform.GetPitch();
	InitialCameraFOV = CameraTransform.GetFOV();
	bHasInitialCameraState = true;
}

void FEditorViewportClient::ResetCameraToInitialState()
{
	if (!bHasInitialCameraState)
	{
		return;
	}

	ResetAnimationStartPosition = CameraTransform.GetPosition();
	ResetAnimationStartYaw = CameraTransform.GetYaw();
	ResetAnimationStartPitch = CameraTransform.GetPitch();
	ResetAnimationStartFOV = CameraTransform.GetFOV();
	ResetAnimationElapsed = 0.0f;
	bResetCameraAnimating = true;
}

bool FEditorViewportClient::CanUseEditingTools(FCore* Core, ULevel*& OutLevel, UWorld*& OutWorld) const
{
	OutLevel = ResolveLevel(Core);
	OutWorld = ResolveWorld(Core);
	return SupportsEditingTools() && OutLevel != nullptr && OutWorld != nullptr;
}

void FEditorViewportClient::HandleEditorHotkeys(WPARAM WParam, bool bRightMouseDown)
{
	if (bRightMouseDown)
	{
		return;
	}

	switch (WParam)
	{
	case 'W':
		Gizmo.SetMode(EGizmoMode::Location);
		return;
	case 'E':
		Gizmo.SetMode(EGizmoMode::Rotation);
		return;
	case 'R':
		Gizmo.SetMode(EGizmoMode::Scale);
		return;
	case 'L':
		Gizmo.ToggleCoordinateSpace();
		UE_LOG("Gizmo Space: %s", Gizmo.GetCoordinateSpace() == EGizmoCoordinateSpace::Local ? "Local" : "World");
		return;
	default:
		return;
	}
}

void FEditorViewportClient::HandleSelectionClick(FCore* Core, UWorld* World, AActor* SelectedActor)
{
#if IS_OBJ_VIEWER
	return;
#endif

	if (SelectedActor && Gizmo.BeginDrag(SelectedActor, &CameraTransform, Picker, ViewportMouseX, ViewportMouseY, ViewportWidth, ViewportHeight))
	{
		return;
	}

	AActor* PickedActor = Picker.PickActor(World->GetAllActors(), &CameraTransform, ViewportMouseX, ViewportMouseY, ViewportWidth, ViewportHeight);
	Core->SetSelectedActor(PickedActor);
	EditorUI.SyncSelectedActorProperty();
}

void FEditorViewportClient::HandleMouseMoveForTools(AActor* SelectedActor)
{
	if (!Gizmo.IsDragging())
	{
		Gizmo.UpdateHover(SelectedActor, &CameraTransform, Picker, ViewportMouseX, ViewportMouseY, ViewportWidth, ViewportHeight);
		return;
	}

	if (Gizmo.UpdateDrag(SelectedActor, &CameraTransform, Picker, ViewportMouseX, ViewportMouseY, ViewportWidth, ViewportHeight))
	{
		EditorUI.SyncSelectedActorProperty();
	}
}

void FEditorViewportClient::HandleMouseReleaseForTools()
{
	if (!Gizmo.IsDragging())
	{
		return;
	}

	Gizmo.EndDrag();
	EditorUI.SyncSelectedActorProperty();
}

AActor* FEditorViewportClient::GetSelectedActor() const
{
	FCore* Core = EditorUI.GetCore();
	return Core ? Core->GetSelectedActor() : nullptr;
}

AActor* FEditorViewportClient::GetGizmoTarget() const
{
	AActor* SelectedActor = GetSelectedActor();
	if (!SelectedActor || SelectedActor->IsA<ASkySphereActor>())
	{
		return nullptr;
	}

	return SelectedActor;
}

void FEditorViewportClient::DrawUI()
{
	if (ViewportWindow == nullptr)
	{
		return;
	}

	FRect WindowRect = ViewportWindow->GetRect();
	ImGui::SetNextWindowPos(ImVec2(WindowRect.Position.X, WindowRect.Position.Y));

	char WindowName[128];
	sprintf_s(WindowName, "ViewportButtonFrame##%p", this);

	if (!ImGui::Begin(WindowName, nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
	{
		ImGui::End();
		return;
	}

	char ButtonName[128];
	const bool bHasParent = ViewportWindow->GetParent() != nullptr;
	if (!bHasParent)
	{
		sprintf_s(ButtonName, "H##%p", this);
		if (ImGui::Button(ButtonName))
		{
			if (SViewportWindow* NewViewportWindow = CreateEditorViewportWindow(EEditorViewportType::Perspective))
			{
				if (ViewportWindow->Split(NewViewportWindow, SplitDirection::Horizontal, SplitOption::LT))
				{
					EditorUI.SaveEditorSettings();
				}
			}
		}

		ImGui::SameLine();
		sprintf_s(ButtonName, "V##%p", this);
		if (ImGui::Button(ButtonName))
		{
			if (SViewportWindow* NewViewportWindow = CreateEditorViewportWindow(EEditorViewportType::Perspective))
			{
				if (ViewportWindow->Split(NewViewportWindow, SplitDirection::Vertical, SplitOption::LT))
				{
					EditorUI.SaveEditorSettings();
				}
			}
		}

		ImGui::SameLine();
		sprintf_s(ButtonName, "+##%p", this);
		if (ImGui::Button(ButtonName))
		{
			SViewportWindow* TopViewportWindow = CreateEditorViewportWindow(EEditorViewportType::Top);
			SViewportWindow* FrontViewportWindow = CreateEditorViewportWindow(EEditorViewportType::Front);
			SViewportWindow* RightViewportWindow = CreateEditorViewportWindow(EEditorViewportType::Right);
			if (TopViewportWindow && FrontViewportWindow && RightViewportWindow)
			{
				if (ViewportWindow->Split4(
					TopViewportWindow,
					FrontViewportWindow,
					RightViewportWindow,
					SplitOption::RB))
				{
					EditorUI.SaveEditorSettings();
				}
			}
		}
	}
	else
	{
		sprintf_s(ButtonName, "-##%p", this);
		if (ImGui::Button(ButtonName))
		{
			ViewportWindow->GetParent()->Merge(ViewportWindow);
			EditorUI.SaveEditorSettings();
		}
	}

	ImGui::SameLine();
	ShowViewOptionPanel();

	ImGui::SameLine();
	DrawCameraOption();
	ImGui::End();
}

void FEditorViewportClient::DrawCameraOption()
{
	ImGui::Spacing();
	if (!ImGui::CollapsingHeader("Camera"))
	{
		return;
	}

	ImGui::SeparatorText("Camera");
	DrawControllerOptions();
}

void FEditorViewportClient::HandleFileDoubleClick(const FString& FilePath)
{
	FCore* Core = EditorUI.GetCore();
	if (!Core || !GRenderer || !FilePath.ends_with(".json"))
	{
		return;
	}

	ULevel* Level = ResolveLevel(Core);
	if (!Level)
	{
		return;
	}

	Core->SetSelectedActor(nullptr);
	Level->ClearActors();

	if (FSceneSerializer::Load(Level, FilePath, GRenderer->GetDevice(), EditorUI.GetPerspectiveCamera()))
	{
		EditorUI.SavePerspectiveCameraInitialState();
		UE_LOG("Level loaded: %s", FilePath.c_str());
	}
	else
	{
		MessageBoxW(nullptr, L"Level information is invalid.", L"Error", MB_OK | MB_ICONWARNING);
	}
}

void FEditorViewportClient::HandleFileDropOnViewport(const FString& FilePath)
{
	FCore* Core = EditorUI.GetCore();
	if (!Core || !GRenderer || !FilePath.ends_with(".obj"))
	{
		return;
	}

	ULevel* Level = ResolveLevel(Core);
	if (!Level)
	{
		return;
	}

	const FRay Ray = Picker.ScreenToRay(&CameraTransform, ViewportMouseX, ViewportMouseY, ViewportWidth, ViewportHeight);
	const FVector SpawnLocation = Ray.Origin + Ray.Direction * 10.0f;

	AStaticMeshActor* MeshActor = Level->SpawnActor<AStaticMeshActor>(std::filesystem::path(FilePath).stem().string());
	if (MeshActor)
	{
		MeshActor->LoadStaticMesh(GRenderer->GetDevice(), FilePath);
		if (USceneComponent* Root = MeshActor->GetRootComponent())
		{
			FTransform Transform = Root->GetRelativeTransform();
			Transform.SetLocation(SpawnLocation);
			Root->SetRelativeTransform(Transform);
		}
#if !IS_OBJ_VIEWER
		Core->SetSelectedActor(MeshActor);
#endif
	}

	EditorUI.SyncSelectedActorProperty();
}

void FEditorViewportClient::BuildRenderCommands(TArray<AActor*>& InActors, FRenderCommandQueue& OutQueue)
{
	FViewportClient::BuildRenderCommands(InActors, OutQueue);

	if (RenderMode == ERenderMode::Wireframe)
	{
		for (FRenderCommand& Command : OutQueue.Commands)
		{
			if (Command.RenderLayer != ERenderLayer::Overlay)
			{
				Command.Material = WireFrameMaterial.get();
			}
		}
	}
	else if (RenderMode == ERenderMode::SolidWireframe)
	{
		TArray<FRenderCommand> SolidWireframeCommands;
		SolidWireframeCommands.reserve(OutQueue.Commands.size() * 2);

		for (const FRenderCommand& Command : OutQueue.Commands)
		{
			if (Command.RenderLayer == ERenderLayer::Overlay)
			{
				SolidWireframeCommands.push_back(Command);
				continue;
			}

			FRenderCommand FillCommand = Command;
			FillCommand.Material = SolidWireFrameFillMaterial.get();
			SolidWireframeCommands.push_back(FillCommand);

			FRenderCommand LineCommand = Command;
			LineCommand.Material = SolidWireFrameLineMaterial.get();
			SolidWireframeCommands.push_back(LineCommand);
		}

		OutQueue.Commands = std::move(SolidWireframeCommands);
	}
	else if (RenderMode == ERenderMode::UV)
	{
		for (FRenderCommand& Command : OutQueue.Commands)
		{
			if (Command.RenderLayer != ERenderLayer::Overlay && ViewerUVMaterial)
			{
				Command.Material = ViewerUVMaterial.get();
			}
		}
	}
	else if (RenderMode == ERenderMode::Normals)
	{
		for (FRenderCommand& Command : OutQueue.Commands)
		{
			if (Command.RenderLayer != ERenderLayer::Overlay && ViewerNormalMaterial)
			{
				Command.Material = ViewerNormalMaterial.get();
			}
		}
	}

#if IS_OBJ_VIEWER
	for (FRenderCommand& Command : OutQueue.Commands)
	{
		if (Command.RenderLayer != ERenderLayer::Overlay)
		{
			ApplyViewerNoCull(Command.Material);
		}
	}
#endif

	if (GridMesh && GridMaterial && IsGridVisible())
	{
		FRenderCommand GridCommand;
		GridCommand.MeshData = GridMesh.get();
		GridCommand.Material = GridMaterial.get();
		GridCommand.WorldMatrix = GetGridWorldMatrix();
		GridCommand.RenderLayer = ERenderLayer::Default;
		OutQueue.AddCommand(GridCommand);
	}

#if !IS_OBJ_VIEWER
	if (AActor* GizmoTarget = GetGizmoTarget())
	{
		Gizmo.BuildRenderCommands(GizmoTarget, &CameraTransform, OutQueue);
	}
#endif
}

void FEditorViewportClient::PostRender(FCore* Core, FRenderer* Renderer)
{
	if (!Core || !Renderer)
	{
		return;
	}

	Core->RenderStatOverlay(Renderer, GetViewportWidth(), GetViewportHeight());

#if IS_OBJ_VIEWER
	return;
#endif

	AActor* SelectedActor = Core->GetSelectedActor();
	if (!SelectedActor || SelectedActor->IsPendingDestroy() || !SelectedActor->IsVisible() || SelectedActor->IsA<ASkySphereActor>())
	{
		return;
	}

	if (!GetShowFlags().HasFlag(EEngineShowFlags::SF_Primitives))
	{
		return;
	}

	for (UActorComponent* Component : SelectedActor->GetComponents())
	{
		if (!Component->IsA(UPrimitiveComponent::StaticClass()))
		{
			continue;
		}

		UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(Component);
		if (PrimitiveComponent->IsA(UMeshComponent::StaticClass()))
		{
			UMeshComponent* MeshComponent = static_cast<UMeshComponent*>(PrimitiveComponent);
			if (FMeshData* MeshData = MeshComponent->GetMeshData())
			{
				Renderer->RenderOutline(MeshData, MeshComponent->GetWorldTransform());
			}
			continue;
		}

		if (PrimitiveComponent->GetPrimitive() && PrimitiveComponent->GetPrimitive()->GetMeshData())
		{
			Renderer->RenderOutline(PrimitiveComponent->GetPrimitive()->GetMeshData(), PrimitiveComponent->GetWorldTransform());
		}
	}
}

void FEditorViewportClient::SetGridSize(float InSize)
{
	GridSize = InSize;
	if (GridMaterial)
	{
		GridMaterial->SetParameterData("GridSize", &GridSize, 4);
	}
}

void FEditorViewportClient::SetLineThickness(float InThickness)
{
	LineThickness = InThickness;
	if (GridMaterial)
	{
		GridMaterial->SetParameterData("LineThickness", &LineThickness, 4);
	}
}

bool FEditorViewportClient::IsGridVisible() const
{
	return GetShowFlags().HasFlag(EEngineShowFlags::SF_Grid);
}

void FEditorViewportClient::SetGridVisible(bool bVisible)
{
	bShowGrid = bVisible;
	GetShowFlags().SetFlag(EEngineShowFlags::SF_Grid, bVisible);
}

void FEditorViewportClient::OnMouseButtonDown(UINT Msg, WPARAM WParam, LPARAM LParam)
{
	(void)Msg;
	(void)WParam;
	(void)LParam;
}

void FEditorViewportClient::OnMouseButtonUp(UINT Msg, WPARAM WParam, LPARAM LParam)
{
	(void)Msg;
	(void)WParam;
	(void)LParam;
}

void FEditorViewportClient::OnMouseMove(WPARAM WParam, LPARAM LParam)
{
	(void)WParam;
	(void)LParam;
}

void FEditorViewportClient::OnMouseWheel(float WheelDelta, WPARAM WParam, LPARAM LParam)
{
	(void)WheelDelta;
	(void)WParam;
	(void)LParam;
}

void FEditorViewportClient::OnKeyDown(WPARAM WParam, LPARAM LParam)
{
	(void)WParam;
	(void)LParam;
}

void FEditorViewportClient::OnKeyUp(WPARAM WParam, LPARAM LParam)
{
	(void)WParam;
	(void)LParam;
}

FVector FEditorViewportClient::GetViewportUpVector() const
{
	const FVector Right = CameraTransform.GetRight().GetSafeNormal();
	const FVector Forward = CameraTransform.GetForward().GetSafeNormal();
	return FVector::CrossProduct(Forward, Right).GetSafeNormal();
}

FMatrix FEditorViewportClient::GetGridWorldMatrix() const
{
	switch (ViewportType)
	{
	case EEditorViewportType::Front:
		return FMatrix::MakeRotationX(FMath::DegreesToRadians(90.0f));
	case EEditorViewportType::Right:
		return FMatrix::MakeRotationY(FMath::DegreesToRadians(90.0f));
	case EEditorViewportType::Top:
	case EEditorViewportType::Perspective:
	default:
		return FMatrix::Identity;
	}
}
