#include "FPerspectToOrtho.h"

#include "Actor/Actor.h"
#include "Camera/Camera.h"
#include "Math/MathUtility.h"
#include "Component/PrimitiveComponent.h"

#include <cmath>

void FPerspectToOrtho::StartTransition()
{
	if (!Camera)
	{
		return;
	}


	StartFOV = Camera->GetFOV();
	ViewDirection = Camera->GetForward().GetSafeNormal();
	PivotPosition = Camera->GetPosition();
	if (ViewDirection.IsNearlyZero())
	{
		ViewDirection = FVector::ForwardVector;
	}

	StartPosition = Camera->GetPosition();
	float defualtSize = 5;
	float OrthoWidth = std::tanf(MinPerspectiveFOV / 2);


	MoveElapsedTime = 0.0f;
	bIsTransition = true;
}

void FPerspectToOrtho::Tick(float DeltaTime)
{
	if (!Camera || !bIsTransition)
	{
		return;
	}

	MoveElapsedTime += DeltaTime;
	const float MoveAlpha = FocusTime > 0.0f ? FMath::Clamp(MoveElapsedTime / FocusTime, 0.0f, 1.0f) : 1.0f;
	const float EasedAlpha = EvaluateEaseInOut(MoveAlpha);

	const float CurrentFov = LerpFloat(StartFOV, MinPerspectiveFOV, EasedAlpha);

	float defualtSize = 5;
	float OrthoWidth = std::tanf(StartFOV / 2);

	const FVector CurrentPosition = StartPosition - ViewDirection * ComputeFocusDistance(FMath::Max(OrthoWidth * defualtSize, 0.01f), CurrentFov);

	Camera->SetFOV(CurrentFov);
	Camera->SetPosition(CurrentPosition);


	if (MoveAlpha >= 1.0f)
	{
		bIsTransition = false;
		Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
		Camera->SetOrthoWidth(OrthoWidth * defualtSize * 2.f);
		Camera->SetPosition(StartPosition);
	}
}
