#include "FOrthoToPerspect.h"

#include "Camera/Camera.h"
#include "Math/MathUtility.h"

#include "Actor/Actor.h"
#include "Component/PrimitiveComponent.h"

#include <cmath>

void FOrthoToPerspect::StartTransition(AActor* InTargetActor, float InTargetFov)
{
	if (!Camera || !InTargetActor)
	{
		return;
	}

	UPrimitiveComponent* PrimitiveComponent = InTargetActor->GetComponentByClass<UPrimitiveComponent>();
	if (!PrimitiveComponent)
	{
		return;
	}

	const FBoxSphereBounds Bounds = PrimitiveComponent->GetWorldBounds();
	TargetFov = InTargetFov;
	ViewDirection = Camera->GetForward().GetSafeNormal();
	PivotPosition = Bounds.Center;
	if (ViewDirection.IsNearlyZero())
	{
		ViewDirection = FVector::ForwardVector;
	}

	TargetRadius = Bounds.Radius * 1.4f;
	StartPosition = Camera->GetPosition() - ViewDirection * ComputeFocusDistance(TargetRadius, MinPerspectiveFOV);;
	TargetPosition = Bounds.Center - ViewDirection * ComputeFocusDistance(TargetRadius, InTargetFov);
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

	const FVector CurrentPosition = PivotPosition - ViewDirection * ComputeFocusDistance(TargetRadius, CurrentFov);
	Camera->SetPosition(CurrentPosition);
	Camera->SetFOV(CurrentFov);


	if (MoveAlpha >= 1.0f)
	{


		bIsTransition = false;
	}
}
