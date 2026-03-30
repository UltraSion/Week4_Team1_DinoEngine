#include "EditorViewportClient.h"

#include "EditorUI.h"
#include "Actor/Actor.h"
#include "Actor/StaticMeshActor.h"
#include "Actor/SkySphereActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/MeshComponent.h"
#include "Core/Core.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include "Input/InputManager.h"
#include "Platform/Windows/Window.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/ShaderMap.h"
#include "Serializer/SceneSerializer.h"
#include "World/Level.h"
#include "Math/MathUtility.h"
#include <cmath>
#include "ViewportWindow.h"
#include "Math/Rect.h"
#include "Core/FEngine.h"

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
}

FEditorViewportClient::FEditorViewportClient(FEditorUI& InEditorUI, FWindow* InMainWindow, EEditorViewportType InViewportType, ELevelType InWorldType)
	: EditorUI(InEditorUI)
	, MainWindow(InMainWindow)
	, ViewportType(InViewportType)
{
	SetWorldType(InWorldType);
	SetGridVisible(bShowGrid);
	ConfigureDefaultView();
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
}

void FEditorViewportClient::Detach()
{
	Gizmo.EndDrag();
	GridMesh.reset();
	GridMaterial.reset();
	WireFrameMaterial.reset();
	SolidWireFrameFillMaterial.reset();
	SolidWireFrameLineMaterial.reset();

	if (EditorUI.GetActiveViewportClient() == this)
	{
		EditorUI.SetActiveViewportClient(nullptr);
	}
}

void FEditorViewportClient::ConfigureDefaultView()
{
	switch (ViewportType)
	{
	case EEditorViewportType::Top:
		CameraTransform.SetProjectionMode(ECameraProjectionMode::Orthographic);
		CameraTransform.SetOrthoWidth(24.0f);
		CameraTransform.SetPosition({ 0.0f, 0.0f, 25.0f });
		CameraTransform.SetRotation(0.0f, -89.0f);
		break;
	case EEditorViewportType::Front:
		CameraTransform.SetProjectionMode(ECameraProjectionMode::Orthographic);
		CameraTransform.SetOrthoWidth(24.0f);
		CameraTransform.SetPosition({ -25.0f, 0.0f, 0.0f });
		CameraTransform.SetRotation(0.0f, 0.0f);
		break;
	case EEditorViewportType::Right:
		CameraTransform.SetProjectionMode(ECameraProjectionMode::Orthographic);
		CameraTransform.SetOrthoWidth(24.0f);
		CameraTransform.SetPosition({ 0.0f, -25.0f, 0.0f });
		CameraTransform.SetRotation(90.0f, 0.0f);
		break;
	case EEditorViewportType::Perspective:
	default:
		CameraTransform.SetProjectionMode(ECameraProjectionMode::Perspective);
		CameraTransform.SetPosition({ -5.0f, 0.0f, 2.0f });
		CameraTransform.SetRotation(0.0f, 0.0f);
		break;
	}
}

void FEditorViewportClient::SetViewportType(EEditorViewportType InViewportType)
{
	ViewportType = InViewportType;
	ConfigureDefaultView();
	SaveInitialCameraState();
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
	(void)LParam;

	if (EditorUI.GetActiveViewportClient() != this)
	{
		EditorUI.SetActiveViewportClient(this);
	}

	ULevel* Level = nullptr;
	UWorld* World = nullptr;
	if (!CanUseEditingTools(Core, Level, World))
	{
		return;
	}

	AActor* SelectedActor = GetSelectedActor();
	const bool bRightMouseDown = InputManager && InputManager->IsMouseButtonDown(FInputManager::MOUSE_RIGHT);

	switch (Msg)
	{
	case WM_KEYDOWN:
		HandleEditorHotkeys(WParam, bRightMouseDown);
		return;
#if IS_OBJ_VIEWER //일단 버그를 막기 위해 뷰어에서만 더블 클릭을 허용합니다
	case WM_LBUTTONDBLCLK:
		ResetCameraToInitialState();
		return;
#endif
	case WM_LBUTTONDOWN:
		HandleSelectionClick(Core, World, SelectedActor);
		return;
	case WM_MOUSEMOVE:
		HandleMouseMove(SelectedActor);
		return;
	case WM_LBUTTONUP:
		HandleMouseRelease();
		return;
	default:
		return;
	}
}

void FEditorViewportClient::Tick(float DeltaTime)
{
	FViewportClient::Tick(DeltaTime);

#if IS_OBJ_VIEWER //뷰어에서 더블 클릭 시 초기 모드로 움직일 때, 부드럽게 움직이는 코드입니다
	if (!bResetCameraAnimating || !bHasInitialCameraState)
	{
		return;
	}

	ResetAnimationElapsed += DeltaTime;
	const float NormalizedTime = FMath::Clamp(ResetAnimationElapsed / ResetAnimationDuration, 0.0f, 1.0f);
	const float SmoothAlpha = NormalizedTime * NormalizedTime * (3.0f - 2.0f * NormalizedTime);

	const FVector NewPosition =
		ResetAnimationStartPosition + (InitialCameraPosition - ResetAnimationStartPosition) * SmoothAlpha;
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
	// 뷰어 모드에서는 픽킹을 통한 선택 변경을 막습니다.
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

void FEditorViewportClient::HandleMouseMove(AActor* SelectedActor)
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

void FEditorViewportClient::HandleMouseRelease()
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
	FRect WindowRect = ViewportWindow->GetRect();
	ImGui::SetNextWindowPos(ImVec2(WindowRect.Position.X + WindowRect.Size.X * 0.5f, WindowRect.Position.Y));

	char windowName[128];
	sprintf_s(windowName, "ViewportButtonFrame##%p", this);

	if (!ImGui::Begin(windowName, nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
	{
		ImGui::End();
		return;
	}

	char buttonName[128];
	sprintf_s(buttonName, "H##%p", this);
	if (ImGui::Button(buttonName))
	{
		ViewportWindow->Split(
			new SViewportWindow(FRect(), GEngine->CreateContext(FRect())),
			SplitDirection::Horizontal,
			SplitOption::LT
		);
	}

	ImGui::SameLine();
	sprintf_s(buttonName, "V##%p", this);
	if (ImGui::Button(buttonName))
	{
		ViewportWindow->Split(
			new SViewportWindow(FRect(), GEngine->CreateContext(FRect())),
			SplitDirection::Vertical,
			SplitOption::LT
		);
	}

	ImGui::SameLine();
	sprintf_s(buttonName, "+##%p", this);
	if (ImGui::Button(buttonName))
	{
		FViewportContext* NewContext1 = GEngine->CreateContext(FRect());
		FViewportContext* NewContext2 = GEngine->CreateContext(FRect());
		FViewportContext* NewContext3 = GEngine->CreateContext(FRect());

		if (FEditorViewportClient* NewViewportClient1 = dynamic_cast<FEditorViewportClient*>(NewContext1 ? NewContext1->GetViewportClient() : nullptr))
		{
			NewViewportClient1->SetViewportType(EEditorViewportType::Top);
		}

		if (FEditorViewportClient* NewViewportClient2 = dynamic_cast<FEditorViewportClient*>(NewContext2 ? NewContext2->GetViewportClient() : nullptr))
		{
			NewViewportClient2->SetViewportType(EEditorViewportType::Front);
		}

		if (FEditorViewportClient* NewViewportClient3 = dynamic_cast<FEditorViewportClient*>(NewContext3 ? NewContext3->GetViewportClient() : nullptr))
		{
			NewViewportClient3->SetViewportType(EEditorViewportType::Right);
		}

		ViewportWindow->Split4(
			new SViewportWindow(FRect(), NewContext1),
			new SViewportWindow(FRect(), NewContext2),
			new SViewportWindow(FRect(), NewContext3),
			SplitOption::RB
		);
	}

	ImGui::SameLine();
	sprintf_s(buttonName, "-##%p", this);
	bool HasParent = ViewportWindow->GetParent() != nullptr;
	if (!HasParent)
	{
		ImGui::BeginDisabled();
	}
	if (ImGui::Button(buttonName))
	{
		ViewportWindow->GetParent()->Merge(ViewportWindow);
	}
	if (!HasParent)
	{
		ImGui::EndDisabled();
	}

	ImGui::SameLine();
	DrawCameraOption(GetCamera());
	ImGui::End();


}

void FEditorViewportClient::DrawCameraOption(FCamera* Camera)
{
	if (ImGui::CollapsingHeader("Camera"))
	{
		ImGui::SeparatorText("Camera");
		if (Camera)
		{
			float Sensitivity = Camera->GetMouseSensitivity();
			if (ImGui::SliderFloat("Mouse Sensitivity", &Sensitivity, 0.01f, 1.0f))
			{
				Camera->SetMouseSensitivity(Sensitivity);
			}

			float Speed = Camera->GetSpeed();
			if (ImGui::SliderFloat("Move Speed", &Speed, 0.1f, 20.0f))
			{
				Camera->SetSpeed(Speed);
			}

			const FVector CameraPosition = Camera->GetPosition();
			float Position[3] = { CameraPosition.X, CameraPosition.Y, CameraPosition.Z };
			if (ImGui::DragFloat3("Position", Position, 0.1f))
			{
				Camera->SetPosition({ Position[0], Position[1], Position[2] });
			}

			float CameraYaw = Camera->GetYaw();
			float CameraPitch = Camera->GetPitch();
			bool bRotationChanged = false;
			bRotationChanged |= ImGui::DragFloat("Yaw", &CameraYaw, 0.5f);
			bRotationChanged |= ImGui::DragFloat("Pitch", &CameraPitch, 0.5f, -89.0f, 89.0f);
			if (bRotationChanged)
			{
				Camera->SetRotation(CameraYaw, CameraPitch);
			}

			int ProjectionModeIndex = (Camera->GetProjectionMode() == ECameraProjectionMode::Orthographic) ? 1 : 0;
			const char* ProjectionModes[] = { "Perspective", "Orthographic" };
			if (ImGui::Combo("Projection", &ProjectionModeIndex, ProjectionModes, IM_ARRAYSIZE(ProjectionModes)))
			{
				Camera->SetProjectionMode(
					ProjectionModeIndex == 0
					? ECameraProjectionMode::Perspective
					: ECameraProjectionMode::Orthographic);
			}

			if (Camera->IsOrthographic())
			{
				float OrthoWidth = Camera->GetOrthoWidth();
				if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.5f, 1.0f, 1000.0f))
				{
					Camera->SetOrthoWidth(OrthoWidth);
				}
			}
			else
			{
				float CameraFOV = Camera->GetFOV();
				if (ImGui::SliderFloat("FOV", &CameraFOV, 10.0f, 120.0f))
				{
					Camera->SetFOV(CameraFOV);
				}
			}
		}
	}
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

	if (FSceneSerializer::Load(Level, FilePath, GRenderer->GetDevice()))
	{
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

	AStaticMeshActor* MeshActor = Level->SpawnActor<AStaticMeshActor>(
		std::filesystem::path(FilePath).stem().string());
	if (MeshActor)
	{
		MeshActor->LoadStaticMesh(GRenderer->GetDevice(), FilePath);
		if (USceneComponent* Root = MeshActor->GetRootComponent())
		{
			FTransform Transform = Root->GetRelativeTransform();
			Transform.SetLocation(SpawnLocation);
			Root->SetRelativeTransform(Transform);
		}
		Core->SetSelectedActor(MeshActor);
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

	if (GridMesh && GridMaterial && IsGridVisible())
	{
		FRenderCommand GridCommand;
		GridCommand.MeshData = GridMesh.get();
		GridCommand.Material = GridMaterial.get();
		GridCommand.WorldMatrix = FMatrix::Identity;
		GridCommand.RenderLayer = ERenderLayer::Default;
		OutQueue.AddCommand(GridCommand);
	}

#if IS_OBJ_VIEWER //뷰어는 gizmo도 안 뜹니다
#else
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

	//Stat Overlay를 띄웁니다.
	Core->RenderStatOverlay(Renderer, GetViewportWidth(), GetViewportHeight());

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
				Renderer->RenderOutline(
					MeshData,
					MeshComponent->GetWorldTransform());
			}
			continue;
		}

		if (PrimitiveComponent->GetPrimitive() && PrimitiveComponent->GetPrimitive()->GetMeshData())
		{
			Renderer->RenderOutline(
				PrimitiveComponent->GetPrimitive()->GetMeshData(),
				PrimitiveComponent->GetWorldTransform());
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

void FEditorViewportClient::ProcessCameraInput(FCore* Core, float DeltaTime)
{
	(void)Core;

	constexpr float WheelZoomForwardScale = 0.5f;
	constexpr float WheelZoomFactor = 1.1f;

	if (!InputManager)
	{
		return;
	}

	float ForwardInput = 0.0f;
	float RightInput = 0.0f;
	float UpInput = 0.0f;

	if (InputManager->IsMouseButtonDown(FInputManager::MOUSE_RIGHT))
	{
#if !IS_OBJ_VIEWER //뷰어에서는 못 움직입니다
		if (InputManager->IsKeyDown('W'))
		{
			ForwardInput += 1.0f;
		}
		if (InputManager->IsKeyDown('S'))
		{
			ForwardInput -= 1.0f;
		}
		if (InputManager->IsKeyDown('D'))
		{
			RightInput += 1.0f;
		}
		if (InputManager->IsKeyDown('A'))
		{
			RightInput -= 1.0f;
		}
		if (InputManager->IsKeyDown('E'))
		{
			UpInput += 1.0f;
		}
		if (InputManager->IsKeyDown('Q'))
		{
			UpInput -= 1.0f;
		}
#endif
	}

	if (ForwardInput != 0.0f)
	{
		CameraTransform.MoveForward(ForwardInput * DeltaTime);
	}
	if (RightInput != 0.0f)
	{
		CameraTransform.MoveRight(RightInput * DeltaTime);
	}
	if (UpInput != 0.0f)
	{
		CameraTransform.MoveUp(UpInput * DeltaTime);
	}

	const float MouseWheelDelta = InputManager->GetMouseWheelDelta();
	if (MouseWheelDelta != 0.0f)
	{
		if (CameraTransform.IsOrthographic())
		{
			const float ZoomFactor = MouseWheelDelta;
			CameraTransform.SetOrthoWidth(CameraTransform.GetOrthoWidth() / ZoomFactor);
			return;
		}
		CameraTransform.MoveForward(MouseWheelDelta*0.03f);
	}

	if (InputManager->IsMouseButtonDown(FInputManager::MOUSE_RIGHT))
	{
		const float MouseDeltaX = InputManager->GetMouseDeltaX();
		const float MouseDeltaY = InputManager->GetMouseDeltaY();
		if (MouseDeltaX != 0.0f || MouseDeltaY != 0.0f)
		{
			CameraTransform.Rotate(
				MouseDeltaX * CameraTransform.GetMouseSensitivity(),
				-MouseDeltaY * CameraTransform.GetMouseSensitivity());
		}
	}

	if (InputManager->IsMouseButtonDown(FInputManager::MOUSE_MIDDLE))
	{
		const float MouseDeltaX = InputManager->GetMouseDeltaX();
		const float MouseDeltaY = InputManager->GetMouseDeltaY();
		if (MouseDeltaX != 0.0f || MouseDeltaY != 0.0f)
		{
			const FVector Right = CameraTransform.GetRight().GetSafeNormal();
			const FVector Forward = CameraTransform.GetForward().GetSafeNormal();
			const FVector Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();

			CameraTransform.OffsetPosition(Right * (-MouseDeltaX * DeltaTime * CameraTransform.GetSpeed()));
			CameraTransform.OffsetPosition(Up * (MouseDeltaY * DeltaTime * CameraTransform.GetSpeed()));
		}
	}
}
