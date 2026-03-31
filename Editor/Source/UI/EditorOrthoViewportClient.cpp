#include "EditorOrthoViewportClient.h"

#include "Math/MathUtility.h"
#include "imgui.h"

#include <cmath>

namespace
{
	constexpr float OrthoMinZoom = 1.0f;
	constexpr float OrthoMaxZoom = 1000.0f;
	constexpr float OrthoZoomStepScale = 1.1f;

	const char* GetOrthoViewLabel(EOrthoViewType OrthoViewType)
	{
		switch (OrthoViewType)
		{
		case EOrthoViewType::Front:
			return "Front";
		case EOrthoViewType::Right:
			return "Right";
		case EOrthoViewType::Top:
		default:
			return "Top";
		}
	}
}

FEditorOrthoViewportClient::FEditorOrthoViewportClient(FEditorUI& InEditorUI, FWindow* InMainWindow, EOrthoViewType InOrthoViewType, ELevelType InWorldType)
	: FEditorViewportClient(InEditorUI, InMainWindow, GetViewportTypeFromOrthoView(InOrthoViewType), InWorldType)
	, OrthoViewType(InOrthoViewType)
{
	ConfigureDefaultView();
	SaveInitialCameraState();
}

void FEditorOrthoViewportClient::ConfigureDefaultView()
{
	CameraTransform.SetProjectionMode(ECameraProjectionMode::Orthographic);
	CameraTransform.SetOrthoWidth(OrthoZoom);

	switch (OrthoViewType)
	{
	case EOrthoViewType::Top:
		ViewDistance = 25.0f;
		CameraTransform.SetRotation(0.0f, -89.99f);
		break;
	case EOrthoViewType::Front:
		ViewDistance = 25.0f;
		CameraTransform.SetRotation(0.0f, 0.0f);
		break;
	case EOrthoViewType::Right:
		ViewDistance = 25.0f;
		CameraTransform.SetRotation(90.f, 0.0f);
		break;
	}

	OrthoCenter = FVector::ZeroVector;
	UpdateCameraTransform();
}

void FEditorOrthoViewportClient::DrawControllerOptions()
{
	ImGui::Text("View: %s", GetOrthoViewLabel(OrthoViewType));

	float Center[3] = { OrthoCenter.X, OrthoCenter.Y, OrthoCenter.Z };
	if (ImGui::DragFloat3("Center", Center, 0.1f))
	{
		OrthoCenter = { Center[0], Center[1], Center[2] };
		UpdateCameraTransform();
	}

	float CameraYaw = CameraTransform.GetYaw();
	float CameraPitch = CameraTransform.GetPitch();
	ImGui::DragFloat("Yaw", &CameraYaw, 0.5f);
	ImGui::DragFloat("Pitch", &CameraPitch, 0.5f);

	CameraTransform.SetRotation(CameraYaw, CameraPitch);


	float Zoom = OrthoZoom;
	if (ImGui::DragFloat("Zoom", &Zoom, 0.5f, OrthoMinZoom, OrthoMaxZoom))
	{
		OrthoZoom = FMath::Clamp(Zoom, OrthoMinZoom, OrthoMaxZoom);
		UpdateCameraTransform();
	}
}

void FEditorOrthoViewportClient::ProcessCameraInput(FCore* Core, float DeltaTime)
{
	(void)Core;
	(void)DeltaTime;

	if (!InputManager)
	{
		return;
	}

	if (PendingZoomStep != 0.0f)
	{
		const float ZoomScale = std::pow(OrthoZoomStepScale, PendingZoomStep);
		OrthoZoom = FMath::Clamp(OrthoZoom / ZoomScale, OrthoMinZoom, OrthoMaxZoom);
		PendingZoomStep = 0.0f;
		UpdateCameraTransform();
	}

	if (!bPanning)
	{
		return;
	}

	const float MouseDeltaX = InputManager->GetMouseDeltaX();
	const float MouseDeltaY = InputManager->GetMouseDeltaY();
	if (MouseDeltaX == 0.0f && MouseDeltaY == 0.0f)
	{
		return;
	}

	const float UnitsPerPixelX = ViewportWidth > 0 ? CameraTransform.GetOrthoWidth() / static_cast<float>(ViewportWidth) : 0.0f;
	const float UnitsPerPixelY = ViewportHeight > 0 ? CameraTransform.GetOrthoHeight() / static_cast<float>(ViewportHeight) : 0.0f;
	OrthoCenter = OrthoCenter
		- GetOrthoRightVector() * (MouseDeltaX * UnitsPerPixelX)
		+ GetOrthoUpVector() * (MouseDeltaY * UnitsPerPixelY);
	UpdateCameraTransform();
}

void FEditorOrthoViewportClient::OnMouseButtonDown(UINT Msg, WPARAM WParam, LPARAM LParam)
{
	(void)WParam;
	(void)LParam;

	if (Msg == WM_MBUTTONDOWN || Msg == WM_MBUTTONDBLCLK || Msg == WM_RBUTTONDOWN || Msg == WM_RBUTTONDBLCLK)
	{
		bPanning = true;
		PanStartMouseX = ViewportMouseX;
		PanStartMouseY = ViewportMouseY;
	}
}

void FEditorOrthoViewportClient::OnMouseButtonUp(UINT Msg, WPARAM WParam, LPARAM LParam)
{
	(void)WParam;
	(void)LParam;

	if (Msg == WM_MBUTTONUP || Msg == WM_RBUTTONUP)
	{
		bPanning = false;
	}
}

void FEditorOrthoViewportClient::OnMouseWheel(float WheelDelta, WPARAM WParam, LPARAM LParam)
{
	(void)WParam;
	(void)LParam;
	PendingZoomStep += WheelDelta;
}

void FEditorOrthoViewportClient::UpdateCameraTransform()
{
	CameraTransform.SetProjectionMode(ECameraProjectionMode::Orthographic);
	CameraTransform.SetOrthoWidth(OrthoZoom);
	CameraTransform.SetPosition(OrthoCenter - GetOrthoForwardVector() * ViewDistance);
}

EEditorViewportType FEditorOrthoViewportClient::GetViewportTypeFromOrthoView(EOrthoViewType InOrthoViewType)
{
	switch (InOrthoViewType)
	{
	case EOrthoViewType::Front:
		return EEditorViewportType::Front;
	case EOrthoViewType::Right:
		return EEditorViewportType::Right;
	case EOrthoViewType::Top:
	default:
		return EEditorViewportType::Top;
	}
}

FVector FEditorOrthoViewportClient::GetOrthoForwardVector() const
{
	return CameraTransform.GetForward().GetSafeNormal();
}

FVector FEditorOrthoViewportClient::GetOrthoRightVector() const
{
	return CameraTransform.GetRight().GetSafeNormal();
}

FVector FEditorOrthoViewportClient::GetOrthoUpVector() const
{
	return GetViewportUpVector();
}
