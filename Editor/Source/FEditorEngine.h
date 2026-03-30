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
	ELevelType GetStartupLevelType() const override { return ELevelType::Editor; }
	FViewportClient* CreateViewportClient() override;

private:
	void RunObjViewerStartupTest();

	FEditorUI EditorUI;
};
