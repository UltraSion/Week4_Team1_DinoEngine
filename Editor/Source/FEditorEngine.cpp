#include "FEditorEngine.h"

#include "UI/EditorViewportClient.h"
#include "UI/EditorOrthoViewportClient.h"
#include "UI/EditorPerspectiveViewportClient.h"
#include "UI/ViewportWindow.h"
#include "Core/Core.h"
#include "Core/Viewport.h"
#include "Core/ConsoleVariableManager.h"
#include "Core/Paths.h"
#include "World/Level.h"
#include "World/World.h"
#include "Camera/Camera.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Debug/EngineLog.h"
#include "Math/MathUtility.h"
#include "Mesh/ObjImporter.h"
#include "Platform/Windows/Window.h"
#include "imgui_impl_win32.h"

#include <algorithm>
#include "Actor/StaticMeshActor.h"
#include "Component/StaticMeshComponent.h"
#include "Renderer/Renderer.h"
#include <commdlg.h>
#include <cmath>
#include <filesystem>

namespace
{
	void SnapObjViewerActorBottomToZero(AActor* Actor, FEditorViewportClient* ViewportClient)
	{
#if IS_OBJ_VIEWER
		if (!Actor || !ViewportClient)
		{
			return;
		}

		USceneComponent* Root = Actor->GetRootComponent();
		if (!Root)
		{
			return;
		}

		FTransform Transform = Root->GetRelativeTransform();
		FVector Location = Transform.GetLocation();
		Location.Z -= ViewportClient->GetObjViewerBottomZ(Actor);
		Transform.SetLocation(Location);
		Root->SetRelativeTransform(Transform);
		ViewportClient->RefreshObjViewerCameraPivot(Actor);
#else
		(void)Actor;
		(void)ViewportClient;
#endif
	}

	FString PromptForObjFilePath()
	{
		char FileName[MAX_PATH] = "";
		const FString MeshDir = FPaths::MeshDir().string();

		OPENFILENAMEA Ofn = {};
		Ofn.lStructSize = sizeof(OPENFILENAMEA);
		Ofn.lpstrFilter = "OBJ Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
		Ofn.lpstrFile = FileName;
		Ofn.nMaxFile = MAX_PATH;
		Ofn.lpstrDefExt = "obj";
		Ofn.lpstrInitialDir = MeshDir.c_str();
		Ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		if (GetOpenFileNameA(&Ofn))
		{
			return FString(FileName);
		}

		return "";
	}
}

bool FEditorEngine::Initialize(HINSTANCE hInstance)
{
	ImGui_ImplWin32_EnableDpiAwareness();

	if (!FEngine::Initialize(hInstance, L"Jungle Editor", 1280, 720))
	{
		return false;
	}

	WindowManager.Initialize(
		InputManager,
		EnhancedInput,
		[this](const FRect& InRect)
		{
			return CreateEditorViewportContext(InRect, EEditorViewportType::Perspective);
		});
	const float Width = MainWindow ? static_cast<float>(MainWindow->GetWidth()) : 1280.0f;
	const float Height = MainWindow ? static_cast<float>(MainWindow->GetHeight()) : 720.0f;
	WindowManager.SetRootRect(FRect(0.0f, 0.0f, Width, Height));
	WindowManager.AddWindow(new SViewportWindow(
		FRect(0.0f, 0.0f, Width, Height),
		CreateEditorViewportContext(FRect(0.0f, 0.0f, Width, Height), EEditorViewportType::Perspective)));

#if IS_OBJ_VIEWER //뷰어를 시작할 때 기본적으로 obj파일을 로딩합니다
	RunObjViewerStartupTest();
#endif
	return true;
}

FEditorEngine::~FEditorEngine()
{
}

void FEditorEngine::OpenNewObj()
{
#if IS_OBJ_VIEWER
	RunObjViewerStartupTest();
#endif
}

void FEditorEngine::Shutdown()
{
	EditorUI.SaveEditorSettings();
	EditorUI.DetachFromRenderer();
	WindowManager.Shutdown();
	FEngine::Shutdown();
}

void FEditorEngine::SaveEditorSettings()
{
	EditorUI.SaveEditorSettings();
}

void FEditorEngine::PreInitialize()
{
	FEngineLog::Get().SetCallback([this](const char* Msg)
		{
			EditorUI.GetConsole().AddLog("%s", Msg);
		});
}

void FEditorEngine::PostInitialize()
{
	FConsoleVariableManager& CVM = FConsoleVariableManager::Get();
	CVM.GetAllNames([this](const FString& Name)
		{
			EditorUI.GetConsole().RegisterCommand(Name.c_str());
		});

	EditorUI.GetConsole().SetCommandHandler([](const char* CommandLine)
		{
			FString Result;
			if (FConsoleVariableManager::Get().Execute(CommandLine, Result))
			{
				FEngineLog::Get().Log("%s", Result.c_str());
			}
			else
			{
				FEngineLog::Get().Log("[error] Unknown command: '%s'", CommandLine);
			}
		});

	EditorUI.Initialize(Core.get(), &WindowManager);
	EditorUI.SetupWindow(MainWindow);
	EditorUI.AttachToRenderer();

	UE_LOG("EditorEngine initialized");
}

void FEditorEngine::SetViewportLayoutBounds(FRect InRect)
{
	WindowManager.SetRootRect(InRect);
}

void FEditorEngine::ProcessInput(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	FEngine::ProcessInput(Hwnd, Msg, WParam, LParam);
	WindowManager.HandleMessage(Core.get(), Hwnd, Msg, WParam, LParam);
}

void FEditorEngine::Tick(float DeltaTime)
{
	Input(Core->GetTimer().GetDeltaTime());
	WindowManager.Tick(DeltaTime);
	WindowManager.CheckParent();
	Render();
}

void FEditorEngine::Render()
{
	if (!Core)
	{
		return;
	}

	if (!GRenderer || GRenderer->IsOccluded())
	{
		return;
	}

	
	SetViewportLayoutBounds(EditorUI.GetCentralDockSpaceRect());
	GRenderer->BeginFrame();
	WindowManager.RenderWindows();
	GRenderer->EndFrame();
}

void FEditorEngine::OnMainWindowResized(int32 Width, int32 Height)
{
	WindowManager.SetRootRect(FRect(
		0.0f,
		0.0f,
		static_cast<float>((std::max)(Width, 0)),
		static_cast<float>((std::max)(Height, 0))));
}

FViewportClient* FEditorEngine::CreateViewportClient()
{
	return CreateEditorViewportClient(EEditorViewportType::Perspective, ELevelType::Editor);
}

FViewportContext* FEditorEngine::CreateEditorViewportContext(const FRect& InRect, EEditorViewportType InViewportType)
{
	FViewportClient* ViewportClient = CreateEditorViewportClient(InViewportType, ELevelType::Editor);
	FViewport* Viewport = new FViewport(InRect);
	FViewportContext* ViewportContext = new FViewportContext(Viewport, ViewportClient);
	ViewportContext->Initialize(Core.get(), InputManager, EnhancedInput);
	return ViewportContext;
}

FEditorViewportClient* FEditorEngine::CreateEditorViewportClient(EEditorViewportType InViewportType, ELevelType InWorldType)
{
	switch (InViewportType)
	{
	case EEditorViewportType::Top:
		return new FEditorOrthoViewportClient(EditorUI, MainWindow, EOrthoViewType::Top, InWorldType);
	case EEditorViewportType::Front:
		return new FEditorOrthoViewportClient(EditorUI, MainWindow, EOrthoViewType::Front, InWorldType);
	case EEditorViewportType::Right:
		return new FEditorOrthoViewportClient(EditorUI, MainWindow, EOrthoViewType::Right, InWorldType);
	case EEditorViewportType::Perspective:
	default:
		return new FEditorPerspectiveViewportClient(EditorUI, MainWindow, InWorldType);
	}
}

void FEditorEngine::RunObjViewerStartupTest()
{
	if (!Core || !GRenderer)
	{
		return;
	}

	FEditorViewportClient* ViewportClient = EditorUI.GetActiveViewportClient();
	if (!ViewportClient)
	{
		return;
	}

	ULevel* Level = ViewportClient->ResolveLevel(Core.get());
	UWorld* World = ViewportClient->ResolveWorld(Core.get());
	if (!Level || !World)
	{
		return;
	}

	const FString SelectedPath = PromptForObjFilePath();
	if (SelectedPath.empty())
	{
		UE_LOG("[ObjViewerTest] Startup mesh selection canceled");
		return;
	}

	const std::filesystem::path AssetPath = FPaths::ToAbsolutePath(SelectedPath);
	const std::filesystem::path Extension = AssetPath.extension();
	if (!std::filesystem::exists(AssetPath) || (Extension != ".obj" && Extension != ".OBJ"))
	{
		UE_LOG("[ObjViewerTest] Invalid startup mesh selection: %s", SelectedPath.c_str());
		return;
	}

#if IS_OBJ_VIEWER
	FObjImporter::SetImportAxisMapping(FObjImporter::MakeDefaultImportAxisMapping());
#endif
	Core->SetSelectedActor(nullptr);
	Level->ClearActors();

	AStaticMeshActor* MeshActor = Level->SpawnActor<AStaticMeshActor>("ObjViewerMesh");
	if (MeshActor)
	{
		MeshActor->LoadStaticMesh(GRenderer->GetDevice(), AssetPath.string());
		Core->SetSelectedActor(MeshActor);
	}
	EditorUI.SyncSelectedActorProperty();

	if (FCamera* Camera = ViewportClient->GetCamera())
	{
#if IS_OBJ_VIEWER //뷰어에서는 mesh의 크기에 따라 다른 위치에 카메라가 놓입니다. 다시 로드할 때도 적용됩니다.
		Camera->SetFOV(60.0f);
		if (MeshActor)
		{
			SnapObjViewerActorBottomToZero(MeshActor, ViewportClient);
			ViewportClient->FrameObjViewerCamera(MeshActor, true);
		}
#else
		Camera->SetPosition({ -6.0f, -6.0f, 5.0f });
		Camera->SetRotation(45.0f, -30.0f);
		Camera->SetFOV(60.0f);
		ViewportClient->SaveInitialCameraState();
#endif
	}

	ViewportClient->SetRenderMode(ERenderMode::SolidWireframe);
	ViewportClient->GetShowFlags().SetFlag(EEngineShowFlags::SF_WorldAxis, false);
	ViewportClient->SetGridVisible(false);
	UE_LOG("[ObjViewerTest] Loaded startup mesh: %s", SelectedPath.c_str());
}
