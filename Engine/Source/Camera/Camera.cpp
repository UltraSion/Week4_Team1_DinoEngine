#include "Camera.h"
#include <algorithm>
#include <cmath>
#include "Math/MathUtility.h"

namespace
{
#if IS_OBJ_VIEWER
	constexpr float MinOrbitPitchDegrees = -89.0f;
	constexpr float MaxOrbitPitchDegrees = 89.0f;

	float NormalizeAngleDegrees(float Angle)
	{
		while (Angle > 180.0f)
		{
			Angle -= 360.0f;
		}
		while (Angle <= -180.0f)
		{
			Angle += 360.0f;
		}
		return Angle;
	}
#endif
}

void FCamera::SetPosition(const FVector& InPosition)
{
	Position = InPosition;

#if IS_OBJ_VIEWER
	PanOffset = FVector::ZeroVector;
	OrbitDistance = Position.Size();

	const FVector ForwardToTarget = (-Position).GetSafeNormal();
	if (!ForwardToTarget.IsNearlyZero())
	{
		Yaw = FMath::RadiansToDegrees(std::atan2(ForwardToTarget.Y, ForwardToTarget.X));
		Pitch = FMath::RadiansToDegrees(std::asin(std::clamp(ForwardToTarget.Z, -1.0f, 1.0f)));
	}
#else
#endif
}

void FCamera::SetRotation(float InYaw, float InPitch)
{
	Yaw = InYaw;
	Pitch = InPitch;

#if IS_OBJ_VIEWER
	Yaw = NormalizeAngleDegrees(Yaw);
	Pitch = std::clamp(Pitch, MinOrbitPitchDegrees, MaxOrbitPitchDegrees);

	if (OrbitDistance <= 0.0f)
	{
		OrbitDistance = Position.Size();
	}

	const float YawRad = FMath::DegreesToRadians(Yaw);
	const float PitchRad = FMath::DegreesToRadians(Pitch);

	FVector ForwardToTarget;
	ForwardToTarget.X = cosf(PitchRad) * cosf(YawRad);
	ForwardToTarget.Y = cosf(PitchRad) * sinf(YawRad);
	ForwardToTarget.Z = sinf(PitchRad);

	Position = -ForwardToTarget * OrbitDistance + PanOffset;
#else
#endif
}

FVector FCamera::GetForward() const
{
	float RadYaw = FMath::DegreesToRadians(Yaw);
	float RadPitch = FMath::DegreesToRadians(Pitch);

	// 변경 (Z-up, 언리얼 방식)
	FVector Forward;
	Forward.X = cosf(RadPitch) * cosf(RadYaw);   // X가 Forward
	Forward.Y = cosf(RadPitch) * sinf(RadYaw);   // Y가 Right
	Forward.Z = sinf(RadPitch);                   // Z가 상하
	return Forward.GetSafeNormal();
}

FVector FCamera::GetRight() const
{
	return FVector::CrossProduct(Up, GetForward()).GetSafeNormal();
}

void FCamera::MoveForward(float Delta)
{
#if IS_OBJ_VIEWER
	if (OrbitDistance <= 0.0f)
	{
		OrbitDistance = Position.Size();
	}

	OrbitDistance = (std::max)(0.01f, OrbitDistance - (Delta * Speed));
	Position = -GetForward() * OrbitDistance + PanOffset;
	return;
#else
#endif

	FVector Forward = GetForward();
	Position = Position + Forward * (Delta * Speed);
}

void FCamera::MoveRight(float Delta)
{
	FVector Right = GetRight();
	OffsetPosition(Right * (Delta * Speed));
}

void FCamera::MoveUp(float Delta)
{
	OffsetPosition(Up * (Delta * Speed));
}

void FCamera::OffsetPosition(const FVector& Delta)
{
	Position = Position + Delta;

#if IS_OBJ_VIEWER
	PanOffset = PanOffset + Delta;
#else
#endif
}

void FCamera::Rotate(float DeltaYaw, float DeltaPitch)
{
	Yaw += DeltaYaw;
	Pitch += DeltaPitch;

#if IS_OBJ_VIEWER
	Yaw = NormalizeAngleDegrees(Yaw);
	Pitch = std::clamp(Pitch, MinOrbitPitchDegrees, MaxOrbitPitchDegrees);

	if (OrbitDistance <= 0.0f)
	{
		OrbitDistance = Position.Size();
	}

	const float YawRad = FMath::DegreesToRadians(Yaw);
	const float PitchRad = FMath::DegreesToRadians(Pitch);
	
	FVector ForwardToTarget;
	ForwardToTarget.X = cosf(PitchRad) * cosf(YawRad);
	ForwardToTarget.Y = cosf(PitchRad) * sinf(YawRad);
	ForwardToTarget.Z = sinf(PitchRad);
	Position = -ForwardToTarget * OrbitDistance + PanOffset;
#else
#endif
}

FMatrix FCamera::GetViewMatrix() const
{
	FVector Target = Position + GetForward();
	return FMatrix::MakeViewLookAtLH(Position, Target, Up);
}

FMatrix FCamera::GetProjectionMatrix() const
{
	if (ProjectionMode == ECameraProjectionMode::Orthographic)
	{
		const float SafeViewWidth = (std::max)(OrthoWidth, 0.01f);
		const float SafeAspectRatio = (std::max)(AspectRatio, 0.01f);
		return FMatrix::MakeOrthographicLH(SafeViewWidth, SafeViewWidth / SafeAspectRatio, NearPlane, FarPlane);
	}

	return FMatrix::MakePerspectiveFovLH(FMath::DegreesToRadians(FOV), AspectRatio, NearPlane, FarPlane);
}

void FCamera::SetAspectRatio(float InAspectRatio)
{
	AspectRatio = (std::max)(InAspectRatio, 0.01f);
}

FVector FCamera::GetPosition() const
{
	return Position;
}

float FCamera::GetYaw() const
{
	return Yaw;
}

float FCamera::GetPitch() const
{
	return Pitch;
}

float FCamera::GetFOV() const
{
	return FOV;
}

void FCamera::SetFOV(float InFOV)
{
	FOV = std::clamp(InFOV, 1.0f, 179.0f);
}

ECameraProjectionMode FCamera::GetProjectionMode() const
{
	return ProjectionMode;
}

bool FCamera::IsOrthographic() const
{
	return ProjectionMode == ECameraProjectionMode::Orthographic;
}

void FCamera::SetProjectionMode(ECameraProjectionMode InProjectionMode)
{
	ProjectionMode = InProjectionMode;
}

float FCamera::GetOrthoWidth() const
{
	return OrthoWidth;
}

float FCamera::GetOrthoHeight() const
{
	return OrthoWidth / (std::max)(AspectRatio, 0.01f);
}

void FCamera::SetOrthoWidth(float InOrthoWidth)
{
	OrthoWidth = (std::max)(InOrthoWidth, 0.01f);
}
