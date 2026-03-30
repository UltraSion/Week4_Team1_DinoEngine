#include "FEditorEngine.h"

#include "UI/EditorViewportClient.h"
#include "UI/ViewportWindow.h"
#include "Core/Core.h"
#include "Core/ConsoleVariableManager.h"
#include "Core/Paths.h"
#include "World/Level.h"
#include "World/World.h"
#include "Actor/ObjActor.h"
#include "Camera/Camera.h"
#include "Debug/EngineLog.h"
#include "Platform/Windows/Window.h"
#include "imgui_impl_win32.h"

#include <commdlg.h>
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

	const float Width = MainWindow ? static_cast<float>(MainWindow->GetWidth()) : 1280.0f;
	const float Height = MainWindow ? static_cast<float>(MainWindow->GetHeight()) : 720.0f;
	WindowManager.AddWindow(new SViewportWindow(FRect(0.0f, 0.0f, Width, Height), CreateContext(FRect(0.0f, 0.0f, Width, Height))));

#if IS_OBJ_VIEWER //뷰어를 시작할 때 기본적으로 obj파일을 로딩합니다
	RunObjViewerStartupTest();
#endif
	return true;
}

FEditorEngine::~FEditorEngine()
{
}

void FEditorEngine::Shutdown()
{
	EditorUI.DetachFromRenderer();
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

	AObjActor* TestActor = Level->SpawnActor<AObjActor>("ObjViewerStartupTest");
	if (!TestActor)
	{
		UE_LOG("[ObjViewerTest] Failed to spawn startup actor");
		return;
	}

	TestActor->SetActorLocation(FVector::ZeroVector);
	TestActor->LoadObj(GRenderer->GetDevice(), FPaths::ToRelativePath(SelectedPath));
	Core->SetSelectedActor(TestActor);
	EditorUI.SyncSelectedActorProperty();

	if (FCamera* Camera = ViewportClient->GetCamera())
	{
		Camera->SetPosition({ -6.0f, -6.0f, 5.0f });
		Camera->SetRotation(45.0f, -30.0f);
		Camera->SetFOV(50.0f);
	}
	ViewportClient->SaveInitialCameraState();

	ViewportClient->SetRenderMode(ERenderMode::SolidWireframe);
	ViewportClient->GetShowFlags().SetFlag(EEngineShowFlags::SF_WorldAxis, false);
	ViewportClient->SetGridVisible(false);
	UE_LOG("[ObjViewerTest] Loaded startup mesh: %s", SelectedPath.c_str());
}
