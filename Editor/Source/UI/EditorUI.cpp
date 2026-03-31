#include "EditorUI.h"

#include "FEditorEngine.h"
#include "Actor/Actor.h"

#include "Camera/Camera.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Core/Core.h"
#include "Core/FEngine.h"
#include "Core/Paths.h"
#include "Core/ShowFlags.h"
#include "Debug/EngineLog.h"
#include "FEditorEngine.h"
#include "Object/Object.h"
#include "Platform/Windows/Window.h"
#include "Primitive/PrimitiveObj.h"
#include "Renderer/Renderer.h"
#include "Serializer/SceneSerializer.h"
#include "UI/EditorViewportClient.h"
#include "World/Level.h"
#include "Actor/StaticMeshActor.h"
#include "Component/StaticMeshComponent.h"
#include "Mesh/ObjImporter.h"
#include "Mesh/ObjManager.h"
#include "Mesh/StaticMeshRenderData.h"
#include "Primitive/PrimitiveBase.h"
#include "Renderer/Renderer.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"

#include <commdlg.h>
#include <filesystem>
#include <windows.h>
#include <windowsx.h>

enum class EFileDialogType
{
	Open,
	Save
};

namespace
{
	ImGuiDockNode* FindCentralDockNode(ImGuiDockNode* Node)
	{
		if (Node == nullptr)
		{
			return nullptr;
		}

		if (Node->IsCentralNode())
		{
			return Node;
		}

		if (ImGuiDockNode* ChildNode = FindCentralDockNode(Node->ChildNodes[0]))
		{
			return ChildNode;
		}

		return FindCentralDockNode(Node->ChildNodes[1]);
	}

	const char* GetAxisDirectionLabel(FObjImporter::EAxisDirection AxisDirection)
	{
		switch (AxisDirection)
		{
		case FObjImporter::EAxisDirection::PositiveX:
			return "+X";
		case FObjImporter::EAxisDirection::NegativeX:
			return "-X";
		case FObjImporter::EAxisDirection::PositiveY:
			return "+Y";
		case FObjImporter::EAxisDirection::NegativeY:
			return "-Y";
		case FObjImporter::EAxisDirection::PositiveZ:
			return "+Z";
		case FObjImporter::EAxisDirection::NegativeZ:
		default:
			return "-Z";
		}
	}

	int GetAxisBaseIndex(FObjImporter::EAxisDirection AxisDirection)
	{
		switch (AxisDirection)
		{
		case FObjImporter::EAxisDirection::PositiveX:
		case FObjImporter::EAxisDirection::NegativeX:
			return 0;
		case FObjImporter::EAxisDirection::PositiveY:
		case FObjImporter::EAxisDirection::NegativeY:
			return 1;
		case FObjImporter::EAxisDirection::PositiveZ:
		case FObjImporter::EAxisDirection::NegativeZ:
		default:
			return 2;
		}
	}

	bool IsAxisNegative(FObjImporter::EAxisDirection AxisDirection)
	{
		switch (AxisDirection)
		{
		case FObjImporter::EAxisDirection::NegativeX:
		case FObjImporter::EAxisDirection::NegativeY:
		case FObjImporter::EAxisDirection::NegativeZ:
			return true;
		case FObjImporter::EAxisDirection::PositiveX:
		case FObjImporter::EAxisDirection::PositiveY:
		case FObjImporter::EAxisDirection::PositiveZ:
		default:
			return false;
		}
	}

	FObjImporter::EAxisDirection MakeAxisDirection(int AxisBaseIndex, bool bNegative)
	{
		switch (AxisBaseIndex)
		{
		case 0:
			return bNegative ? FObjImporter::EAxisDirection::NegativeX : FObjImporter::EAxisDirection::PositiveX;
		case 1:
			return bNegative ? FObjImporter::EAxisDirection::NegativeY : FObjImporter::EAxisDirection::PositiveY;
		case 2:
		default:
			return bNegative ? FObjImporter::EAxisDirection::NegativeZ : FObjImporter::EAxisDirection::PositiveZ;
		}
	}

	FObjImporter::EAxisDirection GetMappingAxisDirection(const FObjImporter::FImportAxisMapping& ImportAxisMapping, int RowIndex)
	{
		switch (RowIndex)
		{
		case 0:
			return ImportAxisMapping.EngineX;
		case 1:
			return ImportAxisMapping.EngineY;
		case 2:
		default:
			return ImportAxisMapping.EngineZ;
		}
	}

	void SetMappingAxisDirection(FObjImporter::FImportAxisMapping& ImportAxisMapping, int RowIndex, FObjImporter::EAxisDirection AxisDirection)
	{
		switch (RowIndex)
		{
		case 0:
			ImportAxisMapping.EngineX = AxisDirection;
			break;
		case 1:
			ImportAxisMapping.EngineY = AxisDirection;
			break;
		case 2:
		default:
			ImportAxisMapping.EngineZ = AxisDirection;
			break;
		}
	}

	void SetMappingAxisBase(FObjImporter::FImportAxisMapping& ImportAxisMapping, int RowIndex, int NewAxisBaseIndex)
	{
		const FObjImporter::EAxisDirection CurrentDirection = GetMappingAxisDirection(ImportAxisMapping, RowIndex);
		const bool bCurrentNegative = IsAxisNegative(CurrentDirection);

		for (int OtherRowIndex = 0; OtherRowIndex < 3; ++OtherRowIndex)
		{
			if (OtherRowIndex == RowIndex)
			{
				continue;
			}

			const FObjImporter::EAxisDirection OtherDirection = GetMappingAxisDirection(ImportAxisMapping, OtherRowIndex);
			if (GetAxisBaseIndex(OtherDirection) == NewAxisBaseIndex)
			{
				const int CurrentAxisBaseIndex = GetAxisBaseIndex(CurrentDirection);
				SetMappingAxisDirection(ImportAxisMapping, OtherRowIndex, MakeAxisDirection(CurrentAxisBaseIndex, IsAxisNegative(OtherDirection)));
				break;
			}
		}

		SetMappingAxisDirection(ImportAxisMapping, RowIndex, MakeAxisDirection(NewAxisBaseIndex, bCurrentNegative));
	}

	void ToggleMappingAxisSign(FObjImporter::FImportAxisMapping& ImportAxisMapping, int RowIndex)
	{
		const FObjImporter::EAxisDirection CurrentDirection = GetMappingAxisDirection(ImportAxisMapping, RowIndex);
		SetMappingAxisDirection(ImportAxisMapping, RowIndex, MakeAxisDirection(GetAxisBaseIndex(CurrentDirection), !IsAxisNegative(CurrentDirection)));
	}

	FString BuildImportAxisSummary(const FObjImporter::FImportAxisMapping& ImportAxisMapping)
	{
		return FString("X=") + GetAxisDirectionLabel(ImportAxisMapping.EngineX)
			+ "  Y=" + GetAxisDirectionLabel(ImportAxisMapping.EngineY)
			+ "  Z=" + GetAxisDirectionLabel(ImportAxisMapping.EngineZ);
	}

	AStaticMeshActor* FindObjViewerMeshActor(ULevel* Level)
	{
		if (!Level)
		{
			return nullptr;
		}

		for (AActor* Actor : Level->GetActors())
		{
			if (AStaticMeshActor* FoundActor = dynamic_cast<AStaticMeshActor*>(Actor))
			{
				return FoundActor;
			}
		}

		return nullptr;
	}

	bool ReloadObjViewerMesh(FCore* Core, const FObjImporter::FImportAxisMapping& ImportAxisMapping)
	{
		if (!Core || !GRenderer || !FObjImporter::IsValidImportAxisMapping(ImportAxisMapping))
		{
			return false;
		}

		AStaticMeshActor* MeshActor = FindObjViewerMeshActor(Core->GetLevel());
		if (!MeshActor)
		{
			return false;
		}

		UStaticMeshComponent* MeshComponent = MeshActor->GetStaticMeshComponent();
		if (!MeshComponent)
		{
			return false;
		}

		const FString AssetPath = MeshComponent->GetStaticMeshAsset();
		if (AssetPath.empty())
		{
			return false;
		}

		FObjImporter::SetImportAxisMapping(ImportAxisMapping);
		MeshActor->LoadStaticMesh(GRenderer->GetDevice(), AssetPath);
		return true;
	}

	std::string GetFilePathUsingDialog(EFileDialogType Type)
	{
		char FileName[MAX_PATH] = "";
		const FString ContentDir = FPaths::ContentDir().string();

		OPENFILENAMEA Ofn = {};
		Ofn.lStructSize = sizeof(OPENFILENAMEA);
		Ofn.lpstrFilter = "Level Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
		Ofn.lpstrFile = FileName;
		Ofn.nMaxFile = MAX_PATH;
		Ofn.lpstrDefExt = "json";
		Ofn.lpstrInitialDir = ContentDir.c_str();

		if (Type == EFileDialogType::Save)
		{
			Ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
			if (GetSaveFileNameA(&Ofn))
			{
				return std::string(FileName);
			}
		}
		else
		{
			Ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
			if (GetOpenFileNameA(&Ofn))
			{
				return std::string(FileName);
			}
		}

		return "";
	}
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

void FEditorUI::Initialize(FCore* InCore, FWindowManager* InWindowManager)
{
	Core = InCore;
	WindowManager = InWindowManager;

	Property.OnChanged = [this](const FVector& Loc, const FVector& Rot, const FVector& Scl)
		{
			if (!Core)
			{
				return;
			}

			AActor* Selected = Core->GetSelectedActor();
			if (!Selected)
			{
				return;
			}

			if (USceneComponent* Root = Selected->GetRootComponent())
			{
				FTransform Transform = Root->GetRelativeTransform();
				Transform.SetLocation(Loc);
				Transform.SetRotation(FRotator::MakeFromEuler(Rot));
				Transform.SetScale3D(Scl);
				Root->SetRelativeTransform(Transform);
			}
		};

	ContentBrowser.OnFileDoubleClickCallback = [this](const FString& FilePath)
		{
			if (ActiveViewportClient)
			{
				ActiveViewportClient->HandleFileDoubleClick(FilePath); //-> 이부분을 수정
			}
		};

	ContentBrowser.OnFileDragEnd = [this](const FString& DraggingFilePath, const FString& ReleaseDirectory)
		{
			if (ContentBrowser.IsHovered() && ContentBrowser.IsMouseOnDirectory())
			{
				std::filesystem::path Src = DraggingFilePath;
				std::filesystem::path DstDir = ReleaseDirectory;
				std::filesystem::path Dst = DstDir / Src.filename();
				std::error_code Ec;

				if (std::filesystem::exists(Dst))
				{
					const int Result = MessageBoxW(
						nullptr,
						L"A file with the same name already exists.\nOverwrite it?",
						L"Overwrite",
						MB_YESNO | MB_ICONWARNING);

					if (Result != IDYES)
					{
						return;
					}

					std::filesystem::remove(Dst, Ec);
					if (Ec)
					{
						MessageBoxW(nullptr, L"Delete failed.", L"Error", MB_OK | MB_ICONERROR);
						return;
					}
				}

				std::filesystem::rename(Src, Dst, Ec);
				if (Ec)
				{
					UE_LOG("Move Failed: %s", Ec.message().c_str());
				}
				else
				{
					UE_LOG("Moved: %s -> %s", Src.string().c_str(), Dst.string().c_str());
				}
			}
			else if (ActiveViewportClient)
			{
				ActiveViewportClient->HandleFileDropOnViewport(DraggingFilePath); // -> 이것도 수정
			}
		};
}

void FEditorUI::AttachToRenderer()
{
	if (!Core || !GRenderer)
	{
		return;
	}

	bViewportClientActive = true;
	CurrentRenderer = GRenderer;

	const HWND Hwnd = GRenderer->GetHwnd();
	ID3D11Device* Device = GRenderer->GetDevice();
	ID3D11DeviceContext* DeviceContext = GRenderer->GetDeviceContext();

	ContentBrowser.SetFolderIcon(CurrentRenderer->GetFolderIconSRV());
	ContentBrowser.SetFileIcon(CurrentRenderer->GetFileIconSRV());

	std::filesystem::path FontPath = FPaths::ProjectRoot() / "Content" / "Fonts" / "NotoSansKR-Bold.ttf";
	std::wstring FontPathWString = FontPath.wstring();
	GRenderer->SetGUICallbacks(
		[Hwnd, Device, DeviceContext, FontPathWString, FontPath]()
		{
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& IO = ImGui::GetIO();
			IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
			//IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; --------------------------> 만악의 근원
			IO.IniFilename = "imgui_editor.ini";

			ImFontConfig FontConfig;
			FontConfig.OversampleH = 1;
			FontConfig.OversampleV = 1;
			FontConfig.PixelSnapH = true;

			ImFont* Font = nullptr;
			FILE* FileHandle = nullptr;
			_wfopen_s(&FileHandle, FontPath.c_str(), L"rb");
			if (FileHandle)
			{
				fseek(FileHandle, 0, SEEK_END);
				const size_t Size = ftell(FileHandle);
				fseek(FileHandle, 0, SEEK_SET);

				void* FontData = IM_ALLOC(Size);
				fread(FontData, 1, Size, FileHandle);
				fclose(FileHandle);

				Font = IO.Fonts->AddFontFromMemoryTTF(FontData, static_cast<int>(Size), 16.0f, &FontConfig, IO.Fonts->GetGlyphRangesKorean());
				ImFontConfig MergeConfig;
				MergeConfig.MergeMode = true;
				IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 16.0f, &MergeConfig, IO.Fonts->GetGlyphRangesChineseFull());

			}

			if (!Font)
			{
				MessageBoxW(nullptr, FontPathWString.c_str(), L"Failed to load font", MB_OK);
				IO.Fonts->AddFontDefault();
			}

			ImGui::StyleColorsDark();

			ImGuiStyle& Style = ImGui::GetStyle();
			Style.WindowPadding = ImVec2(0, 0);
			Style.DisplayWindowPadding = ImVec2(0, 0);
			Style.DisplaySafeAreaPadding = ImVec2(0, 0);

			if (IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				Style.WindowRounding = 0.0f;
				Style.Colors[ImGuiCol_WindowBg].w = 1.0f;
			}

			ImGui_ImplWin32_Init(Hwnd);
			ImGui_ImplDX11_Init(Device, DeviceContext);
		},
		[]()
		{
			ImGui_ImplDX11_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
		},
		[]()
		{
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
		},
		[]()
		{
			ImGui::Render();
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		},
		[]()
		{
			ImGuiIO& IO = ImGui::GetIO();
			if (IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}
		});

	GRenderer->SetGUIUpdateCallback([this]() { Render(); });
	LoadEditorSettings();
}

void FEditorUI::DetachFromRenderer()
{
	bViewportClientActive = false;
	CurrentRenderer = nullptr;

	if (GRenderer)
	{
		GRenderer->ClearLevelRenderTarget();
		GRenderer->ClearViewportCallbacks();
	}
}

void FEditorUI::SetupWindow(FWindow* InWindow)
{
	MainWindow = InWindow;
	if (bWindowSetup || MainWindow == nullptr)
	{
		return;
	}
	bWindowSetup = true;

	MainWindow->AddMessageFilter([this](HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam) -> bool
	{
		if (!bViewportClientActive)
		{
			return false;
		}

		//ImGui에게 먼저 메시지를 던져보고, ImGui가 소유하면 넘겨줍니다.
		const bool bHandledByImGui = ImGui_ImplWin32_WndProcHandler(Hwnd, Msg, WParam, LParam) != 0;

		ImGuiIO& IO = ImGui::GetIO();

		// 키보드 입력 처리
		if (Msg == WM_KEYDOWN || Msg == WM_KEYUP || Msg == WM_SYSKEYDOWN || Msg == WM_SYSKEYUP)
		{
			// 사용자가 텍스트 창에 글씨를 쓰고 있거나, ImGui 창이 포커스를 먹고 있다면 엔진으로 안 넘김
			if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
				return false;

			//그 외의 경우 (뷰포트 조작 중) 에는 엔진(FCore -> InputManager)이 처리하도록 false 반환
			if (IO.WantCaptureKeyboard || IO.WantTextInput)
			{
				return true; // ImGui
			}
			
			return false;// 뷰포트 조작 중이 아닐 때는 기본적으로 엔진도 입력을 받도록 허용
		}

		// 마우스 입력 처리
		if (Msg == WM_MOUSEMOVE || Msg == WM_LBUTTONDOWN || Msg == WM_LBUTTONDBLCLK || Msg == WM_LBUTTONUP ||
			Msg == WM_RBUTTONDOWN || Msg == WM_RBUTTONUP || Msg == WM_MBUTTONDOWN || Msg == WM_MBUTTONUP ||
			Msg == WM_MOUSEWHEEL || Msg == WM_MOUSEHWHEEL)
		{
			// 마우스가 ImGui UI(버튼, 패널 등) 위에 올려져 있으면 엔진으로 클릭 안 넘김
			if (IO.WantCaptureMouse)
			{
				// 단, 마우스 우클릭으로 카메라 회전 중일 때는 밖으로 나가도 계속 드래그를 유지해야 하므로 예외 처리
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Left))
				{
					return false; // 엔진으로 넘겨서 드래그 계속 처리!
				}
				return true; // ImGui가 소유
			}

			// 뷰포트 위에서 마우스를 굴리거나 클릭할 때는 무조건 엔진으로 넘깁니다
			return false;
		}

		// 문자 입력 등 기타 메시지
		const bool bIsImeMessage = Msg == WM_IME_STARTCOMPOSITION || Msg == WM_IME_COMPOSITION || Msg == WM_IME_ENDCOMPOSITION || Msg == WM_IME_NOTIFY || Msg == WM_IME_SETCONTEXT || Msg == WM_IME_CHAR;
		const bool bIsCharMessage = Msg == WM_CHAR || Msg == WM_SYSCHAR || Msg == WM_UNICHAR;

		if (bIsImeMessage || bIsCharMessage)
		{
			if (!IO.WantTextInput) return true; // 텍스트 입력 중 아니면 무시
		}

		return bHandledByImGui;
	});
}

void FEditorUI::BuildDefaultLayout(uint32 DockID)
{
	ImGui::DockBuilderRemoveNode(DockID);
	ImGui::DockBuilderAddNode(DockID, ImGuiDockNodeFlags_DockSpace);

	ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	ImGui::DockBuilderSetNodeSize(DockID, MainViewport->WorkSize);

	ImGuiID DockBottom = 0;
	ImGuiID DockUpper = 0;
	ImGui::DockBuilderSplitNode(DockID, ImGuiDir_Down, 0.25f, &DockBottom, &DockUpper);

	ImGuiID DockLeft = 0;
	ImGuiID DockCenter = 0;
	ImGui::DockBuilderSplitNode(DockUpper, ImGuiDir_Left, 0.20f, &DockLeft, &DockCenter);

	ImGuiID DockRight = 0;
	ImGui::DockBuilderSplitNode(DockCenter, ImGuiDir_Right, 0.25f, &DockRight, &DockCenter);

	ImGuiID DockRightTop = 0;
	ImGuiID DockRightBottom = 0;
	ImGui::DockBuilderSplitNode(DockRight, ImGuiDir_Up, 0.50f, &DockRightTop, &DockRightBottom);
	ImGui::DockBuilderDockWindow("Viewport", DockCenter);
	ImGui::DockBuilderDockWindow("Properties", DockRightTop);
	ImGui::DockBuilderDockWindow("Control Panel", DockRightBottom);
	ImGui::DockBuilderDockWindow("Console", DockBottom);
	ImGui::DockBuilderFinish(DockID);
}

void FEditorUI::LoadEditorSettings()
{
	if (!WindowManager || !MainWindow)
	{
		return;
	}

	const FRect RootRect(
		0.0f,
		0.0f,
		static_cast<float>(MainWindow->GetWidth()),
		static_cast<float>(MainWindow->GetHeight()));
	WindowManager->LoadLayoutFromIni(GetEditorIniPathW(), RootRect);
}

void FEditorUI::SaveEditorSettings()
{
	if (!WindowManager)
	{
		return;
	}

	WindowManager->SaveLayoutToIni(GetEditorIniPathW());
}

std::wstring FEditorUI::GetEditorIniPathW() const
{
	return (FPaths::ProjectRoot() / "editor.ini").wstring();
}

void FEditorUI::Render()
{
	static bool bOpenAboutPopup = false;

	if (!bViewportClientActive)
	{
		return;
	}

	ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(MainViewport->WorkPos);
	ImGui::SetNextWindowSize(MainViewport->WorkSize);
	ImGui::SetNextWindowViewport(MainViewport->ID);

	ImGuiWindowFlags HostFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("##DockSpaceHost", nullptr, HostFlags);
	ImGui::PopStyleVar(3);

	ImGuiID DockID = ImGui::GetID("MainDockSpace");
	if (!bLayoutInitialized)
	{
		bLayoutInitialized = true;

		ImGuiDockNode* Node = ImGui::DockBuilderGetNode(DockID);
		if (!Node || Node->IsEmpty())
		{
			BuildDefaultLayout(DockID);
		}
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::DockSpace(DockID, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::PopStyleVar();

	CachedCentralDockSpaceRect = FRect();
	if (ImGuiDockNode* RootNode = ImGui::DockBuilderGetNode(DockID))
	{
		if (ImGuiDockNode* CentralNode = FindCentralDockNode(RootNode))
		{
			CachedCentralDockSpaceRect = FRect(
				CentralNode->Pos.x,
				CentralNode->Pos.y,
				CentralNode->Size.x,
				CentralNode->Size.y);
		}
	}

	ImGui::End();

	if (Core)
	{
		AActor* Selected = Core->GetSelectedActor();
		if (Selected != CachedSelectedActor)
		{
			SyncSelectedActorProperty();
		}
	}

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
#if IS_OBJ_VIEWER //뷰어에서는 level 개념이 없고 오브젝트를 새로 load할 수만 있습니다
			if (ImGui::MenuItem("New Obj"))
			{
				if (FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(GEngine))
				{
					EditorEngine->OpenNewObj();
				}
			}
#else
			if (ImGui::MenuItem("New Level"))
			{
				if (Core)
				{
					ULevel* ActiveLevel = ActiveViewportClient ? ActiveViewportClient->ResolveLevel(Core) : Core->GetLevel();
					if (ActiveLevel)
					{
						Core->SetSelectedActor(nullptr);
						ActiveLevel->ClearActors();

						if (ActiveViewportClient && ActiveViewportClient->GetViewportType() == EEditorViewportType::Perspective)
						{
							ActiveViewportClient->GetCamera()->SetPosition({ -5.0f, 0.0f, 2.0f });
							ActiveViewportClient->GetCamera()->SetRotation(0.0f, 0.0f);
						}

						SavePerspectiveCameraInitialState();

						SyncSelectedActorProperty();
						UE_LOG("New Level created");
					}
				}
			}

			if (ImGui::MenuItem("Open Level"))
			{
				ULevel* ActiveLevel = (Core && ActiveViewportClient) ? ActiveViewportClient->ResolveLevel(Core) : (Core ? Core->GetActiveLevel() : nullptr);
				if (Core && ActiveLevel && GRenderer)
				{
					const FString Path = GetFilePathUsingDialog(EFileDialogType::Open);
					if (!Path.empty())
					{
						Core->SetSelectedActor(nullptr);
						ActiveLevel->ClearActors();

						if (FSceneSerializer::Load(ActiveLevel, Path, GRenderer->GetDevice(), GetPerspectiveCamera()))
						{
							SavePerspectiveCameraInitialState();
							SyncSelectedActorProperty();
							UE_LOG("Level loaded: %s", Path.c_str());
						}
						else
						{
							MessageBoxW(nullptr, L"Level information is invalid.", L"Error", MB_OK | MB_ICONWARNING);
						}
					}
				}
			}

			if (ImGui::MenuItem("Save Level As..."))
			{
				ULevel* ActiveLevel = (Core && ActiveViewportClient) ? ActiveViewportClient->ResolveLevel(Core) : (Core ? Core->GetActiveLevel() : nullptr);
				if (Core && ActiveLevel)
				{
					const FString Path = GetFilePathUsingDialog(EFileDialogType::Save);
					if (!Path.empty())
					{
						FSceneSerializer::Save(ActiveLevel, Path, GetPerspectiveCamera());
					}
				}
			}
#endif
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			const bool bHasActiveViewport = ActiveViewportClient != nullptr; // -> 수정
			if (!bHasActiveViewport)
			{
				ImGui::BeginDisabled();
			}

			if (ActiveViewportClient) // -> 수정
			{
				FShowFlags& ShowFlags = ActiveViewportClient->GetShowFlags(); // -> 수정
				auto ShowFlagCheckbox = [&ShowFlags](const char* Label, EEngineShowFlags Flag)
					{
						bool bValue = ShowFlags.HasFlag(Flag);
						if (ImGui::Checkbox(Label, &bValue))
						{
							ShowFlags.SetFlag(Flag, bValue);
						}
					};

				ImGui::SeparatorText(ActiveViewportClient->GetViewportLabel()); // -> 수정

				int RenderMode = static_cast<int>(ActiveViewportClient->GetRenderMode()); // -> 수정
#if IS_OBJ_VIEWER
				const char* RenderModes = "Lighting\0No Lighting\0Wireframe\0Solid Wireframe\0UV\0Normals\0";
#else
				const char* RenderModes = "Lighting\0No Lighting\0Wireframe\0";
#endif
				if (ImGui::Combo("Render Mode", &RenderMode, RenderModes))
				{
					ActiveViewportClient->SetRenderMode(static_cast<ERenderMode>(RenderMode)); // -> 수정
				}
				ShowFlagCheckbox("Primitives", EEngineShowFlags::SF_Primitives);
				ShowFlagCheckbox("UUID", EEngineShowFlags::SF_UUID);
				ShowFlagCheckbox("Debug Draw", EEngineShowFlags::SF_DebugDraw);
				ShowFlagCheckbox("Grid", EEngineShowFlags::SF_Grid);
#if !IS_OBJ_VIEWER //뷰어에서는 월드축을 렌더링하지 않으며 끄고 키는 것도 숨깁니다.
				ShowFlagCheckbox("World Axis", EEngineShowFlags::SF_WorldAxis);
#endif
				ShowFlagCheckbox("Collision", EEngineShowFlags::SF_Collision);

				float GridSize = ActiveViewportClient->GetGridSize(); // -> 수정
				if (ImGui::SliderFloat("Grid Size", &GridSize, 1.0f, 100.0f, "%.1f"))
				{
					ActiveViewportClient->SetGridSize(GridSize);
				}

				float Thickness = ActiveViewportClient->GetLineThickness(); // -> 수정
				if (ImGui::SliderFloat("Line Thickness", &Thickness, 0.1f, 5.0f, "%.2f"))
				{
					ActiveViewportClient->SetLineThickness(Thickness); // -> 수정
				}
			}

			if (!bHasActiveViewport)
			{
				ImGui::EndDisabled();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("About"))
			{
				bOpenAboutPopup = true;
			}
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	if (bOpenAboutPopup)
	{
		ImGui::OpenPopup("AboutPopup");
		ImGui::SetNextWindowSize(ImVec2(420, 320), ImGuiCond_Always);
		bOpenAboutPopup = false;
	}

	if (ImGui::BeginPopupModal("AboutPopup", nullptr, ImGuiWindowFlags_NoTitleBar))
	{
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		ImVec2 WinPos = ImGui::GetWindowPos();
		ImVec2 WinSize = ImGui::GetWindowSize();
		DrawList->AddRectFilled(WinPos, ImVec2(WinPos.x + WinSize.x, WinPos.y + 60), IM_COL32(30, 30, 60, 255));

		ImGui::SetCursorPosY(12);
		ImGui::SetCursorPosX((WinSize.x - ImGui::CalcTextSize("Dino Engine").x) * 0.5f);
		ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Dino Engine");

		ImGui::SetCursorPosY(35);
		ImGui::SetCursorPosX((WinSize.x - ImGui::CalcTextSize("v1.0.0").x) * 0.5f);
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "v1.0.0");

		ImGui::SetCursorPosY(70);
		ImGui::SetCursorPosX(20);
		ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "Contributors");
		ImGui::SameLine();
		ImGui::SetCursorPosX(20);
		ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.9f, 0.7f, 0.3f, 0.5f));
		ImGui::Separator();
		ImGui::PopStyleColor();

		ImGui::Spacing();
		const char* Contributors[] = { "Kim Jisu", "Kim Taehyun", "Park Seyoung", "Cho Sanghyun" };
		for (const char* Name : Contributors)
		{
			ImGui::SetCursorPosX(20);
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.6f, 1.0f), "*");
			ImGui::SameLine();
			ImGui::Text("%s", Name);
		}

		ImGui::Spacing();
		ImGui::SetCursorPosX(20);
		ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1, 1, 1, 0.1f));
		ImGui::Separator();
		ImGui::PopStyleColor();
		ImGui::Spacing();

		ImGui::SetCursorPosX(20);
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Copyright (c) 2026  |  MIT License");

		ImGui::Spacing();
		ImGui::Spacing();

		const float ButtonWidth = 100.0f;
		ImGui::SetCursorPosX((WinSize.x - ButtonWidth) * 0.5f);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.7f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
		if (ImGui::Button("Close", ImVec2(ButtonWidth, 28)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);
		ImGui::Spacing();
		ImGui::EndPopup();
	}

#if IS_OBJ_VIEWER //뷰어에서는 패널들 다 죽입니다
	if (Core)
	{
		ULevel* Level = Core->GetLevel();
		if (Level)
		{
			AStaticMeshActor* MeshActor = FindObjViewerMeshActor(Level);

			if (MeshActor)
			{
				UStaticMeshComponent* MeshComp = MeshActor->GetStaticMeshComponent();
				if (MeshComp)
				{
					FStaticMeshRenderData* MeshData = MeshComp->GetStaticMesh();
					if (MeshData)
					{
						FMeshData* CPUData = MeshData->GetMeshData();
						if (CPUData)
						{
							ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
							ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);
							
							ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
							if (ImGui::Begin("OBJ Information", nullptr, WindowFlags))
							{
								// 텍스트 색상 정의
								ImVec4 HeaderColor = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
								ImVec4 LabelColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
								ImVec4 ValueColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
								static FString DraftAxisAssetPath;
								static FObjImporter::FImportAxisMapping DraftImportAxisMapping = FObjImporter::MakeDefaultImportAxisMapping();
								static FObjImporter::FImportAxisMapping LastAppliedImportAxisMapping = FObjImporter::MakeDefaultImportAxisMapping();
								const FObjImporter::FImportAxisMapping AppliedImportAxisMapping = FObjImporter::GetImportAxisMapping();
								const FString MeshAssetPath = MeshData->GetAssetPath();

								const bool bAppliedAxisMappingChanged =
									LastAppliedImportAxisMapping.EngineX != AppliedImportAxisMapping.EngineX ||
									LastAppliedImportAxisMapping.EngineY != AppliedImportAxisMapping.EngineY ||
									LastAppliedImportAxisMapping.EngineZ != AppliedImportAxisMapping.EngineZ;
								if (DraftAxisAssetPath != MeshAssetPath || bAppliedAxisMappingChanged)
								{
									DraftAxisAssetPath = MeshAssetPath;
									DraftImportAxisMapping = AppliedImportAxisMapping;
									LastAppliedImportAxisMapping = AppliedImportAxisMapping;
								}

								// 애셋 정보 섹션
								if (ImGui::CollapsingHeader("Asset Information", ImGuiTreeNodeFlags_DefaultOpen))
								{
									ImGui::Indent();
									ImGui::TextColored(LabelColor, "Path:");
									ImGui::SameLine();
									ImGui::TextWrapped("%s", MeshData->GetAssetPath().c_str());
									ImGui::Unindent();
								}

								// 메시 통계 섹션
								if (ImGui::CollapsingHeader("Mesh Statistics", ImGuiTreeNodeFlags_DefaultOpen))
								{
									ImGui::Indent();
									auto StatRow = [&](const char* Label, int Value) {
										ImGui::TextColored(LabelColor, "%s:", Label);
										ImGui::SameLine(120);
										ImGui::TextColored(ValueColor, "%d", Value);
									};

									StatRow("Vertices", (int)CPUData->Vertices.size());
									StatRow("Indices", (int)CPUData->Indices.size());
									StatRow("Triangles", (int)CPUData->Indices.size() / 3);
									StatRow("Sections", (int)CPUData->Sections.size());
									ImGui::Unindent();
								}

								// 바운딩 박스 섹션
								if (ImGui::CollapsingHeader("Bounding Box", ImGuiTreeNodeFlags_DefaultOpen))
								{
									ImGui::Indent();
									FVector Min = CPUData->GetMinCoord();
									FVector Max = CPUData->GetMaxCoord();
									FVector Center = CPUData->GetCenterCoord();
									
									auto VectorRow = [&](const char* Label, const FVector& V) {
										ImGui::TextColored(LabelColor, "%s:", Label);
										ImGui::SameLine(80);
										ImGui::TextColored(ValueColor, "(%.2f, %.2f, %.2f)", V.X, V.Y, V.Z);
									};

									VectorRow("Min", Min);
									VectorRow("Max", Max);
									VectorRow("Center", Center);
									
									ImGui::TextColored(LabelColor, "Size:");
									ImGui::SameLine(80);
									ImGui::TextColored(ValueColor, "(%.2f, %.2f, %.2f)", Max.X - Min.X, Max.Y - Min.Y, Max.Z - Min.Z);
									
									ImGui::TextColored(LabelColor, "Radius:");
									ImGui::SameLine(80);
									ImGui::TextColored(ValueColor, "%.2f", CPUData->GetLocalBoundRadius());
									ImGui::Unindent();
								}

								if (ImGui::CollapsingHeader("OBJ Axis Mapping", ImGuiTreeNodeFlags_DefaultOpen))
								{
									ImGui::Indent();
									ImGui::TextColored(HeaderColor, "Applied");
									ImGui::TextWrapped("%s", BuildImportAxisSummary(AppliedImportAxisMapping).c_str());
									ImGui::SeparatorText("Draft");
									const char* RowLabels[] = { "Engine X", "Engine Y", "Engine Z" };
									const char* AxisBaseLabels[] = { "X", "Y", "Z" };
									for (int RowIndex = 0; RowIndex < 3; ++RowIndex)
									{
										FObjImporter::EAxisDirection RowDirection = GetMappingAxisDirection(DraftImportAxisMapping, RowIndex);
										ImGui::PushID(RowIndex);
										ImGui::TextColored(LabelColor, "%s:", RowLabels[RowIndex]);
										for (int AxisBaseIndex = 0; AxisBaseIndex < 3; ++AxisBaseIndex)
										{
											ImGui::SameLine(AxisBaseIndex == 0 ? 100.0f : 0.0f);
											if (AxisBaseIndex > 0)
											{
												ImGui::SameLine();
											}

											const bool bSelectedBase = GetAxisBaseIndex(RowDirection) == AxisBaseIndex;
											if (bSelectedBase)
											{
												ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.45f, 0.85f, 1.0f));
												ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.55f, 0.95f, 1.0f));
												ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.40f, 0.80f, 1.0f));
											}
											if (ImGui::Button(AxisBaseLabels[AxisBaseIndex], ImVec2(28.0f, 0.0f)))
											{
												SetMappingAxisBase(DraftImportAxisMapping, RowIndex, AxisBaseIndex);
												RowDirection = GetMappingAxisDirection(DraftImportAxisMapping, RowIndex);
											}
											if (bSelectedBase)
											{
												ImGui::PopStyleColor(3);
											}
										}

										ImGui::SameLine();
										const bool bNegative = IsAxisNegative(RowDirection);
										if (bNegative)
										{
											ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.3f, 0.3f, 1.0f));
											ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.4f, 0.4f, 1.0f));
											ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.25f, 0.25f, 1.0f));
										}
										else
										{
											ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.65f, 0.35f, 1.0f));
											ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.75f, 0.45f, 1.0f));
											ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.55f, 0.3f, 1.0f));
										}
										if (ImGui::Button(bNegative ? "-" : "+", ImVec2(28.0f, 0.0f)))
										{
											ToggleMappingAxisSign(DraftImportAxisMapping, RowIndex);
										}
										ImGui::PopStyleColor(3);
										ImGui::SameLine();
										ImGui::TextColored(ValueColor, "%s", GetAxisDirectionLabel(GetMappingAxisDirection(DraftImportAxisMapping, RowIndex)));
										ImGui::PopID();
									}

									const bool bHasPendingAxisChanges =
										DraftImportAxisMapping.EngineX != AppliedImportAxisMapping.EngineX ||
										DraftImportAxisMapping.EngineY != AppliedImportAxisMapping.EngineY ||
										DraftImportAxisMapping.EngineZ != AppliedImportAxisMapping.EngineZ;

									if (bHasPendingAxisChanges)
									{
										ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.35f, 1.0f), "Pending changes. Click Apply to reload.");
									}
									else
									{
										ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "Axis mapping is up to date.");
									}

									if (!bHasPendingAxisChanges)
									{
										ImGui::BeginDisabled();
									}
									if (ImGui::Button("Apply"))
									{
										if (ReloadObjViewerMesh(Core, DraftImportAxisMapping))
										{
											DraftAxisAssetPath = MeshAssetPath;
											DraftImportAxisMapping = FObjImporter::GetImportAxisMapping();
											LastAppliedImportAxisMapping = DraftImportAxisMapping;
										}
									}
									if (!bHasPendingAxisChanges)
									{
										ImGui::EndDisabled();
									}

									ImGui::Unindent();
								}

							}
							ImGui::End();
						}
					}
				}
			}
		}
	}
#else
	Property.Render(Core);
	Console.Render();
	Stat.Render();
	// Viewport draw data keeps the SRV pointer until ImGui render submission,
	// so rendering the same legacy viewport twice in one frame can invalidate
	// the first draw command if the offscreen target is recreated in-between.
	//ViewportLegacy.Render(nullptr);
	dynamic_cast<FEditorEngine*>(GEngine)->GetWindowManager().DrawWindows();

	Outliner.Render(Core);
	ControlPanel.Render(Core, ActiveViewportClient);
	ContentBrowser.Render();
#endif
}

void FEditorUI::SyncSelectedActorProperty()
{
	if (!Core)
	{
		return;
	}

	AActor* Selected = Core->GetSelectedActor();
	if (Selected)
	{
		if (USceneComponent* Root = Selected->GetRootComponent())
		{
			const FTransform Transform = Root->GetRelativeTransform();
			Property.SetTarget(
				Transform.GetLocation(),
				Transform.Rotator().Euler(),
				Transform.GetScale3D(),
				Selected->GetName().c_str());
		}
	}
	else
	{
		Property.SetTarget({ 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, "None");
	}

	CachedSelectedActor = Selected;
}

FEditorViewportClient* FEditorUI::FindPerspectiveViewportClient() const
{
	return WindowManager ? WindowManager->FindPerspectiveViewportClient() : nullptr;
}

FCamera* FEditorUI::GetPerspectiveCamera() const
{
	FEditorViewportClient* PerspectiveViewportClient = FindPerspectiveViewportClient();
	return PerspectiveViewportClient ? PerspectiveViewportClient->GetCamera() : nullptr;
}

FRect FEditorUI::GetCentralDockSpaceRect() const
{
	return CachedCentralDockSpaceRect;
}

void FEditorUI::SavePerspectiveCameraInitialState() const
{
	if (FEditorViewportClient* PerspectiveViewportClient = FindPerspectiveViewportClient())
	{
		PerspectiveViewportClient->SaveInitialCameraState();
	}
}
