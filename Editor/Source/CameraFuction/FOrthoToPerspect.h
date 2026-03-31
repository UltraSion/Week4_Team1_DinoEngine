#pragma once

#include "CoreMinimal.h"
#include "ICameraFunction.h"

class FCamera;

/**
 * @brief Orthographic → Perspective 카메라 전환 클래스
 *
 * Orthographic 카메라를 Perspective 카메라로 부드럽게 전환한다.
 *
 * 단순 ProjectionMode 스위치가 아니라:
 * 1. 현재 Ortho 상태의 프레이밍을 유지한 채 위치/회전을 보간하고
 * 2. 중간 시점에서 Perspective로 전환한 뒤
 * 3. FOV를 확장하여 자연스럽게 원근감을 되살린다.
 *
 * 특정 Pivot(관심 지점)을 기준으로 카메라 위치를 계산하여
 * 전환 전후에도 대상이 크게 튀지 않도록 한다.
 */
class FOrthoToPerspect : public ICameraFunction
{
private:
	// ===== 시작 상태 =====

	// 전환 시작 시 카메라 위치
	FVector StartPosition = FVector::ZeroVector;

	// 전환 시작 시 카메라 회전 (X=Yaw, Y=Pitch, Z=Roll)
	FVector StartRotation = FVector::ZeroVector;

	// 전환 시작 시 Orthographic Width
	float StartOrthoWidth = 0.0f;


	// ===== 목표 상태 =====

	// 전환 종료 시 카메라 위치
	FVector TargetPosition = FVector::ZeroVector;

	// 전환 종료 시 카메라 회전
	FVector TargetRotation = FVector::ZeroVector;

	// 전환 종료 시 Perspective FOV
	float TargetFOV = 0.0f;


	// ===== 기준 정보 =====

	// 카메라가 바라볼 중심 지점 (Focus 대상)
	FVector PivotPosition = FVector::ZeroVector;

	// Pivot으로부터 카메라까지 거리
	float CameraDistance = 0.0f;


	// ===== 시간 제어 =====

	// 전체 전환 시간
	float TransitionTime = 1.0f;

	// 현재까지 경과 시간
	float ElapsedTime = 0.0f;

	// 현재 전환 진행 여부
	bool bIsTransitioning = false;


	// ===== 내부 상태 =====

	// ProjectionMode가 이미 Perspective로 전환되었는지 여부
	// (중간 타이밍에서 1회만 변경하기 위해 사용)
	bool bSwitchedToPerspective = false;


public:
	/**
	 * @brief Orthographic → Perspective 전환 시작
	 *
	 * @param InPivotPosition   카메라가 바라볼 중심 (Focus 대상)
	 * @param InTargetRotation  목표 회전값 (X=Yaw, Y=Pitch, Z=Roll)
	 * @param InTargetFOV       목표 Perspective FOV
	 * @param InTransitionTime  전환 시간
	 */
	void StartTransition(
		const FVector& InPivotPosition,
		const FVector& InTargetRotation,
		float InTargetFOV,
		float InTransitionTime);

	/**
	 * @brief 매 프레임 호출되어 전환을 진행한다.
	 */
	void Tick(float DeltaTime);

	/**
	 * @brief 전환 완료 여부 반환
	 */
	bool IsFinished() const;


private:
	/**
	 * @brief 실제 전환 업데이트 로직
	 */
	void UpdateTransition(float DeltaTime);

	/**
	 * @brief EaseInOut 보간 함수 (0~1)
	 */
	float EvaluateEaseInOut(float T) const;

	/**
	 * @brief Pivot과 회전값으로부터 카메라 위치를 계산한다.
	 *
	 * 전환 중에도 같은 대상을 계속 바라보는 느낌을 유지하기 위해 사용된다.
	 */
	FVector CalculateOrbitPosition(
		const FVector& InPivotPosition,
		const FVector& InRotation,
		float InDistance) const;

	/**
	 * @brief 회전값 보간 (각도 wrapping 고려)
	 */
	FVector InterpolateRotation(
		const FVector& InStartRotation,
		const FVector& InTargetRotation,
		float T) const;
};