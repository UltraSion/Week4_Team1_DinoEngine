#pragma once

#include "CoreMinimal.h"

class FCamera;
class AActor;

class FFocus
{
	FCamera* Camera = nullptr;

	FVector TargetPosition;
	FVector StartPosition;

	float FocusTime = 1.0f;
	float MoveElapsedTime = 0.0f;

	bool bIsMoving = false;

public:
	void SetCamera(FCamera* InCamera) { Camera = InCamera; }
	void FocusOnActor(const AActor* TargetActor);
	void MoveTowardsTarget(float DeltaTime);

private:
	float EvaluateEaseInOut(float T) const;
};
