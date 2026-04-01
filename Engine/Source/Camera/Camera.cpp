#include "Camera.h"
#include "Core/LaunchOptions.h"
#include <algorithm>
#include <cmath>
#include "Math/MathUtility.h"

namespace
{
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
}

void FCamera::SetPosition(const FVector& InPosition)
{
	Position = InPosition;

	if (FLaunchOptions::IsObjViewerMode())
	{
		OrbitDistance = (Position - OrbitTarget).Size();
		if (OrbitDistance <= 0.0f)
		{
			OrbitDistance = (Position - OrbitTarget).Size();
			if (OrbitDistance <= 0.0f)
			{
				OrbitDistance = Position.Size();
			}
		}

		const FVector ForwardToTarget = (OrbitTarget - Position).GetSafeNormal();
		if (!ForwardToTarget.IsNearlyZero())
		{
			Yaw = FMath::RadiansToDegrees(std::atan2(ForwardToTarget.Y, ForwardToTarget.X));
			Pitch = FMath::RadiansToDegrees(std::asin(std::clamp(ForwardToTarget.Z, -1.0f, 1.0f)));
		}
	}
}

/**
 * orbit rotation을 할 대상을 정합니다.
 * 
 * \param InOrbitTarget
 */
void FCamera::SetOrbitTarget(const FVector& InOrbitTarget)
{
	OrbitTarget = InOrbitTarget;

	if (OrbitDistance <= 0.0f)
	{
		OrbitDistance = (Position - OrbitTarget).Size();
		if (OrbitDistance <= 0.0f)
		{
			OrbitDistance = Position.Size();
		}
	}

	Position = OrbitTarget - GetForward() * OrbitDistance;
}

void FCamera::SetRotation(float InYaw, float InPitch)
{
	Yaw = InYaw;
	Pitch = InPitch;

	if (FLaunchOptions::IsObjViewerMode())
	{
		Yaw = NormalizeAngleDegrees(Yaw);
		Pitch = std::clamp(Pitch, MinOrbitPitchDegrees, MaxOrbitPitchDegrees);

		if (OrbitDistance <= 0.0f)
		{
			OrbitDistance = (Position - OrbitTarget).Size();
			if (OrbitDistance <= 0.0f)
			{
				OrbitDistance = Position.Size();
			}
		}

		const float YawRad = FMath::DegreesToRadians(Yaw);
		const float PitchRad = FMath::DegreesToRadians(Pitch);

		FVector ForwardToTarget;
		ForwardToTarget.X = cosf(PitchRad) * cosf(YawRad);
		ForwardToTarget.Y = cosf(PitchRad) * sinf(YawRad);
		ForwardToTarget.Z = sinf(PitchRad);

		Position = OrbitTarget - ForwardToTarget * OrbitDistance;
	}
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
	if (FLaunchOptions::IsObjViewerMode())
	{
		// OrbitDistance를 업데이트해줍니다.
		if (OrbitDistance <= 0.0f)
		{
			OrbitDistance = (Position - OrbitTarget).Size();
			if (OrbitDistance <= 0.0f)
			{
				OrbitDistance = Position.Size();
			}
		}

		OrbitDistance = (std::max)(0.01f, OrbitDistance - (Delta * Speed));
		Position = OrbitTarget - GetForward() * OrbitDistance;
		return;
	}

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

	if (FLaunchOptions::IsObjViewerMode())
	{
		OrbitTarget = OrbitTarget + Delta;
	}
}

void FCamera::Rotate(float DeltaYaw, float DeltaPitch)
{
	Yaw += DeltaYaw;
	Pitch += DeltaPitch;

	if (FLaunchOptions::IsObjViewerMode())
	{
		Yaw = NormalizeAngleDegrees(Yaw);
		Pitch = std::clamp(Pitch, MinOrbitPitchDegrees, MaxOrbitPitchDegrees);

		if (OrbitDistance <= 0.0f)
		{
			OrbitDistance = (Position - OrbitTarget).Size();
			if (OrbitDistance <= 0.0f)
			{
				OrbitDistance = Position.Size();
			}
		}

		const float YawRad = FMath::DegreesToRadians(Yaw);
		const float PitchRad = FMath::DegreesToRadians(Pitch);
	
		FVector ForwardToTarget;
		ForwardToTarget.X = cosf(PitchRad) * cosf(YawRad);
		ForwardToTarget.Y = cosf(PitchRad) * sinf(YawRad);
		ForwardToTarget.Z = sinf(PitchRad);
		Position = OrbitTarget - ForwardToTarget * OrbitDistance;
	}
}

FMatrix FCamera::GetViewMatrix() const
{
	FVector Target = FLaunchOptions::IsObjViewerMode() ? OrbitTarget : Position + GetForward();
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
	FOV = std::clamp(InFOV, 0.1f, 179.9f);
}

float FCamera::GetNearClip() const
{
	return NearPlane;
}

float FCamera::GetFarClip() const
{
	return FarPlane;
}

void FCamera::SetNearClip(float InNearClip)
{
	NearPlane = (std::max)(InNearClip, 0.001f);
	if (FarPlane <= NearPlane)
	{
		FarPlane = NearPlane + 0.001f;
	}
}

void FCamera::SetFarClip(float InFarClip)
{
	FarPlane = (std::max)(InFarClip, NearPlane + 0.001f);
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

void FCamera::SetOrthoHeight(float InOrthoHeight)
{
	OrthoWidth = InOrthoHeight * ((std::max)(InOrthoHeight, 0.01f));
}
