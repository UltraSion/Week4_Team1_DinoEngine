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
#include "Input/InputAction.h"
#include "Input/EnhancedInputManager.h"
#include "Input/InputMappingContext.h"
#include "Input/InputModifier.h"
#include "Input/InputTrigger.h"

FEditorViewportClient::FEditorViewportClient(FEditorUI& InEditorUI, FWindow* InMainWindow, TArray<AActor*>& InSeletedActors)
	: EditorUI(InEditorUI)
	, MainWindow(InMainWindow)
	, SeletedActors(InSeletedActors)
{
}

void FEditorViewportClient::Attach(FCore* Core, FRenderer* Renderer)
{
	if (!Core || !Renderer || !MainWindow)
	{
		return;
	}

	EditorUI.Initialize(Core);
	EditorUI.SetupWindow(MainWindow);
	EditorUI.AttachToRenderer(Renderer);

	WireFrameMaterial = FMaterialManager::Get().FindByName(WireframeMaterialName);
	CreateGridResource(Renderer);
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

void FEditorViewportClient::Detach(FCore* Core, FRenderer* Renderer)
{
	Gizmo.EndDrag();
	EditorUI.DetachFromRenderer(Renderer);
	GridMesh.reset();
	GridMaterial.reset();
}

void FEditorViewportClient::Tick(FCore* Core, float DeltaTime)
{
	if (!Core)
	{
		return;
	}

	if (ImGui::GetCurrentContext())
	{
		const ImGuiIO& IO = ImGui::GetIO();
		if ((IO.WantCaptureKeyboard || IO.WantCaptureMouse) && !EditorUI.IsViewportInteractive())
		{
			return;
		}
	}

	if (!EditorUI.IsViewportInteractive())
	{
		return;
	}

	FViewportClient::Tick(Core, DeltaTime);
}

void FEditorViewportClient::HandleMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	if (!Core || !EditorUI.IsViewportInteractive())
	{
		return;
	}

	if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse && !EditorUI.IsViewportInteractive())
	{
		return;
	}

	ULevel* Level = ResolveLevel(Core);
	AActor* SelectedActor = Core->GetSelectedActor();
	if (!Level)
	{
		return;
	}

	const bool bHasViewportMouse = EditorUI.GetViewportMousePosition(
		static_cast<int32>(static_cast<short>(LOWORD(LParam))),
		static_cast<int32>(static_cast<short>(HIWORD(LParam))),
		ScreenMouseX,
		ScreenMouseY,
		ScreenWidth,
		ScreenHeight);

	const bool bRightMouseDown = Core->GetInputManager() &&
		Core->GetInputManager()->IsMouseButtonDown(FInputManager::MOUSE_RIGHT);

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
		if (!bHasViewportMouse)
		{
			return;
		}

		if (SelectedActor && Gizmo.BeginDrag(SelectedActor, &CameraTransform, Picker, ScreenMouseX, ScreenMouseY, ScreenWidth, ScreenHeight))
		{
			return;
		}

		{
			AActor* PickedActor = Picker.PickActor(Core->GetActiveWorld()->GetAllActors(), &CameraTransform, ScreenMouseX, ScreenMouseY, ScreenWidth, ScreenHeight);
			Core->SetSelectedActor(PickedActor);
			EditorUI.SyncSelectedActorProperty();
		}
		return;

	case WM_MOUSEMOVE:
		if (!bHasViewportMouse)
		{
			Gizmo.ClearHover();
			return;
		}

		if (!Gizmo.IsDragging())
		{
			Gizmo.UpdateHover(SelectedActor, &CameraTransform, Picker, ScreenMouseX, ScreenMouseY, ScreenWidth, ScreenHeight);
			return;
		}

		if (Gizmo.UpdateDrag(SelectedActor, &CameraTransform, Picker, ScreenMouseX, ScreenMouseY, ScreenWidth, ScreenHeight))
		{
			EditorUI.SyncSelectedActorProperty();
		}
		return;

	case WM_LBUTTONUP:
		if (Gizmo.IsDragging())
		{
			Gizmo.EndDrag();
			if (bHasViewportMouse)
			{
				Gizmo.UpdateHover(SelectedActor, &CameraTransform, Picker, ScreenMouseX, ScreenMouseY, ScreenWidth, ScreenHeight);
			}
			else
			{
				Gizmo.ClearHover();
			}
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
		Core->GetLevel()->ClearActors();
		bool bLoaded = FSceneSerializer::Load(Core->GetLevel(), FilePath, Core->GetRenderer()->GetDevice());

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
	if (Core && Core->GetRenderer() && FilePath.ends_with(".obj"))
	{
		const FRay Ray = Picker.ScreenToRay(&CameraTransform, ScreenMouseX, ScreenMouseY, ScreenWidth, ScreenHeight);

		AObjActor* NewActor = Core->GetLevel()->SpawnActor<AObjActor>("ObjActor");
		NewActor->LoadObj(Core->GetRenderer()->GetDevice(), FPaths::ToRelativePath(FilePath));
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
	CameraContext = new FInputMappingContext();

	auto& W = CameraContext->AddMapping(&MoveForwardAction, 'W');
	W.Triggers.push_back(new FTriggerDown());

	auto& S = CameraContext->AddMapping(&MoveForwardAction, 'S');
	S.Triggers.push_back(new FTriggerDown());
	S.Modifiers.push_back(new FModifierNegative());

	auto& D = CameraContext->AddMapping(&MoveRightAction, 'D');
	D.Triggers.push_back(new FTriggerDown());

	auto& A = CameraContext->AddMapping(&MoveRightAction, 'A');
	A.Triggers.push_back(new FTriggerDown());
	A.Modifiers.push_back(new FModifierNegative());

	auto& E = CameraContext->AddMapping(&MoveUpAction, 'E');
	E.Triggers.push_back(new FTriggerDown());

	auto& Q = CameraContext->AddMapping(&MoveUpAction, 'Q');
	Q.Triggers.push_back(new FTriggerDown());
	Q.Modifiers.push_back(new FModifierNegative());

	CameraContext->AddMapping(&LookXAction, static_cast<int32>(EInputKey::MouseX));
	CameraContext->AddMapping(&LookYAction, static_cast<int32>(EInputKey::MouseY));

	EnhancedInput->AddMappingContext(CameraContext, 0);

	EnhancedInput->BindAction(&MoveForwardAction, ETriggerEvent::Triggered,
		[this](const FInputActionValue& Value)
		{
			if (InputManager && InputManager->IsMouseButtonDown(FInputManager::MOUSE_RIGHT))
			{
				CameraTransform.MoveForward(Value.Get() * CurrentDeltaTime);
			}
		});

	EnhancedInput->BindAction(&MoveRightAction, ETriggerEvent::Triggered,
		[this](const FInputActionValue& Value)
		{
			if (InputManager && InputManager->IsMouseButtonDown(FInputManager::MOUSE_RIGHT))
			{
				CameraTransform.MoveRight(Value.Get() * CurrentDeltaTime);
			}
		});

	EnhancedInput->BindAction(&MoveUpAction, ETriggerEvent::Triggered,
		[this](const FInputActionValue& Value)
		{
			if (InputManager && InputManager->IsMouseButtonDown(FInputManager::MOUSE_RIGHT))
			{
				CameraTransform.MoveUp(Value.Get() * CurrentDeltaTime);
			}
		});

	EnhancedInput->BindAction(&LookXAction, ETriggerEvent::Triggered,
		[this](const FInputActionValue& Value)
		{
			if (InputManager && InputManager->IsMouseButtonDown(FInputManager::MOUSE_RIGHT))
			{
				CameraTransform.Rotate(Value.Get() * CameraTransform.GetMouseSensitivity(), 0.0f);
			}
		});

	EnhancedInput->BindAction(&LookYAction, ETriggerEvent::Triggered,
		[this](const FInputActionValue& Value)
		{
			if (InputManager && InputManager->IsMouseButtonDown(FInputManager::MOUSE_RIGHT))
			{
				CameraTransform.Rotate(0.0f, -Value.Get() * CameraTransform.GetMouseSensitivity());
			}
		});
}
