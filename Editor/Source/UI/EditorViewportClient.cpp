#include "EditorViewportClient.h"

#include "EditorUI.h"
#include "Actor/Actor.h"
#include "Core/Core.h"
#include "Input/InputManager.h"
#include "Debug/EngineLog.h"
#include "Platform/Windows/Window.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/Materialmanager.h"
#include "Renderer/Material.h"
#include "Renderer/ShaderMap.h"
#include "World/Level.h"
#include "Serializer/SceneSerializer.h"
#include "Component/PrimitiveComponent.h"
#include "Core/Paths.h"
#include "imgui.h"
#include "Actor/ObjActor.h"
#include "Actor/SkySphereActor.h"
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
}

FEditorViewportClient::FEditorViewportClient(FEditorUI& InEditorUI, FWindow* InMainWindow, TArray<AActor*>& InSeletedActors, EEditorViewportType InViewportType, ELevelType InWorldType)
	: EditorUI(InEditorUI)
	, MainWindow(InMainWindow)
	, SeletedActors(InSeletedActors)
	, ViewportType(InViewportType)
{
	SetWorldType(InWorldType);
	ConfigureDefaultView();
}

void FEditorViewportClient::Attach(FCore* Core)
{
	if (!Core || !GRenderer || !MainWindow)
	{
		return;
	}

	WireFrameMaterial = FMaterialManager::Get().FindByName(WireframeMaterialName);
	CreateGridResource(GRenderer);
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

void FEditorViewportClient::CreateGridResource(FRenderer* Renderer)
{
	ID3D11Device* Device = Renderer->GetDevice();
	if (!Device)
	{
		return;
	}

	GridMesh = std::make_unique<FMeshData>();
	GridMesh->Topology = EMeshTopology::EMT_TriangleList;
	for (int i = 0; i < 18; ++i)
	{
		FPrimitiveVertex v;
		GridMesh->Vertices.push_back(v);
		GridMesh->Indices.push_back(i);
	}
	GridMesh->CreateVertexAndIndexBuffer(Device);

	std::wstring ShaderDirW = FPaths::ShaderDir();
	std::wstring VSPath = ShaderDirW + L"AxisVertexShader.hlsl";
	std::wstring PSPath = ShaderDirW + L"AxisPixelShader.hlsl";
	auto VS = FShaderMap::Get().GetOrCreateVertexShader(Device, VSPath.c_str());
	auto PS = FShaderMap::Get().GetOrCreatePixelShader(Device, PSPath.c_str());

	GridMaterial = std::make_shared<FMaterial>();
	GridMaterial->SetOriginName("M_EditorGrid");
	GridMaterial->SetVertexShader(VS);
	GridMaterial->SetPixelShader(PS);

	FRasterizerStateOption RasterizerOption;
	RasterizerOption.FillMode = D3D11_FILL_SOLID;
	RasterizerOption.CullMode = D3D11_CULL_NONE;
	auto RS = Renderer->GetRenderStateManager()->GetOrCreateRasterizerState(RasterizerOption);
	GridMaterial->SetRasterizerOption(RasterizerOption);
	GridMaterial->SetRasterizerState(RS);

	FDepthStencilStateOption DepthStencilOption;
	DepthStencilOption.DepthEnable = true;
	DepthStencilOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	auto DSS = Renderer->GetRenderStateManager()->GetOrCreateDepthStencilState(DepthStencilOption);
	GridMaterial->SetDepthStencilOption(DepthStencilOption);
	GridMaterial->SetDepthStencilState(DSS);

	int32 SlotIndex = GridMaterial->CreateConstantBuffer(Device, 32);
	if (SlotIndex >= 0)
	{
		GridMaterial->RegisterParameter("GridSize", SlotIndex, 12, 4);
		GridMaterial->RegisterParameter("LineThickness", SlotIndex, 16, 4);
		GridMaterial->SetParameterData("GridSize", &GridSize, 4);
		GridMaterial->SetParameterData("LineThickness", &LineThickness, 4);
	}
}

void FEditorViewportClient::Detach()
{
	Gizmo.EndDrag();
	GridMesh.reset();
	GridMaterial.reset();
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
	//if (!Core || !EditorUI.IsViewportInteractive())
	//{
	//	return;
	//}

	//if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse && !EditorUI.IsViewportInteractive())
	//{
	//	return;
	//}

	ULevel* Level = ResolveLevel(Core);
	UWorld* World = ResolveWorld(Core);
	AActor* SelectedActor = Core->GetSelectedActor();
	if (!Level || !World)
	{
		return;
	}

	//const bool bHasViewportMouse = EditorUI.GetViewportMousePosition(
	//	static_cast<int32>(static_cast<short>(LOWORD(LParam))),
	//	static_cast<int32>(static_cast<short>(HIWORD(LParam))),
	//	ScreenMouseX,
	//	ScreenMouseY,
	//	ScreenWidth,
	//	ScreenHeight);

	const bool bRightMouseDown = InputManager &&
		InputManager->IsMouseButtonDown(FInputManager::MOUSE_RIGHT);

	switch (Msg)
	{
	case WM_KEYDOWN:
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

	case WM_LBUTTONDOWN:
		//if (!bHasViewportMouse)
		//{
		//	return;
		//}

		if (SelectedActor && Gizmo.BeginDrag(SelectedActor, &CameraTransform, Picker, ViewportMouseX, ViewportMouseY, ViewportWidth, ViewportHeight))
		{
			return;
		}

		{
			AActor* PickedActor = Picker.PickActor(World->GetAllActors(), &CameraTransform, ViewportMouseX, ViewportMouseY, ViewportWidth, ViewportHeight);
			Core->SetSelectedActor(PickedActor);
			EditorUI.SyncSelectedActorProperty();
		}
		return;

	case WM_MOUSEMOVE:
		//if (!bHasViewportMouse)
		//{
		//	Gizmo.ClearHover();
		//	return;
		//}

		if (!Gizmo.IsDragging())
		{
			Gizmo.UpdateHover(SelectedActor, &CameraTransform, Picker, ViewportMouseX, ViewportMouseY, ViewportWidth, ViewportHeight);
			return;
		}

		if (Gizmo.UpdateDrag(SelectedActor, &CameraTransform, Picker, ViewportMouseX, ViewportMouseY, ViewportWidth, ViewportHeight))
		{
			EditorUI.SyncSelectedActorProperty();
		}
		return;

	case WM_LBUTTONUP:
		if (Gizmo.IsDragging())
		{
			Gizmo.EndDrag();
			//if (bHasViewportMouse)
			//{
			//	Gizmo.UpdateHover(SelectedActor, &CameraTransform, Picker, ScreenMouseX, ScreenMouseY, ScreenWidth, ScreenHeight);
			//}
			//else
			//{
			//	Gizmo.ClearHover();
			//}
			EditorUI.SyncSelectedActorProperty();
		}
		return;

	default:
		return;
	}
}

void FEditorViewportClient::HandleFileDoubleClick(const FString& FilePath)
{
	FCore* Core = EditorUI.GetCore();
	if (Core && FilePath.ends_with(".json"))
	{
		Core->SetSelectedActor(nullptr);
		ULevel* Level = ResolveLevel(Core);
		if (!Level)
		{
			return;
		}

		Level->ClearActors();
		bool bLoaded = FSceneSerializer::Load(Level, FilePath, GRenderer->GetDevice());

		if (bLoaded)
		{
			UE_LOG("Level loaded: %s", FilePath.c_str());
		}
		else
		{
			MessageBoxW(nullptr, L"Level information is invalid.", L"Error", MB_OK | MB_ICONWARNING);
		}
	}
}

void FEditorViewportClient::HandleFileDropOnViewport(const FString& FilePath)
{
	FCore* Core = EditorUI.GetCore();
	if (GRenderer && Core && FilePath.ends_with(".obj"))
	{
		ULevel* Level = ResolveLevel(Core);
		if (!Level)
		{
			return;
		}

		const FRay Ray = Picker.ScreenToRay(&CameraTransform, ViewportMouseX, ViewportMouseY, ViewportWidth, ViewportHeight);

		AObjActor* NewActor = Level->SpawnActor<AObjActor>("ObjActor");
		NewActor->LoadObj(GRenderer->GetDevice(), FPaths::ToRelativePath(FilePath));
		FVector V = Ray.Origin + Ray.Direction * 5;
		NewActor->SetActorLocation(V);
	}
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

	if (GridMesh && GridMaterial && bShowGrid)
	{
		FRenderCommand GridCmd;
		GridCmd.MeshData = GridMesh.get();
		GridCmd.Material = GridMaterial.get();
		GridCmd.WorldMatrix = FMatrix::Identity;
		GridCmd.RenderLayer = ERenderLayer::Default;
		OutQueue.AddCommand(GridCmd);
	}

	if (SeletedActors.empty())
	{
		return;
	}

	AActor* GizmoTarget = SeletedActors[0];
	if (GizmoTarget && !GizmoTarget->IsA<ASkySphereActor>())
	{
		Gizmo.BuildRenderCommands(GizmoTarget, &CameraTransform, OutQueue);
	}
}

void FEditorViewportClient::PostRender(FCore* Core, FRenderer* Renderer)
{
	if (!Core || !Renderer)
	{
		return;
	}

	AActor* Selected = Core->GetSelectedActor();
	if (!Selected || Selected->IsPendingDestroy() || !Selected->IsVisible() || Selected->IsA<ASkySphereActor>())
	{
		return;
	}

	if (!GetShowFlags().HasFlag(EEngineShowFlags::SF_Primitives))
	{
		return;
	}

	for (UActorComponent* Component : Selected->GetComponents())
	{
		if (!Component->IsA(UPrimitiveComponent::StaticClass()))
		{
			continue;
		}

		UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(Component);
		if (PrimitiveComponent->GetPrimitive())
		{
			Renderer->RenderOutline(
				PrimitiveComponent->GetPrimitive()->GetMeshData(),
				PrimitiveComponent->GetWorldTransform()
			);
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

void FEditorViewportClient::SetSelection(TArray<AActor*>& SeletedActorArrayPtr)
{
	SeletedActors = SeletedActorArrayPtr;
}

void FEditorViewportClient::SetupInputBindings()
{
}

void FEditorViewportClient::ProcessCameraInput(FCore* Core, float DeltaTime)
{
	(void)Core;

	if (!InputManager || !InputManager->IsMouseButtonDown(FInputManager::MOUSE_RIGHT))
	{
		return;
	}

	float ForwardInput = 0.0f;
	float RightInput = 0.0f;
	float UpInput = 0.0f;

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

	const float MouseDeltaX = InputManager->GetMouseDeltaX();
	const float MouseDeltaY = InputManager->GetMouseDeltaY();
	if (MouseDeltaX != 0.0f || MouseDeltaY != 0.0f)
	{
		CameraTransform.Rotate(
			MouseDeltaX * CameraTransform.GetMouseSensitivity(),
			-MouseDeltaY * CameraTransform.GetMouseSensitivity());
	}
}
