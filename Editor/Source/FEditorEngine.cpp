#include "FEditorEngine.h"

#include "UI/EditorViewportClient.h"
#include "UI/ViewportWindow.h"
#include "Core/Core.h"
#include "Core/ConsoleVariableManager.h"
#include "Core/Paths.h"
#include "World/Level.h"
#include "World/World.h"
#include "Camera/Camera.h"
#include "Component/PrimitiveComponent.h"
#include "Debug/EngineLog.h"
#include "Math/MathUtility.h"
#include "Platform/Windows/Window.h"
#include "imgui_impl_win32.h"

#include <algorithm>
#include "Actor/StaticMeshActor.h"
#include "Renderer/Renderer.h"
#include <commdlg.h>
#include <cmath>
#include <filesystem>

namespace
{
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

	WindowManager.Initialize(InputManager, EnhancedInput);
	const float Width = MainWindow ? static_cast<float>(MainWindow->GetWidth()) : 1280.0f;
	const float Height = MainWindow ? static_cast<float>(MainWindow->GetHeight()) : 720.0f;
	WindowManager.SetRootRect(FRect(0.0f, 0.0f, Width, Height));
	WindowManager.AddWindow(new SViewportWindow(FRect(0.0f, 0.0f, Width, Height), CreateContext(FRect(0.0f, 0.0f, Width, Height))));

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
	EditorUI.DetachFromRenderer();
	WindowManager.Shutdown();
	FEngine::Shutdown();
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

	EditorUI.Initialize(Core.get());
	EditorUI.SetupWindow(MainWindow);
	EditorUI.AttachToRenderer();

	UE_LOG("EditorEngine initialized");
}

void FEditorEngine::SetViewportLayoutBounds(int32 InTopLeftX, int32 InTopLeftY, uint32 InWidth, uint32 InHeight)
{
	WindowManager.SetRootRect(FRect(
		static_cast<float>(InTopLeftX),
		static_cast<float>(InTopLeftY),
		static_cast<float>(InWidth),
		static_cast<float>(InHeight)));
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
	return new FEditorViewportClient(EditorUI, MainWindow, EEditorViewportType::Perspective, ELevelType::Editor);
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
		float CameraDistance = 10.0f;
		if (UPrimitiveComponent* PrimitiveComponent = TestActor->GetPrimitiveComponent())
		{
			const FBoxSphereBounds Bounds = PrimitiveComponent->GetWorldBounds();
			const float SafeRadius = FMath::Max(Bounds.Radius, 0.5f);
			const float HalfFovRadians = FMath::DegreesToRadians(Camera->GetFOV() * 0.5f);
			const float SafeTanHalfFov = FMath::Max(std::tanf(HalfFovRadians), 0.01f);
			CameraDistance = FMath::Max((SafeRadius / SafeTanHalfFov) * 1.2f, SafeRadius * 2.0f);
		}

		Camera->SetPosition({ -CameraDistance, 0.0f, 0.0f });
		Camera->SetRotation(90.0f, 0.0f);
#else
		Camera->SetPosition({ -6.0f, -6.0f, 5.0f });
		Camera->SetRotation(45.0f, -30.0f);
		Camera->SetFOV(60.0f);
#endif
	}
	ViewportClient->SaveInitialCameraState();

	ViewportClient->SetRenderMode(ERenderMode::SolidWireframe);
	ViewportClient->GetShowFlags().SetFlag(EEngineShowFlags::SF_WorldAxis, false);
	ViewportClient->SetGridVisible(false);
	UE_LOG("[ObjViewerTest] Loaded startup mesh: %s", SelectedPath.c_str());
}
