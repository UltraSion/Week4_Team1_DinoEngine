#pragma once

#include "UI/EditorViewportClient.h"

class FEditorOrthoViewportClient : public FEditorViewportClient
{
public:
	FEditorOrthoViewportClient(FEditorUI& InEditorUI, FWindow* InMainWindow, EOrthoViewType InOrthoViewType, ELevelType InWorldType);

protected:
	void ConfigureDefaultView() override;
	void DrawControllerOptions() override;
	void ProcessCameraInput(FCore* Core, float DeltaTime) override;
	void OnMouseButtonDown(UINT Msg, WPARAM WParam, LPARAM LParam) override;
	void OnMouseButtonUp(UINT Msg, WPARAM WParam, LPARAM LParam) override;
	void OnMouseWheel(float WheelDelta, WPARAM WParam, LPARAM LParam) override;

private:
	void UpdateCameraTransform();
	static EEditorViewportType GetViewportTypeFromOrthoView(EOrthoViewType InOrthoViewType);
	FVector GetOrthoForwardVector() const;
	FVector GetOrthoRightVector() const;
	FVector GetOrthoUpVector() const;

	EOrthoViewType OrthoViewType = EOrthoViewType::Top;
	FVector OrthoCenter = FVector::ZeroVector;
	float OrthoZoom = 24.0f;
	float ViewDistance = 25.0f;
	float PendingZoomStep = 0.0f;
	bool bPanning = false;
	int32 PanStartMouseX = 0;
	int32 PanStartMouseY = 0;
};
