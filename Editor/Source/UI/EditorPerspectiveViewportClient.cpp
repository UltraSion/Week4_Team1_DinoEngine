#include "EditorPerspectiveViewportClient.h"

#include "Math/MathUtility.h"
#include "imgui.h"

namespace
{
	constexpr float PerspectivePitchMin = -89.0f;
	constexpr float PerspectivePitchMax = 89.0f;
	constexpr float PerspectiveWheelMoveScale = 1.0f;
}

FEditorPerspectiveViewportClient::FEditorPerspectiveViewportClient(FEditorUI& InEditorUI, FWindow* InMainWindow, ELevelType InWorldType)
	: FEditorViewportClient(InEditorUI, InMainWindow, EEditorViewportType::Perspective, InWorldType)
{
	ConfigureDefaultView();
	SaveInitialCameraState();
}

void FEditorPerspectiveViewportClient::ConfigureDefaultView()
{
	CameraTransform.SetProjectionMode(ECameraProjectionMode::Perspective);
	CameraTransform.SetPosition({ -5.0f, 0.0f, 2.0f });
	CameraTransform.SetRotation(0.0f, 0.0f);
}

void FEditorPerspectiveViewportClient::DrawControllerOptions()
{
	float Sensitivity = CameraTransform.GetMouseSensitivity();
	if (ImGui::SliderFloat("Mouse Sensitivity", &Sensitivity, 0.01f, 1.0f))
	{
		CameraTransform.SetMouseSensitivity(Sensitivity);
	}

	float Speed = CameraTransform.GetSpeed();
	if (ImGui::SliderFloat("Move Speed", &Speed, 0.1f, 20.0f))
	{
		CameraTransform.SetSpeed(Speed);
	}

	const FVector CameraPosition = CameraTransform.GetPosition();
	float Position[3] = { CameraPosition.X, CameraPosition.Y, CameraPosition.Z };
	if (ImGui::DragFloat3("Position", Position, 0.1f))
	{
		CameraTransform.SetPosition({ Position[0], Position[1], Position[2] });
	}

	float CameraYaw = CameraTransform.GetYaw();
	float CameraPitch = CameraTransform.GetPitch();
	bool bRotationChanged = false;
	bRotationChanged |= ImGui::DragFloat("Yaw", &CameraYaw, 0.5f);
	bRotationChanged |= ImGui::DragFloat("Pitch", &CameraPitch, 0.5f, PerspectivePitchMin, PerspectivePitchMax);
	if (bRotationChanged)
	{
		CameraTransform.SetRotation(CameraYaw, FMath::Clamp(CameraPitch, PerspectivePitchMin, PerspectivePitchMax));
	}

	float CameraFOV = CameraTransform.GetFOV();
	if (ImGui::SliderFloat("FOV", &CameraFOV, 10.0f, 120.0f))
	{
		CameraTransform.SetFOV(CameraFOV);
	}
}

void FEditorPerspectiveViewportClient::ProcessCameraInput(FCore* Core, float DeltaTime)
{
	(void)Core;

	if (!InputManager)
	{
		return;
	}

	if (const float MouseWheelDelta = InputManager->GetMouseWheelDelta(); MouseWheelDelta != 0.0f)
	{
		CameraTransform.MoveForward(MouseWheelDelta * PerspectiveWheelMoveScale);
	}

	if (!bRotating)
	{
		return;
	}

	const float MouseDeltaX = InputManager->GetMouseDeltaX();
	const float MouseDeltaY = InputManager->GetMouseDeltaY();
	if (MouseDeltaX != 0.0f || MouseDeltaY != 0.0f)
	{
		ApplyLookInput(MouseDeltaX, MouseDeltaY);
	}

	float ForwardInput = 0.0f;
	float RightInput = 0.0f;
	float UpInput = 0.0f;
	if (bMoveForward) ForwardInput += 1.0f;
	if (bMoveBackward) ForwardInput -= 1.0f;
	if (bMoveRight) RightInput += 1.0f;
	if (bMoveLeft) RightInput -= 1.0f;
	if (bMoveUp) UpInput += 1.0f;
	if (bMoveDown) UpInput -= 1.0f;

	if (ForwardInput != 0.0f)
	{
		CameraTransform.MoveForward(ForwardInput * DeltaTime);
	}
	if (RightInput != 0.0f)
	{
		CameraTransform.MoveRight(RightInput * DeltaTime);
	}
	if (UpInput != 0.0f)
	{
		CameraTransform.MoveUp(UpInput * DeltaTime);
	}
}

void FEditorPerspectiveViewportClient::OnMouseButtonDown(UINT Msg, WPARAM WParam, LPARAM LParam)
{
	(void)WParam;
	(void)LParam;

	if (Msg == WM_RBUTTONDOWN || Msg == WM_RBUTTONDBLCLK)
	{
		bRotating = true;
	}
}

void FEditorPerspectiveViewportClient::OnMouseButtonUp(UINT Msg, WPARAM WParam, LPARAM LParam)
{
	(void)WParam;
	(void)LParam;

	if (Msg == WM_RBUTTONUP)
	{
		bRotating = false;
		ResetMovementState();
	}
}

void FEditorPerspectiveViewportClient::OnKeyDown(WPARAM WParam, LPARAM LParam)
{
	(void)LParam;

	switch (WParam)
	{
	case 'W': bMoveForward = true; break;
	case 'S': bMoveBackward = true; break;
	case 'D': bMoveRight = true; break;
	case 'A': bMoveLeft = true; break;
	case 'E': bMoveUp = true; break;
	case 'Q': bMoveDown = true; break;
	default: break;
	}
}

void FEditorPerspectiveViewportClient::OnKeyUp(WPARAM WParam, LPARAM LParam)
{
	(void)LParam;

	switch (WParam)
	{
	case 'W': bMoveForward = false; break;
	case 'S': bMoveBackward = false; break;
	case 'D': bMoveRight = false; break;
	case 'A': bMoveLeft = false; break;
	case 'E': bMoveUp = false; break;
	case 'Q': bMoveDown = false; break;
	default: break;
	}
}

void FEditorPerspectiveViewportClient::ResetMovementState()
{
	bMoveForward = false;
	bMoveBackward = false;
	bMoveRight = false;
	bMoveLeft = false;
	bMoveUp = false;
	bMoveDown = false;
}

void FEditorPerspectiveViewportClient::ApplyLookInput(float MouseDeltaX, float MouseDeltaY)
{
	const float NewYaw = CameraTransform.GetYaw() + MouseDeltaX * CameraTransform.GetMouseSensitivity();
	const float NewPitch = FMath::Clamp(
		CameraTransform.GetPitch() - MouseDeltaY * CameraTransform.GetMouseSensitivity(),
		PerspectivePitchMin,
		PerspectivePitchMax);
	CameraTransform.SetRotation(NewYaw, NewPitch);
}
