#include "CameraComponent.h"
#include "Object/Class.h"
#include "Camera/Camera.h"

IMPLEMENT_RTTI(UCameraComponent, USceneComponent)

void UCameraComponent::Initialize()
{
	bCanEverTick = true;
	Camera = new FCamera();
}

UCameraComponent::~UCameraComponent()
{
	delete Camera;
	Camera = nullptr;
}

void UCameraComponent::Tick(float DeltaTime)
{
	USceneComponent::Tick(DeltaTime);

	//TODO : will be add CameraArm, shake and interpolation  
}

void UCameraComponent::MoveForward(float Value)
{
#if IS_OBJ_VIEWER
#else
	Camera->MoveForward(Value);
#endif
}

void UCameraComponent::MoveRight(float Value)
{
#if IS_OBJ_VIEWER
#else
	Camera->MoveRight(Value);
#endif
}

void UCameraComponent::MoveUp(float Value)
{
#if IS_OBJ_VIEWER
#else
	Camera->MoveUp(Value);
#endif
}

void UCameraComponent::PanRight(float Value)
{
	const FVector Right = Camera->GetRight().GetSafeNormal();
	Camera->OffsetPosition(Right * (Value * Camera->GetSpeed()));
}

void UCameraComponent::PanUp(float Value)
{
	//카메라 기준 local up을 계산합니다.
	const FVector Forward = Camera->GetForward().GetSafeNormal();
	const FVector Right = Camera->GetRight().GetSafeNormal();
	const FVector PanUp = FVector::CrossProduct(Forward, Right).GetSafeNormal();
	Camera->OffsetPosition(PanUp * (Value * Camera->GetSpeed()));
}

void UCameraComponent::FootZoom(float Value)
{
	if (Camera->IsOrthographic())
	{
		const float ZoomScale = (Value * 0.1f);
		Camera->SetOrthoWidth(Camera->GetOrthoWidth() * ZoomScale);
		return;
	}
#if IS_OBJ_VIEWER
	Camera->MoveForward(Value*0.1f);
#else
	Camera->MoveForward(Value);
#endif
}

void UCameraComponent::Rotate(float DeltaYaw, float DeltaPitch)
{
	Camera->Rotate(DeltaYaw, DeltaPitch);
}

FCamera* UCameraComponent::GetCamera() const
{
	return Camera;
}

FMatrix UCameraComponent::GetViewMatrix() const
{
	return Camera->GetViewMatrix();
}

FMatrix UCameraComponent::GetProjectionMatrix() const
{
	return Camera->GetProjectionMatrix();
}

void UCameraComponent::SetFov(float inFov)
{
	Camera->SetFOV(inFov);
}

void UCameraComponent::SetSpeed(float Inspeed)
{
	Camera->SetSpeed(Inspeed);
}

void UCameraComponent::SetSensitivity(float InSetSensitivity)
{
	Camera->SetMouseSensitivity(InSetSensitivity);
}
