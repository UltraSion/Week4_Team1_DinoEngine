#include "FOrthoToPerspect.h"

#include "Camera/Camera.h"
#include "Math/MathUtility.h"

#include <cmath>

namespace
{
	constexpr float MinTransitionTime = 0.01f;
	constexpr float MinOrthoWidth = 0.01f;
	constexpr float MinPerspectiveFOV = 1.0f;
	constexpr float MaxPerspectiveFOV = 179.0f;
	constexpr float MinOrbitDistance = 0.01f;

	float LerpFloat(float A, float B, float Alpha)
	{
		return A + (B - A) * Alpha;
	}

	float LerpAngleDegrees(float Start, float End, float Alpha)
	{
		float Delta = std::fmod(End - Start, 360.0f);
		if (Delta > 180.0f)
		{
			Delta -= 360.0f;
		}
		else if (Delta < -180.0f)
		{
			Delta += 360.0f;
		}

		return Start + Delta * Alpha;
	}
}

void FOrthoToPerspect::StartTransition(
	const FVector& InPivotPosition,
	const FVector& InTargetRotation,
	float InTargetFOV,
	float InTransitionTime)
{
	if (!Camera)
	{
		return;
	}

	StartPosition = Camera->GetPosition();
	StartRotation = FVector(Camera->GetYaw(), Camera->GetPitch(), 0.0f);
	StartOrthoWidth = Camera->GetOrthoWidth();

	PivotPosition = InPivotPosition;
	TargetRotation = InTargetRotation;
	TargetFOV = FMath::Clamp(InTargetFOV, MinPerspectiveFOV, MaxPerspectiveFOV);

	CameraDistance = FMath::Max(FVector::Dist(StartPosition, PivotPosition), MinOrbitDistance);
	TargetPosition = CalculateOrbitPosition(PivotPosition, TargetRotation, CameraDistance);

	TransitionTime = FMath::Max(InTransitionTime, MinTransitionTime);
	ElapsedTime = 0.0f;
	bIsTransitioning = true;
	bSwitchedToPerspective = false;

	Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
	Camera->SetOrthoWidth(StartOrthoWidth);
}

void FOrthoToPerspect::Tick(float DeltaTime)
{
	UpdateTransition(DeltaTime);
}

bool FOrthoToPerspect::IsFinished() const
{
	return !bIsTransitioning;
}

void FOrthoToPerspect::UpdateTransition(float DeltaTime)
{
	if (!Camera || !bIsTransitioning)
	{
		return;
	}

	ElapsedTime += DeltaTime;

	const float Alpha = FMath::Clamp(ElapsedTime / TransitionTime, 0.0f, 1.0f);
	const float EasedAlpha = EvaluateEaseInOut(Alpha);

	const FVector CurrentRotation = InterpolateRotation(StartRotation, TargetRotation, EasedAlpha);
	const FVector CurrentPosition = CalculateOrbitPosition(PivotPosition, CurrentRotation, CameraDistance);

	Camera->SetRotation(CurrentRotation.X, CurrentRotation.Y);
	Camera->SetPosition(CurrentPosition);

	const float SwitchOrthoWidth = FMath::Max(StartOrthoWidth * 0.05f, MinOrthoWidth);
	if (!bSwitchedToPerspective)
	{
		const float OrthoPhaseAlpha = FMath::Clamp(Alpha / 0.5f, 0.0f, 1.0f);
		const float OrthoEase = EvaluateEaseInOut(OrthoPhaseAlpha);
		Camera->SetOrthoWidth(LerpFloat(StartOrthoWidth, SwitchOrthoWidth, OrthoEase));

		if (Alpha >= 0.5f)
		{
			bSwitchedToPerspective = true;
			Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
			Camera->SetFOV(MinPerspectiveFOV);
		}
	}

	if (bSwitchedToPerspective)
	{
		const float PerspectivePhaseAlpha = FMath::Clamp((Alpha - 0.5f) / 0.5f, 0.0f, 1.0f);
		const float PerspectiveEase = EvaluateEaseInOut(PerspectivePhaseAlpha);
		Camera->SetFOV(LerpFloat(MinPerspectiveFOV, TargetFOV, PerspectiveEase));
	}

	if (Alpha >= 1.0f)
	{
		Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
		Camera->SetRotation(TargetRotation.X, TargetRotation.Y);
		Camera->SetPosition(TargetPosition);
		Camera->SetFOV(TargetFOV);
		bIsTransitioning = false;
	}
}

float FOrthoToPerspect::EvaluateEaseInOut(float T) const
{
	const float ClampedT = FMath::Clamp(T, 0.0f, 1.0f);
	return ClampedT * ClampedT * (3.0f - 2.0f * ClampedT);
}

FVector FOrthoToPerspect::CalculateOrbitPosition(
	const FVector& InPivotPosition,
	const FVector& InRotation,
	float InDistance) const
{
	const float YawRadians = FMath::DegreesToRadians(InRotation.X);
	const float PitchRadians = FMath::DegreesToRadians(InRotation.Y);

	FVector Forward;
	Forward.X = std::cos(PitchRadians) * std::cos(YawRadians);
	Forward.Y = std::cos(PitchRadians) * std::sin(YawRadians);
	Forward.Z = std::sin(PitchRadians);
	Forward = Forward.GetSafeNormal();

	return InPivotPosition - Forward * InDistance;
}

FVector FOrthoToPerspect::InterpolateRotation(
	const FVector& InStartRotation,
	const FVector& InTargetRotation,
	float T) const
{
	return FVector(
		LerpAngleDegrees(InStartRotation.X, InTargetRotation.X, T),
		LerpAngleDegrees(InStartRotation.Y, InTargetRotation.Y, T),
		LerpAngleDegrees(InStartRotation.Z, InTargetRotation.Z, T));
}
