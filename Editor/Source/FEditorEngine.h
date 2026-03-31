#pragma once

#include "Core/FEngine.h"
#include "UI/EditorUI.h"
#include "UI/WindowManager.h"

class FEditorEngine : public FEngine
{
public:
	FEditorEngine() = default;
	~FEditorEngine() override;

	bool Initialize(HINSTANCE hInstance);
	void OpenNewObj();
	void Shutdown() override;
	FWindowManager& GetWindowManager() { return WindowManager; }
	void SaveEditorSettings();
	void SetViewportLayoutBounds(FRect InRect);

protected:
	void PreInitialize() override;
	void PostInitialize() override;
	void ProcessInput(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam) override;
	void Tick(float DeltaTime) override;
	void Render() override;
	void OnMainWindowResized(int32 Width, int32 Height) override;
	ELevelType GetStartupLevelType() const override { return ELevelType::Editor; }
	FViewportClient* CreateViewportClient() override;

private:
	void RunObjViewerStartupTest();

	FEditorUI EditorUI;
	FWindowManager WindowManager;

};
