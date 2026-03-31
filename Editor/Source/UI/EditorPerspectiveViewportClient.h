#pragma once

#include "UI/EditorViewportClient.h"

class FEditorPerspectiveViewportClient : public FEditorViewportClient
{
public:
	FEditorPerspectiveViewportClient(FEditorUI& InEditorUI, FWindow* InMainWindow, ELevelType InWorldType);

protected:
	void ConfigureDefaultView() override;
	void DrawViewportSpecificOptions() override;
	void DrawControllerOptions() override;
	void ProcessCameraInput(FCore* Core, float DeltaTime) override;
	void OnMouseButtonDown(UINT Msg, WPARAM WParam, LPARAM LParam) override;
	void OnMouseButtonUp(UINT Msg, WPARAM WParam, LPARAM LParam) override;
	void OnKeyDown(WPARAM WParam, LPARAM LParam) override;
	void OnKeyUp(WPARAM WParam, LPARAM LParam) override;

private:
	void StartOrthoTransition(EOrthoViewType OrthoViewType);
	void ResetMovementState();
	void ApplyLookInput(float MouseDeltaX, float MouseDeltaY);

	bool bRotating = false;
	bool bMoveForward = false;
	bool bMoveBackward = false;
	bool bMoveRight = false;
	bool bMoveLeft = false;
	bool bMoveUp = false;
	bool bMoveDown = false;
};
