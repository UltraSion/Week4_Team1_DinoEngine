#pragma once

#include "Core/FEngine.h"
#include "UI/EditorUI.h"

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
	void ConfigureViewportContext(size_t Index, FViewportContext& Context) override;
	void OnActiveViewportContextChanged(FViewportContext* NewActiveContext, FViewportContext* PreviousActiveContext) override;

private:
	void RefreshEditorViewportClients();
	FEditorViewportClient* ResolveEditorViewportClient(FViewportContext* ViewportContext) const;

	FEditorUI EditorUI;
	//TArray<std::unique_ptr<FViewportClient>> AdditionalViewportClients;
	//FEditorViewportController ViewportController;
};
