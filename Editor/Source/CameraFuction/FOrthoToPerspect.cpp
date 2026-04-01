#include "FOrthoToPerspect.h"

#include "Camera/Camera.h"
#include "Math/MathUtility.h"

#include "Actor/Actor.h"
#include "Component/PrimitiveComponent.h"

#include <cmath>

void FOrthoToPerspect::StartTransition(float InTargetFov)
{
	if (!Camera)
	{
		return;
	}

	TargetFov = InTargetFov;
	ViewDirection = Camera->GetForward().GetSafeNormal();
	PivotPosition =	Camera->GetPosition();
	if (ViewDirection.IsNearlyZero())
	{
		ViewDirection = FVector::ForwardVector;
	}

	StartPosition = Camera->GetPosition() - ViewDirection * ComputeFocusDistance(FMath::Max(std::tanf(MinPerspectiveFOV / 2), 0.01f), MinPerspectiveFOV);;
	TargetPosition = Camera->GetPosition() - ViewDirection * ComputeFocusDistance(FMath::Max(std::tanf(InTargetFov / 2), 0.01f), InTargetFov);
	Camera->SetPosition(StartPosition);

	MoveElapsedTime = 0.0f;
	bIsTransition = true;
}

void FOrthoToPerspect::Tick(float DeltaTime)
{
	if (!Camera || !bIsTransition)
	{
		return;
	}
	Camera->SetProjectionMode(ECameraProjectionMode::Perspective);

	MoveElapsedTime += DeltaTime;
	const float MoveAlpha = FocusTime > 0.0f ? FMath::Clamp(MoveElapsedTime / FocusTime, 0.0f, 1.0f) : 1.0f;
	const float EasedAlpha = EvaluateEaseInOut(MoveAlpha);

	const float CurrentFov = LerpFloat(MinPerspectiveFOV, TargetFov, EasedAlpha);

	const FVector CurrentPosition = PivotPosition - ViewDirection * ComputeFocusDistance(FMath::Max(std::tanf(TargetFov / 2), 0.01f), CurrentFov);
	Camera->SetPosition(CurrentPosition);
	Camera->SetFOV(CurrentFov);


	if (MoveAlpha >= 1.0f)
	{


		bIsTransition = false;
	}
}
