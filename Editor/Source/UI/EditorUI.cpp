#include "EditorUI.h"

#include "Actor/Actor.h"
#include "Actor/ObjActor.h"
#include "Camera/Camera.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Core/Core.h"
#include "Core/FEngine.h"
#include "Core/Paths.h"
#include "Core/ShowFlags.h"
#include "Debug/EngineLog.h"
#include "Object/Object.h"
#include "Platform/Windows/Window.h"
#include "Primitive/PrimitiveObj.h"
#include "Renderer/Renderer.h"
#include "Serializer/SceneSerializer.h"
#include "UI/EditorViewportClient.h"
#include "World/Level.h"

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

#if IS_OBJ_VIEWER
	AObjActor* FindObjViewerActor(FCore* Core)
	{
		if (!Core || !Core->GetLevel())
		{
			return nullptr;
		}

		if (AActor* SelectedActor = Core->GetSelectedActor())
		{
			if (SelectedActor->IsA<AObjActor>())
			{
				return static_cast<AObjActor*>(SelectedActor);
			}
		}

		for (AActor* Actor : Core->GetLevel()->GetActors())
		{
			if (Actor && Actor->IsA<AObjActor>())
			{
				return static_cast<AObjActor*>(Actor);
			}
		}

		return nullptr;
	}

	void ReloadObjViewerActor(FCore* Core)
	{
		if (!Core || !GRenderer)
		{
			return;
		}

		AObjActor* ObjActor = FindObjViewerActor(Core);
		if (!ObjActor)
		{
			UE_LOG("[ObjViewer] No OBJ actor to reload");
			return;
		}

		UPrimitiveComponent* PrimitiveComponent = ObjActor->GetComponentByClass<UPrimitiveComponent>();
		if (!PrimitiveComponent)
		{
			return;
		}

		const FString PrimitiveFileName = PrimitiveComponent->GetPrimitiveFileName();
		if (PrimitiveFileName.empty())
		{
			return;
		}

		ObjActor->LoadObj(GRenderer->GetDevice(), PrimitiveFileName);
		Core->SetSelectedActor(ObjActor);
		UE_LOG(
			"[ObjViewer] Reloaded OBJ with axis mode: %s",
			FPrimitiveObj::GetImportAxisMode() == FPrimitiveObj::EImportAxisMode::YUpToZUp ? "Y-Up" : "Z-Up");
	}
#endif
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

void FEditorUI::Initialize(FCore* InCore)
{
	Core = InCore;

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
				ActiveViewportClient->HandleFileDoubleClick(FilePath);
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
				ActiveViewportClient->HandleFileDropOnViewport(DraggingFilePath);
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
			IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
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

			const bool bIsMouseMessage =
				Msg == WM_MOUSEMOVE ||
				Msg == WM_LBUTTONDOWN ||
				Msg == WM_LBUTTONUP ||
				Msg == WM_RBUTTONDOWN ||
				Msg == WM_RBUTTONUP ||
				Msg == WM_MBUTTONDOWN ||
				Msg == WM_MBUTTONUP ||
				Msg == WM_MOUSEWHEEL ||
				Msg == WM_MOUSEHWHEEL;

			const bool bIsKeyboardMessage =
				Msg == WM_KEYDOWN ||
				Msg == WM_KEYUP ||
				Msg == WM_SYSKEYDOWN ||
				Msg == WM_SYSKEYUP;

			const bool bIsImeMessage =
				Msg == WM_IME_STARTCOMPOSITION ||
				Msg == WM_IME_COMPOSITION ||
				Msg == WM_IME_ENDCOMPOSITION ||
				Msg == WM_IME_NOTIFY ||
				Msg == WM_IME_SETCONTEXT ||
				Msg == WM_IME_CHAR;

			const bool bIsCharMessage =
				Msg == WM_CHAR ||
				Msg == WM_SYSCHAR ||
				Msg == WM_UNICHAR;

			if (bIsImeMessage || bIsCharMessage)
			{
				if (ImGui::GetCurrentContext())
				{
					const ImGuiIO& IO = ImGui::GetIO();
					if (!IO.WantTextInput)
					{
						return true;
					}
				}
				else
				{
					return true;
				}
			}

			bool bViewportInteractive = false;
			if (GEngine)
			{
				POINT ClientPoint = {};
				if (bIsMouseMessage)
				{
					ClientPoint.x = GET_X_LPARAM(LParam);
					ClientPoint.y = GET_Y_LPARAM(LParam);
				}
				else if (bIsKeyboardMessage)
				{
					POINT ScreenPoint = {};
					if (::GetCursorPos(&ScreenPoint))
					{
						ClientPoint = ScreenPoint;
						::ScreenToClient(Hwnd, &ClientPoint);
					}
				}

				if (bIsMouseMessage || bIsKeyboardMessage)
				{
					bViewportInteractive =
						GEngine->GetWindowManager().GetWindowAtPoint(
							FPoint(static_cast<float>(ClientPoint.x), static_cast<float>(ClientPoint.y))) != nullptr;
				}
			}

			const bool bHandledByImGui = ImGui_ImplWin32_WndProcHandler(Hwnd, Msg, WParam, LParam) != 0;
			if (bViewportInteractive && (bIsMouseMessage || bIsKeyboardMessage))
			{
				return false;
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
	ImGui::DockBuilderDockWindow("Stats", DockLeft);
	ImGui::DockBuilderDockWindow("Properties", DockRightTop);
	ImGui::DockBuilderDockWindow("Control Panel", DockRightBottom);
	ImGui::DockBuilderDockWindow("Console", DockBottom);
	ImGui::DockBuilderFinish(DockID);
}

void FEditorUI::LoadEditorSettings()
{
}

void FEditorUI::SaveEditorSettings()
{
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
	ImGui::End();

	if (Core)
	{
		AActor* Selected = Core->GetSelectedActor();
		if (Selected != CachedSelectedActor)
		{
			SyncSelectedActorProperty();
		}

		const FTimer& Timer = Core->GetTimer();
		Stat.SetFPS(Timer.GetDisplayFPS());
		Stat.SetFrameTimeMs(Timer.GetFrameTimeMs());
	}

	Stat.SetObjectCount(UObject::TotalAllocationCounts);
	Stat.SetHeapUsage(UObject::TotalAllocationBytes);

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
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

						if (FSceneSerializer::Load(ActiveLevel, Path, GRenderer->GetDevice()))
						{
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
						FSceneSerializer::Save(ActiveLevel, Path);
					}
				}
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			const bool bHasActiveViewport = ActiveViewportClient != nullptr;
			if (!bHasActiveViewport)
			{
				ImGui::BeginDisabled();
			}

			if (ActiveViewportClient)
			{
				FShowFlags& ShowFlags = ActiveViewportClient->GetShowFlags();
				auto ShowFlagCheckbox = [&ShowFlags](const char* Label, EEngineShowFlags Flag)
					{
						bool bValue = ShowFlags.HasFlag(Flag);
						if (ImGui::Checkbox(Label, &bValue))
						{
							ShowFlags.SetFlag(Flag, bValue);
						}
					};

				ImGui::SeparatorText(ActiveViewportClient->GetViewportLabel());

				int RenderMode = static_cast<int>(ActiveViewportClient->GetRenderMode());
				const char* RenderModes = "Lighting\0No Lighting\0Wireframe\0Solid Wireframe\0";
				if (ImGui::Combo("Render Mode", &RenderMode, RenderModes))
				{
					ActiveViewportClient->SetRenderMode(static_cast<ERenderMode>(RenderMode));
				}

				ShowFlagCheckbox("Primitives", EEngineShowFlags::SF_Primitives);
				ShowFlagCheckbox("UUID", EEngineShowFlags::SF_UUID);
				ShowFlagCheckbox("Debug Draw", EEngineShowFlags::SF_DebugDraw);
#if IS_OBJ_VIEWER
#else
				ShowFlagCheckbox("World Axis", EEngineShowFlags::SF_WorldAxis);
#endif
				ShowFlagCheckbox("Collision", EEngineShowFlags::SF_Collision);

				bool bShowGrid = ActiveViewportClient->IsGridVisible();
				if (ImGui::Checkbox("Show Grid", &bShowGrid))
				{
					ActiveViewportClient->SetGridVisible(bShowGrid);
				}

				float GridSize = ActiveViewportClient->GetGridSize();
				if (ImGui::SliderFloat("Grid Size", &GridSize, 1.0f, 100.0f, "%.1f"))
				{
					ActiveViewportClient->SetGridSize(GridSize);
				}

				float Thickness = ActiveViewportClient->GetLineThickness();
				if (ImGui::SliderFloat("Line Thickness", &Thickness, 0.1f, 5.0f, "%.2f"))
				{
					ActiveViewportClient->SetLineThickness(Thickness);
				}

#if IS_OBJ_VIEWER
				ImGui::SeparatorText("OBJ Axis");
				int AxisMode = (FPrimitiveObj::GetImportAxisMode() == FPrimitiveObj::EImportAxisMode::YUpToZUp) ? 1 : 0;
				if (ImGui::RadioButton("Z-Up", AxisMode == 0))
				{
					FPrimitiveObj::SetImportAxisMode(FPrimitiveObj::EImportAxisMode::ZUp);
					ReloadObjViewerActor(Core);
				}
				if (ImGui::RadioButton("Y-Up -> Z-Up", AxisMode == 1))
				{
					FPrimitiveObj::SetImportAxisMode(FPrimitiveObj::EImportAxisMode::YUpToZUp);
					ReloadObjViewerActor(Core);
				}
#endif
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

#if IS_OBJ_VIEWER
#else
	Property.Render(Core);
	Console.Render();
	Stat.Render();
	ViewportLegacy.Render(nullptr);
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
