#include "ObjViewerPanel.h"

#include "Actor/Actor.h"
#include "Actor/StaticMeshActor.h"
#include "Asset/AssetCooker.h"
#include "Asset/AssetManager.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Core/Core.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include "Mesh/ObjManager.h"
#include "Mesh/StaticMeshRenderData.h"
#include "Object/StaticMesh.h"
#include "Renderer/Renderer.h"
#include "UI/EditorUI.h"
#include "UI/EditorViewportClient.h"
#include "World/Level.h"
#include "imgui.h"

#include <cmath>
#include <filesystem>

namespace
{
	constexpr float ObjViewerImportScaleTolerance = 1.0e-4f;

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
			+ "  Z=" + GetAxisDirectionLabel(ImportAxisMapping.EngineZ)
			+ "  Scale=" + std::to_string(ImportAxisMapping.ImportScale);
	}

	AStaticMeshActor* FindObjViewerMeshActor(ULevel* Level)
	{
		if (!Level)
		{
			return nullptr;
		}

		for (AActor* Actor : Level->GetActors())
		{
			if (Actor && Actor->IsA(AStaticMeshActor::StaticClass()))
			{
				return static_cast<AStaticMeshActor*>(Actor);
			}
		}

		return nullptr;
	}

	void SnapObjViewerActorBottomTo(AActor* Actor, FEditorViewportClient* ViewportClient, float TargetBottomZ)
	{
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
		FVector ActualLocation = Transform.GetLocation();
		ActualLocation.Z += TargetBottomZ - ViewportClient->GetObjViewerBottomZ(Actor);
		Transform.SetLocation(ActualLocation);
		Root->SetRelativeTransform(Transform);
		ViewportClient->RefreshObjViewerCameraPivot(Actor);
	}
}

FVector FObjViewerPanel::GetDisplayedLocation(AActor* Actor, FEditorViewportClient* ViewportClient) const
{
	if (!Actor || !ViewportClient)
	{
		return FVector::ZeroVector;
	}

	if (USceneComponent* Root = Actor->GetRootComponent())
	{
		FVector DisplayLocation = Root->GetRelativeTransform().GetLocation();
		DisplayLocation.Z = ViewportClient->GetObjViewerBottomZ(Actor);
		return DisplayLocation;
	}

	return FVector::ZeroVector;
}

void FObjViewerPanel::ApplyDisplayedLocation(AActor* Actor, FEditorViewportClient* ViewportClient, const FVector& DisplayLocation) const
{
	if (!Actor)
	{
		return;
	}

	if (USceneComponent* Root = Actor->GetRootComponent())
	{
		FTransform Transform = Root->GetRelativeTransform();
		Transform.SetLocation({ DisplayLocation.X, DisplayLocation.Y, Transform.GetLocation().Z });
		Root->SetRelativeTransform(Transform);

		if (ViewportClient)
		{
			SnapObjViewerActorBottomTo(Actor, ViewportClient, DisplayLocation.Z);
		}
	}
}

void FObjViewerPanel::Render(FCore* Core, FEditorViewportClient* ActiveViewportClient, FEditorUI& EditorUI)
{
	if (!Core || !ActiveViewportClient || !GRenderer)
	{
		return;
	}

	ULevel* Level = ActiveViewportClient->ResolveLevel(Core);
	if (!Level)
	{
		return;
	}

	AStaticMeshActor* MeshActor = FindObjViewerMeshActor(Level);
	if (!MeshActor)
	{
		return;
	}

	UStaticMeshComponent* MeshComp = MeshActor->GetStaticMeshComponent();
	if (!MeshComp)
	{
		return;
	}

	UStaticMesh* StaticMesh = MeshComp->GetStaticMesh();
	if (!StaticMesh)
	{
		return;
	}

	FStaticMeshRenderData* MeshData = StaticMesh->GetStaticMeshAsset();
	if (!MeshData)
	{
		return;
	}

	FMeshData* CPUData = MeshData->GetMeshData();
	if (!CPUData)
	{
		return;
	}

	ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
	if (!ImGui::Begin("OBJ Information", nullptr, WindowFlags))
	{
		ImGui::End();
		return;
	}

	ImVec4 HeaderColor = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
	ImVec4 LabelColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
	ImVec4 ValueColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	const FObjImporter::FImportAxisMapping AppliedImportAxisMapping = FObjImporter::GetImportAxisMapping();
	const FString MeshAssetPath = MeshData->GetAssetPath();

	const bool bAppliedAxisMappingChanged =
		LastAppliedImportAxisMapping.EngineX != AppliedImportAxisMapping.EngineX ||
		LastAppliedImportAxisMapping.EngineY != AppliedImportAxisMapping.EngineY ||
		LastAppliedImportAxisMapping.EngineZ != AppliedImportAxisMapping.EngineZ ||
		!(std::fabs(LastAppliedImportAxisMapping.ImportScale - AppliedImportAxisMapping.ImportScale) <= ObjViewerImportScaleTolerance);
	if (DraftAxisAssetPath != MeshAssetPath || bAppliedAxisMappingChanged)
	{
		DraftAxisAssetPath = MeshAssetPath;
		DraftImportAxisMapping = AppliedImportAxisMapping;
		LastAppliedImportAxisMapping = AppliedImportAxisMapping;
	}

	if (ImGui::CollapsingHeader("Asset Information", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent();
		ImGui::TextColored(LabelColor, "Path:");
		ImGui::SameLine();
		ImGui::TextWrapped("%s", MeshData->GetAssetPath().c_str());
		ImGui::Unindent();
	}

	if (ImGui::CollapsingHeader("Mesh Statistics", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent();
		auto StatRow = [&](const char* Label, int Value)
			{
				ImGui::TextColored(LabelColor, "%s:", Label);
				ImGui::SameLine(120);
				ImGui::TextColored(ValueColor, "%d", Value);
			};

		StatRow("Vertices", static_cast<int>(CPUData->Vertices.size()));
		StatRow("Indices", static_cast<int>(CPUData->Indices.size()));
		StatRow("Triangles", static_cast<int>(CPUData->Indices.size()) / 3);
		StatRow("Sections", static_cast<int>(CPUData->Sections.size()));
		ImGui::Unindent();
	}

	if (ImGui::CollapsingHeader("Bounding Box", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent();
		FVector Min = CPUData->GetMinCoord();
		FVector Max = CPUData->GetMaxCoord();
		FVector Center = CPUData->GetCenterCoord();

		auto VectorRow = [&](const char* Label, const FVector& V)
			{
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

		ImGui::TextColored(LabelColor, "Import Scale:");
		ImGui::SameLine(100.0f);
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat("##ImportScale", &DraftImportAxisMapping.ImportScale, 0.1f, 0.1f, 10.0f);

		const bool bHasValidDraftScale = DraftImportAxisMapping.ImportScale > ObjViewerImportScaleTolerance;
		const bool bHasPendingAxisChanges =
			DraftImportAxisMapping.EngineX != AppliedImportAxisMapping.EngineX ||
			DraftImportAxisMapping.EngineY != AppliedImportAxisMapping.EngineY ||
			DraftImportAxisMapping.EngineZ != AppliedImportAxisMapping.EngineZ ||
			!(std::fabs(DraftImportAxisMapping.ImportScale - AppliedImportAxisMapping.ImportScale) <= ObjViewerImportScaleTolerance);

		if (!bHasValidDraftScale)
		{
			ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "Import scale must be greater than 0.");
		}
		else if (bHasPendingAxisChanges)
		{
			ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.35f, 1.0f), "Pending changes. Click Apply to reload.");
		}
		else
		{
			ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "Axis mapping is up to date.");
		}

		if (!bHasPendingAxisChanges || !bHasValidDraftScale)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Apply"))
		{
			const FString AssetPath = MeshComp->GetStaticMeshAsset();
			const FTransform PreviousTransform = MeshActor->GetRootComponent()->GetRelativeTransform();
			const FVector PreviousDisplayedLocation = GetDisplayedLocation(MeshActor, ActiveViewportClient);
			const FString CookedPath = FAssetCooker::GetCookedPath(AssetPath);
			const bool bCookedExists = std::filesystem::exists(std::filesystem::path(FPaths::ToWide(CookedPath)));
			std::error_code Ec;
			if (bCookedExists)
			{
				std::filesystem::remove(std::filesystem::path(FPaths::ToWide(CookedPath)), Ec);
			}

			if (Ec)
			{
				UE_LOG("[ObjViewer] Reload failed: could not delete cooked asset '%s' (%s)", CookedPath.c_str(), Ec.message().c_str());
			}
			else if (FObjImporter::IsValidImportAxisMapping(DraftImportAxisMapping))
			{
				FObjImporter::SetImportAxisMapping(DraftImportAxisMapping);
				FAssetManager::Get().InvalidateStaticMesh(AssetPath);
				FObjManager::ClearAssetCache(AssetPath);
				MeshActor->LoadStaticMesh(GRenderer->GetDevice(), AssetPath);

				if (MeshComp->GetStaticMesh() && MeshComp->GetMeshData())
				{
					MeshActor->GetRootComponent()->SetRelativeTransform(PreviousTransform);
					SnapObjViewerActorBottomTo(MeshActor, ActiveViewportClient, PreviousDisplayedLocation.Z);
					Core->SetSelectedActor(MeshActor);
					ActiveViewportClient->FrameObjViewerCamera(MeshActor, true);
					EditorUI.SyncSelectedActorProperty();
					DraftAxisAssetPath = MeshAssetPath;
					DraftImportAxisMapping = FObjImporter::GetImportAxisMapping();
					LastAppliedImportAxisMapping = DraftImportAxisMapping;
				}
				else
				{
					UE_LOG("[ObjViewer] Reload failed: mesh load failed for '%s'", AssetPath.c_str());
				}
			}
			else
			{
				UE_LOG("[ObjViewer] Reload failed: invalid reload context");
			}
		}

		if (!bHasPendingAxisChanges || !bHasValidDraftScale)
		{
			ImGui::EndDisabled();
		}

		ImGui::Unindent();
	}

	ImGui::End();
}
