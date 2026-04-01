#include "FPerspectToOrtho.h"

#include "Actor/Actor.h"
#include "Camera/Camera.h"
#include "Math/MathUtility.h"
#include "Component/PrimitiveComponent.h"

#include <cmath>

void FPerspectToOrtho::StartTransition(AActor* InTargetActor)
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
	StartPosition = Camera->GetPosition();
	StartFOV = Camera->GetFOV();
	ViewDirection = Camera->GetForward().GetSafeNormal();
	PivotPosition = Bounds.Center;
	if (ViewDirection.IsNearlyZero())
	{
		ViewDirection = FVector::ForwardVector;
	}

	TargetRadius = Bounds.Radius * 2.f;
	StartPosition = Bounds.Center - ViewDirection * ComputeFocusDistance(TargetRadius, Camera->GetFOV());
	TargetPosition = Bounds.Center - ViewDirection * ComputeFocusDistance(TargetRadius, MinPerspectiveFOV);

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
	const FVector CurrentPosition = PivotPosition - ViewDirection * ComputeFocusDistance(TargetRadius, CurrentFov);

	Camera->SetFOV(CurrentFov);
	Camera->SetPosition(CurrentPosition);


	if (MoveAlpha >= 1.0f)
	{
		bIsTransition = false;
		Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
		Camera->SetOrthoHeight(TargetRadius * 2.f);
		Camera->SetPosition(PivotPosition - ViewDirection * TargetRadius * 2.f);
	}
}
