#pragma once

#include "Core/FEngine.h"
#include "UI/EditorUI.h"
#include "Controller/EditorViewportController.h"

class AEditorCameraPawn;

class FEditorEngine : public FEngine
{
public:
	FEditorEngine() = default;
	~FEditorEngine() override;

	bool Initialize(HINSTANCE hInstance);
	void Shutdown() override;

protected:
	void PreInitialize() override;
	void PostInitialize() override;
	void Tick(float DeltaTime) override;
	void Render() override;
	ELevelType GetStartupLevelType() const override { return ELevelType::Editor; }
	std::unique_ptr<FViewportClient> CreateViewportClient() override;

	FEditorViewportController* GetViewportController();

private:
	void SyncViewportClient();

	FEditorUI EditorUI;
	FViewportClient* ActiveViewportClient = nullptr;
	//TArray<std::unique_ptr<FViewportClient>> AdditionalViewportClients;
	//FEditorViewportController ViewportController;
	TArray<AActor*> SeletedActors;
};
