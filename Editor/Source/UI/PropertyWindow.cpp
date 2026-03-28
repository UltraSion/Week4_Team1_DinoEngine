#include "PropertyWindow.h"
#include "Core/Core.h"
#include "Actor/Actor.h"
#include "Component/SubUVComponent.h"
#include "Component/TextComponent.h"
#include "Component/UUIDBillboardComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Actor/StaticMeshActor.h"
#include "Renderer/Renderer.h"
#include "Renderer/MaterialManager.h"
#include "Core/Paths.h"
#include <filesystem>
void FPropertyWindow::SetTarget(const FVector& Location, const FVector& Rotation,
	const FVector& Scale, const char* ActorName)
{
	EditLocation = Location;
	EditRotation = Rotation;
	EditScale = Scale;
	bModified = false;

	if (ActorName)
		snprintf(ActorNameBuf, sizeof(ActorNameBuf), "%s", ActorName);
	else
		snprintf(ActorNameBuf, sizeof(ActorNameBuf), "None");
}

void FPropertyWindow::DrawTransformSection()
{
	float Loc[3] = { EditLocation.X, EditLocation.Y, EditLocation.Z };
	float Rot[3] = { EditRotation.X, EditRotation.Y, EditRotation.Z };
	float Scl[3] = { EditScale.X,    EditScale.Y,    EditScale.Z };

	const float ResetBtnWidth = 14.0f;
	const float Spacing = ImGui::GetStyle().ItemInnerSpacing.x;
	const float DragUIWidth = 200.f;

	// Location
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
	if (ImGui::Button("##RL", ImVec2(ResetBtnWidth, 0)))
	{
		EditLocation = { 0.0f, 0.0f, 0.0f };
		bModified = true;
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Location");
	ImGui::PopStyleColor(3);

	ImGui::SameLine(0, Spacing);
	// ImGui::PushItemWidth(-(ResetBtnWidth));
	ImGui::PushItemWidth(DragUIWidth);
	if (ImGui::DragFloat3("Location", Loc, 0.1f, 0.0f, 0.0f, "%.2f"))
	{
		EditLocation = { Loc[0], Loc[1], Loc[2] };
		bModified = true;
	}
	ImGui::PopItemWidth();

	// Rotation
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.4f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
	if (ImGui::Button("##RR", ImVec2(ResetBtnWidth, 0)))
	{
		EditRotation = { 0.0f, 0.0f, 0.0f };
		bModified = true;
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Rotation");
	ImGui::PopStyleColor(3);

	ImGui::SameLine(0, Spacing);
	// ImGui::PushItemWidth(-(ResetBtnWidth));
	ImGui::PushItemWidth(DragUIWidth);
	if (ImGui::DragFloat3("Rotation", Rot, 0.5f, -360.0f, 360.0f, "%.1f"))
	{
		EditRotation = { Rot[0], Rot[1], Rot[2] };
		bModified = true;
	}
	ImGui::PopItemWidth();

	// Scale
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.2f, 0.5f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.3f, 0.7f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.4f, 0.9f, 1.0f));
	if (ImGui::Button("##RS", ImVec2(ResetBtnWidth, 0)))
	{
		EditScale = { 1.0f, 1.0f, 1.0f };
		bModified = true;
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Scale");
	ImGui::PopStyleColor(3);

	ImGui::SameLine(0, Spacing);
	// ImGui::PushItemWidth(-(ResetBtnWidth));
	ImGui::PushItemWidth(DragUIWidth);
	if (ImGui::DragFloat3("Scale", Scl, 0.01f, 0.001f, 100.0f, "%.3f"))
	{
		EditScale = { Scl[0], Scl[1], Scl[2] };
		bModified = true;
	}
	ImGui::PopItemWidth();

	if (bModified && OnChanged)
		OnChanged(EditLocation, EditRotation, EditScale);
}

void FPropertyWindow::Render(FCore* Core)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	bool bOpen = ImGui::Begin("Properties");
	ImGui::PopStyleVar();

	if (!bOpen)
	{
		ImGui::End();
		return;
	}

	bModified = false;

	ImGui::TextDisabled("Selected:");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.4f, 1.0f), "%s", ActorNameBuf);

	ImGui::Separator();

	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent(8.0f);
		DrawTransformSection();
		ImGui::Unindent(8.0f);
	}
	if (Core)
	{
		AActor* SelectedActor = Core->GetSelectedActor();
		if (SelectedActor)
		{
			if (ImGui::CollapsingHeader("Billboard", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Indent(8.0f);
				for (UActorComponent* Component : SelectedActor->GetComponents())
				{
					if (!Component) continue;

					if (Component->IsA(USubUVComponent::StaticClass()))
					{
						USubUVComponent* SubUVComp = static_cast<USubUVComponent*>(Component);
						bool bBillboard = SubUVComp->IsBillboard();
						if (ImGui::Checkbox("SubUV Billboard", &bBillboard))
							SubUVComp->SetBillboard(bBillboard);
					}
					else if (Component->IsA(UTextComponent::StaticClass()) && !Component->IsA(UUUIDBillboardComponent::StaticClass()))
					{
						UTextComponent* TextComp = static_cast<UTextComponent*>(Component);
						bool bBillboard = TextComp->IsBillboard();
						if (ImGui::Checkbox("Text Billboard", &bBillboard))
							TextComp->SetBillboard(bBillboard);
					}
				}
				ImGui::Unindent(8.0f);
			}
			if (SelectedActor && SelectedActor->IsA(AStaticMeshActor::StaticClass()))
			{
				if (ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Indent(8.0f);

					// .obj 파일 목록 스캔 (최초 1회)
					static TArray<FString> MeshFiles;
					static bool bScanned = false;
					if (!bScanned)
					{
						namespace fs = std::filesystem;
						auto MeshDir = FPaths::ProjectRoot() / "Assets" / "Meshes";
						if (fs::exists(MeshDir))
						{
							for (const auto& entry : fs::directory_iterator(MeshDir))
							{
								if (entry.is_regular_file() && entry.path().extension() == ".obj")
								{
									auto Rel = fs::relative(entry.path(), FPaths::ProjectRoot());
									MeshFiles.push_back(Rel.generic_string());
								}
							}
						}
						bScanned = true;
					}
					// ── 텍스처 파일 목록 스캔 (최초 1회) ──
					static TArray<FString> TextureFiles;
					static bool bTextureScanned = false;
					if (!bTextureScanned)
					{
						namespace fs = std::filesystem;
						auto MeshDir = FPaths::ProjectRoot() / "Assets" / "Meshes";
						if (fs::exists(MeshDir))
						{
							for (const auto& entry : fs::directory_iterator(MeshDir))
							{
								if (entry.is_regular_file())
								{
									auto ext = entry.path().extension().string();
									if (ext == ".png" || ext == ".PNG" || ext == ".jpg" || ext == ".jpeg")
									{
										auto Rel = fs::relative(entry.path(), FPaths::ProjectRoot());
										TextureFiles.push_back(Rel.generic_string());
									}
								}
							}
						}
						bTextureScanned = true;
					}
					AStaticMeshActor* SMActor = static_cast<AStaticMeshActor*>(SelectedActor);
					UStaticMeshComponent* SMComp = SMActor->GetStaticMeshComponent();
					FString CurrentAsset = SMComp ? SMComp->GetStaticMeshAsset() : "";
					ImGui::Text("Current: %s", CurrentAsset.empty() ? "None" : CurrentAsset.c_str());
					{
						std::vector<const char*> Items;
						Items.push_back("None");
						for (const auto& F : MeshFiles)
							Items.push_back(F.c_str());

						// 현재 선택 동기화
						SelectedMeshIndex = 0; // None
						for (int i = 0; i < (int)MeshFiles.size(); ++i)
						{
							if (MeshFiles[i] == CurrentAsset)
							{
								SelectedMeshIndex = i + 1; // +1 because None is at 0
								break;
							}
						}

						if (ImGui::Combo("Mesh Asset", &SelectedMeshIndex, Items.data(), (int)Items.size()))
						{
							if (Core && SelectedMeshIndex > 0)
							{
								ID3D11Device* Device = Core->GetRenderer()->GetDevice();
								SMActor->LoadStaticMesh(Device, MeshFiles[SelectedMeshIndex - 1]);
							}
						}
					}
					if (!CurrentAsset.empty())
					{
						static TArray<FString> MaterialNames;
						static bool bMatScanned = false;
						if (!bMatScanned)
						{
							namespace fs = std::filesystem;
							auto MatDir = FPaths::ProjectRoot() / "Assets" / "Materials";
							if (fs::exists(MatDir))
							{
								for (const auto& entry : fs::directory_iterator(MatDir))
								{
									if (entry.is_regular_file() && entry.path().extension() == ".json")
									{
										FString Name = entry.path().stem().string();
										MaterialNames.push_back(Name);
									}
								}
							}
							bMatScanned = true;
						}

						std::vector<const char*> MatItems;
						MatItems.push_back("Default");
						for (const auto& N : MaterialNames)
							MatItems.push_back(N.c_str());

						if (ImGui::Combo("Material", &SelectedMaterialIndex, MatItems.data(), (int)MatItems.size()))
						{
							if (Core && SMComp && SMComp->GetMesh() && SelectedMaterialIndex > 0)
							{
								FString MatName = MaterialNames[SelectedMaterialIndex - 1];
								auto Mat = FMaterialManager::Get().FindByName(MatName);
								if (Mat)
								{
									for (uint32 i = 0; i < SMComp->GetNumMaterials(); ++i)
										SMComp->SetMaterial(i, Mat.get());
								}
							}
						}

						// ── Texture 콤보 ──
						std::vector<const char*> TexItems;
						TexItems.push_back("None");
						for (const auto& T : TextureFiles)
							TexItems.push_back(T.c_str());

						if (ImGui::Combo("Texture", &SelectedTextureIndex, TexItems.data(), (int)TexItems.size()))
						{
							if (Core && SMComp && SelectedTextureIndex > 0)
							{
								ID3D11Device* Device = Core->GetRenderer()->GetDevice();
								SMComp->LoadTexture(Device, TextureFiles[SelectedTextureIndex - 1]);
							}
						}
					}

					ImGui::Unindent(8.0f);
				}
			}
		}
	}
	ImGui::End();
}