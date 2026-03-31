#include "FPerspectToOrtho.h"

#include "Camera/Camera.h"
#include "Math/MathUtility.h"

namespace
{
	FVector LerpVector(const FVector& A, const FVector& B, float Alpha)
	{
		return A + (B - A) * Alpha;
	}

	float LerpFloat(float A, float B, float Alpha)
	{
		return A + (B - A) * Alpha;
	}
}

void FPerspectToOrtho::StartTransition(const FVector& InTargetPosition, float InOrthoWidth, float InTransitionTime)
{
	if (!Camera)
	{
		return;
	}

	StartFOV = Camera->GetFOV();
	StartPosition = Camera->GetPosition();
	TargetPosition = InTargetPosition;
	TargetOrthoWidth = FMath::Max(InOrthoWidth, 0.01f);
	TransitionTime = FMath::Max(InTransitionTime, 0.01f);
	ElapsedTime = 0.0f;
	bIsTransitioning = true;
	bSwitchedToOrtho = Camera->IsOrthographic();

	if (bSwitchedToOrtho)
	{
		Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
	}
	else
	{
		Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
		Camera->SetFOV(StartFOV);
	}
}

void FPerspectToOrtho::Tick(float DeltaTime)
{
	if (!Camera || !bIsTransitioning)
	{
		return;
	}

	ElapsedTime += DeltaTime;
	const float Alpha = FMath::Clamp(ElapsedTime / TransitionTime, 0.0f, 1.0f);
	const float EasedAlpha = EvaluateEaseInOut(Alpha);

	Camera->SetPosition(LerpVector(StartPosition, TargetPosition, EasedAlpha));

	if (!bSwitchedToOrtho)
	{
		const float PerspectivePhaseAlpha = FMath::Clamp(Alpha / 0.5f, 0.0f, 1.0f);
		const float PerspectiveEase = EvaluateEaseInOut(PerspectivePhaseAlpha);
		Camera->SetFOV(LerpFloat(StartFOV, 1.0f, PerspectiveEase));

		if (Alpha >= 0.5f)
		{
			bSwitchedToOrtho = true;
			Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
			Camera->SetOrthoWidth(FMath::Max(TargetOrthoWidth * 0.05f, 0.01f));
		}
	}

	if (bSwitchedToOrtho)
	{
		const float OrthoPhaseAlpha = FMath::Clamp((Alpha - 0.5f) / 0.5f, 0.0f, 1.0f);
		const float OrthoEase = EvaluateEaseInOut(OrthoPhaseAlpha);
		const float StartOrthoWidth = FMath::Max(TargetOrthoWidth * 0.05f, 0.01f);
		Camera->SetOrthoWidth(LerpFloat(StartOrthoWidth, TargetOrthoWidth, OrthoEase));
	}

	if (Alpha >= 1.0f)
	{
		Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
		Camera->SetPosition(TargetPosition);
		Camera->SetOrthoWidth(TargetOrthoWidth);
		bIsTransitioning = false;
	}
}

float FPerspectToOrtho::EvaluateEaseInOut(float T) const
{
	const float ClampedT = FMath::Clamp(T, 0.0f, 1.0f);
	return ClampedT * ClampedT * (3.0f - 2.0f * ClampedT);
}
