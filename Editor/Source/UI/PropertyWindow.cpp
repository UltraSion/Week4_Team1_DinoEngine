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
#include "Object/ObjectIterator.h"
#include "Object/StaticMesh.h"
#include "Core/Paths.h"
#include <filesystem>
#include <algorithm>

void FPropertyWindow::SetTarget(const FVector& Location, const FVector& Rotation, const FVector& Scale, const char* ActorName)
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
	const float ResetBtnWidth = 14.0f;
	const float Spacing = ImGui::GetStyle().ItemInnerSpacing.x;
	const float DragUIWidth = 200.f;

	// Transform UI 그리기 람다화
	auto DrawTransformRow = [&](const char* Label, const char* BtnID, FVector& Vec, const FVector& ResetVal, const ImVec4& BaseColor, float Step, float Min, float Max, const char* Format) {
		float Val[3] = { Vec.X, Vec.Y, Vec.Z };

		ImGui::PushStyleColor(ImGuiCol_Button, BaseColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(BaseColor.x + 0.2f, BaseColor.y + 0.1f, BaseColor.z + 0.1f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(BaseColor.x + 0.4f, BaseColor.y + 0.2f, BaseColor.z + 0.2f, 1.0f));
		
		if (ImGui::Button(BtnID, ImVec2(ResetBtnWidth, 0))) {
			Vec = ResetVal;
			bModified = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset %s", Label);
		ImGui::PopStyleColor(3);

		ImGui::SameLine(0, Spacing);
		ImGui::PushItemWidth(DragUIWidth);
		if (ImGui::DragFloat3(Label, Val, Step, Min, Max, Format)) {
			Vec = { Val[0], Val[1], Val[2] };
			bModified = true;
		}
		ImGui::PopItemWidth();
	};

	// 람다를 사용해 단 3줄로 UI 렌더링 끝
	DrawTransformRow("Location", "##RL", EditLocation, { 0.f, 0.f, 0.f }, ImVec4(0.5f, 0.1f, 0.1f, 1.0f), 0.1f, 0.0f, 0.0f, "%.2f");
	DrawTransformRow("Rotation", "##RR", EditRotation, { 0.f, 0.f, 0.f }, ImVec4(0.1f, 0.4f, 0.1f, 1.0f), 0.5f, -360.0f, 360.0f, "%.1f");
	DrawTransformRow("Scale",    "##RS", EditScale,    { 1.f, 1.f, 1.f }, ImVec4(0.1f, 0.2f, 0.5f, 1.0f), 0.01f, 0.001f, 100.0f, "%.3f");

	if (bModified && OnChanged)
		OnChanged(EditLocation, EditRotation, EditScale);
}

void FPropertyWindow::DrawBillboardSection(AActor* SelectedActor)
{
	if (!ImGui::CollapsingHeader("Billboard", ImGuiTreeNodeFlags_DefaultOpen)) return;

	ImGui::Indent(8.0f);
	for (UActorComponent* Component : SelectedActor->GetComponents())
	{
		if (!Component) continue;

		if (Component->IsA(USubUVComponent::StaticClass())) {
			auto* SubUVComp = static_cast<USubUVComponent*>(Component);
			bool bBillboard = SubUVComp->IsBillboard();
			if (ImGui::Checkbox("SubUV Billboard", &bBillboard)) SubUVComp->SetBillboard(bBillboard);
		}
		else if (Component->IsA(UTextComponent::StaticClass()) && !Component->IsA(UUUIDBillboardComponent::StaticClass())) {
			auto* TextComp = static_cast<UTextComponent*>(Component);
			bool bBillboard = TextComp->IsBillboard();
			if (ImGui::Checkbox("Text Billboard", &bBillboard)) TextComp->SetBillboard(bBillboard);
		}
	}
	ImGui::Unindent(8.0f);
}

// 공통 Drag & Drop 처리 람다 (어디서든 재사용 가능)
auto ProcessDragDrop = [](const std::vector<std::string>& TargetExts, auto OnDropValid) {
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
			std::string AbsolutePath = (const char*)Payload->Data;
			std::string LowerPath = AbsolutePath;
			std::transform(LowerPath.begin(), LowerPath.end(), LowerPath.begin(), ::tolower);

			// 확장자 검사
			bool bValid = false;
			for (const auto& Ext : TargetExts) {
				if (LowerPath.find(Ext) != std::string::npos) { bValid = true; break; }
			}

			if (bValid) {
				namespace fs = std::filesystem;
				std::string RelativePath = fs::relative(fs::path(AbsolutePath), fs::path(FPaths::ProjectRoot())).generic_string();
				OnDropValid(AbsolutePath, RelativePath);
			}
		}
		ImGui::EndDragDropTarget();
	}
};

void FPropertyWindow::DrawMaterialSlots(FCore* Core, UStaticMeshComponent* SMComp, AActor* SelectedActor)
{
	//  폴더 스캔 람다화
	auto ScanFiles = [](const std::filesystem::path& SubDir, const std::vector<std::string>& Extensions, TArray<FString>& OutNames, bool bStemOnly) {
		namespace fs = std::filesystem;
		auto TargetDir = FPaths::ProjectRoot() / SubDir;
		if (fs::exists(TargetDir)) {
			for (const auto& entry : fs::directory_iterator(TargetDir)) {
				if (!entry.is_regular_file()) continue;
				std::string ext = entry.path().extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
				
				if (std::find(Extensions.begin(), Extensions.end(), ext) != Extensions.end()) {
					if (bStemOnly) OutNames.push_back(entry.path().stem().string());
					else OutNames.push_back(fs::relative(entry.path(), FPaths::ProjectRoot()).generic_string());
				}
			}
		}
	};

	static TArray<FString> MaterialNames;
	static TArray<FString> TextureFiles;
	static bool bScanned = false;

	if (!bScanned) {
		ScanFiles("Assets/Materials", { ".json" }, MaterialNames, true);
		ScanFiles("Assets/Meshes", { ".png", ".jpg", ".jpeg" }, TextureFiles, false);
		bScanned = true;
	}

	std::vector<const char*> MatItems = { "Default" };
	for (const auto& N : MaterialNames) MatItems.push_back(N.c_str());

	std::vector<const char*> TexItems = { "None" };
	for (const auto& T : TextureFiles) TexItems.push_back(T.c_str());

	uint32 NumSlots = SMComp->GetNumMaterials();
	static AActor* LastSelectedActor = nullptr;
	static std::vector<int> SlotMatIndices;
	static std::vector<int> SlotTexIndices;

	if (LastSelectedActor != SelectedActor) {
		SlotMatIndices.clear();
		SlotTexIndices.clear();
		LastSelectedActor = SelectedActor;
	}
	if (SlotMatIndices.size() < NumSlots) SlotMatIndices.resize(NumSlots, 0);
	if (SlotTexIndices.size() < NumSlots) SlotTexIndices.resize(NumSlots, 0);

	for (uint32 SlotIdx = 0; SlotIdx < NumSlots; ++SlotIdx)
	{
		ImGui::PushID(SlotIdx);
		if (ImGui::TreeNodeEx(("Material Slot " + std::to_string(SlotIdx)).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Combo("Material", &SlotMatIndices[SlotIdx], MatItems.data(), (int)MatItems.size()) && Core && SlotMatIndices[SlotIdx] > 0) {
				if (auto Mat = FMaterialManager::Get().FindByName(MaterialNames[SlotMatIndices[SlotIdx] - 1]))
					SMComp->SetMaterial(SlotIdx, Mat.get());
			}

			if (ImGui::Combo("Texture", &SlotTexIndices[SlotIdx], TexItems.data(), (int)TexItems.size()) && Core && SlotTexIndices[SlotIdx] > 0) {
				SMComp->LoadTextureToSlot(Core->GetRenderer()->GetDevice(), TextureFiles[SlotTexIndices[SlotIdx] - 1], SlotIdx);
			}

			// Texture 드롭 타겟!
			ProcessDragDrop({ ".png", ".jpg", ".jpeg" }, [&](const std::string& AbsPath, const std::string& RelPath) {
				SMComp->LoadTextureToSlot(Core->GetRenderer()->GetDevice(), AbsPath, SlotIdx);
				for (int i = 0; i < (int)TextureFiles.size(); ++i) {
					if (TextureFiles[i] == RelPath) { SlotTexIndices[SlotIdx] = i + 1; break; }
				}
			});

			ImGui::TreePop();
		}
		ImGui::PopID();
	}
}

void FPropertyWindow::DrawStaticMeshSection(FCore* Core, AStaticMeshActor* SMActor)
{
	if (!ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen)) return;

	ImGui::Indent(8.0f);
	UStaticMeshComponent* SMComp = SMActor->GetStaticMeshComponent();
	if (!SMComp) { ImGui::Unindent(8.0f); return; }

	std::vector<UStaticMesh*> LoadedMeshes;
	std::vector<const char*> Items = { "None" };

	for (TObjectIterator<UStaticMesh> It; It; ++It) {
		LoadedMeshes.push_back(*It);
		Items.push_back((*It)->GetAssetPathFileName().c_str());
	}

	int SelectedMeshIndex = 0;
	FString CurrentAsset = SMComp->GetStaticMeshAsset();
	for (int i = 0; i < (int)LoadedMeshes.size(); ++i) {
		if (LoadedMeshes[i]->GetAssetPathFileName() == CurrentAsset) { SelectedMeshIndex = i + 1; break; }
	}

	if (ImGui::Combo("Mesh Asset", &SelectedMeshIndex, Items.data(), (int)Items.size()) && Core && SelectedMeshIndex > 0) {
		SMComp->SetStaticMeshData(Core->GetRenderer()->GetDevice(), LoadedMeshes[SelectedMeshIndex - 1]->GetStaticMeshAsset());
	}

	// 람다를 활용한 Mesh 드롭 타겟
	ProcessDragDrop({ ".obj" }, [&](const std::string& AbsPath, const std::string& RelPath) {
		SMComp->LoadStaticMesh(Core->GetRenderer()->GetDevice(), RelPath);
	});

	if (!CurrentAsset.empty()) DrawMaterialSlots(Core, SMComp, SMActor);
	
	ImGui::Unindent(8.0f);
}

void FPropertyWindow::Render(FCore* Core)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	bool bOpen = ImGui::Begin("Properties");
	ImGui::PopStyleVar();

	if (!bOpen) { ImGui::End(); return; }

	bModified = false;
	ImGui::TextDisabled("Selected:"); ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.4f, 1.0f), "%s", ActorNameBuf);
	ImGui::Separator();

	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Indent(8.0f);
		DrawTransformSection();
		ImGui::Unindent(8.0f);
	}

	if (Core && Core->GetSelectedActor()) {
		AActor* SelectedActor = Core->GetSelectedActor();
		DrawBillboardSection(SelectedActor);

		if (SelectedActor->IsA(AStaticMeshActor::StaticClass()))
			DrawStaticMeshSection(Core, static_cast<AStaticMeshActor*>(SelectedActor));
	}
	ImGui::End();
}