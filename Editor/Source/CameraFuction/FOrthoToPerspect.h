#pragma once

#include "CoreMinimal.h"
#include "ICameraFunction.h"

class AActor;
class FCamera;

class FOrthoToPerspect : public ICameraFunction
{
private:
	float TargetFov = 45.f;
	FVector StartPosition = FVector::ZeroVector;
	FVector TargetPosition = FVector::ZeroVector;
	FVector ViewDirection = FVector::ZeroVector;
	FVector PivotPosition = FVector::ZeroVector;

	bool bIsTransition = false;
	float MoveElapsedTime = 0.0f;
	float FocusTime = 1.f;


public:
	void StartTransition(float InTargetFov);
	virtual bool IsFinished() const override { return !bIsTransition; }
	virtual void Tick(float DeltaTime) override;
};